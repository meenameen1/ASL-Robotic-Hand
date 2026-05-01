vimport queue
import re
import threading
import time

import serial
from evdev import InputDevice, categorize, ecodes, list_devices

from moonshine_voice import (
    Transcriber,
    TranscriptEventListener,
    get_model_for_language,
)

from audio_capture import AudioCapture, TARGET_RATE, BLOCK_DURATION_MS

LANGUAGE          = "en"
UPDATE_INTERVAL_S = 0.1
QUEUE_MAXSIZE     = int(4.0 * 1000 / BLOCK_DURATION_MS)

PICO_SERIAL_PORT  = "/dev/serial0"
PICO_BAUD_RATE    = 56000
LETTER_INTERVAL_S = 0.1
SEND_SPACES       = True

_CLEAN_RE = re.compile(r"[^a-z ]+")

KEY_TO_CHAR = {
    ecodes.KEY_A: "a", ecodes.KEY_B: "b", ecodes.KEY_C: "c", ecodes.KEY_D: "d",
    ecodes.KEY_E: "e", ecodes.KEY_F: "f", ecodes.KEY_G: "g", ecodes.KEY_H: "h",
    ecodes.KEY_I: "i", ecodes.KEY_J: "j", ecodes.KEY_K: "k", ecodes.KEY_L: "l",
    ecodes.KEY_M: "m", ecodes.KEY_N: "n", ecodes.KEY_O: "o", ecodes.KEY_P: "p",
    ecodes.KEY_Q: "q", ecodes.KEY_R: "r", ecodes.KEY_S: "s", ecodes.KEY_T: "t",
    ecodes.KEY_U: "u", ecodes.KEY_V: "v", ecodes.KEY_W: "w", ecodes.KEY_X: "x",
    ecodes.KEY_Y: "y", ecodes.KEY_Z: "z", ecodes.KEY_SPACE: " ",
    ecodes.KEY_BACKSPACE: "\b", ecodes.KEY_ENTER: "\r", ecodes.KEY_3: "3", ecodes.KEY_ESC: "\x1b",
}


class SpeechListener(TranscriptEventListener):
    def __init__(self, on_final):
        self._on_final = on_final

    def on_line_completed(self, event):
        text = event.line.text.strip()
        if text:
            self._on_final(text)


class STTWorker:
    def __init__(self, input_queue, on_final):
        self.input_queue = input_queue
        self.on_final    = on_final

        self._stop_event  = threading.Event()
        self._stream      = None
        self._transcriber = None
        self._thread      = None

    def start(self):
        model_path, model_arch = get_model_for_language(LANGUAGE)
        self._transcriber = Transcriber(
            model_path=model_path, model_arch=model_arch
        )
        self._stream = self._transcriber.create_stream(
            update_interval=UPDATE_INTERVAL_S
        )
        self._stream.add_listener(SpeechListener(on_final=self.on_final))
        self._stream.start()

        self._stop_event.clear()
        self._thread = threading.Thread(
            target=self._run, daemon=True, name="stt-feed"
        )
        self._thread.start()

    def stop(self, timeout=5.0):
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=timeout)
        if self._stream:
            self._stream.stop()
        if self._transcriber:
            self._transcriber.stop()
        self._stream = None
        self._transcriber = None
        self._thread = None

    def _run(self):
        while not self._stop_event.is_set():
            try:
                block = self.input_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            try:
                self._stream.add_audio(block, TARGET_RATE)
            except Exception:
                pass
            finally:
                self.input_queue.task_done()


