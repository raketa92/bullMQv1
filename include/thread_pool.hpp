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
#include <cstdint>

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
    auto enqueueWithPriority(
        std::uint32_t priority,
        F &&function,
        Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
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

            tasks_.push(
                PrioritizedTask{
                    priority,
                    nextTaskSequence_,
                    [packagedTask]
                    {
                        (*packagedTask)();
                    }});

            ++nextTaskSequence_;
        }

        logger_.info("Task enqueued");
        condition_.notify_one();

        return future;
    }

    template <class F, class... Args>
    auto enqueue(F &&function, Args &&...args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {

        return enqueueWithPriority(0, std::forward<F>(function), std::forward<Args>(args)...);
    }

private:
    struct PrioritizedTask
    {
        std::uint32_t priority;
        std::uint64_t sequence;
        std::function<void()> function;
    };

    struct TaskComesAfter
    {
        bool operator()(
            const PrioritizedTask &left,
            const PrioritizedTask &right) const;
    };

    std::vector<std::thread> workers_;
    std::priority_queue<PrioritizedTask, std::vector<PrioritizedTask>, TaskComesAfter> tasks_;
    std::uint64_t nextTaskSequence_ = 0;

    std::mutex queueMutex_;

    // Workers wait here while tasks_ is empty.
    std::condition_variable condition_;

    // Producers wait here while tasks_ is full.
    std::condition_variable notFull_;

    bool stop_;
    Logger &logger_;
    std::size_t maxQueueSize_;
};