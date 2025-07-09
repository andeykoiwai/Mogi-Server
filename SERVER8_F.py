from flask import Flask, request, jsonify, send_from_directory, render_template_string, session, redirect, url_for
import os
import speech_recognition as sr
import google.generativeai as genai
from gtts import gTTS
import random
import json
from datetime import datetime
import threading
import time
from tinydb import TinyDB, Query
from functools import wraps
import sqlite3

app = Flask(__name__)
app.secret_key = 'mogi_secret_key_2025'  # Secret key untuk session
client_sessions = {}  # Dictionary untuk menyimpan state per client
record_file = 'recording.wav'
output_file = 'output.mp3'
server_user = ''  # Menyimpan server_user global

# TinyDB file paths
SERIAL_VALIDATION_DB = 'mogi_serial_validation.json'
QUIZ_RESULTS_DB = 'mogi_quiz_results.json'
COMM_LOG_DB = 'mogi_comm_log.json'

# Initialize TinyDB databases
serial_db = TinyDB(SERIAL_VALIDATION_DB)
quiz_db = TinyDB(QUIZ_RESULTS_DB)
comm_log_db = TinyDB(COMM_LOG_DB)

# Setel API key secara manual (hanya untuk testing)
os.environ["GEMINI_API_KEY"] = "AIzaSyBgbnja1evGcdA9PxY-1P0yRf5Z7bohpPQ"
genai.configure(api_key=os.environ["GEMINI_API_KEY"])

model = genai.GenerativeModel("gemini-1.5-flash")

# Admin credentials
ADMIN_USERNAME = 'admin'
ADMIN_PASSWORD = 'admin'

# Login required decorator
def login_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'logged_in' not in session:
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated_function

# Login page
@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        if request.form['username'] == ADMIN_USERNAME and request.form['password'] == ADMIN_PASSWORD:
            session['logged_in'] = True
            return redirect(url_for('admin_dashboard'))
        return 'Invalid credentials'
    return render_template_string('''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Mogi Admin Login</title>
            <style>
                body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f2f5; }
                .login-container { max-width: 400px; margin: 100px auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
                h1 { text-align: center; color: #1a73e8; }
                .form-group { margin-bottom: 15px; }
                label { display: block; margin-bottom: 5px; }
                input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
                button { width: 100%; padding: 10px; background: #1a73e8; color: white; border: none; border-radius: 4px; cursor: pointer; }
                button:hover { background: #1557b0; }
            </style>
        </head>
        <body>
            <div class="login-container">
                <h1>Mogi Admin Login</h1>
                <form method="POST">
                    <div class="form-group">
                        <label>Username:</label>
                        <input type="text" name="username" required>
                    </div>
                    <div class="form-group">
                        <label>Password:</label>
                        <input type="password" name="password" required>
                    </div>
                    <button type="submit">Login</button>
                </form>
            </div>
        </body>
        </html>
    ''')

# Logout route
@app.route('/logout')
def logout():
    session.pop('logged_in', None)
    return redirect(url_for('login'))

