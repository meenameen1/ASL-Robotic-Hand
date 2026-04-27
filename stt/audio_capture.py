"""
audio_capture.py
────────────────────────────────────────────────────────────────────────────────
Captures audio from a 48 kHz microphone, applies a FIR anti-aliasing low-pass
filter, and downsamples to 16 kHz for moonshine-voice.

Architecture — why no callback:
  The audio callback approach has a fundamental problem on the Pi 4: the
  callback runs on portaudio's thread but still requires the GIL to execute
  Python code. When moonshine-voice holds the GIL during inference (which
  can last 50-150 ms on the Pi 4), the audio callback literally cannot run,
  portaudio's hardware buffer overflows, and samples are lost.

  The fix is to use sd.InputStream in blocking-read mode (no callback at
  all). Portaudio then uses its own internal thread to fill a large buffer,
  and we pull from that buffer on our own Python thread. The buffer size is
  controlled by `latency=` — we request 300 ms, which means even if our
  Python thread is blocked for 200 ms waiting on the GIL, no audio is lost.

Install deps:
    pip install sounddevice numpy scipy
"""

import time
import queue
import threading
import numpy as np
import sounddevice as sd
from scipy.signal import firwin, lfilter, lfilter_zi

# ── Configuration ────────────────────────────────────────────────────────────

MIC_SAMPLE_RATE   = 48_000
TARGET_RATE       = 16_000
DOWNSAMPLE_FACTOR = MIC_SAMPLE_RATE // TARGET_RATE   # 3

CHANNELS          = 1
BLOCK_DURATION_MS = 60                               # 60 ms per read
BLOCK_SIZE        = int(MIC_SAMPLE_RATE * BLOCK_DURATION_MS / 1000)  # 2880 samples

# Portaudio internal buffer size. 300 ms gives us plenty of headroom for
# any GIL-hold from inference. This is the *single knob* that fixes the
# overflow issue — anything ≥ ~200 ms should be safe on a Pi 4.
INPUT_LATENCY_S   = 0.3

FIR_NUM_TAPS  = 127
FIR_CUTOFF_HZ = 7_500


def _build_filter():
    nyquist = MIC_SAMPLE_RATE / 2.0
    coeffs  = firwin(FIR_NUM_TAPS, FIR_CUTOFF_HZ / nyquist, window=("kaiser", 8.0))
    zi      = lfilter_zi(coeffs, [1.0]).copy()
    return coeffs, zi


class AudioCapture:
    """
    Blocking-read pipeline:

        [portaudio native thread]
             │  fills its internal 300 ms buffer (no GIL needed)
             ▼
        [stream.read()]  ← called from our _reader thread
             │  FIR filter + decimate to 16 kHz
             ▼
        [output_queue]  ← consumed by STT worker
    """

    def __init__(self, output_queue: queue.Queue, device: int | None = None):
        self.output_queue = output_queue
        self.device       = device

        self._fir_coeffs, self._fir_zi = _build_filter()
        self._stop_event = threading.Event()
        self._overflow_count = 0
        self._last_overflow_warn = 0.0

    def _reader(self, stream: sd.InputStream):
        """Blocking-read loop on a dedicated thread."""
        while not self._stop_event.is_set():
            # stream.read() releases the GIL while waiting, so the STT
            # thread can do inference during that wait. When inference
            # finishes and we get the GIL back, we pull accumulated
            # samples out of portaudio's buffer all at once.
            try:
                data, overflowed = stream.read(BLOCK_SIZE)
            except sd.PortAudioError as exc:
                print(f"[AudioCapture] Read error: {exc}")
                continue

            if overflowed:
                self._overflow_count += 1
                now = time.monotonic()
                if now - self._last_overflow_warn > 5.0:
                    print(f"[AudioCapture] {self._overflow_count} overflow(s) "
                          f"in the last 5 s — consider raising "
                          f"INPUT_LATENCY_S or UPDATE_INTERVAL_S")
                    self._overflow_count = 0
                    self._last_overflow_warn = now

            # Extract mono float32 column
            mono = data[:, 0] if data.ndim > 1 else data
            mono = mono.astype(np.float32, copy=False)

            # FIR low-pass filter — stateful zi preserves phase across blocks
            filtered, self._fir_zi = lfilter(
                self._fir_coeffs, [1.0], mono, zi=self._fir_zi
            )
            downsampled = filtered[::DOWNSAMPLE_FACTOR].astype(np.float32)

            try:
                self.output_queue.put_nowait(downsampled)
            except queue.Full:
                # Drop oldest to keep latency bounded
                try:
                    self.output_queue.get_nowait()
                    self.output_queue.put_nowait(downsampled)
                except (queue.Empty, queue.Full):
                    pass

    def start(self):
        print(f"[AudioCapture] {MIC_SAMPLE_RATE} Hz -> FIR {FIR_NUM_TAPS}-tap "
              f"{FIR_CUTOFF_HZ} Hz cutoff -> x{DOWNSAMPLE_FACTOR} decimate "
              f"-> {TARGET_RATE} Hz")
        print(f"[AudioCapture] Portaudio buffer: {INPUT_LATENCY_S*1000:.0f} ms")

        stream = sd.InputStream(
            samplerate=MIC_SAMPLE_RATE,
            blocksize=BLOCK_SIZE,
            device=self.device,
            channels=CHANNELS,
            dtype="float32",
            latency=INPUT_LATENCY_S,
            # no callback -> blocking-read mode
        )

        with stream:
            print("[AudioCapture] Streaming... (Ctrl-C to stop)")
            reader_thread = threading.Thread(
                target=self._reader, args=(stream,),
                daemon=True, name="audio-reader"
            )
            reader_thread.start()
            try:
                while not self._stop_event.is_set():
                    time.sleep(0.1)
            finally:
                self._stop_event.set()
                reader_thread.join(timeout=1.0)

        print("[AudioCapture] Stream closed.")

    def stop(self):
        self._stop_event.set()


if __name__ == "__main__":
    import sys
    if "--list-devices" in sys.argv:
        print(sd.query_devices())
        raise SystemExit
    device_index = int(sys.argv[1]) if len(sys.argv) > 1 else None
    out_q: queue.Queue = queue.Queue(maxsize=200)
    capture = AudioCapture(output_queue=out_q, device=device_index)
    try:
        capture.start()
    except KeyboardInterrupt:
        capture.stop()