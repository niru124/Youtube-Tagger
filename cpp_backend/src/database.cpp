#include "database.h"
#include "config.h"
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>

void Database::connect() {
    conn = mysql_init(NULL);
    if (!conn) {
        throw std::runtime_error("mysql_init failed");
    }

    // Connect without specifying a database initially
    if (!mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), DB_PASS.c_str(), NULL, DB_PORT, NULL, 0)) {
        std::string error_msg = "Failed to connect to MySQL: ";
        error_msg += mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error(error_msg);
    }
    std::cout << "Connected to MySQL server." << std::endl;
}

void Database::createTables() {
    std::string create_db_sql = "CREATE DATABASE IF NOT EXISTS " + DB_NAME;
    if (mysql_query(conn, create_db_sql.c_str())) {
        std::cerr << "Error creating database: " << mysql_error(conn) << std::endl;
    }

    std::string use_db_sql = "USE " + DB_NAME;
    if (mysql_query(conn, use_db_sql.c_str())) {
        std::cerr << "Error using database: " << mysql_error(conn) << std::endl;
    }

    // Corrected multi-line SQL string literals using raw string literals
    std::string create_videos_sql = R"(
        CREATE TABLE IF NOT EXISTS videos (
        id VARCHAR(255) PRIMARY KEY,
        title VARCHAR(255) NULL,
        upload_date VARCHAR(255) NULL,
        last_updated DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (mysql_query(conn, create_videos_sql.c_str())) {
        std::cerr << "Error creating videos table: " << mysql_error(conn) << std::endl;
    }

    std::string create_topics_sql = R"(
        CREATE TABLE IF NOT EXISTS topics (
        id INT PRIMARY KEY AUTO_INCREMENT,
        name VARCHAR(255) UNIQUE NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (mysql_query(conn, create_topics_sql.c_str())) {
        std::cerr << "Error creating topics table: " << mysql_error(conn) << std::endl;
    }

    std::string create_video_topics_sql = R"(
        CREATE TABLE IF NOT EXISTS video_topics (
        video_id VARCHAR(255) NOT NULL,
        topic_id INT NOT NULL,
        user_id VARCHAR(255) NOT NULL,
        vote INT NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY (video_id, topic_id, user_id),
        FOREIGN KEY (video_id) REFERENCES videos(id),
        FOREIGN KEY (topic_id) REFERENCES topics(id)
        )
    )";
    if (mysql_query(conn, create_video_topics_sql.c_str())) {
        std::cerr << "Error creating video_topics table: " << mysql_error(conn) << std::endl;
    }

    std::string create_users_sql = R"(
        CREATE TABLE IF NOT EXISTS users (
        id VARCHAR(255) PRIMARY KEY,
        username VARCHAR(255) UNIQUE,
        reputation INT DEFAULT 0,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (mysql_query(conn, create_users_sql.c_str())) {
        std::cerr << "Error creating users table: " << mysql_error(conn) << std::endl;
    }
    std::cout << "Database tables checked/created successfully." << std::endl;
}

Database::Database() {
    connect();
    createTables();
}

Database::~Database() {
    mysql_close(conn);
}

MYSQL* Database::getConnection() {
    return conn;
}

nlohmann::json Database::getVideoById(const std::string& videoId) {
    std::string query = "SELECT id, title, upload_date, last_updated FROM videos WHERE id = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)videoId.c_str();
    bind[0].buffer_length = videoId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        nlohmann::json video_data;
        char id_buf[256], title_buf[256], upload_date_buf[256], last_updated_buf[256];
        unsigned long id_len, title_len, upload_date_len, last_updated_len;
        my_bool is_null[4];

        MYSQL_BIND res_bind[4];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = id_buf; res_bind[0].buffer_length = sizeof(id_buf); res_bind[0].length = &id_len; res_bind[0].is_null = &is_null[0];
        res_bind[1].buffer_type = MYSQL_TYPE_STRING; res_bind[1].buffer = title_buf; res_bind[1].buffer_length = sizeof(title_buf); res_bind[1].length = &title_len; res_bind[1].is_null = &is_null[1];
        res_bind[2].buffer_type = MYSQL_TYPE_STRING; res_bind[2].buffer = upload_date_buf; res_bind[2].buffer_length = sizeof(upload_date_buf); res_bind[2].length = &upload_date_len; res_bind[2].is_null = &is_null[2];
        res_bind[3].buffer_type = MYSQL_TYPE_STRING; res_bind[3].buffer = last_updated_buf; res_bind[3].buffer_length = sizeof(last_updated_buf); res_bind[3].length = &last_updated_len; res_bind[3].is_null = &is_null[3];

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        video_data["id"] = std::string(id_buf, id_len);
        video_data["title"] = is_null[1] ? "" : std::string(title_buf, title_len);
        video_data["upload_date"] = is_null[2] ? "" : std::string(upload_date_buf, upload_date_len);
        video_data["last_updated"] = is_null[3] ? "" : std::string(last_updated_buf, last_updated_len);
        return video_data;
    }
    return nlohmann::json(); // Return empty JSON if not found
}

nlohmann::json Database::insertVideo(const std::string& videoId, const std::string& title) {
    std::string query = "INSERT INTO videos (id, title) VALUES (?, ?)";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)videoId.c_str();
    bind[0].buffer_length = videoId.length();

    if (!title.empty()) {
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (char*)title.c_str();
        bind[1].buffer_length = title.length();
    } else {
        bind[1].buffer_type = MYSQL_TYPE_NULL;
    }

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    nlohmann::json video_data;
    video_data["id"] = videoId;
    video_data["title"] = title.empty() ? nullptr : title;
    return video_data;
}