# Admin dashboard
@app.route('/admin')
@login_required
def admin_dashboard():
    # Get data from all databases
    serials = serial_db.all()
    quiz_results = quiz_db.all()
    comm_logs = comm_log_db.all()
    
    # Get messages from database
    conn = sqlite3.connect('mogi_messages.db')
    c = conn.cursor()
    c.execute('''SELECT id, from_serial, to_serial, content, timestamp, is_read 
                 FROM messages 
                 ORDER BY timestamp DESC''')
    messages = c.fetchall()
    conn.close()
    
    return render_template_string('''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Mogi Admin Dashboard</title>
            <style>
                body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f2f5; }
                .container { max-width: 1200px; margin: 0 auto; }
                .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
                h1 { color: #1a73e8; margin: 0; }
                .logout-btn { padding: 8px 16px; background: #dc3545; color: white; text-decoration: none; border-radius: 4px; }
                .section { background: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
                h2 { color: #1a73e8; margin-top: 0; }
                table { width: 100%; border-collapse: collapse; margin-top: 10px; }
                th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
                th { background: #f8f9fa; }
                tr:hover { background: #f8f9fa; }
                .tab-buttons { margin-bottom: 20px; }
                .tab-button { padding: 10px 20px; margin-right: 10px; border: none; background: #e9ecef; cursor: pointer; }
                .tab-button.active { background: #1a73e8; color: white; }
                .tab-content { display: none; }
                .tab-content.active { display: block; }
                .broadcast-form {
                    margin-bottom: 20px;
                    padding: 15px;
                    background-color: #e9ecef;
                    border-radius: 4px;
                }
                .broadcast-form textarea {
                    width: 100%;
                    padding: 10px;
                    margin: 10px 0;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    resize: vertical;
                }
                .broadcast-form button {
                    background-color: #28a745;
                    color: white;
                    border: none;
                    padding: 10px 20px;
                    border-radius: 4px;
                    cursor: pointer;
                }
                .broadcast-form button:hover {
                    background-color: #218838;
                }
                .message {
                    border: 1px solid #ddd;
                    margin: 10px 0;
                    padding: 15px;
                    border-radius: 4px;
                    background-color: #fff;
                }
                .message.unread {
                    background-color: #f8f9fa;
                    border-left: 4px solid #007bff;
                }
                .message-header {
                    display: flex;
                    justify-content: space-between;
                    margin-bottom: 10px;
                    color: #666;
                    font-size: 0.9em;
                }
                .message-content {
                    color: #333;
                    margin: 10px 0;
                }
                .delete-btn {
                    background-color: #dc3545;
                    color: white;
                    border: none;
                    padding: 5px 10px;
                    border-radius: 4px;
                    cursor: pointer;
                }
                .delete-btn:hover {
                    background-color: #c82333;
                }
                .filter-form {
                    margin-bottom: 20px;
                    padding: 15px;
                    background-color: #f8f9fa;
                    border-radius: 4px;
                }
                .filter-form select, .filter-form input {
                    padding: 5px;
                    margin: 5px;
                }
                .filter-form button {
                    background-color: #007bff;
                    color: white;
                    border: none;
                    padding: 5px 15px;
                    border-radius: 4px;
                    cursor: pointer;
                }
                .filter-form button:hover {
                    background-color: #0056b3;
                }
                .nav-links {
                    margin-bottom: 20px;
                }
                .nav-links a {
                    display: inline-block;
                    padding: 10px 20px;
                    margin-right: 10px;
                    background-color: #1a73e8;
                    color: white;
                    text-decoration: none;
                    border-radius: 4px;
                }
                .nav-links a:hover {
                    background-color: #1557b0;
                }
            </style>
            <script>
                function showTab(tabName) {
                    document.querySelectorAll('.tab-content').forEach(content => {
                        content.classList.remove('active');
                    });
                    document.querySelectorAll('.tab-button').forEach(button => {
                        button.classList.remove('active');
                    });
                    document.getElementById(tabName).classList.add('active');
                    document.querySelector(`[onclick="showTab('${tabName}')"]`).classList.add('active');
                }

                // Broadcast message handler
                document.getElementById('broadcastForm').addEventListener('submit', async function(e) {
                    e.preventDefault();
                    const content = this.querySelector('textarea[name="content"]').value;
                    
                    try {
                        const response = await fetch('/broadcast_message', {
                            method: 'POST',
                            headers: {
                                'Content-Type': 'application/json',
                            },
                            body: JSON.stringify({ content: content })
                        });
                        
                        const data = await response.json();
                        if (response.ok) {
                            alert(data.message);
                            location.reload();
                        } else {
                            alert('Error: ' + data.error);
                        }
                    } catch (error) {
                        alert('Terjadi kesalahan: ' + error);
                    }
                });
            </script>
        </head>
        <body>
            <div class="container">
                <div class="header">
                    <h1>Mogi Admin Dashboard</h1>
                    <a href="/logout" class="logout-btn">Logout</a>
                </div>
                
                <div class="nav-links">
                    <a href="/messages">Messages</a>
                    <a href="/server_messages">Server Messages History</a>
                </div>
                
                <div class="tab-buttons">
                    <button class="tab-button active" onclick="showTab('serials')">Serial Numbers</button>
                    <button class="tab-button" onclick="showTab('quiz')">Quiz Results</button>
                    <button class="tab-button" onclick="showTab('logs')">Communication Logs</button>
                    <button class="tab-button" onclick="showTab('messages')">Messages</button>
                </div>
                
                <div id="serials" class="tab-content active">
                    <div class="section">
                        <h2>Serial Numbers</h2>
                        <form id="addSerialForm" style="margin-bottom: 20px; padding: 15px; background: #f8f9fa; border-radius: 4px;">
                            <div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px;">
                                <div>
                                    <label>Serial Number:</label>
                                    <input type="text" id="newSerialNumber" required style="width: 100%;">
                                </div>
                                <div>
                                    <label>Status:</label>
                                    <select id="newStatus" style="width: 100%;">
                                        <option value="true">Valid</option>
                                        <option value="false">Invalid</option>
                                    </select>
                                </div>
                            </div>
                            <button type="submit" style="margin-top: 10px; padding: 8px 16px; background: #1a73e8; color: white; border: none; border-radius: 4px; cursor: pointer;">Add Serial</button>
                        </form>
                        <table>
                            <tr>
                                <th>Serial Number</th>
                                <th>Device Name</th>
                                <th>Activation Date</th>
                                <th>Status</th>
                                <th>Last Connection</th>
                                <th>Firmware Version</th>
                                <th>Aksi</th>
                            </tr>
                            {% for serial in serials %}
                            <tr>
                                <td>{{ serial.serial_number }}</td>
                                <td>{{ serial.device_name }}</td>
                                <td>{{ serial.activation_date }}</td>
                                <td>
                                    <select id="status_{{ serial.serial_number }}" style="padding: 4px; border-radius: 4px; border: 1px solid #ddd;">
                                        <option value="true" {{ 'selected' if serial.is_valid else '' }}>Valid</option>
                                        <option value="false" {{ 'selected' if not serial.is_valid else '' }}>Invalid</option>
                                    </select>
                                </td>
                                <td>{{ serial.last_connection or 'Never' }}</td>
                                <td>{{ serial.firmware_version }}</td>
                                <td>
                                    <button onclick="updateStatus('{{ serial.serial_number }}')" 
                                            style="padding: 4px 8px; background: #1a73e8; 
                                                   color: white; border: none; border-radius: 4px; cursor: pointer;">
                                        Update
                                    </button>
                                </td>
                            </tr>
                            {% endfor %}
                        </table>
                    </div>
                </div>
                
                <div id="quiz" class="tab-content">
                    <div class="section">
                        <h2>Quiz Results</h2>
                        <table>
                            <tr>
                                <th>Name</th>
                                <th>Serial Number</th>
                                <th>Quiz Type</th>
                                <th>Question</th>
                                <th>Answer</th>
                                <th>Score</th>
                                <th>Date</th>
                            </tr>
                            {% for result in quiz_results %}
                            <tr>
                                <td>{{ result.name }}</td>
                                <td>{{ result.serial_number }}</td>
                                <td>{{ result.quiz_type }}</td>
                                <td>{{ result.question }}</td>
                                <td>{{ result.answer }}</td>
                                <td>{{ result.score }}/{{ result.total_questions }}</td>
                                <td>{{ result.date_time }}</td>
                            </tr>
                            {% endfor %}
                        </table>
                    </div>
                </div>
                
                <div id="logs" class="tab-content">
                    <div class="section">
                        <h2>Communication Logs</h2>
                        <table>
                            <tr>
                                <th>Date Time</th>
                                <th>Device Name</th>
                                <th>Serial Number</th>
                                <th>IP Address</th>
                                <th>User Input</th>
                                <th>Server Response</th>
                            </tr>
                            {% for log in comm_logs %}
                            <tr>
                                <td>{{ log.datetime }}</td>
                                <td>{{ log.device_name }}</td>
                                <td>{{ log.serial_number }}</td>
                                <td>{{ log.ip_address }}</td>
                                <td>{{ log.esp_user }}</td>
                                <td>{{ log.server_user }}</td>
                            </tr>
                            {% endfor %}
                        </table>
                    </div>
                </div>

                <!-- New Messages tab -->
                <div id="messages" class="tab-content">
                    <div class="section">
                        <h2>Messages</h2>
                        
                        <form class="broadcast-form" id="broadcastForm">
                            <h3>Broadcast Message ke Semua Device</h3>
                            <textarea name="content" rows="4" placeholder="Tulis pesan broadcast di sini..." required></textarea>
                            <button type="submit">Kirim ke Semua Device</button>
                        </form>
                        
                        <form class="filter-form" method="GET" action="/admin">
                            <select name="filter">
                                <option value="all">Semua Pesan</option>
                                <option value="unread">Belum Dibaca</option>
                            </select>
                            <input type="text" name="search" placeholder="Cari berdasarkan isi pesan...">
                            <button type="submit">Filter</button>
                        </form>

                        {% for msg in messages %}
                        <div class="message {{ 'unread' if not msg[5] else '' }}">
                            <div class="message-header">
                                <span>Dari: {{ msg[1] }} → Ke: {{ msg[2] }}</span>
                                <span>{{ msg[4] }}</span>
                            </div>
                            <div class="message-content">{{ msg[3] }}</div>
                            <form method="POST" action="/delete_message" style="display: inline;">
                                <input type="hidden" name="message_id" value="{{ msg[0] }}">
                                <button type="submit" class="delete-btn">Hapus</button>
                            </form>
                        </div>
                        {% endfor %}
                    </div>
                </div>
            </div>
        </body>
        </html>
    ''', serials=serials, quiz_results=quiz_results, comm_logs=comm_logs, messages=messages)

