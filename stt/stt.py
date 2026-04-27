"""
stt.py
────────────────────────────────────────────────────────────────────────────────
Feeds 16 kHz audio blocks from audio_capture.py into moonshine-voice.

The audio capture now uses a blocking-read architecture with a 300 ms
portaudio buffer, so GIL-hold from inference no longer causes overflows.
That means this worker can be simple — no batching, no catch-up logic.

Install deps:
    pip install moonshine-voice numpy

Usage:
    python stt.py [device_index]
    python audio_capture.py --list-devices   # find your mic index
"""

import queue
import threading
import time

from moonshine_voice import (       # type: ignore
    Transcriber,
    TranscriptEventListener,
    get_model_for_language,
)

from audio_capture import AudioCapture, TARGET_RATE, BLOCK_DURATION_MS

# ── Configuration ────────────────────────────────────────────────────────────

LANGUAGE          = "en"

# Partial-update cadence — how often moonshine re-transcribes buffered audio
# while the user is still speaking. 0.5 s balances responsiveness and CPU.
UPDATE_INTERVAL_S = 0.5

# Queue headroom (seconds) — amply more than the portaudio buffer, so the
# drain-pressure never reaches this layer.
QUEUE_MAXSIZE = int(4.0 * 1000 / BLOCK_DURATION_MS)   # ~66 blocks


# ── Event listener ────────────────────────────────────────────────────────────

class SpeechListener(TranscriptEventListener):
    def __init__(self, on_final=None, on_partial=None):
        self._on_final   = on_final
        self._on_partial = on_partial

    def on_line_started(self, event):
        print("\n[STT] Speech detected...", flush=True)

    def on_line_text_changed(self, event):
        text = event.line.text.strip()
        if text and self._on_partial:
            self._on_partial(text)

    def on_line_completed(self, event):
        text = event.line.text.strip()
        if text and self._on_final:
            self._on_final(text)


# ── STT worker ────────────────────────────────────────────────────────────────

class STTWorker:
    def __init__(self, input_queue: queue.Queue, on_final=None, on_partial=None):
        self.input_queue = input_queue
        self.on_final    = on_final
        self.on_partial  = on_partial

        self._stop_event  = threading.Event()
        self._stream      = None
        self._transcriber = None
        self._thread      = threading.Thread(
            target=self._run, daemon=True, name="stt-feed"
        )

    def start(self):
        print("[STT] Loading model...")
        t0 = time.perf_counter()
        model_path, model_arch = get_model_for_language(LANGUAGE)
        print(f"[STT] Model ready in {time.perf_counter() - t0:.1f}s")

        self._transcriber = Transcriber(
            model_path=model_path, model_arch=model_arch
        )
        self._stream = self._transcriber.create_stream(
            update_interval=UPDATE_INTERVAL_S
        )
        self._stream.add_listener(SpeechListener(
            on_final=self.on_final,
            on_partial=self.on_partial,
        ))
        self._stream.start()

        self._thread.start()
        print("[STT] Ready — listening for speech.")

    def stop(self, timeout: float = 5.0):
        self._stop_event.set()
        self._thread.join(timeout=timeout)
        if self._stream:
            self._stream.stop()
        if self._transcriber:
            self._transcriber.stop()
        print("[STT] Worker stopped.")

    def _run(self):
        while not self._stop_event.is_set():
            try:
                block = self.input_queue.get(timeout=0.1)
            except queue.Empty:
                continue

            try:
                self._stream.add_audio(block, TARGET_RATE)
            except Exception as exc:
                print(f"[STT] add_audio error: {exc}")
            finally:
                self.input_queue.task_done()


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import sys

    device_index = int(sys.argv[1]) if len(sys.argv) > 1 else None

    audio_queue: queue.Queue = queue.Queue(maxsize=QUEUE_MAXSIZE)

    def on_final(text: str):
        print(f">>> {text}\n", flush=True)

    def on_partial(text: str):
        print(f"\r    [{text}]          ", end="", flush=True)

    worker = STTWorker(
        input_queue=audio_queue,
        on_final=on_final,
        on_partial=on_partial,
    )
    worker.start()

    capture = AudioCapture(output_queue=audio_queue, device=device_index)

    try:
        capture.start()
    except KeyboardInterrupt:
        print("\n[Pipeline] Shutting down...")
    finally:
        capture.stop()
        time.sleep(0.5)
        worker.stop()
        print("[Pipeline] Done.")