class UARTSender:
    def __init__(self, port, baud, interval_s):
        self._serial = serial.Serial(
            port,
            baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1,
        )
        self._interval_s  = interval_s
        self._letters     = queue.Queue()
        self._stop_event  = threading.Event()
        self._thread      = threading.Thread(
            target=self._run, daemon=True, name="uart-sender"
        )

    def start(self):
        self._thread.start()

    def stop(self, timeout=5.0):
        self._stop_event.set()
        self._thread.join(timeout=timeout)
        self._serial.close()

    def enqueue_text(self, text):
        cleaned = _CLEAN_RE.sub("", text.lower()).upper()
        for ch in cleaned:
            if ch == " " and not SEND_SPACES:
                continue
            self._letters.put(ch)

    def enqueue_char(self, ch):
        if ch == " " and not SEND_SPACES:
            return
        if ch == " " or ("a" <= ch <= "z") or ch == "\b" or ch == "\r" or ch == "3" or ch == "\x1b":
            self._letters.put(ch.upper())

    def _run(self):
        while not self._stop_event.is_set():
            try:
                ch = self._letters.get(timeout=0.1)
            except queue.Empty:
                continue
            try:
                self._serial.write(ch.encode())
                self._serial.flush()
                print(f"{ch} ({ord(ch)})", flush=True)
            except Exception:
                pass
            time.sleep(self._interval_s)


def find_keyboard(timeout_s=15):
    print(f"[Keyboard] Waiting up to {timeout_s}s for keyboard to enumerate...", flush=True)
    start_time = time.time()

    while time.time() - start_time < timeout_s:
        for path in list_devices():
            try:
                dev = InputDevice(path)
                caps = dev.capabilities().get(ecodes.EV_KEY, [])
                if ecodes.KEY_1 in caps and ecodes.KEY_2 in caps:
                    return dev
            except OSError:
                # Ignore devices we can't read or that disappear
                pass

        time.sleep(1.0) # Wait 1 second before checking again

    return None


# class KeyboardController:
#     def __init__(self, on_mic_mode, on_keyboard_mode, on_letter):
#         self._on_mic_mode      = on_mic_mode
#         self._on_keyboard_mode = on_keyboard_mode
#         self._on_letter        = on_letter

#         self._keyboard_mode = False
#         self._stop_event    = threading.Event()
#         self._thread        = threading.Thread(
#             target=self._run, daemon=True, name="keyboard"
#         )

#     def start(self):
#         self._thread.start()

#     def stop(self, timeout=2.0):
#         self._stop_event.set()
#         self._thread.join(timeout=timeout)

#     def _run(self):
#         dev = find_keyboard()
#         if dev is None:
#             print("[Keyboard] No keyboard found.", flush=True)
#             return
#         print(f"[Keyboard] Using {dev.name}. "
#               "Press 1=mic mode, 2=keyboard mode.", flush=True)
#         try:
#             for event in dev.read_loop():
#                 if self._stop_event.is_set():
#                     break
#                 if event.type != ecodes.EV_KEY:
#                     continue
#                 key = categorize(event)
#                 if key.keystate != key.key_down:
#                 # if key.keystate != ecodes.KeyEvent.key_down:
#                     continue

#                 code = key.scancode
#                 if code == ecodes.KEY_1:
#                     self._keyboard_mode = False
#                     self._on_mic_mode()
#                 elif code == ecodes.KEY_2:
#                     self._keyboard_mode = True
#                     self._on_keyboard_mode()
#                 elif self._keyboard_mode and code in KEY_TO_CHAR:
#                     self._on_letter(KEY_TO_CHAR[code])
#         except OSError:
#             pass
class KeyboardController:
    def __init__(self, on_ptt_press, on_ptt_release, on_letter):
        self._on_ptt_press   = on_ptt_press
        self._on_ptt_release = on_ptt_release
        self._on_letter      = on_letter

        self._stop_event = threading.Event()
        self._thread     = threading.Thread(
            target=self._run, daemon=True, name="keyboard"
        )

    def start(self):
        self._thread.start()

    def stop(self, timeout=2.0):
        self._stop_event.set()
        self._thread.join(timeout=timeout)

    def _run(self):
        dev = find_keyboard()
        if dev is None:
            print("[Keyboard] No keyboard found.", flush=True)
            return

        print(f"[Keyboard] Using {dev.name}. "
              "Keyboard is ALWAYS ON. Hold '1' for Push-to-Talk.", flush=True)

        dev.grab() # Take exclusive control of the keyboard

        try:
            for event in dev.read_loop():
                if self._stop_event.is_set():
                    break
                if event.type != ecodes.EV_KEY:
                    continue

                key = categorize(event)
                code = key.scancode

                # --- PUSH TO TALK LOGIC ---
                if code == ecodes.KEY_1:
                    if key.keystate == key.key_down:
                        self._on_ptt_press()
                    elif key.keystate == key.key_up:
                        self._on_ptt_release()

                # --- ALWAYS-ON TYPING LOGIC ---
                else:
                    # Only send characters on the initial press (ignore hold/release)
                    if key.keystate == key.key_down:
                        if code in KEY_TO_CHAR:
                            self._on_letter(KEY_TO_CHAR[code])
        except OSError:
            pass
        finally:
            try:
                dev.ungrab()
            except OSError:
                pass


