#ifndef DATABASE_H
#define DATABASE_H

#include <memory>
#include <pqxx/pqxx>
using namespace pqxx;
#include <nlohmann/json.hpp>
#include <string>
#include <future>
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>

class Database {
private:
  std::unique_ptr<pqxx::connection> conn;
  boost::asio::thread_pool thread_pool;

  void connect();
  void createTables();

public:
  Database();
  ~Database();

  // Get PostgreSQL connection for database operations
  pqxx::connection& getConnection();

  // Async versions of database operations
  std::future<nlohmann::json> getVideoByIdAsync(const std::string& videoId);
  std::future<nlohmann::json> insertVideoAsync(const std::string& videoId, const std::string& title = "");
  std::future<nlohmann::json> getTopicByNameAsync(const std::string& topicName);
  std::future<int> insertTopicAsync(const std::string& topicName);
  std::future<nlohmann::json> getAggregatedTopicsForVideoAsync(const std::string& videoId);
  std::future<nlohmann::json> getSimilarVideosAsync(const std::string& videoId);
  std::future<void> updateVideoEmbeddingAsync(const std::string& videoId, const std::vector<float>& embedding);
  std::future<nlohmann::json> getSimilarVideosByVectorAsync(const std::string& videoId, int limit = 10);
  std::future<nlohmann::json> getUserDetailsAsync(const std::string& userId);
  std::future<int> getUserSubmissionsCountAsync(const std::string& userId);
  std::future<std::string> getUserLastSubmissionDateAsync(const std::string& userId);
  std::future<nlohmann::json> getUserMostFrequentTagAsync(const std::string& userId);
  std::future<void> upsertUserAsync(const std::string& userId, const std::string& username = "");

  nlohmann::json getVideoById(const std::string &videoId);
  nlohmann::json insertVideo(const std::string &videoId,
                             const std::string &title);

  nlohmann::json getTopicByName(const std::string &topicName);
  int insertTopic(const std::string &topicName);

  nlohmann::json getVideoTopicVote(const std::string &videoId, int topicId,
                                   const std::string &userId);
  void updateVideoTopicVote(const std::string &videoId, int topicId,
                            const std::string &userId, int vote);
  void insertVideoTopicVote(const std::string &videoId, int topicId,
                            const std::string &userId, int vote);

  nlohmann::json getAggregatedTopicsForVideo(const std::string &videoId);
  nlohmann::json getSimilarVideos(const std::string &videoId);

  // New functions for user stats
  nlohmann::json getUserDetails(const std::string &userId);
  int getUserSubmissionsCount(const std::string &userId);
  std::string getUserLastSubmissionDate(const std::string &userId);
  nlohmann::json getUserMostFrequentTag(const std::string &userId);
  nlohmann::json getAllUsersWithContributionCounts();
  void upsertUser(const std::string &userId, const std::string &username = "");
  void deleteVideoTopicVote(const std::string &videoId, int topicId, const std::string &userId);
  void updateVideoEmbedding(const std::string& videoId, const std::vector<float>& embedding);
  nlohmann::json getSimilarVideosByVector(const std::string& videoId, int limit = 10);
};

#endif // DATABASE_H
