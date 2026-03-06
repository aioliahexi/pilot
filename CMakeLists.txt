cmake_minimum_required(VERSION 3.16)
project(pilot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib (disable OpenSSL and zlib to keep it simple)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.1
)
FetchContent_MakeAvailable(httplib)

# Threads
find_package(Threads REQUIRED)

# PostgreSQL (optional)
find_package(PostgreSQL)

# Collect all source files (excluding main.cpp)
file(GLOB_RECURSE PILOT_SOURCES
    src/sensors/*.cpp
    src/engine/*.cpp
    src/control/*.cpp
    src/db/*.cpp
    src/server/*.cpp
    src/config/*.cpp
)

# Core library used by both the main executable and tests
add_library(pilot_core STATIC ${PILOT_SOURCES})
target_include_directories(pilot_core PUBLIC include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(pilot_core PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    Threads::Threads
)

if(PostgreSQL_FOUND)
    target_compile_definitions(pilot_core PUBLIC PILOT_HAVE_POSTGRESQL)
    target_link_libraries(pilot_core PUBLIC PostgreSQL::PostgreSQL)
    message(STATUS "PostgreSQL found and enabled")
else()
    message(STATUS "PostgreSQL not found - running without DB support")
endif()

# Main executable
add_executable(pilot_server src/main.cpp)
target_link_libraries(pilot_server PRIVATE pilot_core)

enable_testing()
add_subdirectory(tests)
