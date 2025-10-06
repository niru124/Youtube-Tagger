#include "database.h"
#include "config.h"
#include "helpers.h" // Added for formatVector
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>

void Database::connect() {
    std::string conn_str = "host=" + DB_HOST + " port=" + std::to_string(DB_PORT) + " user=" + DB_USER + " password=" + DB_PASS + " dbname=" + DB_NAME;
    try {
        conn = std::make_unique<pqxx::connection>(conn_str);
        std::cout << "Connected to PostgreSQL server." << std::endl;
    } catch (const pqxx::broken_connection &e) {
        std::string error_msg = "Failed to connect to PostgreSQL: ";
        error_msg += e.what();
        throw std::runtime_error(error_msg);
    }
}

void Database::createTables() {
    try {
        pqxx::work txn(getConnection());

        // Enable the vector extension
        txn.exec("CREATE EXTENSION IF NOT EXISTS vector;");

        // Drop tables if they exist to ensure schema updates during development
        txn.exec("DROP TABLE IF EXISTS video_topics CASCADE;");
        txn.exec("DROP TABLE IF EXISTS videos CASCADE;");
        txn.exec("DROP TABLE IF EXISTS topics CASCADE;");
        txn.exec("DROP TABLE IF EXISTS users CASCADE;");

        std::string create_videos_sql = R"(
            CREATE TABLE IF NOT EXISTS videos (
            id VARCHAR(255) PRIMARY KEY,
            title VARCHAR(255),
            upload_date VARCHAR(255),
            last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            vector_embedding VECTOR(384)
            )
        )";
        txn.exec(create_videos_sql);

        std::string create_topics_sql = R"(
            CREATE TABLE IF NOT EXISTS topics (
            id SERIAL PRIMARY KEY,
            name VARCHAR(255) UNIQUE NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";
        txn.exec(create_topics_sql);

        std::string create_video_topics_sql = R"(
            CREATE TABLE IF NOT EXISTS video_topics (
            video_id VARCHAR(255) NOT NULL,
            topic_id INT NOT NULL,
            user_id VARCHAR(255) NOT NULL,
            vote INT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (video_id, topic_id, user_id),
            FOREIGN KEY (video_id) REFERENCES videos(id),
            FOREIGN KEY (topic_id) REFERENCES topics(id)
            )
        )";
        txn.exec(create_video_topics_sql);

        std::string create_users_sql = R"(
            CREATE TABLE IF NOT EXISTS users (
            id VARCHAR(255) PRIMARY KEY,
            username VARCHAR(255) UNIQUE,
            reputation INT DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";
        txn.exec(create_users_sql);

        // Create vector index for efficient similarity search
        txn.exec("CREATE INDEX IF NOT EXISTS videos_vector_idx ON videos USING ivfflat (vector_embedding vector_cosine_ops);");

        txn.commit();
        std::cout << "Database tables and indexes checked/created successfully." << std::endl;
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error creating tables: " << e.what() << std::endl;
        throw;
    }
}

