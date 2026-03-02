import os
import json
import psutil
import pyaudio
import queue
import threading
import time
from vosk import Model, KaldiRecognizer
from flask import Flask, render_template_string
from flask_socketio import SocketIO
from collections import deque

MODEL_PATH = "vosk-model-small-en-us-0.15" 
SAMPLE_RATE = 16000
CHUNK_SIZE = 2000 
WEB_PORT = 5001
WEB_HOST = "0.0.0.0"
LETTER_SEND_INTERVAL = 1.5 # seconds between sending letters

audio_queue = queue.Queue()
letter_queue = deque()  
output_history = deque(maxlen=100)
queue_lock = threading.Lock()
output_lock = threading.Lock()
recognition_enabled = True  
recognition_lock = threading.Lock()

# --- FLASK WEB SERVER SETUP ---
app = Flask(__name__)
app.config['SECRET_KEY'] = 'queue-controller'
socketio = SocketIO(app, cors_allowed_origins="*")

HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>ASL Robotic Hand - Queue Controller</title>
    <script src="https://cdn.socket.io/4.5.4/socket.io.min.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.3);
            width: 100%;
            max-width: 900px;
            padding: 30px;
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            text-align: center;
        }
        .status {
            text-align: center;
            margin-bottom: 20px;
            font-size: 14px;
        }
        .status.active {
            color: #27ae60;
            font-weight: bold;
        }
        .status.inactive {
            color: #e74c3c;
        }
        .stats {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 15px;
            margin-bottom: 25px;
        }
        .stat-box {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }
        .stat-label {
            font-size: 12px;
            color: #666;
            text-transform: uppercase;
            margin-bottom: 5px;
        }
        .stat-value {
            font-size: 24px;
            font-weight: bold;
            color: #333;
        }
        .queue-section {
            margin: 25px 0;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 8px;
            border: 2px solid #667eea;
        }
        .queue-section h2 {
            font-size: 18px;
            color: #333;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .queue-container {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            min-height: 60px;
            align-items: center;
            padding: 15px;
            background: white;
            border-radius: 8px;
        }
        .queue-empty {
            color: #999;
            font-style: italic;
        }
        .letter-box {
            display: flex;
            justify-content: center;
            align-items: center;
            width: 50px;
            height: 50px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            font-size: 24px;
            font-weight: bold;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
            transition: all 0.3s ease;
        }
        .letter-box.first {
            background: linear-gradient(135deg, #f12711 0%, #f5af19 100%);
            box-shadow: 0 4px 12px rgba(241, 39, 17, 0.4);
            animation: pulse 0.5s ease-out;
        }
        @keyframes pulse {
            0% { transform: scale(1.2); }
            100% { transform: scale(1); }
        }
        .output-section {
            margin-top: 25px;
        }
        .output-section h2 {
            font-size: 18px;
            color: #333;
            margin-bottom: 15px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .output-list {
            max-height: 300px;
            overflow-y: auto;
            background: #f8f9fa;
            border-radius: 8px;
            padding: 0;
        }
        .output-item {
            padding: 12px 15px;
            border-bottom: 1px solid #e0e0e0;
            font-family: 'Courier New', monospace;
            font-size: 14px;
            display: grid;
            grid-template-columns: 2fr 3fr;
            gap: 10px;
        }
        .output-item:last-child {
            border-bottom: none;
        }
        .output-item:hover {
            background: #e8f0f8;
        }
        .output-item.new {
            background: #d4edda;
            animation: fadeIn 0.5s ease-in;
        }
        @keyframes fadeIn {
            from { background: #fff3cd; }
            to { background: #d4edda; }
        }
        .empty-state {
            padding: 40px 20px;
            text-align: center;
            color: #999;
        }
        .control-section {
            margin: 25px 0;
            text-align: center;
        }
        .toggle-btn {
            padding: 12px 30px;
            font-size: 16px;
            font-weight: bold;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
        }
        .toggle-btn.active {
            background: linear-gradient(135deg, #27ae60 0%, #229954 100%);
            color: white;
        }
        .toggle-btn.active:hover {
            box-shadow: 0 6px 16px rgba(39, 174, 96, 0.4);
            transform: translateY(-2px);
        }
        .toggle-btn.inactive {
            background: linear-gradient(135deg, #e74c3c 0%, #c0392b 100%);
            color: white;
        }
        .toggle-btn.inactive:hover {
            box-shadow: 0 6px 16px rgba(231, 76, 60, 0.4);
            transform: translateY(-2px);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 ASL Robotic Hand - Queue Controller</h1>
        <div class="status active" id="status">● LISTENING & QUEUEING</div>
        
        <div class="stats">
            <div class="stat-box">
                <div class="stat-label">RAM Usage</div>
                <div class="stat-value" id="ram-usage">-- MB</div>
            </div>
            <div class="stat-box">
                <div class="stat-label">Queue Length</div>
                <div class="stat-value" id="queue-length">0</div>
            </div>
            <div class="stat-box">
                <div class="stat-label">Letters Sent</div>
                <div class="stat-value" id="sent-count">0</div>
            </div>
        </div>
        
        <div class="queue-section">
            <h2>📋 Current Queue</h2>
            <div class="queue-container" id="queue-display">
                <span class="queue-empty">Queue is empty...</span>
            </div>
        </div>

        <div class="control-section">
            <button class="toggle-btn active" id="recognition-btn" onclick="toggleRecognition()">⏸️ Stop Recognition</button>
        </div>
        
        <div class="output-section">
            <h2>🎤 Speech Recognition</h2>
            <div class="output-list" id="output-list">
                <div class="empty-state">Waiting for speech input...</div>
            </div>
        </div>
    </div>

    <script>
        const socket = io();
        let sentCount = 0;
        let recognitionActive = true;
        
        function toggleRecognition() {
            recognitionActive = !recognitionActive;
            socket.emit('toggle_recognition', { enabled: recognitionActive });
            updateButtonState();
        }
        
        function updateButtonState() {
            const btn = document.getElementById('recognition-btn');
            if (recognitionActive) {
                btn.textContent = '⏸️ Stop Recognition';
                btn.className = 'toggle-btn active';
            } else {
                btn.textContent = '▶️ Start Recognition';
                btn.className = 'toggle-btn inactive';
            }
        }
        
        socket.on('connect', function() {
            document.getElementById('status').textContent = '● LISTENING & QUEUEING';
            document.getElementById('status').className = 'status active';
        });
        
        socket.on('disconnect', function() {
            document.getElementById('status').textContent = '● DISCONNECTED';
            document.getElementById('status').className = 'status inactive';
        });
        
        socket.on('queue_update', function(data) {
            const queueDisplay = document.getElementById('queue-display');
            
            if (data.queue.length === 0) {
                queueDisplay.innerHTML = '<span class="queue-empty">Queue is empty...</span>';
            } else {
                queueDisplay.innerHTML = '';
                data.queue.forEach((letter, index) => {
                    const letterBox = document.createElement('div');
                    letterBox.className = 'letter-box' + (index === 0 ? ' first' : '');
                    letterBox.textContent = letter;
                    queueDisplay.appendChild(letterBox);
                });
            }
            
            document.getElementById('queue-length').textContent = data.queue.length;
        });
        
        
        socket.on('new_output', function(data) {
            const outputList = document.getElementById('output-list');
            if (outputList.innerHTML.includes('empty-state')) {
                outputList.innerHTML = '';
            }
            
            const item = document.createElement('div');
            item.className = 'output-item new';
            item.innerHTML = `
                <div><strong>${escapeHtml(data.text)}</strong></div>
                <div><code>${escapeHtml(data.letters)}</code></div>
            `;
            outputList.insertBefore(item, outputList.firstChild);
            
            setTimeout(() => item.classList.remove('new'), 500);
        });
        
        socket.on('update_ram', function(data) {
            document.getElementById('ram-usage').textContent = data.ram.toFixed(2) + ' MB';
        });
        
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
    </script>
</body>
</html>
"""

def get_ram_usage():
    return psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)

def record_audio():
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=SAMPLE_RATE, 
                    input=True, frames_per_buffer=CHUNK_SIZE)
    stream.start_stream()
    while True:
        audio_queue.put(stream.read(CHUNK_SIZE, exception_on_overflow=False))

def send_letters_periodically():
    while True:
        time.sleep(LETTER_SEND_INTERVAL)
        with queue_lock:
            if letter_queue:
                letter = letter_queue.popleft()
                print(f"📤 SENT TO HAND: {letter} | Queue size: {len(letter_queue)}")
                socketio.emit('letter_sent', {'letter': letter})
                # Update queue display
                socketio.emit('queue_update', {'queue': list(letter_queue)})

def broadcast_ram_usage():
    while True:
        time.sleep(1.0)  # Update every 1 second
        ram = get_ram_usage()
        socketio.emit('update_ram', {'ram': ram})

def emit_queue_update():
    with queue_lock:
        socketio.emit('queue_update', {'queue': list(letter_queue)})

def add_letters_to_queue(letters):
    with queue_lock:
        for letter in letters:
            letter_queue.append(letter)
        socketio.emit('queue_update', {'queue': list(letter_queue)})
    print(f"📥 ADDED TO QUEUE: {letters} | Queue size: {len(letter_queue)}")

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

@socketio.on('connect')
def handle_connect():
    print("[WEB] Client connected")
    with queue_lock:
        socketio.emit('queue_update', {'queue': list(letter_queue)})
    for output in list(output_history):
        socketio.emit('new_output', output)

@socketio.on('toggle_recognition')
def handle_toggle_recognition(data):
    global recognition_enabled
    with recognition_lock:
        recognition_enabled = data['enabled']
    status = "ACTIVE" if recognition_enabled else "PAUSED"
    print(f"[WEB] Recognition {status}")
    socketio.emit('recognition_status', {'enabled': recognition_enabled})

if not os.path.exists(MODEL_PATH):
    print("Please ensure model path is correct.")
    exit()

# Start Flask web server in a separate thread
def run_web_server():
    socketio.run(app, host=WEB_HOST, port=WEB_PORT, debug=False, use_reloader=False)

web_thread = threading.Thread(target=run_web_server, daemon=True)
web_thread.start()

# Start the letter sending thread
send_thread = threading.Thread(target=send_letters_periodically, daemon=True)
send_thread.start()

# Start the RAM usage broadcast thread
ram_thread = threading.Thread(target=broadcast_ram_usage, daemon=True)
ram_thread.start()

print(f"Loading Model... Starting RAM: {get_ram_usage():.2f} MB")
print(f"🌐 Web Server running at http://{WEB_HOST}:{WEB_PORT}")
print(f"⏱️  Letters sent to hand every {LETTER_SEND_INTERVAL} seconds")
print("\n--- SYSTEM ACTIVE: Speak clearly ---")
print(f"{'Recognized Words':<25} | {'Letters Added to Queue':<30} | {'Queue Size':<10}")
print("-" * 70)

model = Model(MODEL_PATH)
rec = KaldiRecognizer(model, SAMPLE_RATE)
rec.SetMaxAlternatives(0) 

threading.Thread(target=record_audio, daemon=True).start()

try:
    while True:
        data = audio_queue.get()
        with recognition_lock:
            is_enabled = recognition_enabled
        
        if is_enabled and rec.AcceptWaveform(data):
            res = json.loads(rec.Result())
            text = res.get("text", "").strip()
            
            if text:
                letters = [char.upper() for char in text if char.isalpha()]
                ram = get_ram_usage()
                
                print(f"{text:<25} | {str(letters):<30} | {len(letter_queue):>8}")
                
                # Add to queue and emit to web
                add_letters_to_queue(letters)
                
                # Log to output history
                output_data = {
                    'text': text,
                    'letters': str(letters)
                }
                with output_lock:
                    output_history.append(output_data)
                socketio.emit('new_output', output_data)
                
except KeyboardInterrupt:
    print("\nClosing Queue Controller...")
