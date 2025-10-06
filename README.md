# YouTube Topic Dataset Backend

This document outlines how to set up and interact with the YouTube Topic Dataset backend, which includes a C++ API service and a PostgreSQL database with `pgvector` for efficient similarity searches.

## Project Setup

The project uses Docker Compose to manage the PostgreSQL database and the C++ backend service.

### Prerequisites

*   Docker and Docker Compose installed on your system.

### Getting Started

1.  **Navigate to the project root:**
    ```bash
    cd /home/nirantar/Downloads/seperate_1/youtube-topic-dataset/backend/ALGOBAAP
    ```

2.  **Build and run the Docker services:**
    This command will build the custom PostgreSQL image (with `pgvector`) and the C++ backend service, then start both in detached mode.
    ```bash
    docker-compose up --build -d
    ```

## Project Setup

The project uses Docker Compose to manage the PostgreSQL database and the C++ backend service.

### Prerequisites

*   Docker and Docker Compose installed on your system.

### Getting Started (Docker Compose)

1.  **Navigate to the project root:**
    ```bash
    cd /home/nirantar/Downloads/seperate_1/youtube-topic-dataset/backend/ALGOBAAP
    ```

2.  **Build and run the Docker services:**
    This command will build the custom PostgreSQL image (with `pgvector`) and the C++ backend service, then start both in detached mode.
    ```bash
    docker-compose up --build -d
    ```

## Manual Installation

If you prefer not to use Docker Compose, you can set up the PostgreSQL database and C++ backend manually.

### Manual Installation (PostgreSQL with pgvector)

1.  **Install PostgreSQL:**
    Follow the official PostgreSQL installation guide for your operating system.

2.  **Install `pgvector`:**
    Refer to the `Dockerfile.postgres` file in this project for the exact steps to build and install `pgvector` from source. This typically involves installing PostgreSQL development headers, `git`, cloning the `pgvector` repository, and then building and installing it.

3.  **Create Database and Enable Extension:**
    After installing PostgreSQL and `pgvector`, create your database and enable the `vector` extension:
    ```sql
    CREATE DATABASE youtube_topics;
    \c youtube_topics;
    CREATE EXTENSION vector;
    ```

4.  **Configure Database Credentials:**
    Ensure your PostgreSQL user (`testuser`) and password (`testpass`) match the values expected by the C++ backend, or update the C++ backend's configuration (`cpp_backend/src/config.h`) accordingly.

### Manual Installation (C++ Backend)

1.  **Install Dependencies:**
    You will need the following development libraries and tools:
    *   `build-essential` (or equivalent for your OS)
    *   `cmake`
    *   `libpqxx-dev` (PostgreSQL C++ client library development files)
    *   `libssl-dev` (OpenSSL development files)
    *   `zlib1g-dev` (ZLib development files)
    *   `libboost-system-dev`, `libboost-thread-dev` (Boost libraries)
    *   `git`
    *   `nlohmann/json` (header-only JSON library)
    *   `Crow` (C++ web framework)

    Example for Ubuntu/Debian:
    ```bash
    sudo apt-get update
    sudo apt-get install -y build-essential cmake libpqxx-dev libssl-dev zlib1g-dev libboost-system-dev libboost-thread-dev git
    ```

2.  **Install nlohmann/json and Crow:**
    Since these are header-only libraries, you can clone their repositories and copy the headers to a system include path:
    ```bash
    git clone https://github.com/nlohmann/json.git /tmp/json
    sudo cp -r /tmp/json/include/nlohmann /usr/local/include/
    rm -rf /tmp/json

    git clone https://github.com/ipkn/crow.git /tmp/crow
    sudo mkdir -p /usr/local/include/crow
    sudo cp -r /tmp/crow/include/. /usr/local/include/crow
    rm -rf /tmp/crow
    ```

