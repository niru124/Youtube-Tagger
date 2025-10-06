#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <regex>
#include <random>
#include <sstream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>

// Helper to extract YouTube video ID
std::string getYouTubeVideoId(const std::string& url);

// Helper to generate a simple UUID-like string for user_id
std::string generateUserId();

// Helper to format a std::vector<float> into a string for pgvector
std::string formatVectorForPgvector(const std::vector<float>& vec);

#endif // HELPERS_H