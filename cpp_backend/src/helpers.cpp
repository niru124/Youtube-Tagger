#include "helpers.h"

// Helper to extract YouTube video ID
std::string getYouTubeVideoId(const std::string& url) {
    size_t v_pos = url.find("v=");
    if (v_pos != std::string::npos) {
        std::string id = url.substr(v_pos + 2, 11);
        if (id.length() == 11) return id;
    }

    size_t youtu_be_pos = url.find("youtu.be/");
    if (youtu_be_pos != std::string::npos) {
        std::string id = url.substr(youtu_be_pos + 9, 11);
        if (id.length() == 11) return id;
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
std::string formatVectorForPgvector(const std::vector<float>& vec) {
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