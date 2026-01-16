cmake_minimum_required(VERSION 3.15)
project(cpp-chat-system VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(PostgreSQL REQUIRED)

# Find libpqxx
find_path(PQXX_INCLUDE_DIR pqxx/pqxx
    HINTS /opt/homebrew/include /usr/local/include
)
find_library(PQXX_LIBRARY NAMES pqxx
    HINTS /opt/homebrew/lib /usr/local/lib
)

if(NOT PQXX_INCLUDE_DIR OR NOT PQXX_LIBRARY)
    message(FATAL_ERROR "libpqxx not found. Install: brew install libpqxx")
endif()

message(STATUS "Found libpqxx: ${PQXX_LIBRARY}")
message(STATUS "libpqxx include: ${PQXX_INCLUDE_DIR}")

# Options
option(BUILD_API_SERVER "Build API server" ON)
option(BUILD_NOTIFICATION_SERVICE "Build notification service" ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

if(BUILD_API_SERVER)
    add_subdirectory(services/api-server)
endif()

if(BUILD_NOTIFICATION_SERVICE)
    add_subdirectory(services/notification-service)
endif()

# Documentation
add_subdirectory(docs)

# Print build configuration
message(STATUS "=== Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build API server: ${BUILD_API_SERVER}")
message(STATUS "Build notification service: ${BUILD_NOTIFICATION_SERVICE}")