# Function to create databases if they don't exist
def create_databases():
    # 1. Serial Number Validation Database
    if not os.path.exists(SERIAL_VALIDATION_DB):
        print("\nMembuat database serial number baru")
        initial_serials = [
            {
                'serial_number': 'MOGI001',
                'device_name': 'Mogi Alpha',
                'activation_date': datetime.now().strftime('%Y-%m-%d'),
                'is_valid': True,
                'last_connection': None,
                'firmware_version': 'V01_2804_2025',
                'notes': ''
            },
            {
                'serial_number': 'MOGI002',
                'device_name': 'Mogi Beta',
                'activation_date': datetime.now().strftime('%Y-%m-%d'),
                'is_valid': True,
                'last_connection': None,
                'firmware_version': 'V01_2804_2025',
                'notes': ''
            },
            {
                'serial_number': 'MOGI003',
                'device_name': 'Mogi Gamma',
                'activation_date': datetime.now().strftime('%Y-%m-%d'),
                'is_valid': True,
                'last_connection': None,
                'firmware_version': 'V01_2804_2025',
                'notes': ''
            },
            {
                'serial_number': 'MOGI004',
                'device_name': 'Mogi Delta',
                'activation_date': datetime.now().strftime('%Y-%m-%d'),
                'is_valid': True,
                'last_connection': None,
                'firmware_version': 'V01_2804_2025',
                'notes': ''
            },
            {
                'serial_number': 'MOGI005',
                'device_name': 'Mogi Epsilon',
                'activation_date': datetime.now().strftime('%Y-%m-%d'),
                'is_valid': True,
                'last_connection': None,
                'firmware_version': 'V01_2804_2025',
                'notes': ''
            }
        ]
        serial_db.insert_multiple(initial_serials)
        print(f"Database {SERIAL_VALIDATION_DB} berhasil dibuat")
    else:
        print(f"\nDatabase {SERIAL_VALIDATION_DB} sudah ada")
        # Tampilkan semua serial yang ada
        all_serials = serial_db.all()
        print(f"Jumlah serial dalam database: {len(all_serials)}")
        for serial in all_serials:
            print(f"Serial: {serial['serial_number']}, Valid: {serial['is_valid']}")

# Function to validate serial number
def validate_serial_number(serial_number):
    try:
        print(f"\nValidasi serial number: {serial_number}")
        
        # Cek di database serial
        Serial = Query()
        device = serial_db.get(Serial.serial_number == serial_number)
        
        if device:
            print(f"Device ditemukan di database serial: {device}")
            if device['is_valid']:
                print("Device valid")
                # Update last connection time
                serial_db.update(
                    {'last_connection': datetime.now().strftime('%Y-%m-%d %H:%M:%S')},
                    Serial.serial_number == serial_number
                )
                return True
            else:
                print("Device tidak valid")
        else:
            # Cek di communication logs
            CommLog = Query()
            comm_logs = comm_log_db.search(CommLog.serial_number == serial_number)
            if comm_logs:
                print(f"Device ditemukan di communication logs")
                # Tambahkan ke database serial jika belum ada
                new_record = {
                    'serial_number': serial_number,
                    'device_name': comm_logs[0].get('device_name', f'Device {serial_number}'),
                    'activation_date': datetime.now().strftime('%Y-%m-%d'),
                    'is_valid': True,
                    'last_connection': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                    'firmware_version': 'V01_2804_2025',
                    'notes': 'Auto-registered from communication logs'
                }
                serial_db.insert(new_record)
                print(f"Device otomatis terdaftar dari communication logs")
                return True
            else:
                print("Device tidak ditemukan di database maupun communication logs")
        return False
    except Exception as e:
        print(f"Error validating serial number: {e}")
        return False

# Function to log quiz results
def log_quiz_result(name, serial_number, ip_address, quiz_type, questions, answers, 
                   correct_answers, scores, total_score, total_questions):
    try:
        # Current timestamp
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        # Calculate percentage
        percentage = (total_score / total_questions) * 100
        
        # Add each question-answer pair as a separate document
        for i in range(len(questions)):
            new_record = {
                'name': name,
                'serial_number': serial_number,
                'ip_address': ip_address,
                'quiz_type': quiz_type,
                'question': questions[i],
                'answer': answers[i],
                'correct_answer': correct_answers[i],
                'is_correct': scores[i] == 1,
                'score': total_score,
                'total_questions': total_questions,
                'percentage': percentage,
                'date_time': timestamp
            }
            quiz_db.insert(new_record)
        return True
    except Exception as e:
        print(f"Error logging quiz result: {e}")
        return False