Database::Database() : thread_pool(4) {  // Initialize thread pool with 4 threads
    connect();
    createTables();

    // Prepare statements
    pqxx::connection& c = getConnection();
    c.prepare("get_video_by_id", "SELECT id, title, upload_date, last_updated FROM videos WHERE id = $1");
    c.prepare("insert_video", "INSERT INTO videos (id, title) VALUES ($1, $2)");
    c.prepare("insert_video_no_title", "INSERT INTO videos (id) VALUES ($1)");
    c.prepare("get_topic_by_name", "SELECT id, name, created_at FROM topics WHERE name = $1");
    c.prepare("insert_topic", "INSERT INTO topics (name) VALUES ($1) RETURNING id");
    c.prepare("get_video_topic_vote", "SELECT video_id, topic_id, user_id, vote, created_at FROM video_topics WHERE video_id = $1 AND topic_id = $2 AND user_id = $3");
    c.prepare("update_video_topic_vote", "UPDATE video_topics SET vote = $1, created_at = CURRENT_TIMESTAMP WHERE video_id = $2 AND topic_id = $3 AND user_id = $4");
    c.prepare("insert_video_topic_vote", "INSERT INTO video_topics (video_id, topic_id, user_id, vote) VALUES ($1, $2, $3, $4)");
    c.prepare("delete_video_topic_vote", "DELETE FROM video_topics WHERE video_id = $1 AND topic_id = $2 AND user_id = $3");
    c.prepare("get_aggregated_topics_for_video",
        "SELECT t.id AS topic_id, t.name AS topic_name, SUM(vt.vote) AS total_votes "
        "FROM video_topics vt "
        "JOIN topics t ON vt.topic_id = t.id "
        "WHERE vt.video_id = $1 "
        "GROUP BY t.id, t.name "
        "ORDER BY total_votes DESC");
    c.prepare("get_similar_videos",
        "SELECT vt2.video_id, v2.title, COUNT(DISTINCT vt2.topic_id) AS shared_topics_count "
        "FROM video_topics vt1 "
        "JOIN video_topics vt2 ON vt1.topic_id = vt2.topic_id "
        "JOIN videos v2 ON vt2.video_id = v2.id "
        "WHERE vt1.video_id = $1 AND vt2.video_id != $2 "
        "GROUP BY vt2.video_id, v2.title "
        "ORDER BY shared_topics_count DESC");
    c.prepare("get_user_details", "SELECT id, username, reputation, created_at FROM users WHERE id = $1");
    c.prepare("get_user_submissions_count", "SELECT COUNT(*) FROM video_topics WHERE user_id = $1");
    c.prepare("get_user_last_submission_date", "SELECT created_at FROM video_topics WHERE user_id = $1 ORDER BY created_at DESC LIMIT 1");
    c.prepare("get_user_most_frequent_tag",
        "SELECT t.name AS topic_name, COUNT(vt.topic_id) AS topic_count "
        "FROM video_topics vt "
        "JOIN topics t ON vt.topic_id = t.id "
        "WHERE vt.user_id = $1 "
        "GROUP BY t.name "
        "ORDER BY topic_count DESC "
        "LIMIT 1");
    c.prepare("upsert_user", "INSERT INTO users (id, username) VALUES ($1, $2) ON CONFLICT (id) DO UPDATE SET username = EXCLUDED.username");
    c.prepare("upsert_user_no_username", "INSERT INTO users (id) VALUES ($1) ON CONFLICT (id) DO NOTHING");
    c.prepare("update_video_embedding", "UPDATE videos SET vector_embedding = $1 WHERE id = $2");
    c.prepare("get_video_embedding", "SELECT vector_embedding FROM videos WHERE id = $1");
    c.prepare("get_similar_videos_by_vector",
        "SELECT id, title, upload_date, last_updated, 1 - (vector_embedding <=> $1) AS similarity "
        "FROM videos ");
}

Database::~Database() {
    // Connection is managed by unique_ptr, no explicit disconnect needed.
}

pqxx::connection& Database::getConnection() {
    if (!conn || !conn->is_open()) {
        throw std::runtime_error("Database connection is not open.");
    }
    return *conn;
}

