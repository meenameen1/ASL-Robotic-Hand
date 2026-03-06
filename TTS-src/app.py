

import json
import queue
import threading
import time
import os

import psutil
import pyaudio
import vosk
from flask import Flask, render_template
from flask_socketio import SocketIO

# ── Import your functions unchanged ──────────────────────────────────────────
from demo import (
    initialize_model,
    initialize_pyaudio,
    open_microphone_stream,
)

app      = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

stop_event    = threading.Event()
pause_event   = threading.Event()  # True = paused, False = listening
pause_event.set()  # Start in paused state
letter_queue  = queue.Queue()


# ── Mirrors print_ram() but emits to UI instead of printing ──────────────────
def ram_monitor():
    process = psutil.Process(os.getpid())
    while not stop_event.is_set():
        ram_mb = process.memory_info().rss / (1024 ** 2)
        socketio.emit("ram_update", {"ram_mb": ram_mb})
        time.sleep(5)


# ── Mirrors send_to_hardware() but emits to UI ───────────────────────────────
def send_to_hardware():
    while not stop_event.is_set():
        time.sleep(2)
        try:
            letter = letter_queue.get_nowait()
            socketio.emit("sending_letter",  {"letter": letter})
            socketio.emit("queue_update",    list(letter_queue.queue))
        except queue.Empty:
            pass


# ── Mirrors recognize_speech() but emits to UI ───────────────────────────────
def stt_loop(model, audio_device, stream, recognizer):
    last_partial  = ""
    result_times  = []
    partial_times = []
    last_latency_emit = time.perf_counter()

    try:
        while not stop_event.is_set():
            # Skip processing if paused
            if pause_event.is_set():
                time.sleep(0.1)
                continue
            
            chunk_start = time.perf_counter()
            data        = stream.read(4096, exception_on_overflow=False)

            if recognizer.AcceptWaveform(data):
                result_times.append(time.perf_counter() - chunk_start)
                text = json.loads(recognizer.Result()).get("text", "")
                if text:
                    socketio.emit("log", {"line": f"You said: {text}"})
                    for letter in text:
                        if letter.strip():
                            letter_queue.put(letter.upper())
                    socketio.emit("queue_update", list(letter_queue.queue))
                last_partial = ""
            else:
                t0           = time.perf_counter()
                partial_text = json.loads(recognizer.PartialResult()).get("partial", "")
                partial_times.append(time.perf_counter() - t0)
                if partial_text != last_partial:
                    last_partial = partial_text

            # Emit latency updates every 0.5 seconds
            current_time = time.perf_counter()
            if current_time - last_latency_emit >= 0.5:
                latency_data = {}
                if result_times:
                    latency_data["avg_result"] = (sum(result_times) / len(result_times)) * 1000
                    latency_data["max_result"] = max(result_times) * 1000
                if partial_times:
                    latency_data["avg_partial"] = (sum(partial_times) / len(partial_times)) * 1000
                if latency_data:
                    socketio.emit("latency_update", latency_data)
                last_latency_emit = current_time

    except Exception as e:
        socketio.emit("log", {"line": f"Error: {e}"})
    finally:
        if result_times:
            avg_r = sum(result_times) / len(result_times)
            max_r = max(result_times)
            socketio.emit("latency_update", {"avg_result": avg_r*1000, "max_result": max_r*1000})
            socketio.emit("log", {"line": f"Avg Result Latency: {avg_r*1000:.2f}ms"})
            socketio.emit("log", {"line": f"Max Result Latency: {max_r*1000:.2f}ms"})
        if partial_times:
            avg_p = sum(partial_times) / len(partial_times)
            socketio.emit("latency_update", {"avg_partial": avg_p*1000})
            socketio.emit("log", {"line": f"Avg Partial Latency: {avg_p*1000:.2f}ms"})

        stop_event.set()
        stream.stop_stream()
        stream.close()
        audio_device.terminate()


# ── Initialize Vosk and PyAudio in main thread ───────────────────────────────
model_path   = "vosk-model-small-en-us-0.15"
model        = initialize_model(model_path)
audio_device = initialize_pyaudio()
stream       = open_microphone_stream(audio_device, 16000, 1, 8192)
recognizer   = vosk.KaldiRecognizer(model, 16000)

# ── Start background threads ──────────────────────────────────────────────────
threading.Thread(target=ram_monitor,    daemon=True).start()
threading.Thread(target=send_to_hardware, daemon=True).start()
threading.Thread(target=stt_loop, args=(model, audio_device, stream, recognizer), daemon=True).start()


# ── Routes ────────────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return render_template("index.html")


@socketio.on("start_recognizing")
def on_start_recognizing():
    pause_event.clear()
    socketio.emit("recognizing_status", {"status": "listening"})


@socketio.on("stop_recognizing")
def on_stop_recognizing():
    pause_event.set()
    socketio.emit("recognizing_status", {"status": "paused"})


if __name__ == "__main__":
    socketio.run(app, port=5005, use_reloader=False)