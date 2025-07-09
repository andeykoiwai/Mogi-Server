from flask import Flask, request, jsonify, send_from_directory
import os
import speech_recognition as sr
import google.generativeai as genai
from gtts import gTTS
import random
import json

app = Flask(__name__)
record_file = 'recording.wav'
output_file = 'output.mp3'
server_user = ''  # Menyimpan server_user global

# Quiz state variables
current_quiz = None
quiz_questions = []
quiz_answers = []
quiz_scores = []
current_question_index = 0

# Setel API key secara manual (hanya untuk testing)
os.environ["GEMINI_API_KEY"] = "AIzaSyBgbnja1evGcdA9PxY-1P0yRf5Z7bohpPQ"
genai.configure(api_key=os.environ["GEMINI_API_KEY"])

model = genai.GenerativeModel("gemini-1.5-flash")

# Fungsi untuk menerima file audio secara utuh
@app.route('/uploadAudio', methods=['POST'])
def upload_audio():
    global server_user, current_quiz, current_question_index, quiz_questions, quiz_answers, quiz_scores
    
    if request.method == 'POST':
        try:
            # Save the uploaded file
            with open(record_file, 'wb') as f:
                f.write(request.data)

            # Transcribe the audio file
            esp_user = speech_to_text(record_file)
            
            # Check if we're in a quiz mode
            if current_quiz:
                if current_question_index < len(quiz_questions):
                    # Record the answer
                    quiz_answers.append(esp_user)
                    
                    # Check if answer is correct based on quiz type
                    if current_quiz == "math":
                        # Math quiz checking
                        correct_answer = str(eval(quiz_questions[current_question_index]['answer']))
                        if esp_user.replace(" ", "").strip() == correct_answer.strip():
                            quiz_scores.append(1)
                            feedback = f"Jawaban benar! {esp_user} adalah jawaban yang tepat."
                        else:
                            quiz_scores.append(0)
                            feedback = f"Jawaban kurang tepat. Jawaban yang benar adalah {correct_answer}."
                    elif current_quiz == "english":
                        # English quiz checking
                        correct_answer = quiz_questions[current_question_index]['answer'].lower()
                        user_answer = esp_user.lower().strip()
                        
                        if user_answer == correct_answer:
                            quiz_scores.append(1)
                            feedback = f"Correct! '{esp_user}' is the right answer."
                        else:
                            quiz_scores.append(0)
                            feedback = f"Not quite. The correct answer is '{correct_answer}'."
                    
                    current_question_index += 1
                    
                    # Check if we've finished all questions
                    if current_question_index >= len(quiz_questions):
                        final_score = sum(quiz_scores)
                        total_questions = len(quiz_questions)
                        percentage = (final_score / total_questions) * 100
                        
                        if current_quiz == "math":
                            server_user = f"Kuis selesai! Skor kamu adalah {final_score} dari {total_questions} pertanyaan, atau {percentage:.1f}%. "
                            
                            if percentage >= 80:
                                server_user += "Hebat sekali! Kamu sudah sangat pandai berhitung."
                            elif percentage >= 60:
                                server_user += "Bagus! Teruslah berlatih berhitung ya."
                            else:
                                server_user += "Tidak apa-apa, mari kita berlatih lebih banyak lagi."
                        elif current_quiz == "english":
                            server_user = f"Quiz completed! Your score is {final_score} out of {total_questions} questions, or {percentage:.1f}%. "
                            
                            if percentage >= 80:
                                server_user += "Excellent! Your English vocabulary is very good."
                            elif percentage >= 60:
                                server_user += "Good job! Keep practicing your English."
                            else:
                                server_user += "That's okay, let's keep learning more English words."
                            
                        # Reset quiz state
                        current_quiz = None
                        quiz_questions = []
                        quiz_answers = []
                        quiz_scores = []
                        current_question_index = 0
                    else:
                        # Prepare next question
                        server_user = feedback + " " + quiz_questions[current_question_index]['text']
                else:
                    # This shouldn't happen but just in case
                    server_user = "Maaf, terjadi kesalahan dalam kuis."
                    current_quiz = None
            else:
                # Check if user wants to start a quiz
                if "belajar menghitung" in esp_user.lower():
                    # Initialize math quiz
                    current_quiz = "math"
                    quiz_questions = generate_math_questions()
                    quiz_answers = []
                    quiz_scores = []
                    current_question_index = 0
                    
                    server_user = "Baik, mari belajar menghitung! Saya akan memberikan 5 pertanyaan matematika. " + quiz_questions[0]['text']
                elif "belajar bahasa inggris" in esp_user.lower():
                    # Initialize English quiz
                    current_quiz = "english"
                    quiz_questions = generate_english_questions()
                    quiz_answers = []
                    quiz_scores = []
                    current_question_index = 0
                    
                    server_user = "Let's learn English! I'll give you 5 vocabulary questions. " + quiz_questions[0]['text']
                else:
                    # Normal conversation mode
                    response = model.generate_content('jawab dengan singkat, ' + esp_user)
                    server_user = response.text  # Menyimpan server_user
            
            print(f'esp_user: {esp_user}')
            print(f'server_user: {server_user}')

            # Mengubah server_user ke dalam bentuk suara menggunakan gTTS
            if current_quiz == "english":
                language = 'en'  # Use English for the English quiz
            else:
                language = 'id'  # Use Indonesian for other purposes
                
            tts = gTTS(text=server_user, lang=language, slow=False)
            tts.save(output_file)
            print(f'server_user disimpan dalam {output_file}')

            # Mengembalikan hasil transkripsi dalam format JSON
            return jsonify({'esp_user': esp_user, 'server_user': server_user, 'in_quiz': current_quiz is not None, 'quiz_type': current_quiz}), 200
        except Exception as e:
            return str(e), 500
    else:
        return 'Method Not Allowed', 405

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
        # Mengembalikan status server
        return jsonify({'status': 'Server is up and running'}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

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
            language = 'en-US' if current_quiz == "english" else 'id-ID'
            
            # Menggunakan Google Speech Recognition untuk transkripsi
            text = recognizer.recognize_google(audio_data, language=language)
            print(f'esp_user: {text}')

            return text
        except sr.UnknownValueError:
            return "Could not understand audio"
        except sr.RequestError as e:
            return f"Could not request results from Google Speech Recognition service; {e}"

if __name__ == '__main__':
    port = 8888
    app.run(host='0.0.0.0', port=port)
    print(f'Listening at {port}')