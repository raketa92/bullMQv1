cmake_minimum_required(VERSION 3.20)

project(JobEngine
    VERSION 0.1.0
    LANGUAGES CXX
)

add_executable(job_engine
    src/main.cpp
    src/logger.cpp
    src/thread_pool.cpp
)

target_compile_features(job_engine PRIVATE cxx_std_20)

target_include_directories(job_engine PRIVATE include)

if (MSVC)
    target_compile_options(job_engine PRIVATE /W4)
else ()
    target_compile_options(job_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()