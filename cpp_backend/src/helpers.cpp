#include "helpers.h"

// Helper to extract YouTube video ID
std::string getYouTubeVideoId(const std::string& url) {
    // Corrected regex escape sequences
    std::regex regExp("(?:https?://[^/]+/)?(?:https?://)?(?:www\.)?(?:m\.)?(?:youtube\.com|youtu\.be)/(?:watch\?v=|embed/|v/|)([^&?\n]{11})");
    std::smatch match;
    if (std::regex_search(url, match, regExp) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

// Helper to generate a simple UUID-like string for user_id
std::string generateUserId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 15);

    std::stringstream ss;
    ss << "user-";
    for (int i = 0; i < 9; ++i) {
        ss << std::hex << distrib(gen);
    }
    return ss.str();
}

// Helper to format a std::vector<float> into a string for pgvector
std::string formatVector(const std::vector<float>& vec) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        ss << std::fixed << std::setprecision(8) << vec[i];
        if (i < vec.size() - 1) {
            ss << ",";
        }
    }
    ss << "]";
    return ss.str();
}