3.  **Build the C++ Backend:**
    Navigate to the `cpp_backend` directory and build the project:
    ```bash
    cd /home/nirantar/Downloads/seperate_1/youtube-topic-dataset/backend/ALGOBAAP/cpp_backend
    mkdir -p build
    cmake -S . -B build
    cmake --build build --target youtube-topic-crow
    ```

4.  **Run the C++ Backend:**
    After a successful build, you can run the executable:
    ```bash
    ./build/youtube-topic-crow
    ```
    This will start the API server on port `8000` and connect to your PostgreSQL database to create tables.

## Database Configuration (PostgreSQL with pgvector)

The `Dockerfile.postgres` sets up a PostgreSQL 13 database with the `pgvector` extension. The C++ backend service (`youtube-topic-crow`) automatically creates the necessary tables and enables the `vector` extension upon startup.

### Verifying pgvector Setup

You can verify that `pgvector` is correctly installed and the tables are created by connecting to the PostgreSQL container:

1.  **Connect to the PostgreSQL container:**
    ```bash
    docker exec -it youtube_topics_postgres psql -U testuser -d youtube_topics
    ```

2.  **Inside the PostgreSQL prompt, verify the `vector` extension and table schema:**
    ```sql
    SELECT * FROM pg_extension WHERE extname = 'vector';
    \d videos
    ```
    You should see an entry for the `vector` extension and the `videos` table description should include a `vector_embedding` column of type `vector(384)` and an `ivfflat` index.

## Backend API Routes Documentation

This section outlines the available API routes for the YouTube Topic Dataset backend. The C++ backend runs on `http://localhost:8000`.

### 1. Video Management

#### `POST /videos`
Adds a new video to the database. If the video already exists, it returns the existing video's information.

-   **Method:** `POST`
-   **Headers:** `Content-Type: application/json`
-   **Body:**
    ```json
    {
        "id": "https://www.youtube.com/watch?v=VIDEO_ID",
        "title": "Optional Video Title"
    }
    ```
-   **Example Request:**
    ```bash
    curl -X POST http://localhost:8000/videos -H "Content-Type: application/json" -d '{"id":"https://www.youtube.com/watch?v=SJCnLY4onWc","title":"My Awesome Video"}'
    ```
-   **Example Success Response (201 Created):**
    ```json
    {"id":"SJCnLY4onWc","title":"My Awesome Video"}
    ```
-   **Example Success Response (200 OK, if video already exists):**
    ```json
    {"id":"SJCnLY4onWc","title":"My Awesome Video"}
    ```
-   **Example Error Response (400 Bad Request):**
    ```json
    {"error":"Video URL is required."}
    ```

#### `GET /videos/:id`
Retrieves details for a specific video by its ID.

-   **Method:** `GET`
-   **URL Parameters:** `:id` - The YouTube video ID.
-   **Example Request:**
    ```bash
    curl -X GET http://localhost:8000/videos/SJCnLY4onWc
    ```
-   **Example Success Response (200 OK):**
    ```json
    {
        "id": "SJCnLY4onWc",
        "title": "My Awesome Video",
        "upload_date": "",
        "last_updated": "2025-10-06 12:34:56"
    }
    ```
-   **Example Error Response (404 Not Found):**
    ```json
    {"error":"Video not found."}
    ```

#### `POST /videos/:id/embedding`
Updates a video's vector embedding. This is typically used after an ML model generates an embedding for the video.

-   **Method:** `POST`
-   **URL Parameters:** `:id` - The YouTube video ID.
-   **Headers:** `Content-Type: application/json`
-   **Body:**
    ```json
    {
        "embedding": [0.1, 0.2, ..., 0.384] // An array of 384 float values
    }
    ```
-   **Example Request:**
    ```bash
    # Generate a dummy embedding (384 floats)
    EMBEDDING_DATA=$(python -c "import json; print(json.dumps({'embedding': [i/1000.0 for i in range(384)]}))")
    curl -X POST -H "Content-Type: application/json" -d "$EMBEDDING_DATA" http://localhost:8000/videos/SJCnLY4onWc/embedding
    ```
