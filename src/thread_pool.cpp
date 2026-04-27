#include "thread_pool.hpp"
#include "logger.hpp"

Logger logger("[ThreadPool]: ");

ThreadPool::ThreadPool(size_t threadCount): stop_(false) {
    for (size_t i = 0; i < threadCount; i++)
    {
        workers_.emplace_back([this]{
            while (true)
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queueMutex_);

                    this->condition_.wait(lock, [this] {
                        return this->stop_ || !this->tasks_.empty();
                    });

                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }

                    task = std::move(this->tasks_.front());
                    logger.log("Task assigned");
                    this->tasks_.pop();
                }
            }
            
        });
    }    
};

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }

    condition_.notify_all();

    for (std::thread &worker : workers_)
    {
        if (worker.joinable()) {
            worker.join();
            logger.log("Worker thread joined");
        }
    }
};

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        tasks_.emplace(std::bind(std::forward<std::function<void()>>(f), std::forward<Args>(args)));
        logger.log("Task enqueued");
    }

    condition_.notify_one();
};