void Database::updateVideoEmbedding(const std::string& videoId, const std::vector<float>& embedding) {
    try {
        pqxx::work txn(getConnection());
        std::string embedding_str = formatVectorForPgvector(embedding);
        txn.exec_prepared("update_video_embedding", embedding_str, videoId);
        txn.commit();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in updateVideoEmbedding: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::getSimilarVideosByVector(const std::string& videoId, int limit) {
    nlohmann::json similar_videos = nlohmann::json::array();
    try {
        pqxx::work txn(getConnection());

        // First, get the embedding of the target video
        pqxx::result r_embedding = txn.exec_prepared("get_video_embedding", videoId);
        if (r_embedding.empty() || r_embedding[0][0].is_null()) {
            std::cerr << "Error: Embedding not found for video ID: " << videoId << std::endl;
            return similar_videos; // Return empty if target embedding not found
        }
        std::string target_embedding_str = r_embedding[0][0].as<std::string>();
        std::cerr << "  Target embedding for " << videoId << ": " << target_embedding_str.substr(0, 50) << "..." << std::endl; // Log first 50 chars

        // Now, use this embedding to find similar videos
        pqxx::result r = txn.exec_prepared("get_similar_videos_by_vector", target_embedding_str);
        std::cerr << "  Executed get_similar_videos_by_vector for " << videoId << ", returned " << r.size() << " rows." << std::endl;

        for (const auto& row : r) {
            nlohmann::json video_data;
            video_data["id"] = row["id"].as<std::string>();
            video_data["title"] = row["title"].is_null() ? nullptr : row["title"].c_str();
            video_data["upload_date"] = row["upload_date"].is_null() ? nullptr : row["upload_date"].c_str();
            video_data["last_updated"] = row["last_updated"].is_null() ? nullptr : row["last_updated"].c_str();
            video_data["similarity"] = row["similarity"].as<double>();
            similar_videos.push_back(video_data);
        }
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getSimilarVideosByVector: " << e.what() << std::endl;
        // It's important to return an empty JSON array in case of an error
        return nlohmann::json::array();
    }
    return similar_videos;
}

nlohmann::json Database::getVideoById(const std::string& videoId) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_video_by_id", videoId);

        if (!r.empty()) {
            nlohmann::json video_data;
            const auto& row = r[0];
            video_data["id"] = row["id"].as<std::string>();
            video_data["title"] = row["title"].is_null() ? "" : row["title"].as<std::string>();
            video_data["upload_date"] = row["upload_date"].is_null() ? "" : row["upload_date"].as<std::string>();
            video_data["last_updated"] = row["last_updated"].is_null() ? "" : row["last_updated"].as<std::string>();
            return video_data;
        }
        return nlohmann::json();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getVideoById: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::insertVideo(const std::string& videoId, const std::string& title) {
    try {
        pqxx::work txn(getConnection());
        if (title.empty()) {
            txn.exec_prepared("insert_video_no_title", videoId);
        } else {
            txn.exec_prepared("insert_video", videoId, title);
        }
        txn.commit();

        nlohmann::json video_data;
        video_data["id"] = videoId;
        video_data["title"] = title.empty() ? nullptr : title;
        return video_data;
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in insertVideo: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::getTopicByName(const std::string& topicName) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_topic_by_name", topicName);

        if (!r.empty()) {
            nlohmann::json topic_data;
            const auto& row = r[0];
            topic_data["id"] = row["id"].as<int>();
            topic_data["name"] = row["name"].as<std::string>();
            topic_data["created_at"] = row["created_at"].as<std::string>();
            return topic_data;
        }
        return nlohmann::json();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getTopicByName: " << e.what() << std::endl;
        throw;
    }
}

int Database::insertTopic(const std::string& topicName) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("insert_topic", topicName);
        txn.commit();
        return r[0][0].as<int>(); // Assuming the query returns the ID of the inserted topic
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in insertTopic: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::getVideoTopicVote(const std::string& videoId, int topicId, const std::string& userId) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_video_topic_vote", videoId, topicId, userId);

        if (!r.empty()) {
            nlohmann::json vote_data;
            const auto& row = r[0];
            vote_data["video_id"] = row["video_id"].as<std::string>();
            vote_data["topic_id"] = row["topic_id"].as<int>();
            vote_data["user_id"] = row["user_id"].as<std::string>();
            vote_data["vote"] = row["vote"].as<int>();
            vote_data["created_at"] = row["created_at"].as<std::string>();
            return vote_data;
        }
        return nlohmann::json();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getVideoTopicVote: " << e.what() << std::endl;
        throw;
    }
}