# if __name__ == "__main__":
#     import sys

#     device_index = int(sys.argv[1]) if len(sys.argv) > 1 else None

#     audio_queue = queue.Queue(maxsize=QUEUE_MAXSIZE)

#     sender = UARTSender(PICO_SERIAL_PORT, PICO_BAUD_RATE, LETTER_INTERVAL_S)
#     sender.start()

#     worker = STTWorker(
#         input_queue=audio_queue,
#         on_final=sender.enqueue_text,
#     )

#     capture = None
#     mic_active = False
#     state_lock = threading.Lock()

#     def stop_mic():
#         global capture, mic_active
#         if not mic_active:
#             return
#         if capture:
#             capture.stop()
#             capture = None
#         worker.stop()
#         mic_active = False

#     def enter_mic_mode():
#         global capture, mic_active
#         with state_lock:
#             if mic_active:
#                 return
#             print("[Control] MIC MODE", flush=True)
#             worker.start()
#             capture = AudioCapture(
#                 output_queue=audio_queue, device=device_index
#             )
#             threading.Thread(
#                 target=capture.start, daemon=True, name="audio-capture"
#             ).start()
#             mic_active = True

#     def enter_keyboard_mode():
#         with state_lock:
#             print("[Control] KEYBOARD MODE", flush=True)
#             stop_mic()

#     def on_keyboard_letter(ch):
#         sender.enqueue_char(ch)

#     keyboard = KeyboardController(
#         on_mic_mode=enter_mic_mode,
#         on_keyboard_mode=enter_keyboard_mode,
#         on_letter=on_keyboard_letter,
#     )
#     keyboard.start()

#     try:
#         while True:
#             time.sleep(1.0)
#     except KeyboardInterrupt:
#         pass
#     finally:
#         keyboard.stop()
#         with state_lock:
#             stop_mic()
#         sender.stop()
if __name__ == "__main__":
    import sys

    device_index = int(sys.argv[1]) if len(sys.argv) > 1 else None
    audio_queue = queue.Queue(maxsize=QUEUE_MAXSIZE)

    sender = UARTSender(PICO_SERIAL_PORT, PICO_BAUD_RATE, LETTER_INTERVAL_S)
    sender.start()

    print("\n[System] Loading STT Model... this will take a moment but only happens once.", flush=True)
    worker = STTWorker(
        input_queue=audio_queue,
        on_final=sender.enqueue_text,
    )
    worker.start() # LOAD THE MODEL ONCE HERE
    print("[System] STT Model loaded and ready!\n", flush=True)

    capture = None
    state_lock = threading.Lock()

    def on_ptt_press():
        global capture
        with state_lock:
            if capture is not None:
                return
            print("[Control] PTT HELD - Recording audio...", flush=True)
            capture = AudioCapture(
                output_queue=audio_queue, device=device_index
            )
            threading.Thread(
                target=capture.start, daemon=True, name="audio-capture"
            ).start()

    def on_ptt_release():
        global capture
        with state_lock:
            if capture is None:
                return
            print("[Control] PTT RELEASED - Stopping audio...", flush=True)
            capture.stop()
            capture = None

    def on_keyboard_letter(ch):
        sender.enqueue_char(ch)

    keyboard = KeyboardController(
        on_ptt_press=on_ptt_press,
        on_ptt_release=on_ptt_release,
        on_letter=on_keyboard_letter,
    )
    keyboard.start()

    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        keyboard.stop()
        with state_lock:
            if capture:
                capture.stop()
        worker.stop()
        sender.stop()
