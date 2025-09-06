#include <crow.h>
#include <crow/middlewares/cors.h>
#include <nlohmann/json.hpp>

#include "config.h"
#include "helpers.h"
#include "database.h"

int main() {
    crow::App<crow::CORSHandler> app;
    Database db; // Initialize database connection

    // Enable CORS for all routes
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors
        .global()
        .headers("Content-Type", "Authorization") // Allow these headers
        .methods("POST"_method, "GET"_method, "OPTIONS"_method) // Allow these HTTP methods
        .origin("*") // Allow requests from any origin (for development)
        .allow_credentials();
        



    // POST /videos: Add a new video with optional title
    // GET /videos/:id: Get a video by its ID
    CROW_ROUTE(app, "/videos/<string>").methods("GET"_method)([&](const crow::request& req, std::string videoId) {
        std::cerr << "GET /videos/" << videoId << " received." << std::endl;
        try {
            nlohmann::json video = db.getVideoById(videoId);
            if (video.is_null()) {
                std::cerr << "Video " << videoId << " not found in DB." << std::endl;
                return crow::response(404, nlohmann::json{{"error", "Video not found."}}.dump());
            }
            std::cerr << "Video " << videoId << " found in DB: " << video.dump() << std::endl;
            return crow::response(200, video.dump());
        } catch (const std::exception& e) {
            std::cerr << "Error in GET /videos/" << videoId << ": " << e.what() << std::endl;
            return crow::response(500, nlohmann::json{{"error", e.what()}}.dump());
        }
    });

    CROW_ROUTE(app, "/videos").methods("POST"_method)([&](const crow::request& req) {
        auto json_body = nlohmann::json::parse(req.body);
        std::string url = json_body.value("id", ""); // Frontend sends URL in 'id' field
        std::string title = json_body.value("title", "");
        std::cerr << "POST /videos received. URL: " << url << ", Title: " << title << std::endl;

        if (url.empty()) {
            std::cerr << "Error: Video URL is required." << std::endl;
            return crow::response(400, nlohmann::json{{"error", "Video URL is required."}}.dump());
        }

        std::string youtubeId = getYouTubeVideoId(url);
        std::cerr << "Extracted YouTube ID: " << youtubeId << std::endl;
        if (youtubeId.empty()) {
            std::cerr << "Error: Invalid YouTube URL." << std::endl;
            return crow::response(400, nlohmann::json{{"error", "Invalid YouTube URL."}}.dump());
        }

        try {
            nlohmann::json existingVideo = db.getVideoById(youtubeId);
            if (!existingVideo.is_null()) {
                std::cerr << "Video " << youtubeId << " already exists. Returning existing video." << std::endl;
                return crow::response(200, existingVideo.dump());
            }

            std::cerr << "Video " << youtubeId << " not found. Inserting new video." << std::endl;
            nlohmann::json newVideo = db.insertVideo(youtubeId, title);
            std::cerr << "New video inserted: " << newVideo.dump() << std::endl;
            return crow::response(201, newVideo.dump());

        } catch (const std::exception& e) {
            std::cerr << "Error in POST /videos for " << youtubeId << ": " << e.what() << std::endl;
            return crow::response(500, nlohmann::json{{"error", e.what()}}.dump());
        }
    });

    // GET /videos/:id/topics: Get topics and their aggregated votes for a video
    CROW_ROUTE(app, "/videos/<string>/topics").methods("GET"_method)([&](const crow::request& req, std::string videoId) {
        std::cerr << "GET /videos/" << videoId << "/topics received." << std::endl;
        try {
            nlohmann::json topics = db.getAggregatedTopicsForVideo(videoId);
            nlohmann::json response_json;
            response_json["video_id"] = videoId;
            response_json["topics"] = topics;
            std::cerr << "Returning topics for " << videoId << ": " << response_json.dump() << std::endl;
            return crow::response(200, response_json.dump());
        } catch (const std::exception& e) {
            std::cerr << "Error in GET /videos/" << videoId << "/topics: " << e.what() << std::endl;
            return crow::response(500, nlohmann::json{{"error", e.what()}}.dump());
        }
    });

    // POST /videos/:id/topics: Submit a new topic or vote on an existing one
    CROW_ROUTE(app, "/videos/<string>/topics").methods("POST"_method)([&](const crow::request& req, std::string videoId) {
        std::cerr << "POST /videos/" << videoId << "/topics received." << std::endl;
        auto json_body = nlohmann::json::parse(req.body);
        std::string topicName = json_body.value("name", "");
        int desiredVote = json_body.value("desired_vote", 0); // Expecting 1 for upvote, -1 for downvote
        std::string userId = json_body.value("user_id", ""); // Expecting user_id from request body
        int topicId = json_body.value("topic_id", 0);
        std::cerr << "  Topic: " << topicName << ", Vote: " << desiredVote << ", User: " << userId << std::endl;

        // Sanitize desiredVote: clamp to 1, 0, or -1
        if (desiredVote > 1) {
            desiredVote = 1;
        } else if (desiredVote < -1) {
            desiredVote = -1;
        }

        if (desiredVote != 1 && desiredVote != -1 && desiredVote != 0) {
            nlohmann::json error_json;
            error_json["error"] = "Desired vote must be 1 (upvote), -1 (downvote), or 0 (no vote).";
            std::cerr << "  Error: Invalid desired vote." << std::endl;
            return crow::response(400, error_json.dump());
        }

        if (userId.empty()) {
            userId = generateUserId(); // Fallback if frontend doesn't provide user_id
            std::cerr << "  Generated new user ID: " << userId << std::endl;
        }

        try {
            db.upsertUser(userId);
            mysql_query(db.getConnection(), "START TRANSACTION");
            std::cerr << "  Transaction started." << std::endl;

            if (!topicName.empty()) {
                nlohmann::json existingTopic = db.getTopicByName(topicName);
                if (existingTopic.is_null()) {
                    std::cerr << "  Topic '" << topicName << "' not found. Inserting new topic." << std::endl;
                    topicId = db.insertTopic(topicName);
                    if (topicId == 0) {
                        mysql_query(db.getConnection(), "ROLLBACK");
                        std::cerr << "  Error: Failed to create new topic. Transaction rolled back." << std::endl;
                        nlohmann::json error_json;
                        error_json["error"] = "Failed to create new topic.";
                        return crow::response(500, error_json.dump());
                    }
                    std::cerr << "  New topic '" << topicName << "' inserted with ID: " << topicId << std::endl;
                } else {
                    topicId = existingTopic["id"].get<int>();
                    std::cerr << "  Topic '" << topicName << "' found with ID: " << topicId << std::endl;
                }
            } else if (topicId == 0) {
                mysql_query(db.getConnection(), "ROLLBACK");
                std::cerr << "  Error: Topic name or topic ID is required. Transaction rolled back." << std::endl;
                nlohmann::json error_json;
                error_json["error"] = "Topic name or topic ID is required.";
                return crow::response(400, error_json.dump());
            }

            nlohmann::json existingVote = db.getVideoTopicVote(videoId, topicId, userId);
            if (!existingVote.is_null()) {
                int currentVote = existingVote["vote"].get<int>();
                std::cerr << "  Existing vote found for video " << videoId << ", topic " << topicId << ", user " << userId << ": " << currentVote << std::endl;
                if (currentVote == desiredVote) {
                    // User is toggling off their vote
                    db.deleteVideoTopicVote(videoId, topicId, userId);
                    mysql_query(db.getConnection(), "COMMIT");
                    std::cerr << "  Vote removed. Transaction committed." << std::endl;
                    nlohmann::json success_json;
                    success_json["message"] = "Vote removed successfully";
                    success_json["user_id"] = userId;
                    return crow::response(200, success_json.dump());
                } else {
                    // User is changing their vote (e.g., from +1 to -1, or -1 to +1)
                    db.updateVideoTopicVote(videoId, topicId, userId, desiredVote);
                    mysql_query(db.getConnection(), "COMMIT");
                    std::cerr << "  Vote updated to " << desiredVote << ". Transaction committed." << std::endl;
                    nlohmann::json success_json;
                    success_json["message"] = "Vote updated successfully";
                    success_json["user_id"] = userId;
                    return crow::response(200, success_json.dump());
                }
            } else {
                // No existing vote, insert new vote
                db.insertVideoTopicVote(videoId, topicId, userId, desiredVote);
                mysql_query(db.getConnection(), "COMMIT");
                std::cerr << "  New vote " << desiredVote << " recorded. Transaction committed." << std::endl;
                nlohmann::json success_json;
                success_json["message"] = "Vote recorded successfully";
                success_json["user_id"] = userId;
                return crow::response(201, success_json.dump());
            }

        } catch (const std::exception& e) {
            mysql_query(db.getConnection(), "ROLLBACK");
            
            nlohmann::json error_json;
            error_json["error"] = e.what();
            return crow::response(500, error_json.dump());
        }
    });

    // GET /videos/:id/similar: Get similar videos based on shared topics
    CROW_ROUTE(app, "/videos/<string>/similar").methods("GET"_method)([&](const crow::request& req, std::string videoId) {
        try {
            nlohmann::json similar = db.getSimilarVideos(videoId);
            return crow::response(200, similar.dump());
        } catch (const std::exception& e) {
            return crow::response(500, nlohmann::json{{"error", e.what()}}.dump());
        }
    });

    // GET /users/:id/stats: Get user statistics
    CROW_ROUTE(app, "/users/<string>/stats").methods("GET"_method)([&](const crow::request& req, std::string userId) {
        try {
            nlohmann::json userDetails = db.getUserDetails(userId);
            if (userDetails.is_null()) {
                return crow::response(404, nlohmann::json{{"error", "User not found."}}.dump());
            }

            int submissionsCount = db.getUserSubmissionsCount(userId);
            std::string lastSubmissionDate = db.getUserLastSubmissionDate(userId);
            nlohmann::json mostFrequentTag = db.getUserMostFrequentTag(userId);

            nlohmann::json response_json;
            response_json["user_id"] = userDetails["id"];
            response_json["username"] = userDetails["username"];
            response_json["reputation"] = userDetails["reputation"];
            response_json["created_at"] = userDetails["created_at"];
            response_json["submissions_count"] = submissionsCount;
            response_json["last_submission_date"] = lastSubmissionDate;
            response_json["most_frequent_tag"] = mostFrequentTag;

            return crow::response(200, response_json.dump());
        } catch (const std::exception& e) {
            return crow::response(500, nlohmann::json{{"error", e.what()}}.dump());
        }
    });

    // GET /users/contributions: Get all users with their contribution counts
    CROW_ROUTE(app, "/users/contributions").methods("GET"_method)([&](const crow::request& req) {
        try {
            nlohmann::json usersWithContributions = db.getAllUsersWithContributionCounts();
            return crow::response(200, usersWithContributions.dump());
        } catch (const std::exception& e) {
            return crow::response(500, nlohmann::json{{"error", e.what()}}.dump());
        }
    });


    // Test route
    CROW_ROUTE(app, "/test")([&](){
        return "Test successful!";
    });

    app.port(8000).multithreaded().run();
}
