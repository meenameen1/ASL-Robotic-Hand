"""
stt_send.py
────────────────────────────────────────────────────────────────────────────────
Listens to the microphone, transcribes speech with moonshine-voice, strips the
transcripts down to lowercase letters + spaces, and streams them one at a
time to a Raspberry Pi Pico (RP2350) over the Pi 4's hardware UART at
2-second intervals.

Hardware transport:
  The Pi 4 sits on top of the custom PCB via its 40-pin GPIO header (J43).
  The board routes Pi GPIO14 (TX) and GPIO15 (RX) to the RP2350's UART1
  pins (GPIO20/21), so no USB cable or jumper wires are needed between
  the two controllers. The Pi's serial device is /dev/serial0.

Prerequisites (one-time setup on the Pi 4):
  sudo raspi-config
    -> Interface Options -> Serial Port
    -> Login shell over serial? NO
    -> Serial port hardware enabled? YES
    -> Reboot

Install deps:
    pip install moonshine-voice numpy sounddevice scipy pyserial

Usage:
    python stt_send.py
"""

import queue
import re
import threading
import time
from collections import deque

import numpy as np
import sounddevice as sd
import serial                                   # pyserial

from moonshine_voice import (                   # type: ignore
    Transcriber,
    TranscriptEventListener,
    get_model_for_language,
)

from audio_capture import AudioCapture, TARGET_RATE, BLOCK_DURATION_MS

# ── Configuration ────────────────────────────────────────────────────────────

# --- Microphone ------------------------------------------------------------
DEFAULT_MIC_DEVICE_INDEX = 2           # Usual USB mic slot on this Pi
MIC_POLL_INTERVAL_S      = 1.0         # How often to check for plug/unplug

# --- Moonshine -------------------------------------------------------------
LANGUAGE          = "en"
UPDATE_INTERVAL_S = 0.5

# --- Pico UART link --------------------------------------------------------
# /dev/serial0 is the Pi 4's hardware UART (GPIO14 TX / GPIO15 RX), which
# is wired on the PCB straight to the RP2350's UART1 pins.
PICO_SERIAL_PORT  = "/dev/serial0"
PICO_BAUD_RATE    = 115_200

# --- Fingerspelling cadence ------------------------------------------------
LETTER_INTERVAL_S = 2.0                # One letter every 2 seconds
SEND_SPACES       = True               # Letters + spaces (per user choice)

# --- Letter queue ----------------------------------------------------------
# ~2 sentences of conversational backlog. Drop-oldest keeps the robot
# near real-time with live speech when the speaker runs long.
LETTER_QUEUE_MAX  = 150
DROP_POLICY       = "oldest"           # "oldest" | "newest"

# --- Audio buffer (feeds the STT worker) ----------------------------------
AUDIO_QUEUE_MAX   = int(4.0 * 1000 / BLOCK_DURATION_MS)


# ── Letter cleanup ───────────────────────────────────────────────────────────

# Keep a-z only; everything else (digits, punctuation, accents) becomes
# whitespace, which then collapses into single-space separators.
_NON_LETTER_RE = re.compile(r"[^a-z]+")

def clean_for_fingerspelling(text: str) -> str:
    """Lowercase; letters only; single spaces between words."""
    lowered  = text.lower()
    squashed = _NON_LETTER_RE.sub(" ", lowered).strip()
    if not SEND_SPACES:
        squashed = squashed.replace(" ", "")
    return squashed


# ── Serial link to the Pico over UART ────────────────────────────────────────