-   **Example Success Response (202 Accepted):**
    ```json
    {"message":"Embedding update accepted."}
    ```
-   **Example Error Response (400 Bad Request):**
    ```json
    {"error":"Invalid embedding size. Expected 384 dimensions."}
    ```

#### `GET /videos/:id/similar_by_vector`
Retrieves videos similar to the given video ID based on their vector embeddings.

-   **Method:** `GET`
-   **URL Parameters:** `:id` - The YouTube video ID.
-   **Query Parameters:** `limit` (optional) - Number of similar videos to return (default: 10).
-   **Example Request:**
    ```bash
    curl -X GET http://localhost:8000/videos/SJCnLY4onWc/similar_by_vector?limit=5
    ```
-   **Example Success Response (200 OK):**
    ```json
    [
        {
            "id": "anotherVideoId1",
            "title": "Another Similar Video 1",
            "upload_date": "",
            "last_updated": "2025-10-06 12:35:00",
            "similarity": 0.95
        },
        {
            "id": "anotherVideoId2",
            "title": "Another Similar Video 2",
            "upload_date": "",
            "last_updated": "2025-10-06 12:36:00",
            "similarity": 0.92
        }
    ]
    ```
-   **Example Error Response (500 Internal Server Error):**
    ```json
    {"error":"Error in getSimilarVideosByVector: Database error message."}
    ```

### 2. Topic Management

#### `GET /videos/:id/topics`
Retrieves all topics associated with a specific video ID, along with their aggregated vote counts.

-   **Method:** `GET`
-   **URL Parameters:** `:id` - The YouTube video ID.
-   **Example Request:**
    ```bash
    curl http://localhost:8000/videos/SJCnLY4onWc/topics
    ```
-   **Example Success Response (200 OK):**
    ```json
    {
        "video_id": "SJCnLY4onWc",
        "topics": [
            {
                "topic_id": 1,
                "topic_name": "Machine Learning",
                "total_votes": 1
            }
        ]
    }
    ```
-   **Example Error Response (500 Internal Server Error):**
    ```json
    {"error":"Database error message."}
    ```

#### `POST /videos/:id/topics`
Submits a new topic or casts/changes/removes a vote on an existing topic for a given video and user.
This route implements a "one user, one vote per topic" system with toggling functionality.
The `desired_vote` value is clamped: any value > 1 becomes 1, and any value < -1 becomes -1.

-   **Method:** `POST`
-   **URL Parameters:** `:id` - The YouTube video ID.
-   **Headers:** `Content-Type: application/json`
-   **Body:**
    ```json
    {
        "name": "Optional New Topic Name", // Required if topic_id is 0 or not provided
        "topic_id": 0,                     // Required if name is empty (for existing topics)
        "desired_vote": 1,                 // 1 for upvote, -1 for downvote, 0 to remove vote
        "user_id": "user-123"              // Unique identifier for the user (expected from frontend localStorage)
    }
    ```
-   **Behavior:**
    *   If `user_id` is not provided in the request body, a new one will be generated by the backend (fallback).
    *   If `name` is provided and `topic_id` is 0, a new topic will be created if it doesn't exist.
    *   **Voting Logic:**
        *   If the user has no existing vote for this topic/video, a new vote with `desired_vote` (1 or -1) is recorded.
        *   If the user has an existing vote:
            *   If `current_vote == desired_vote` (e.g., user upvoted, then upvoted again), the vote is **removed** (toggled off).
            *   If `current_vote != desired_vote` (e.g., user upvoted, then downvoted), the vote is **updated** to `desired_vote`.
            *   If `desired_vote == 0`, the vote is **removed**.
-   **Example Request (New Topic Upvote):**
    ```bash
    curl -X POST http://localhost:8000/videos/SJCnLY4onWc/topics -H "Content-Type: application/json" -d '{"name":"Machine Learning","desired_vote":1,"user_id":"test_user_1"}'
    ```
