import os
import json
import psutil
import pyaudio
import queue
import threading
from vosk import Model, KaldiRecognizer

# --- CONFIGURATION ---
MODEL_PATH = "vosk-model-small-en-us-0.15" 
SAMPLE_RATE = 16000
# smaller chunks = faster "Accepted" triggers
CHUNK_SIZE = 2000 

audio_queue = queue.Queue()

def get_ram_usage():
    """tracks RAM for your PoC team presentation."""
    return psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)

def record_audio():
    """background thread to ensure we never miss a sample"""
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=SAMPLE_RATE, 
                    input=True, frames_per_buffer=CHUNK_SIZE)
    stream.start_stream()
    while True:
        audio_queue.put(stream.read(CHUNK_SIZE, exception_on_overflow=False))

if not os.path.exists(MODEL_PATH):
    print("Please ensure model path is correct.")
    exit()

print(f"Loading Model... Starting RAM: {get_ram_usage():.2f} MB")
model = Model(MODEL_PATH)
rec = KaldiRecognizer(model, SAMPLE_RATE)
rec.SetMaxAlternatives(0) 

threading.Thread(target=record_audio, daemon=True).start()

print("\n--- SYSTEM ACTIVE: Speak clearly ---")
print(f"{'Final Word(s)':<25} | {'Letters for Motors':<25} | {'RAM (MB)':<10}")
print("-" * 70)

try:
    while True:
        data = audio_queue.get()
        if rec.AcceptWaveform(data):
            # triggers only when a phrase/word is fully 'vetted'
            res = json.loads(rec.Result())
            text = res.get("text", "").strip()
            
            if text:
                letters = [char.upper() for char in text if char.isalpha()]
                ram = get_ram_usage()
                
                print(f"{text:<25} | {str(letters):<25} | {ram:>8.2f} MB")
                
                # Here you would call: motor_controller.send(letters)
                
except KeyboardInterrupt:
    print("\nClosing PoC...")