nlohmann::json Database::getTopicByName(const std::string& topicName) {
    std::string query = "SELECT id, name, created_at FROM topics WHERE name = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)topicName.c_str();
    bind[0].buffer_length = topicName.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        nlohmann::json topic_data;
        int id_buf;
        char name_buf[256], created_at_buf[256];
        unsigned long name_len, created_at_len;
        my_bool is_null[2];

        MYSQL_BIND res_bind[3];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_LONG; res_bind[0].buffer = &id_buf;
        res_bind[1].buffer_type = MYSQL_TYPE_STRING; res_bind[1].buffer = name_buf; res_bind[1].buffer_length = sizeof(name_buf); res_bind[1].length = &name_len; res_bind[1].is_null = &is_null[0];
        res_bind[2].buffer_type = MYSQL_TYPE_STRING; res_bind[2].buffer = created_at_buf; res_bind[2].buffer_length = sizeof(created_at_buf); res_bind[2].length = &created_at_len; res_bind[2].is_null = &is_null[1];

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        topic_data["id"] = id_buf;
        topic_data["name"] = std::string(name_buf, name_len);
        topic_data["created_at"] = std::string(created_at_buf, created_at_len);
        return topic_data;
    }
    return nlohmann::json();
}

int Database::insertTopic(const std::string& topicName) {
    std::string query = "INSERT INTO topics (name) VALUES (?)";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)topicName.c_str();
    bind[0].buffer_length = topicName.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    return mysql_stmt_insert_id(stmt.get());
}

nlohmann::json Database::getVideoTopicVote(const std::string& videoId, int topicId, const std::string& userId) {
    std::string query = "SELECT video_id, topic_id, user_id, vote, created_at FROM video_topics WHERE video_id = ? AND topic_id = ? AND user_id = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char*)videoId.c_str(); bind[0].buffer_length = videoId.length();
    bind[1].buffer_type = MYSQL_TYPE_LONG; bind[1].buffer = (char*)&topicId;
    bind[2].buffer_type = MYSQL_TYPE_STRING; bind[2].buffer = (char*)userId.c_str(); bind[2].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        nlohmann::json vote_data;
        char video_id_buf[256], user_id_buf[256], created_at_buf[256];
        int topic_id_buf, vote_buf;
        unsigned long video_id_len, user_id_len, created_at_len;
        my_bool is_null[3];

        MYSQL_BIND res_bind[5];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = video_id_buf; res_bind[0].buffer_length = sizeof(video_id_buf); res_bind[0].length = &video_id_len; res_bind[0].is_null = &is_null[0];
        res_bind[1].buffer_type = MYSQL_TYPE_LONG; res_bind[1].buffer = &topic_id_buf;
        res_bind[2].buffer_type = MYSQL_TYPE_STRING; res_bind[2].buffer = user_id_buf; res_bind[2].buffer_length = sizeof(user_id_buf); res_bind[2].length = &user_id_len; res_bind[2].is_null = &is_null[1];
        res_bind[3].buffer_type = MYSQL_TYPE_LONG; res_bind[3].buffer = &vote_buf;
        res_bind[4].buffer_type = MYSQL_TYPE_STRING; res_bind[4].buffer = created_at_buf; res_bind[4].buffer_length = sizeof(created_at_buf); res_bind[4].length = &created_at_len; res_bind[4].is_null = &is_null[2];

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        vote_data["video_id"] = std::string(video_id_buf, video_id_len);
        vote_data["topic_id"] = topic_id_buf;
        vote_data["user_id"] = std::string(user_id_buf, user_id_len);
        vote_data["vote"] = vote_buf;
        vote_data["created_at"] = std::string(created_at_buf, created_at_len);
        return vote_data;
    }
    return nlohmann::json();
}

