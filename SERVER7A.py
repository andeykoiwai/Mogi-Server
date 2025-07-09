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
            </style>
            <script>
                function showTab(tabName) {
                    // Hide all tab contents
                    document.querySelectorAll('.tab-content').forEach(content => {
                        content.classList.remove('active');
                    });
                    
                    // Remove active class from all buttons
                    document.querySelectorAll('.tab-button').forEach(button => {
                        button.classList.remove('active');
                    });
                    
                    // Show selected tab content and activate button
                    document.getElementById(tabName).classList.add('active');
                    document.querySelector(`[onclick="showTab('${tabName}')"]`).classList.add('active');
                }
            </script>
        </head>
        <body>
            <div class="container">
                <div class="header">
                    <h1>Mogi Admin Dashboard</h1>
                    <a href="/logout" class="logout-btn">Logout</a>
                </div>
                
                <div class="tab-buttons">
                    <button class="tab-button active" onclick="showTab('serials')">Serial Numbers</button>
                    <button class="tab-button" onclick="showTab('quiz')">Quiz Results</button>
                    <button class="tab-button" onclick="showTab('logs')">Communication Logs</button>
                </div>
                
                <div id="serials" class="tab-content active">
                    <div class="section">
                        <h2>Serial Numbers</h2>
                        <table>
                            <tr>
                                <th>Serial Number</th>
                                <th>Device Name</th>
                                <th>Activation Date</th>
                                <th>Status</th>
                                <th>Last Connection</th>
                                <th>Firmware Version</th>
                            </tr>
                            {% for serial in serials %}
                            <tr>
                                <td>{{ serial.serial_number }}</td>
                                <td>{{ serial.device_name }}</td>
                                <td>{{ serial.activation_date }}</td>
                                <td>{{ 'Valid' if serial.is_valid else 'Invalid' }}</td>
                                <td>{{ serial.last_connection or 'Never' }}</td>
                                <td>{{ serial.firmware_version }}</td>
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
            </div>
        </body>
        </html>
    ''', serials=serials, quiz_results=quiz_results, comm_logs=comm_logs)

# Function to create databases if they don't exist
def create_databases():
    # 1. Serial Number Validation Database
    if not os.path.exists(SERIAL_VALIDATION_DB):
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
        print(f"Created {SERIAL_VALIDATION_DB}")

# Function to validate serial number
def validate_serial_number(serial_number):
    try:
        Serial = Query()
        device = serial_db.get(Serial.serial_number == serial_number)
        
        if device and device['is_valid']:
            # Update last connection time
            serial_db.update(
                {'last_connection': datetime.now().strftime('%Y-%m-%d %H:%M:%S')},
                Serial.serial_number == serial_number
            )
            return True
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
                
            if not serial_db.get(Serial.serial_number == data['serial_number']):
                return jsonify({'error': 'Serial number not found'}), 404
                
            # Update the record
            update_data = {k: v for k, v in data.items() if k != 'serial_number'}
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

if __name__ == '__main__':
    # Create databases if they don't exist
    create_databases()
    
    port = 8888
    app.run(host='0.0.0.0', port=port)
    print(f'Listening at port {port}')