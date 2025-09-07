#ifndef DATABASE_H
#define DATABASE_H

#include <memory>
#include <pqxx/pqxx>
using namespace pqxx;
#include <nlohmann/json.hpp>
#include <string>

class Database {
private:
  std::unique_ptr<pqxx::connection> conn;

  void connect();
  void createTables();

public:
  Database();
  ~Database();

   // Get PostgreSQL connection for database operations
   pqxx::connection& getConnection();

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
};

#endif // DATABASE_H
