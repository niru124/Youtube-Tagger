#ifndef CONFIG_H
#define CONFIG_H
#include <iostream>
// Database connection details - matching docker-compose.yml

const std::string DB_HOST = "127.0.0.1";
const std::string DB_USER = "testuser";
const std::string DB_PASS = "testpass";
const unsigned int DB_PORT = 3306;
const std::string DB_NAME = "youtube_topics";

#endif // CONFIG_H
