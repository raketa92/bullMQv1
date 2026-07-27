#pragma once

#include "logger.hpp"

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool
{
public:
    ThreadPool(
        std::size_t threadCount,
        Logger &logger,
        std::size_t maxQueueSize);

    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    template <class F, class... Args>
    auto enqueue(F &&function, Args &&...args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        /*
         * Store the function and arguments in a no-argument callable.
         *
         * Example:
         *
         * enqueue(add, 2, 3)
         *
         * becomes conceptually:
         *
         * [stored add, stored 2, stored 3]() {
         *     return add(2, 3);
         * }
         */
        auto packagedTask =
            std::make_shared<std::packaged_task<ReturnType()>>(
                [function = std::forward<F>(function),
                 ... capturedArgs =
                     std::forward<Args>(args)]() mutable -> ReturnType
                {
                    return std::invoke(
                        std::move(function),
                        std::move(capturedArgs)...);
                });

        std::future<ReturnType> future =
            packagedTask->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            /*
             * Backpressure:
             *
             * If the internal ThreadPool queue is full,
             * the producer waits until a worker removes a task.
             *
             * stop_ is included so a waiting producer can wake
             * during ThreadPool shutdown.
             */
            notFull_.wait(lock, [this]
                          { return stop_ ||
                                   tasks_.size() < maxQueueSize_; });

            if (stop_)
            {
                throw std::runtime_error(
                    "enqueue on stopped ThreadPool");
            }

            /*
             * tasks_ stores std::function<void()>.
             *
             * packagedTask is wrapped in a no-argument,
             * no-return-value lambda.
             *
             * Calling (*packagedTask)() executes the original
             * function and saves its result in the future.
             */
            tasks_.emplace([packagedTask]
                           { (*packagedTask)(); });
        }

        logger_.info("Task enqueued");
        condition_.notify_one();

        return future;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex queueMutex_;

    // Workers wait here while tasks_ is empty.
    std::condition_variable condition_;

    // Producers wait here while tasks_ is full.
    std::condition_variable notFull_;

    bool stop_;
    Logger &logger_;
    std::size_t maxQueueSize_;
};