void Database::updateVideoTopicVote(const std::string& videoId, int topicId, const std::string& userId, int newVoteValue) {
    try {
        pqxx::work txn(getConnection());
        txn.exec_prepared("update_video_topic_vote", newVoteValue, videoId, topicId, userId);
        txn.commit();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in updateVideoTopicVote: " << e.what() << std::endl;
        throw;
    }
}

void Database::insertVideoTopicVote(const std::string& videoId, int topicId, const std::string& userId, int voteValue) {
    try {
        pqxx::work txn(getConnection());
        txn.exec_prepared("insert_video_topic_vote", videoId, topicId, userId, voteValue);
        txn.commit();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in insertVideoTopicVote: " << e.what() << std::endl;
        throw;
    }
}

void Database::deleteVideoTopicVote(const std::string &videoId, int topicId, const std::string &userId) {
    try {
        pqxx::work txn(getConnection());
        txn.exec_prepared("delete_video_topic_vote", videoId, topicId, userId);
        txn.commit();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in deleteVideoTopicVote: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::getAggregatedTopicsForVideo(const std::string& videoId) {
    nlohmann::json topics_list = nlohmann::json::array();
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_aggregated_topics_for_video", videoId);

        for (const auto& row : r) {
            nlohmann::json topic_data;
            topic_data["topic_id"] = row["topic_id"].as<int>();
            topic_data["topic_name"] = row["topic_name"].as<std::string>();
            topic_data["total_votes"] = row["total_votes"].as<int>();
            topics_list.push_back(topic_data);
        }
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getAggregatedTopicsForVideo: " << e.what() << std::endl;
        throw;
    }
    return topics_list;
}

nlohmann::json Database::getSimilarVideos(const std::string& videoId) {
    nlohmann::json similar_videos = nlohmann::json::array();
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_similar_videos", videoId, videoId);

        for (const auto& row : r) {
            nlohmann::json video_data;
            video_data["video_id"] = row["video_id"].as<std::string>();
            video_data["title"] = row["title"].as<std::string>();
            video_data["shared_topics_count"] = row["shared_topics_count"].as<int>();
            similar_videos.push_back(video_data);
        }
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getSimilarVideos: " << e.what() << std::endl;
        throw;
    }
    return similar_videos;
}

nlohmann::json Database::getUserDetails(const std::string &userId) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_user_details", userId);

        if (!r.empty()) {
            nlohmann::json user_data;
            const auto& row = r[0];
            user_data["id"] = row["id"].as<std::string>();
            user_data["username"] = row["username"].as<std::string>();
            user_data["reputation"] = row["reputation"].as<int>();
            user_data["created_at"] = row["created_at"].as<std::string>();
            return user_data;
        }
        return nlohmann::json();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getUserDetails: " << e.what() << std::endl;
        throw;
    }
}

int Database::getUserSubmissionsCount(const std::string &userId) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_user_submissions_count", userId);

        if (!r.empty()) {
            return r[0][0].as<int>();
        }
        return 0;
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getUserSubmissionsCount: " << e.what() << std::endl;
        throw;
    }
}

std::string Database::getUserLastSubmissionDate(const std::string &userId) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_user_last_submission_date", userId);

        if (!r.empty()) {
            return r[0][0].as<std::string>();
        }
        return "";
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getUserLastSubmissionDate: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::getUserMostFrequentTag(const std::string &userId) {
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec_prepared("get_user_most_frequent_tag", userId);

        if (!r.empty()) {
            nlohmann::json tag_data;
            const auto& row = r[0];
            tag_data["topic_name"] = row["topic_name"].as<std::string>();
            tag_data["topic_count"] = row["topic_count"].as<int>();
            return tag_data;
        }
        return nlohmann::json();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getUserMostFrequentTag: " << e.what() << std::endl;
        throw;
    }
}

