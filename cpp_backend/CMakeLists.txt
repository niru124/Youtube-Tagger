cmake_minimum_required(VERSION 3.10)
project(youtube-topic-crow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Crow
find_package(Crow REQUIRED)

# Find MariaDB Client Library (manual search)
find_path(MARIADB_INCLUDE_DIR NAMES mysql.h HINTS /usr/include/mysql /usr/include/mariadb)
find_library(MARIADB_LIBRARY NAMES mariadbclient mariadb HINTS /usr/lib /usr/lib/mysql /usr/lib/mariadb)

if(NOT MARIADB_INCLUDE_DIR OR NOT MARIADB_LIBRARY)
    message(FATAL_ERROR "Could not find MariaDB client library. Please ensure mariadb-clients is installed.")
endif()

# Add nlohmann/json (header-only library)
# You might need to install it via pacman: sudo pacman -S nlohmann-json
# Or, if not available, download and place it in a 'include' directory
# For now, assuming it's installed or discoverable.

# Add source files
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/database.cpp
    src/helpers.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Crow::Crow
    ${MARIADB_LIBRARY}
    z
    ssl
    crypto
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    /usr/include/nlohmann # Assuming nlohmann/json is installed here
    ${MARIADB_INCLUDE_DIR}
)