-   **Example Request (Toggle Off Existing Upvote):**
    ```bash
    curl -X POST http://localhost:8000/videos/SJCnLY4onWc/topics -H "Content-Type: application/json" -d '{"name":"Machine Learning","desired_vote":1,"user_id":"test_user_1"}'
    ```
-   **Example Request (Change Vote from Up to Down):**
    ```bash
    curl -X POST http://localhost:8000/videos/SJCnLY4onWc/topics -H "Content-Type: application/json" -d '{"name":"Machine Learning","desired_vote":-1,"user_id":"test_user_1"}'
    ```
-   **Example Request (Clamped Vote - `desired_vote:5` becomes `1`):**
    ```bash
    curl -X POST http://localhost:8000/videos/SJCnLY4onWc/topics -H "Content-Type: application/json" -d '{"name":"Clamped Topic","desired_vote":5,"user_id":"user-789"}'
    ```
-   **Example Success Response (201 Created):**
    ```json
    {"message":"Vote recorded successfully","user_id":"test_user_1"}
    ```
-   **Example Success Response (200 OK):**
    ```json
    {"message":"Vote updated successfully","user_id":"test_user_1"}
    ```
-   **Example Success Response (200 OK, vote removed):**
    ```json
    {"message":"Vote removed successfully","user_id":"test_user_1"}
    ```
-   **Example Error Response (400 Bad Request):**
    ```json
    {"error":"Desired vote must be 1 (upvote), -1 (downvote), or 0 (no vote)."}
    ```

### 3. Similar Videos (Topic-Based)

#### `GET /videos/:id/similar`
Retrieves videos similar to the given video ID based on shared topics.

-   **Method:** `GET`
-   **URL Parameters:** `:id` - The YouTube video ID.
-   **Example Request:**
    ```bash
    curl http://localhost:8000/videos/SJCnLY4onWc/similar
    ```
-   **Example Success Response (200 OK):**
    ```json
    [
        {
            "shared_topics_count": 2,
            "title": "Another Related Video",
            "video_id": "anotherVideoId"
        }
    ]
    ```
-   **Example Error Response (500 Internal Server Error):**
    ```json
    {"error":"Database error message."}
    ```

### 4. User Statistics

#### `GET /users/:id/stats`
Retrieves detailed statistics for a specific user.

-   **Method:** `GET`
-   **URL Parameters:** `:id` - The user ID.
-   **Example Request:**
    ```bash
    curl http://localhost:8000/users/test_user_1/stats
    ```
-   **Example Success Response (200 OK):**
    ```json
    {
        "created_at": "2023-10-27 10:00:00",
        "last_submission_date": "2023-10-27 11:30:00",
        "most_frequent_tag": {
            "topic_count": 1,
            "topic_name": "Machine Learning"
        },
        "reputation": 0,
        "submissions_count": 1,
        "user_id": "test_user_1",
        "username": null
    }
    ```
-   **Example Error Response (404 Not Found):**
    ```json
    {"error":"User not found."}
    ```
-   **Example Error Response (500 Internal Server Error):**
    ```json
    {"error":"Database error message."}
    ```

#### `GET /users/contributions`
Retrieves a list of all users along with their total contribution counts (number of topics submitted/voted on).

-   **Method:** `GET`
-   **Example Request:**
    ```bash
    curl http://localhost:8000/users/contributions
    ```
-   **Example Success Response (200 OK):**
    ```json
    [
        {
            "contributions_count": 1,
            "id": "test_user_1",
            "username": null
        }
    ]
    ```
-   **Example Error Response (500 Internal Server Error):**
    ```json
    {"error":"Database error message."}
    ```

### 5. General Test Route

#### `GET /test`
A simple test route to check if the backend is running.

-   **Method:** `GET`
-   **Example Request:**
    ```bash
    curl -X GET http://localhost:8000/test
    ```
-   **Example Success Response (200 OK):**
    ```
    Test successful!
    ```