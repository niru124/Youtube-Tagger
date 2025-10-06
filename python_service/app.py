from flask import Flask, request, jsonify
from sentence_transformers import SentenceTransformer
import psycopg2
import numpy as np
import os
import requests

app = Flask(__name__)

# Load the Sentence Transformer model
# You might want to choose a different model based on your needs
model = SentenceTransformer('all-MiniLM-L6-v2')

# Database connection details (replace with your actual details or use environment variables)
DB_HOST = os.getenv("DB_HOST", "localhost")
DB_PORT = os.getenv("DB_PORT", "5432")
DB_NAME = os.getenv("DB_NAME", "youtube_topics")
DB_USER = os.getenv("DB_USER", "user")
DB_PASS = os.getenv("DB_PASS", "password")

# C++ Backend URL
CPP_BACKEND_URL = os.getenv("CPP_BACKEND_URL", "http://localhost:8000")

def get_db_connection():
    conn = psycopg2.connect(
        host=DB_HOST,
        port=DB_PORT,
        database=DB_NAME,
        user=DB_USER,
        password=DB_PASS
    )
    return conn

@app.route('/generate_and_store_embedding', methods=['POST'])
def generate_and_store_embedding():
    data = request.get_json()
    video_id = data.get('video_id')
    text_to_embed = data.get('text_to_embed') # This could be title, description, or combined

    if not video_id or not text_to_embed:
        return jsonify({"error": "video_id and text_to_embed are required"}), 400

    try:
        # Generate embedding
        embedding = model.encode(text_to_embed).tolist()

        # Send embedding to C++ backend
        cpp_backend_embedding_url = f"{CPP_BACKEND_URL}/videos/{video_id}/embedding"
        response = requests.post(cpp_backend_embedding_url, json={"embedding": embedding})
        response.raise_for_status() # Raise an exception for HTTP errors

        return jsonify({"message": "Embedding generated and sent to C++ backend successfully", "video_id": video_id}), 200

    except requests.exceptions.RequestException as e:
        return jsonify({"error": f"Failed to send embedding to C++ backend: {e}"}), 500
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/get_video_text/<video_id>', methods=['GET'])
def get_video_text(video_id):
    conn = None
    try:
        conn = get_db_connection()
        cur = conn.cursor()
        cur.execute("SELECT title FROM videos WHERE id = %s", (video_id,))
        result = cur.fetchone()
        cur.close()
        conn.close()

        if result:
            return jsonify({"video_id": video_id, "title": result[0]}), 200
        else:
            return jsonify({"error": "Video not found"}), 404
    except Exception as e:
        if conn:
            conn.close()
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
