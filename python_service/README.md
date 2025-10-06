# Python Embedding Service

This Flask service is responsible for generating vector embeddings for video content (e.g., titles, descriptions) and pushing them to the C++ backend.

## Setup

1.  **Navigate to the service directory:**
    ```bash
    cd /home/nirantar/Downloads/seperate_1/youtube-topic-dataset/backend/ALGOBAAP/python_service
    ```

2.  **Create a virtual environment (recommended):**
    ```bash
    python3 -m venv venv
    source venv/bin/activate
    ```

3.  **Install dependencies:**
    ```bash
    pip install -r requirements.txt
    ```

## Configuration

Set the following environment variables for database connection and C++ backend URL:

*   `DB_HOST`: PostgreSQL host (default: `localhost`)
*   `DB_PORT`: PostgreSQL port (default: `5432`)
*   `DB_NAME`: PostgreSQL database name (default: `youtube_topics`)
*   `DB_USER`: PostgreSQL username (default: `user`)
*   `DB_PASS`: PostgreSQL password (default: `password`)
*   `CPP_BACKEND_URL`: URL of the C++ backend (default: `http://localhost:8000`)

Example:
```bash
export DB_HOST=localhost
export DB_PORT=5432
export DB_NAME=youtube_topics
export DB_USER=your_db_user
export DB_PASS=your_db_password
export CPP_BACKEND_URL=http://localhost:8000
```

## Running the Service

```bash
python app.py
```

The service will run on `http://0.0.0.0:5000`.

## Endpoints

### `POST /generate_and_store_embedding`

Generates an embedding for the provided text and sends it to the C++ backend.

**Request Body:**

```json
{
    "video_id": "<youtube_video_id>",
    "text_to_embed": "<text_content_to_embed>"
}
```

**Example using `curl`:**

```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"video_id": "dQw4w9WgXcQ", "text_to_embed": "Rick Astley - Never Gonna Give You Up (Official Music Video)"}' \
     http://localhost:5000/generate_and_store_embedding
```

### `GET /get_video_text/<video_id>`

Retrieves the title of a video from the PostgreSQL database.

**Example using `curl`:**

```bash
curl http://localhost:5000/get_video_text/dQw4w9WgXcQ
```