void Database::updateVideoTopicVote(const std::string& videoId, int topicId, const std::string& userId, int newVoteValue) {
    std::string query = "UPDATE video_topics SET vote = ?, created_at = CURRENT_TIMESTAMP WHERE video_id = ? AND topic_id = ? AND user_id = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_LONG; bind[0].buffer = (char*)&newVoteValue;
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char*)videoId.c_str(); bind[1].buffer_length = videoId.length();
    bind[2].buffer_type = MYSQL_TYPE_LONG; bind[2].buffer = (char*)&topicId;
    bind[3].buffer_type = MYSQL_TYPE_STRING; bind[3].buffer = (char*)userId.c_str(); bind[3].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
}

void Database::insertVideoTopicVote(const std::string& videoId, int topicId, const std::string& userId, int voteValue) {
    std::string query = "INSERT INTO video_topics (video_id, topic_id, user_id, vote) VALUES (?, ?, ?, ?)";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char*)videoId.c_str(); bind[0].buffer_length = videoId.length();
    bind[1].buffer_type = MYSQL_TYPE_LONG; bind[1].buffer = (char*)&topicId;
    bind[2].buffer_type = MYSQL_TYPE_STRING; bind[2].buffer = (char*)userId.c_str(); bind[2].buffer_length = userId.length();
    bind[3].buffer_type = MYSQL_TYPE_LONG; bind[3].buffer = (char*)&voteValue;

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
}

void Database::deleteVideoTopicVote(const std::string &videoId, int topicId, const std::string &userId) {
    std::string query = "DELETE FROM video_topics WHERE video_id = ? AND topic_id = ? AND user_id = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char*)videoId.c_str(); bind[0].buffer_length = videoId.length();
    bind[1].buffer_type = MYSQL_TYPE_LONG; bind[1].buffer = (char*)&topicId;
    bind[2].buffer_type = MYSQL_TYPE_STRING; bind[2].buffer = (char*)userId.c_str(); bind[2].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
}

nlohmann::json Database::getAggregatedTopicsForVideo(const std::string& videoId) {
    nlohmann::json topics_list = nlohmann::json::array();
    std::string query =
        "SELECT t.id AS topic_id, t.name AS topic_name, SUM(vt.vote) AS total_votes "
        "FROM video_topics vt "
        "JOIN topics t ON vt.topic_id = t.id "
        "WHERE vt.video_id = ? "
        "GROUP BY t.id, t.name "
        "ORDER BY total_votes DESC";

    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)videoId.c_str();
    bind[0].buffer_length = videoId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        int topic_id_buf, total_votes_buf;
        char topic_name_buf[256];
        unsigned long topic_name_len;
        my_bool is_null[1];

        MYSQL_BIND res_bind[3];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_LONG; res_bind[0].buffer = &topic_id_buf;
        res_bind[1].buffer_type = MYSQL_TYPE_STRING; res_bind[1].buffer = topic_name_buf; res_bind[1].buffer_length = sizeof(topic_name_buf); res_bind[1].length = &topic_name_len; res_bind[1].is_null = &is_null[0];
        res_bind[2].buffer_type = MYSQL_TYPE_LONG; res_bind[2].buffer = &total_votes_buf;

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        while (mysql_stmt_fetch(stmt.get()) == 0) {
            nlohmann::json topic_data;
            topic_data["topic_id"] = topic_id_buf;
            topic_data["topic_name"] = std::string(topic_name_buf, topic_name_len);
            topic_data["total_votes"] = total_votes_buf;
            topics_list.push_back(topic_data);
        }
    }
    return topics_list;
}