nlohmann::json Database::getAllUsersWithContributionCounts() {
    nlohmann::json users_list = nlohmann::json::array();
    try {
        pqxx::work txn(getConnection());
        pqxx::result r = txn.exec(
            "SELECT u.id, u.username, COUNT(vt.user_id) AS contributions_count "
            "FROM users u "
            "LEFT JOIN video_topics vt ON u.id = vt.user_id "
            "GROUP BY u.id, u.username "
            "ORDER BY contributions_count DESC, u.username ASC"
        );

        for (const auto& row : r) {
            nlohmann::json user_data;
            user_data["id"] = row["id"].as<std::string>();
            user_data["username"] = row["username"].as<std::string>();
            user_data["contributions_count"] = row["contributions_count"].as<int>();
            users_list.push_back(user_data);
        }
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in getAllUsersWithContributionCounts: " << e.what() << std::endl;
        throw;
    }
    return users_list;
}

void Database::upsertUser(const std::string &userId, const std::string &username) {
    try {
        pqxx::work txn(getConnection());
        if (username.empty()) {
            txn.exec_prepared("upsert_user_no_username", userId);
        } else {
            txn.exec_prepared("upsert_user", userId, username);
        }
        txn.commit();
    } catch (const pqxx::sql_error &e) {
        std::cerr << "Error in upsertUser: " << e.what() << std::endl;
        throw;
    }
}

// Async implementations
std::future<nlohmann::json> Database::getVideoByIdAsync(const std::string& videoId) {
    return std::async(std::launch::async, [this, videoId]() {
        return this->getVideoById(videoId);
    });
}

std::future<nlohmann::json> Database::insertVideoAsync(const std::string& videoId, const std::string& title) {
    return std::async(std::launch::async, [this, videoId, title]() {
        return this->insertVideo(videoId, title);
    });
}

std::future<nlohmann::json> Database::getTopicByNameAsync(const std::string& topicName) {
    return std::async(std::launch::async, [this, topicName]() {
        return this->getTopicByName(topicName);
    });
}

std::future<int> Database::insertTopicAsync(const std::string& topicName) {
    return std::async(std::launch::async, [this, topicName]() {
        return this->insertTopic(topicName);
    });
}

std::future<nlohmann::json> Database::getAggregatedTopicsForVideoAsync(const std::string& videoId) {
    return std::async(std::launch::async, [this, videoId]() {
        return this->getAggregatedTopicsForVideo(videoId);
    });
}

std::future<nlohmann::json> Database::getSimilarVideosAsync(const std::string& videoId) {
    return std::async(std::launch::async, [this, videoId]() {
        return this->getSimilarVideos(videoId);
    });
}

std::future<nlohmann::json> Database::getUserDetailsAsync(const std::string& userId) {
    return std::async(std::launch::async, [this, userId]() {
        return this->getUserDetails(userId);
    });
}

std::future<int> Database::getUserSubmissionsCountAsync(const std::string& userId) {
    return std::async(std::launch::async, [this, userId]() {
        return this->getUserSubmissionsCount(userId);
    });
}

std::future<std::string> Database::getUserLastSubmissionDateAsync(const std::string& userId) {
    return std::async(std::launch::async, [this, userId]() {
        return this->getUserLastSubmissionDate(userId);
    });
}

std::future<nlohmann::json> Database::getUserMostFrequentTagAsync(const std::string& userId) {
    return std::async(std::launch::async, [this, userId]() {
        return this->getUserMostFrequentTag(userId);
    });
}

std::future<void> Database::upsertUserAsync(const std::string& userId, const std::string& username) {
    return std::async(std::launch::async, [this, userId, username]() {
        this->upsertUser(userId, username);
    });
}

std::future<void> Database::updateVideoEmbeddingAsync(const std::string& videoId, const std::vector<float>& embedding) {
    return std::async(std::launch::async, [this, videoId, embedding]() {
        this->updateVideoEmbedding(videoId, embedding);
    });
}

std::future<nlohmann::json> Database::getSimilarVideosByVectorAsync(const std::string& videoId, int limit) {
    return std::async(std::launch::async, [this, videoId, limit]() {
        return this->getSimilarVideosByVector(videoId, limit);
    });
}