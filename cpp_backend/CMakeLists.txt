cmake_minimum_required(VERSION 3.10)
project(youtube-topic-crow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use Boost.ASIO for Crow
add_definitions(-DCROW_USE_BOOST)

# Find Crow


# Find PostgreSQL Client Library (libpqxx)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

if(NOT PQXX_FOUND)
    message(FATAL_ERROR "Could not find libpqxx. Please ensure libpqxx-dev (or similar) is installed.")
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find ZLIB
find_package(ZLIB REQUIRED)

# Find Boost
find_package(Boost REQUIRED COMPONENTS system thread)

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
    
    ${PQXX_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Boost::system
    Boost::thread
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    /usr/include/nlohmann # Assuming nlohmann/json is installed here
    ${PQXX_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    /usr/local/include/crow
)
