cmake_minimum_required(VERSION 3.20)

project(job_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

include(FetchContent)

FetchContent_Declare(
    cpp_httplib
    URL 
        https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.53.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cpp_httplib)

FetchContent_Declare(
    nlohmann_json
    URL 
        https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(job_engine
    src/main.cpp
    src/http_server.cpp
    src/logger.cpp
    src/thread_pool.cpp
    src/job_queue.cpp
    src/job_processor.cpp
    src/job_store.cpp
    src/job_service.cpp
    src/worker.cpp
    src/delayed_job_scheduler.cpp
    src/sqlite_database.cpp
    src/sqlite_statement.cpp
)

target_include_directories(job_engine PRIVATE include)

target_link_libraries(job_engine PRIVATE
    Threads::Threads
    SQLite3::SQLite3
    httplib::httplib
    nlohmann_json::nlohmann_json
)

target_compile_options(job_engine PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)