nlohmann::json Database::getSimilarVideos(const std::string& videoId) {
    nlohmann::json similar_videos = nlohmann::json::array();
    std::string query =
        "SELECT vt2.video_id, v2.title, COUNT(DISTINCT vt2.topic_id) AS shared_topics_count "
        "FROM video_topics vt1 "
        "JOIN video_topics vt2 ON vt1.topic_id = vt2.topic_id "
        "JOIN videos v2 ON vt2.video_id = v2.id "
        "WHERE vt1.video_id = ? AND vt2.video_id != ? "
        "GROUP BY vt2.video_id, v2.title "
        "ORDER BY shared_topics_count DESC";

    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char*)videoId.c_str(); bind[0].buffer_length = videoId.length();
    bind[1].buffer_type = MYSQL_TYPE_STRING; bind[1].buffer = (char*)videoId.c_str(); bind[1].buffer_length = videoId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        char video_id_buf[256], title_buf[256];
        int shared_count_buf;
        unsigned long video_id_len, title_len;
        my_bool is_null[2];

        MYSQL_BIND res_bind[3];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = video_id_buf; res_bind[0].buffer_length = sizeof(video_id_buf); res_bind[0].length = &video_id_len; res_bind[0].is_null = &is_null[0];
        res_bind[1].buffer_type = MYSQL_TYPE_STRING; res_bind[1].buffer = title_buf; res_bind[1].buffer_length = sizeof(title_buf); res_bind[1].length = &title_len; res_bind[1].is_null = &is_null[1];
        res_bind[2].buffer_type = MYSQL_TYPE_LONG; res_bind[2].buffer = &shared_count_buf;

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        while (mysql_stmt_fetch(stmt.get()) == 0) {
            nlohmann::json video_data;
            video_data["video_id"] = std::string(video_id_buf, video_id_len);
            video_data["title"] = is_null[1] ? nullptr : std::string(title_buf, title_len);
            video_data["shared_topics_count"] = shared_count_buf;
            similar_videos.push_back(video_data);
        }
    }
    return similar_videos;
}

nlohmann::json Database::getUserDetails(const std::string &userId) {
    std::string query = "SELECT id, username, reputation, created_at FROM users WHERE id = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)userId.c_str();
    bind[0].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        nlohmann::json user_data;
        char id_buf[256], username_buf[256], created_at_buf[256];
        int reputation_buf;
        unsigned long id_len, username_len, created_at_len;
        my_bool is_null[3];

        MYSQL_BIND res_bind[4];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = id_buf; res_bind[0].buffer_length = sizeof(id_buf); res_bind[0].length = &id_len; res_bind[0].is_null = &is_null[0];
        res_bind[1].buffer_type = MYSQL_TYPE_STRING; res_bind[1].buffer = username_buf; res_bind[1].buffer_length = sizeof(username_buf); res_bind[1].length = &username_len; res_bind[1].is_null = &is_null[1];
        res_bind[2].buffer_type = MYSQL_TYPE_LONG; res_bind[2].buffer = &reputation_buf;
        res_bind[3].buffer_type = MYSQL_TYPE_STRING; res_bind[3].buffer = created_at_buf; res_bind[3].buffer_length = sizeof(created_at_buf); res_bind[3].length = &created_at_len; res_bind[3].is_null = &is_null[2];

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        user_data["id"] = std::string(id_buf, id_len);
        user_data["username"] = is_null[1] ? "" : std::string(username_buf, username_len);
        user_data["reputation"] = reputation_buf;
        user_data["created_at"] = is_null[2] ? "" : std::string(created_at_buf, created_at_len);
        return user_data;
    }
    return nlohmann::json();
}

int Database::getUserSubmissionsCount(const std::string &userId) {
    std::string query = "SELECT COUNT(*) FROM video_topics WHERE user_id = ?";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)userId.c_str();
    bind[0].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        int count = 0;
        MYSQL_BIND res_bind[1];
        memset(res_bind, 0, sizeof(res_bind));
        res_bind[0].buffer_type = MYSQL_TYPE_LONG;
        res_bind[0].buffer = &count;

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        return count;
    }
    return 0;
}

std::string Database::getUserLastSubmissionDate(const std::string &userId) {
    std::string query = "SELECT created_at FROM video_topics WHERE user_id = ? ORDER BY created_at DESC LIMIT 1";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)userId.c_str();
    bind[0].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        char created_at_buf[256];
        unsigned long created_at_len;
        my_bool is_null[1];

        MYSQL_BIND res_bind[1];
        memset(res_bind, 0, sizeof(res_bind));
        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = created_at_buf; res_bind[0].buffer_length = sizeof(created_at_buf); res_bind[0].length = &created_at_len; res_bind[0].is_null = &is_null[0];

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        return std::string(created_at_buf, created_at_len);
    }
    return "";
}