# Fungsi untuk menerima file audio secara utuh
@app.route('/uploadAudio', methods=['POST'])
def upload_audio():
    if request.method == 'POST':
        try:
            # Get client identifiers
            serial_number = request.headers.get('Serial-Number', 'unknown')
            device_name = request.headers.get('Device-Name', 'Unknown Mogi')
            
            # Create unique filenames for this client
            client_record_file = f'recording_{serial_number}.wav'
            client_output_file = f'output_{serial_number}.mp3'
            
            # Save the uploaded file with client-specific name
            with open(client_record_file, 'wb') as f:
                f.write(request.data)

            # Validate serial number (skip validation if serial is empty during development)
            serial_valid = True
            if serial_number:
                serial_valid = validate_serial_number(serial_number)
    
            # Transcribe the audio file
            esp_user = speech_to_text(client_record_file)
            
            # Get IP address for logging
            ip_address = request.remote_addr
            
            # Check if we're in a quiz mode
            session = client_sessions.get(serial_number, {
                'current_quiz': None,
                'quiz_questions': [],
                'quiz_answers': [],
                'current_question_index': 0
            })
            client_sessions[serial_number] = session

            if session['current_quiz']:
                if session['current_question_index'] < len(session['quiz_questions']):
                    # Record the answer
                    session['quiz_answers'].append(esp_user)
                    
                    # Check if answer is correct based on quiz type
                    if session['current_quiz'] == "math":
                        # Math quiz checking
                        try:
                            # Clean user's answer
                            user_answer = clean_number_answer(esp_user)
                            
                            # Calculate and clean correct answer
                            correct_answer = clean_number_answer(str(eval(session['quiz_questions'][session['current_question_index']]['answer'])))
                            
                            print(f"Comparing: user={user_answer}, correct={correct_answer}")
                            
                            # Compare normalized answers
                            if user_answer == correct_answer:
                                session['quiz_scores'].append(1)
                                feedback = f"Jawaban benar! {user_answer} adalah jawaban yang tepat."
                            else:
                                session['quiz_scores'].append(0)
                                feedback = f"Jawaban kurang tepat. Jawaban yang benar adalah {correct_answer}."
                        except Exception as e:
                            print(f"Error in math quiz checking: {str(e)}")
                            session['quiz_scores'].append(0)
                            feedback = "Maaf, saya tidak mengerti jawabanmu. Tolong jawab dengan angka saja."
                    elif session['current_quiz'] == "english":
                        # English quiz checking
                        correct_answer = clean_english_answer(session['quiz_questions'][session['current_question_index']]['answer'])
                        user_answer = clean_english_answer(esp_user)
                        
                        # Check if correct answer is contained in user's answer
                        if correct_answer in user_answer or user_answer in correct_answer:
                            session['quiz_scores'].append(1)
                            feedback = f"Benar sekali! '{session['quiz_questions'][session['current_question_index']]['answer']}' adalah jawaban yang tepat."
                        else:
                            session['quiz_scores'].append(0)
                            feedback = f"Belum tepat. Jawaban yang benar adalah '{session['quiz_questions'][session['current_question_index']]['answer']}'."
                    
                    session['current_question_index'] += 1
                    
                    # Check if we've finished all questions
                    if session['current_question_index'] >= len(session['quiz_questions']):
                        final_score = sum(session['quiz_scores'])
                        total_questions = len(session['quiz_questions'])
                        percentage = (final_score / total_questions) * 100
                        
                        if session['current_quiz'] == "math":
                            server_user = f"Kuis selesai! Skor kamu adalah {final_score} dari {total_questions} pertanyaan, atau {percentage:.1f}%. "
                            
                            if percentage >= 80:
                                server_user += "Hebat sekali! Kamu sudah sangat pandai berhitung."
                            elif percentage >= 60:
                                server_user += "Bagus! Teruslah berlatih berhitung ya."
                            else:
                                server_user += "Tidak apa-apa, mari kita berlatih lebih banyak lagi."
                        elif session['current_quiz'] == "english":
                            server_user = f"Kuis selesai! Skor kamu adalah {final_score} dari {total_questions} pertanyaan, atau {percentage:.1f}%. "
                            
                            if percentage >= 80:
                                server_user += "Hebat sekali! Kemampuan bahasa Inggrismu sudah sangat bagus."
                            elif percentage >= 60:
                                server_user += "Bagus! Teruslah berlatih bahasa Inggrismu ya."
                            else:
                                server_user += "Tidak apa-apa, mari kita terus belajar kata-kata bahasa Inggris bersama."
                        
                        # Log quiz results to Excel
                        questions_text = [q['text'] for q in session['quiz_questions']]
                        correct_answers = [q['answer'] for q in session['quiz_questions']]
                        
                        log_quiz_result(
                            name=device_name,
                            serial_number=serial_number,
                            ip_address=ip_address,
                            quiz_type=session['current_quiz'],
                            questions=questions_text,
                            answers=session['quiz_answers'],
                            correct_answers=correct_answers,
                            scores=session['quiz_scores'],
                            total_score=final_score,
                            total_questions=total_questions
                        )
                            
                        # Reset quiz state
                        session['current_quiz'] = None
                        session['quiz_questions'] = []
                        session['quiz_answers'] = []
                        session['quiz_scores'] = []
                        session['current_question_index'] = 0
                    else:
                        # Prepare next question
                        server_user = feedback + " " + session['quiz_questions'][session['current_question_index']]['text']
                else:
                    # This shouldn't happen but just in case
                    server_user = "Maaf, terjadi kesalahan dalam kuis."
                    session['current_quiz'] = None
            else:
                # Check if user wants to start a quiz
                if serial_valid and "belajar menghitung" in esp_user.lower():
                    # Initialize math quiz
                    session['current_quiz'] = "math"
                    session['quiz_questions'] = generate_math_questions()
                    session['quiz_answers'] = []
                    session['quiz_scores'] = []
                    session['current_question_index'] = 0
                    
                    server_user = "Baik, mari belajar menghitung! Saya akan memberikan 5 pertanyaan matematika. " + session['quiz_questions'][0]['text']
                elif serial_valid and "belajar bahasa inggris" in esp_user.lower():
                    # Initialize English quiz
                    session['current_quiz'] = "english"
                    session['quiz_questions'] = generate_english_questions()
                    session['quiz_answers'] = []
                    session['quiz_scores'] = []
                    session['current_question_index'] = 0
                    
                    server_user = "Let's learn English! I'll give you 5 vocabulary questions. " + session['quiz_questions'][0]['text']
                else:
                    if not serial_valid and ("belajar menghitung" in esp_user.lower() or "belajar bahasa inggris" in esp_user.lower()):
                        server_user = "Perangkat ini belum diaktifkan untuk mengikuti kuis. Hubungi admin."
                    else:
                        response = model.generate_content('jawab dengan singkat, ' + esp_user)
                        server_user = response.text
            
            print(f'esp_user: {esp_user}')
            print(f'server_user: {server_user}')

            # Mengubah server_user ke dalam bentuk suara menggunakan gTTS
            if session['current_quiz']:
                language = 'id'  # Always use Indonesian for TTS
            else:
                language = 'id'
                
            tts = gTTS(text=server_user, lang=language, slow=False)
            tts.save(client_output_file)
            print(f'server_user disimpan dalam {client_output_file}')
            log_communication(device_name, serial_number, ip_address, esp_user, server_user)

            # Clean up old recording file
            if os.path.exists(client_record_file):
                os.remove(client_record_file)

            # Mengembalikan hasil transkripsi dalam format JSON
            return jsonify({'esp_user': esp_user, 
                           'server_user': server_user, 
                           'in_quiz': session['current_quiz'] is not None, 
                           'quiz_type': session['current_quiz']}), 200
        except Exception as e:
            print(f"Error in upload_audio: {str(e)}")
            return str(e), 500
    else:
        return 'Method Not Allowed', 405