class PicoLink:
    """
    Thin pyserial wrapper over the Pi 4's hardware UART. Reopens the port
    transparently if it closes (e.g. the Pico briefly loses power).

    Wire format: one ASCII character + '\\n' per letter, so the Pico can
    parse with uart.readline().
    """

    def __init__(self, port: str, baud: int):
        self._port = port
        self._baud = baud
        self._serial: serial.Serial | None = None
        self._lock = threading.Lock()

    def _ensure_open(self) -> bool:
        if self._serial and self._serial.is_open:
            return True
        try:
            self._serial = serial.Serial(
                port=self._port, baudrate=self._baud,
                timeout=0.5, write_timeout=1.0,
            )
            print(f"[Pico] UART open on {self._port} @ {self._baud} baud")
            return True
        except (serial.SerialException, OSError) as exc:
            print(f"[Pico] UART open failed ({self._port}): {exc}")
            self._serial = None
            return False

    def send(self, ch: str) -> bool:
        with self._lock:
            if not self._ensure_open():
                return False
            try:
                self._serial.write(f"{ch}\n".encode("ascii"))
                self._serial.flush()
                return True
            except (serial.SerialException, OSError) as exc:
                print(f"[Pico] Write failed: {exc} — closing port")
                try: self._serial.close()
                except Exception: pass
                self._serial = None
                return False

    def close(self):
        with self._lock:
            if self._serial:
                try: self._serial.close()
                except Exception: pass
                self._serial = None


# ── Letter queue with drop-oldest policy ──────────────────────────────────────

class LetterQueue:
    def __init__(self, maxlen: int, drop: str = "oldest"):
        self._dq   = deque()
        self._max  = maxlen
        self._drop = drop
        self._cv   = threading.Condition()

    def extend(self, chars: str):
        if not chars:
            return
        with self._cv:
            for ch in chars:
                if len(self._dq) >= self._max:
                    if self._drop == "oldest":
                        self._dq.popleft()
                    else:
                        continue
                self._dq.append(ch)
            self._cv.notify_all()

    def pop_or_wait(self, timeout: float) -> str | None:
        with self._cv:
            if not self._dq:
                self._cv.wait(timeout=timeout)
            if not self._dq:
                return None
            return self._dq.popleft()

    def __len__(self):
        with self._cv:
            return len(self._dq)


# ── Moonshine listener ───────────────────────────────────────────────────────

class FingerspellListener(TranscriptEventListener):
    """Only completed utterances go into the queue — partials would re-send prefixes."""

    def __init__(self, letter_queue: LetterQueue):
        self._letters = letter_queue
        self._last_final_time = 0.0

    def on_line_started(self, event):  pass
    def on_line_text_changed(self, event):  pass

    def on_line_completed(self, event):
        raw = event.line.text or ""
        clean = clean_for_fingerspelling(raw)
        if not clean:
            return

        now = time.monotonic()
        if SEND_SPACES and self._last_final_time and now - self._last_final_time > 1.5:
            clean = " " + clean
        self._last_final_time = now

        print(f"[STT] \"{raw.strip()}\" -> queued {len(clean)} chars", flush=True)
        self._letters.extend(clean)


# ── Microphone presence monitor ──────────────────────────────────────────────

def microphone_present(device_index: int) -> bool:
    try:
        sd._terminate()
        sd._initialize()
        info = sd.query_devices(device_index, "input")
        return info.get("max_input_channels", 0) > 0
    except (sd.PortAudioError, ValueError, KeyError):
        return False


# ── STT manager — starts/stops capture as the mic plugs/unplugs ──────────────