nlohmann::json Database::getUserMostFrequentTag(const std::string &userId) {
    std::string query = R"(
        SELECT t.name AS topic_name, COUNT(vt.topic_id) AS topic_count
        FROM video_topics vt
        JOIN topics t ON vt.topic_id = t.id
        WHERE vt.user_id = ?
        GROUP BY t.name
        ORDER BY topic_count DESC
        LIMIT 1
    )";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)userId.c_str();
    bind[0].buffer_length = userId.length();

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        nlohmann::json tag_data;
        char topic_name_buf[256];
        int topic_count_buf;
        unsigned long topic_name_len;
        my_bool is_null[1];

        MYSQL_BIND res_bind[2];
        memset(res_bind, 0, sizeof(res_bind));
        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = topic_name_buf; res_bind[0].buffer_length = sizeof(topic_name_buf); res_bind[0].length = &topic_name_len; res_bind[0].is_null = &is_null[0];
        res_bind[1].buffer_type = MYSQL_TYPE_LONG; res_bind[1].buffer = &topic_count_buf;

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
        if (mysql_stmt_fetch(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        tag_data["topic_name"] = std::string(topic_name_buf, topic_name_len);
        tag_data["topic_count"] = topic_count_buf;
        return tag_data;
    }
    return nlohmann::json();
}

nlohmann::json Database::getAllUsersWithContributionCounts() {
    nlohmann::json users_list = nlohmann::json::array();
    std::string query = R"(
        SELECT u.id, u.username, COUNT(vt.user_id) AS contributions_count
        FROM users u
        LEFT JOIN video_topics vt ON u.id = vt.user_id
        GROUP BY u.id, u.username
        ORDER BY contributions_count DESC, u.username ASC
    )";

    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_store_result(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    if (mysql_stmt_num_rows(stmt.get()) > 0) {
        char id_buf[256], username_buf[256];
        int contributions_count_buf;
        unsigned long id_len, username_len;
        my_bool is_null[2];

        MYSQL_BIND res_bind[3];
        memset(res_bind, 0, sizeof(res_bind));

        res_bind[0].buffer_type = MYSQL_TYPE_STRING; res_bind[0].buffer = id_buf; res_bind[0].buffer_length = sizeof(id_buf); res_bind[0].length = &id_len; res_bind[0].is_null = &is_null[0];
        res_bind[1].buffer_type = MYSQL_TYPE_STRING; res_bind[1].buffer = username_buf; res_bind[1].buffer_length = sizeof(username_buf); res_bind[1].length = &username_len; res_bind[1].is_null = &is_null[1];
        res_bind[2].buffer_type = MYSQL_TYPE_LONG; res_bind[2].buffer = &contributions_count_buf;

        if (mysql_stmt_bind_result(stmt.get(), res_bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));

        while (mysql_stmt_fetch(stmt.get()) == 0) {
            nlohmann::json user_data;
            user_data["id"] = std::string(id_buf, id_len);
            user_data["username"] = is_null[1] ? "" : std::string(username_buf, username_len);
            user_data["contributions_count"] = contributions_count_buf;
            users_list.push_back(user_data);
        }
    }
    return users_list;
}

void Database::upsertUser(const std::string &userId, const std::string &username) {
    std::string query = "INSERT IGNORE INTO users (id, username) VALUES (?, ?)";
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn), mysql_stmt_close);
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    if (mysql_stmt_prepare(stmt.get(), query.c_str(), query.length())) throw std::runtime_error(mysql_stmt_error(stmt.get()));

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING; bind[0].buffer = (char*)userId.c_str(); bind[0].buffer_length = userId.length();

    if (!username.empty()) {
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (char*)username.c_str();
        bind[1].buffer_length = username.length();
    } else {
        bind[1].buffer_type = MYSQL_TYPE_NULL;
    }

    if (mysql_stmt_bind_param(stmt.get(), bind)) throw std::runtime_error(mysql_stmt_error(stmt.get()));
    if (mysql_stmt_execute(stmt.get())) throw std::runtime_error(mysql_stmt_error(stmt.get()));
}