# Add a route for managing serial numbers through API
@app.route('/serial', methods=['GET', 'POST', 'PUT', 'DELETE'])
def manage_serials():
    try:
        Serial = Query()
        
        if request.method == 'GET':
            # List all serials
            return jsonify(serial_db.all()), 200
            
        elif request.method == 'POST':
            # Add a new serial
            data = request.json
            if not data or 'serial_number' not in data:
                return jsonify({'error': 'Missing serial_number'}), 400
                
            if serial_db.get(Serial.serial_number == data['serial_number']):
                return jsonify({'error': 'Serial number already exists'}), 409
                
            new_record = {
                'serial_number': data['serial_number'],
                'device_name': data.get('device_name', 'New Device'),
                'activation_date': datetime.now().strftime('%Y-%m-%d'),
                'is_valid': data.get('is_valid', True),
                'last_connection': None,
                'firmware_version': data.get('firmware_version', 'V01_2804_2025'),
                'notes': data.get('notes', '')
            }
            
            serial_db.insert(new_record)
            return jsonify({'message': 'Serial added successfully'}), 201
            
        elif request.method == 'PUT':
            # Update a serial
            data = request.json
            if not data or 'serial_number' not in data:
                return jsonify({'error': 'Missing serial_number'}), 400
                
            existing_serial = serial_db.get(Serial.serial_number == data['serial_number'])
            if not existing_serial:
                return jsonify({'error': 'Serial number not found'}), 404
                
            # Update only the is_valid field
            update_data = {'is_valid': data.get('is_valid', existing_serial['is_valid'])}
            serial_db.update(update_data, Serial.serial_number == data['serial_number'])
            
            return jsonify({'message': 'Serial updated successfully'}), 200
            
        elif request.method == 'DELETE':
            # Delete a serial
            data = request.json
            if not data or 'serial_number' not in data:
                return jsonify({'error': 'Missing serial_number'}), 400
                
            if not serial_db.get(Serial.serial_number == data['serial_number']):
                return jsonify({'error': 'Serial number not found'}), 404
                
            # Remove the record
            serial_db.remove(Serial.serial_number == data['serial_number'])
            
            return jsonify({'message': 'Serial deleted successfully'}), 200
            
    except Exception as e:
        print(f"Error in manage_serials: {str(e)}")  # Tambahkan logging
        return jsonify({'error': str(e)}), 500

# Add route to view quiz results
@app.route('/quiz_results', methods=['GET'])
def get_quiz_results():
    try:
        Quiz = Query()
        results = quiz_db.all()
        
        # Filter by query params if provided
        filters = {}
        for param in ['serial_number', 'quiz_type', 'name']:
            if param in request.args:
                filters[param] = request.args.get(param)
                
        if filters:
            for key, value in filters.items():
                results = [r for r in results if r[key] == value]
                
        return jsonify(results), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

def generate_math_questions():
    """Generate 5 random simple math questions"""
    questions = []
    
    # Addition (2 questions)
    for _ in range(2):
        a = random.randint(1, 20)
        b = random.randint(1, 20)
        text = f"Berapa {a} ditambah {b}?"
        questions.append({
            'text': text,
            'answer': f"{a} + {b}"
        })
    
    # Subtraction (1 question)
    a = random.randint(10, 30)
    b = random.randint(1, 9)
    text = f"Berapa {a} dikurangi {b}?"
    questions.append({
        'text': text,
        'answer': f"{a} - {b}"
    })
    
    # Multiplication (1 question)
    a = random.randint(2, 10)
    b = random.randint(2, 10)
    text = f"Berapa {a} dikali {b}?"
    questions.append({
        'text': text,
        'answer': f"{a} * {b}"
    })
    
    # Division (simple, 1 question)
    b = random.randint(2, 5)
    a = b * random.randint(1, 5)  # Ensure it's divisible evenly
    text = f"Berapa {a} dibagi {b}?"
    questions.append({
        'text': text,
        'answer': f"{a} / {b}"
    })
    
    random.shuffle(questions)
    return questions

def generate_english_questions():
    """Generate 5 English vocabulary questions"""
    vocab_pairs = [
        {"id": "anjing", "en": "dog"},
        {"id": "kucing", "en": "cat"},
        {"id": "rumah", "en": "house"},
        {"id": "mobil", "en": "car"},
        {"id": "buku", "en": "book"},
        {"id": "air", "en": "water"},
        {"id": "makan", "en": "eat"},
        {"id": "minum", "en": "drink"},
        {"id": "tidur", "en": "sleep"},
        {"id": "lari", "en": "run"},
        {"id": "jalan", "en": "walk"},
        {"id": "sekolah", "en": "school"},
        {"id": "guru", "en": "teacher"},
        {"id": "murid", "en": "student"},
        {"id": "bunga", "en": "flower"},
        {"id": "pohon", "en": "tree"},
        {"id": "langit", "en": "sky"},
        {"id": "matahari", "en": "sun"},
        {"id": "bulan", "en": "moon"},
        {"id": "bintang", "en": "star"}
    ]
    
    # Randomly select 5 vocabulary pairs
    selected_pairs = random.sample(vocab_pairs, 5)
    questions = []
    
    for pair in selected_pairs:
        # Randomly choose to ask for English or Indonesian translation
        if random.choice([True, False]):
            # Ask for English translation
            text = f"Apa bahasa Inggris dari kata '{pair['id']}'?"
            answer = pair['en']
        else:
            # Ask for Indonesian translation
            text = f"What is the Indonesian word for '{pair['en']}'?"
            answer = pair['id']
        
        questions.append({
            'text': text,
            'answer': answer
        })
    
    return questions

def clean_number_answer(text):
    """Clean and normalize number answers"""
    import re
    
    # Remove all non-essential text (like "jawabannya", "adalah", etc)
    text = text.lower()
    text = re.sub(r'[^0-9\.-]', '', text)
    
    try:
        # Convert to float
        num = float(text)
        # If it's a whole number, convert to int
        if num.is_integer():
            return str(int(num))
        return str(num)  # Keep decimal numbers as is
    except:
        return text

