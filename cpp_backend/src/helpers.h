#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <regex>
#include <random>
#include <sstream>

// Helper to extract YouTube video ID
std::string getYouTubeVideoId(const std::string& url);

// Helper to generate a simple UUID-like string for user_id
std::string generateUserId();

#endif // HELPERS_H