cmake_minimum_required(VERSION 3.20)

project(job_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(job_engine
    src/main.cpp
    src/logger.cpp
    src/thread_pool.cpp
    src/job_queue.cpp
    src/job_processor.cpp
    src/job_store.cpp
    src/job_service.cpp
    src/worker.cpp
    src/delayed_job_scheduler.cpp
)

target_include_directories(job_engine PRIVATE include)

target_link_libraries(job_engine PRIVATE Threads::Threads)

target_compile_options(job_engine PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)