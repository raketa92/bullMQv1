#pragma once

#include <cstddef>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
    public:
        ThreadPool(size_t threadCount);
        ~ThreadPool();

        void enqueue(std::function<void()> task);
    
    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queueMutex_;
        std::condition_variable condition_;
        bool stop_;
};