class STTManager:
    def __init__(self, letter_queue: LetterQueue, mic_device: int = DEFAULT_MIC_DEVICE_INDEX):
        self._letters    = letter_queue
        self._mic_device = mic_device
        self._stop_event = threading.Event()

        self._capture: AudioCapture | None = None
        self._capture_thread: threading.Thread | None = None
        self._audio_queue: queue.Queue | None = None

        self._transcriber = None
        self._stream      = None
        self._stt_thread: threading.Thread | None = None

        self._supervisor_thread = threading.Thread(
            target=self._supervise, daemon=True, name="mic-supervisor"
        )

    def start(self):
        print("[STT] Loading moonshine-voice model...")
        t0 = time.perf_counter()
        model_path, model_arch = get_model_for_language(LANGUAGE)
        print(f"[STT] Model ready in {time.perf_counter() - t0:.1f}s")

        self._transcriber = Transcriber(model_path=model_path, model_arch=model_arch)
        self._stream = self._transcriber.create_stream(update_interval=UPDATE_INTERVAL_S)
        self._stream.add_listener(FingerspellListener(self._letters))
        self._stream.start()

        self._supervisor_thread.start()
        print("[STT] Supervisor running — listening when mic is "
              f"present at device {self._mic_device}.")

    def stop(self, timeout: float = 5.0):
        self._stop_event.set()
        self._stop_capture()
        self._supervisor_thread.join(timeout=timeout)
        if self._stream:
            try: self._stream.stop()
            except Exception: pass
        if self._transcriber:
            try: self._transcriber.stop()
            except Exception: pass
        print("[STT] Stopped.")

    def _start_capture(self):
        if self._capture is not None:
            return
        print(f"[STT] Mic detected at device {self._mic_device} — starting capture.")
        self._audio_queue = queue.Queue(maxsize=AUDIO_QUEUE_MAX)
        self._capture     = AudioCapture(
            output_queue=self._audio_queue, device=self._mic_device
        )
        self._capture_thread = threading.Thread(
            target=self._capture.start, daemon=True, name="audio-capture"
        )
        self._capture_thread.start()
        self._stt_thread = threading.Thread(
            target=self._feed_loop, daemon=True, name="stt-feed"
        )
        self._stt_thread.start()

    def _stop_capture(self):
        if self._capture is None:
            return
        print("[STT] Mic gone — pausing capture (queue keeps draining).")
        self._capture.stop()
        if self._capture_thread: self._capture_thread.join(timeout=2.0)
        if self._stt_thread:     self._stt_thread.join(timeout=2.0)
        self._capture = None
        self._capture_thread = None
        self._stt_thread = None
        self._audio_queue = None

    def _feed_loop(self):
        assert self._audio_queue is not None and self._stream is not None
        while self._capture and not self._stop_event.is_set():
            try:
                block = self._audio_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            try:
                self._stream.add_audio(block, TARGET_RATE)
            except Exception as exc:
                print(f"[STT] add_audio error: {exc}")

    def _supervise(self):
        last_state: bool | None = None
        while not self._stop_event.is_set():
            present = microphone_present(self._mic_device)
            if present != last_state:
                if present: self._start_capture()
                else:       self._stop_capture()
                last_state = present
            self._stop_event.wait(MIC_POLL_INTERVAL_S)


# ── Pico sender — ticks one letter every LETTER_INTERVAL_S ───────────────────

class PicoSender:
    """Pops one char every LETTER_INTERVAL_S and writes it to the Pico."""

    def __init__(self, letter_queue: LetterQueue, pico: PicoLink):
        self._letters = letter_queue
        self._pico    = pico
        self._stop_event = threading.Event()
        self._thread  = threading.Thread(
            target=self._run, daemon=True, name="pico-sender"
        )

    def start(self):
        self._thread.start()

    def stop(self, timeout: float = 5.0):
        self._stop_event.set()
        self._thread.join(timeout=timeout)
        self._pico.close()

    def _run(self):
        while not self._stop_event.is_set():
            ch = self._letters.pop_or_wait(timeout=LETTER_INTERVAL_S)
            if ch is None:
                continue
            label     = "_" if ch == " " else ch
            sent      = self._pico.send(ch)
            remaining = len(self._letters)
            print(f"[Pico] '{label}'  (sent={sent}, queued={remaining})",
                  flush=True)
            self._stop_event.wait(LETTER_INTERVAL_S)


# ── Entry point ──────────────────────────────────────────────────────────────

def main():
    letters = LetterQueue(maxlen=LETTER_QUEUE_MAX, drop=DROP_POLICY)
    pico    = PicoLink(port=PICO_SERIAL_PORT, baud=PICO_BAUD_RATE)

    sender  = PicoSender(letter_queue=letters, pico=pico)
    stt     = STTManager(letter_queue=letters, mic_device=DEFAULT_MIC_DEVICE_INDEX)

    sender.start()
    stt.start()

    print("[Main] Running. Ctrl-C to exit.")
    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        print("\n[Main] Shutting down...")
    finally:
        stt.stop()
        sender.stop()
        print("[Main] Done.")


if __name__ == "__main__":
    main()
