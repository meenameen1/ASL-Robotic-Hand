import os
import json
import psutil
import pyaudio
import queue
import threading
from vosk import Model, KaldiRecognizer
from flask import Flask, render_template_string
from flask_socketio import SocketIO
from collections import deque

# --- CONFIGURATION ---
MODEL_PATH = "vosk-model-small-en-us-0.15" 
SAMPLE_RATE = 16000
# smaller chunks = faster "Accepted" triggers
CHUNK_SIZE = 2000 
WEB_PORT = 5000
WEB_HOST = "0.0.0.0"

audio_queue = queue.Queue()
output_history = deque(maxlen=100)  # Store last 100 outputs
output_lock = threading.Lock()

# --- FLASK WEB SERVER SETUP ---
app = Flask(__name__)
app.config['SECRET_KEY'] = 'vosk-stt-server'
socketio = SocketIO(app, cors_allowed_origins="*")

HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Vosk STT Server</title>
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
            max-width: 800px;
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
            grid-template-columns: 1fr 1fr;
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
            max-height: 400px;
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
            grid-template-columns: 2fr 2fr 1fr;
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
        .stat-box.highlight {
            border-left-color: #e74c3c;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎤 Vosk Speech-to-Text Server</h1>
        <div class="status active" id="status">● LISTENING</div>
        
        <div class="stats">
            <div class="stat-box">
                <div class="stat-label">RAM Usage</div>
                <div class="stat-value" id="ram-usage">-- MB</div>
            </div>
            <div class="stat-box">
                <div class="stat-label">Outputs</div>
                <div class="stat-value" id="output-count">0</div>
            </div>
        </div>
        
        <div class="output-section">
            <h2>Recent Outputs</h2>
            <div class="output-list" id="output-list">
                <div class="empty-state">Waiting for speech input...</div>
            </div>
        </div>
    </div>

    <script>
        const socket = io();
        
        socket.on('connect', function() {
            document.getElementById('status').textContent = '● LISTENING';
            document.getElementById('status').className = 'status active';
        });
        
        socket.on('disconnect', function() {
            document.getElementById('status').textContent = '● DISCONNECTED';
            document.getElementById('status').className = 'status inactive';
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
                <div>${data.ram.toFixed(2)} MB</div>
            `;
            outputList.insertBefore(item, outputList.firstChild);
            
            // Remove animation class after animation completes
            setTimeout(() => item.classList.remove('new'), 500);
            
            // Update count
            document.getElementById('output-count').textContent = data.count;
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

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

@socketio.on('connect')
def handle_connect():
    print("[WEB] Client connected")
    # Send existing history to new client
    with output_lock:
        for output in list(output_history):
            socketio.emit('new_output', output)

def emit_output(text, letters, ram):
    """Emit output to all connected web clients"""
    output_data = {
        'text': text,
        'letters': str(letters),
        'ram': ram,
        'count': len(output_history)
    }
    with output_lock:
        output_history.append(output_data)
    socketio.emit('new_output', output_data)

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

# Start Flask web server in a separate thread
def run_web_server():
    socketio.run(app, host=WEB_HOST, port=WEB_PORT, debug=False, use_reloader=False)

web_thread = threading.Thread(target=run_web_server, daemon=True)
web_thread.start()

print(f"Loading Model... Starting RAM: {get_ram_usage():.2f} MB")
print(f"🌐 Web Server running at http://{WEB_HOST}:{WEB_PORT}")
print("\n--- SYSTEM ACTIVE: Speak clearly ---")
print(f"{'Final Word(s)':<25} | {'Letters for Motors':<25} | {'RAM (MB)':<10}")
print("-" * 70)

model = Model(MODEL_PATH)
rec = KaldiRecognizer(model, SAMPLE_RATE)
rec.SetMaxAlternatives(0) 

threading.Thread(target=record_audio, daemon=True).start()

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
                emit_output(text, letters, ram)
                
                # Here you would call: motor_controller.send(letters)
                
except KeyboardInterrupt:
    print("\nClosing PoC...")