def clean_english_answer(answer):
    """Clean and normalize English/Indonesian answers"""
    # Convert to lowercase and remove extra spaces
    answer = answer.lower().strip()
    # Remove punctuation and common words
    common_words = ['adalah', 'itu', 'the', 'in', 'bahasa', 'indonesia', 'inggris', 'artinya']
    for word in common_words:
        answer = answer.replace(word, '')
    # Remove extra spaces and trim
    answer = ' '.join(answer.split())
    return answer

@app.route('/downloadAudio/<filename>', methods=['GET'])
def download_audio(filename):
    try:
        # update 
        file_path = os.path.join(os.getcwd(), filename)
        file_size = os.path.getsize(file_path)

        # Mengembalikan file MP3 yang telah dihasilkan
        response = send_from_directory(os.getcwd(), filename, as_attachment=True)
        response.headers['Content-Length'] = str(file_size)

        return response
    except Exception as e:
        return str(e), 500

@app.route('/checkStatus', methods=['GET'])
def check_status():
    try:
        # Get serial number from header if provided
        serial_number = request.headers.get('Serial-Number', '')
        
        # If serial number provided, validate it
        if serial_number:
            is_valid = validate_serial_number(serial_number)
            return jsonify({
                'status': 'Server is up and running',
                'serial_valid': is_valid
            }), 200
        else:
            # Just return server status
            return jsonify({'status': 'Server is up and running'}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

# Function to log communication
def log_communication(name, serial_number, ip, esp_user, server_user):
    try:
        new_record = {
            'datetime': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            'serial_number': serial_number,
            'device_name': name,
            'ip_address': ip,
            'esp_user': esp_user,
            'server_user': server_user
        }
        comm_log_db.insert(new_record)
        print("Log komunikasi disimpan.")
    except Exception as e:
        print(f"Gagal menyimpan log komunikasi: {e}")


def speech_to_text(record_file):
    global server_user  # Mengakses variabel server_user global
    # Initialize the recognizer
    recognizer = sr.Recognizer()

    # Open the audio file
    with sr.AudioFile(record_file) as source:
        # Listen for the data (load audio to memory)
        audio_data = recognizer.record(source)

        # Recognize (convert from speech to text)
        try:
            # Detect language based on current quiz type
            language = 'en-US' if client_sessions.get('current_quiz') == "english" else 'id-ID'
            
            # Menggunakan Google Speech Recognition untuk transkripsi
            text = recognizer.recognize_google(audio_data, language=language)
            print(f'esp_user: {text}')

            return text
        except sr.UnknownValueError:
            return "Could not understand audio"
        except sr.RequestError as e:
            return f"Could not request results from Google Speech Recognition service; {e}"

# Inisialisasi database
def init_db():
    conn = sqlite3.connect('mogi_messages.db')
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS messages
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  from_serial TEXT NOT NULL,
                  to_serial TEXT NOT NULL,
                  content TEXT NOT NULL,
                  timestamp TEXT NOT NULL,
                  is_read INTEGER DEFAULT 0)''')
    
    # Tambah tabel untuk riwayat pesan server
    c.execute('''CREATE TABLE IF NOT EXISTS server_messages
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  content TEXT NOT NULL,
                  timestamp TEXT NOT NULL,
                  sent_to TEXT NOT NULL,
                  status TEXT NOT NULL)''')
    conn.commit()
    conn.close()

# Panggil init_db saat server dimulai
init_db()

@app.route('/send_message', methods=['POST'])
def send_message():
    try:
        # Ambil serial number pengirim dari header
        from_serial = request.headers.get('Serial-Number')
        if not from_serial:
            return jsonify({"error": "Serial number tidak ditemukan"}), 400

        # Ambil data pesan dari body request
        data = request.get_json()
        if not data or 'to' not in data or 'content' not in data:
            return jsonify({"error": "Data pesan tidak lengkap"}), 400

        to_serial = data['to']
        content = data['content']
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Simpan pesan ke database
        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        c.execute('''INSERT INTO messages (from_serial, to_serial, content, timestamp)
                     VALUES (?, ?, ?, ?)''', (from_serial, to_serial, content, timestamp))
        conn.commit()
        conn.close()

        return jsonify({"status": "success", "message": "Pesan berhasil dikirim"}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/get_messages', methods=['GET'])
def get_messages():
    try:
        # Ambil serial number dari header
        serial_number = request.headers.get('Serial-Number')
        if not serial_number:
            return jsonify({"error": "Serial number tidak ditemukan"}), 400

        # Validasi serial number
        if not validate_serial_number(serial_number):
            return jsonify({"error": "Serial number tidak valid"}), 401

        # Ambil pesan yang belum dibaca dari database
        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        
        # Ambil pesan yang ditujukan ke device ini
        c.execute('''SELECT from_serial, content, timestamp, is_read 
                     FROM messages 
                     WHERE to_serial = ? 
                     ORDER BY timestamp DESC''', (serial_number,))
        messages = c.fetchall()
        
        # Log untuk debugging
        print(f"Mengambil pesan untuk device {serial_number}")
        print(f"Jumlah pesan yang ditemukan: {len(messages)}")
        
        # Format pesan untuk response
        formatted_messages = []
        for msg in messages:
            formatted_messages.append({
                "from": msg[0],
                "content": msg[1],
                "timestamp": msg[2],
                "is_read": bool(msg[3])
            })
            print(f"Pesan: dari={msg[0]}, isi={msg[1]}, waktu={msg[2]}, dibaca={msg[3]}")

        conn.close()
        return jsonify(formatted_messages), 200

    except Exception as e:
        print(f"Error dalam get_messages: {str(e)}")
        return jsonify({"error": str(e)}), 500

@app.route('/mark_as_read', methods=['POST'])
def mark_as_read():
    try:
        # Ambil serial number dari header
        serial_number = request.headers.get('Serial-Number')
        if not serial_number:
            return jsonify({"error": "Serial number tidak ditemukan"}), 400

        # Ambil data dari body request
        data = request.get_json()
        if not data or 'message_id' not in data:
            return jsonify({"error": "ID pesan tidak ditemukan"}), 400

        message_id = data['message_id']

        # Update status pesan di database
        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        c.execute('''UPDATE messages 
                     SET is_read = 1 
                     WHERE id = ? AND to_serial = ?''', (message_id, serial_number))
        conn.commit()
        conn.close()

        return jsonify({"status": "success", "message": "Pesan ditandai sebagai telah dibaca"}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/delete_message', methods=['POST'])
def delete_message():
    try:
        message_id = request.form.get('message_id')
        if not message_id:
            return "Message ID not provided", 400

        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        c.execute('DELETE FROM messages WHERE id = ?', (message_id,))
        conn.commit()
        conn.close()

        return redirect('/messages')

    except Exception as e:
        return f"Error: {str(e)}", 500

# Tambahkan route untuk broadcast message
@app.route('/broadcast_message', methods=['POST'])
@login_required
def broadcast_message():
    try:
        data = request.get_json()
        if not data or 'content' not in data:
            print("Error: Konten pesan tidak ditemukan")
            return jsonify({"error": "Konten pesan tidak ditemukan"}), 400

        content = data['content']
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"\nMemulai broadcast message: {content}")

        # Ambil semua serial number dari communication logs
        all_comm_logs = comm_log_db.all()
        unique_devices = {}
        for log in all_comm_logs:
            if log['serial_number'] not in unique_devices:
                unique_devices[log['serial_number']] = {
                    'device_name': log['device_name'],
                    'last_connection': log['datetime']
                }

        print(f"Jumlah device unik dari communication logs: {len(unique_devices)}")
        
        if not unique_devices:
            print("Error: Tidak ada device yang ditemukan di communication logs")
            return jsonify({"error": "Tidak ada device yang ditemukan"}), 400
        
        # Kirim pesan ke semua device
        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        
        success_count = 0
        sent_to_devices = []
        for serial_number, device_info in unique_devices.items():
            try:
                print(f"\nMencoba mengirim ke device: {serial_number}")
                # Simpan pesan ke database
                c.execute('''INSERT INTO messages (from_serial, to_serial, content, timestamp, is_read)
                            VALUES (?, ?, ?, ?, ?)''', 
                            ('SERVER', serial_number, content, timestamp, 0))
                success_count += 1
                sent_to_devices.append(serial_number)
                print(f"Berhasil mengirim ke {serial_number}")
                
                # Log ke communication log
                log_communication(
                    name=device_info['device_name'],
                    serial_number=serial_number,
                    ip='broadcast',
                    esp_user='BROADCAST',
                    server_user=content
                )
            except Exception as e:
                print(f"Error mengirim broadcast ke {serial_number}: {str(e)}")
                continue
        
        # Simpan riwayat pesan server
        try:
            c.execute('''INSERT INTO server_messages (content, timestamp, sent_to, status)
                        VALUES (?, ?, ?, ?)''',
                        (content, timestamp, ','.join(sent_to_devices), 
                         'success' if success_count > 0 else 'failed'))
            print(f"\nRiwayat pesan server disimpan dengan status: {'success' if success_count > 0 else 'failed'}")
        except Exception as e:
            print(f"Error menyimpan riwayat pesan server: {str(e)}")
        
        conn.commit()
        conn.close()

        print(f"\nRingkasan broadcast:")
        print(f"Total device unik: {len(unique_devices)}")
        print(f"Berhasil dikirim ke: {success_count} device")
        print(f"Device yang menerima: {sent_to_devices}")

        if success_count > 0:
            return jsonify({
                "status": "success", 
                "message": f"Pesan berhasil dikirim ke {success_count} device",
                "sent_to": sent_to_devices
            }), 200
        else:
            return jsonify({"error": "Gagal mengirim pesan ke semua device"}), 500

    except Exception as e:
        print(f"Error dalam broadcast_message: {str(e)}")
        return jsonify({"error": str(e)}), 500

@app.route('/messages')
@login_required
def view_messages():
    try:
        # Ambil semua pesan dari database
        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        c.execute('''SELECT id, from_serial, to_serial, content, timestamp, is_read 
                     FROM messages 
                     ORDER BY timestamp DESC''')
        messages = c.fetchall()
        conn.close()

        return render_template_string('''
            <!DOCTYPE html>
            <html>
            <head>
                <title>Mogi Messages</title>
                <style>
                    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
                    .container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
                    h1 { color: #333; text-align: center; }
                    .message { border: 1px solid #ddd; margin: 10px 0; padding: 15px; border-radius: 4px; background-color: #fff; }
                    .message.unread { background-color: #f8f9fa; border-left: 4px solid #007bff; }
                    .message-header { display: flex; justify-content: space-between; margin-bottom: 10px; color: #666; font-size: 0.9em; }
                    .message-content { color: #333; margin: 10px 0; }
                    .delete-btn { background-color: #dc3545; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; }
                    .delete-btn:hover { background-color: #c82333; }
                    .filter-form { margin-bottom: 20px; padding: 15px; background-color: #f8f9fa; border-radius: 4px; }
                    .filter-form select, .filter-form input { padding: 5px; margin: 5px; }
                    .filter-form button { background-color: #007bff; color: white; border: none; padding: 5px 15px; border-radius: 4px; cursor: pointer; }
                    .filter-form button:hover { background-color: #0056b3; }
                    .broadcast-form { margin-bottom: 20px; padding: 15px; background-color: #e9ecef; border-radius: 4px; }
                    .broadcast-form textarea { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; resize: vertical; }
                    .broadcast-form button { background-color: #28a745; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; }
                    .broadcast-form button:hover { background-color: #218838; }
                    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
                    .logout-btn { padding: 8px 16px; background: #dc3545; color: white; text-decoration: none; border-radius: 4px; }
                    .nav-links { margin-bottom: 20px; }
                    .nav-links a { display: inline-block; padding: 10px 20px; margin-right: 10px; background-color: #1a73e8; color: white; text-decoration: none; border-radius: 4px; }
                    .nav-links a:hover { background-color: #1557b0; }
                </style>
            </head>
            <body>
                <div class="container">
                    <div class="header">
                        <h1>Mogi Messages</h1>
                        <a href="/logout" class="logout-btn">Logout</a>
                    </div>
                    
                    <div class="nav-links">
                        <a href="/admin">Dashboard</a>
                        <a href="/server_messages">Server Messages History</a>
                    </div>
                    
                    <form class="broadcast-form" id="broadcastForm">
                        <h3>Broadcast Message ke Semua Device</h3>
                        <textarea name="content" rows="4" placeholder="Tulis pesan broadcast di sini..." required></textarea>
                        <button type="submit">Kirim ke Semua Device</button>
                    </form>
                    
                    <form class="filter-form" method="GET" action="/messages">
                        <select name="filter">
                            <option value="all">Semua Pesan</option>
                            <option value="unread">Belum Dibaca</option>
                        </select>
                        <input type="text" name="search" placeholder="Cari berdasarkan isi pesan...">
                        <button type="submit">Filter</button>
                    </form>

                    {% for msg in messages %}
                    <div class="message {{ 'unread' if not msg[5] else '' }}">
                        <div class="message-header">
                            <span>Dari: {{ msg[1] }} → Ke: {{ msg[2] }}</span>
                            <span>{{ msg[4] }}</span>
                        </div>
                        <div class="message-content">{{ msg[3] }}</div>
                        <form method="POST" action="/delete_message" style="display: inline;">
                            <input type="hidden" name="message_id" value="{{ msg[0] }}">
                            <button type="submit" class="delete-btn">Hapus</button>
                        </form>
                    </div>
                    {% endfor %}
                </div>
                <script>
                    document.getElementById('broadcastForm').addEventListener('submit', async function(e) {
                        e.preventDefault();
                        const content = this.querySelector('textarea[name="content"]').value;
                        
                        try {
                            const response = await fetch('/broadcast_message', {
                                method: 'POST',
                                headers: {
                                    'Content-Type': 'application/json',
                                },
                                body: JSON.stringify({ content: content })
                            });
                            
                            const data = await response.json();
                            if (response.ok) {
                                alert(data.message);
                                location.reload();
                            } else {
                                alert('Error: ' + data.error);
                            }
                        } catch (error) {
                            alert('Terjadi kesalahan: ' + error);
                        }
                    });
                </script>
            </body>
            </html>
        ''', messages=messages)

    except Exception as e:
        return str(e), 500

@app.route('/server_messages', methods=['GET'])
@login_required
def get_server_messages():
    try:
        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        c.execute('''SELECT id, content, timestamp, sent_to, status 
                    FROM server_messages 
                    ORDER BY timestamp DESC''')
        messages = c.fetchall()
        conn.close()

        return render_template_string('''
            <!DOCTYPE html>
            <html>
            <head>
                <title>Server Messages History</title>
                <style>
                    body { font-family: Arial, sans-serif; margin: 20px; }
                    .container { max-width: 800px; margin: 0 auto; }
                    .message { 
                        border: 1px solid #ddd; 
                        margin: 10px 0; 
                        padding: 15px; 
                        border-radius: 4px;
                        background-color: #fff;
                    }
                    .message-header {
                        display: flex;
                        justify-content: space-between;
                        margin-bottom: 10px;
                        color: #666;
                    }
                    .delete-btn {
                        background-color: #dc3545;
                        color: white;
                        border: none;
                        padding: 5px 10px;
                        border-radius: 4px;
                        cursor: pointer;
                    }
                    .status-success { color: #28a745; }
                    .status-failed { color: #dc3545; }
                    .nav-links { margin-bottom: 20px; }
                    .nav-links a { 
                        display: inline-block; 
                        padding: 10px 20px; 
                        margin-right: 10px; 
                        background-color: #1a73e8; 
                        color: white; 
                        text-decoration: none; 
                        border-radius: 4px; 
                    }
                    .nav-links a:hover { background-color: #1557b0; }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>Server Messages History</h1>
                    
                    <div class="nav-links">
                        <a href="/admin">Dashboard</a>
                        <a href="/messages">Messages</a>
                    </div>
                    
                    {% for msg in messages %}
                    <div class="message">
                        <div class="message-header">
                            <span>{{ msg[2] }}</span>
                            <span class="status-{{ msg[4] }}">{{ msg[4] }}</span>
                        </div>
                        <div class="message-content">{{ msg[1] }}</div>
                        <div>Sent to: {{ msg[3] }}</div>
                        <form method="POST" action="/delete_server_message" style="display: inline;">
                            <input type="hidden" name="message_id" value="{{ msg[0] }}">
                            <button type="submit" class="delete-btn">Delete</button>
                        </form>
                    </div>
                    {% endfor %}
                </div>
            </body>
            </html>
        ''', messages=messages)

    except Exception as e:
        return str(e), 500

@app.route('/delete_server_message', methods=['POST'])
@login_required
def delete_server_message():
    try:
        message_id = request.form.get('message_id')
        if not message_id:
            return "Message ID not provided", 400

        conn = sqlite3.connect('mogi_messages.db')
        c = conn.cursor()
        c.execute('DELETE FROM server_messages WHERE id = ?', (message_id,))
        conn.commit()
        conn.close()

        return redirect('/server_messages')

    except Exception as e:
        return f"Error: {str(e)}", 500

@app.route('/add_serial', methods=['POST'])
@login_required
def add_serial():
    try:
        data = request.get_json()
        if not data or 'serial_number' not in data:
            return jsonify({"error": "Serial number tidak ditemukan"}), 400
            
        serial_number = data['serial_number']
        Serial = Query()
        if serial_db.get(Serial.serial_number == serial_number):
            return jsonify({"error": "Serial number sudah ada"}), 409
            
        # Cek di communication logs untuk device name
        CommLog = Query()
        comm_logs = comm_log_db.search(CommLog.serial_number == serial_number)
        device_name = data.get('device_name')
        if not device_name and comm_logs:
            device_name = comm_logs[0].get('device_name', f'Device {serial_number}')
            
        new_record = {
            'serial_number': serial_number,
            'device_name': device_name or f'Device {serial_number}',
            'activation_date': datetime.now().strftime('%Y-%m-%d'),
            'is_valid': data.get('is_valid', True),
            'last_connection': None,
            'firmware_version': data.get('firmware_version', 'V01_2804_2025'),
            'notes': data.get('notes', '')
        }
        
        serial_db.insert(new_record)
        print(f"Serial number baru ditambahkan: {serial_number}")
        return jsonify({"message": "Serial number berhasil ditambahkan"}), 201
        
    except Exception as e:
        print(f"Error menambahkan serial number: {str(e)}")
        return jsonify({"error": str(e)}), 500

@app.route('/list_serials', methods=['GET'])
@login_required
def list_serials():
    try:
        all_serials = serial_db.all()
        return jsonify({
            "total": len(all_serials),
            "serials": all_serials
        }), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    # Create databases if they don't exist
    create_databases()
    
    port = 8888
    app.run(host='0.0.0.0', port=port)
    print(f'Listening at port {port}')