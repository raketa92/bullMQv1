#include "thread_pool.hpp"

#include <stdexcept>
#include <utility>

ThreadPool::ThreadPool(
    std::size_t threadCount,
    Logger &logger,
    std::size_t maxQueueSize)
    : stop_(false),
      logger_(logger),
      maxQueueSize_(maxQueueSize)
{
    if (threadCount == 0)
    {
        throw std::invalid_argument(
            "ThreadPool requires at least one worker");
    }

    if (maxQueueSize == 0)
    {
        throw std::invalid_argument(
            "ThreadPool queue size must be greater than zero");
    }

    workers_.reserve(threadCount);

    for (std::size_t i = 0; i < threadCount; ++i)
    {
        workers_.emplace_back([this]
                              {
            while (true)
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(
                        queueMutex_);

                    condition_.wait(lock, [this]
                    {
                        return stop_ || !tasks_.empty();
                    });

                    /*
                     * Graceful ThreadPool shutdown:
                     *
                     * If stop was requested but tasks remain,
                     * workers continue draining the queue.
                     *
                     * They exit only when:
                     * stop_ == true AND tasks_ is empty.
                     */
                    if (stop_ && tasks_.empty())
                    {
                        return;
                    }

                    task = tasks_.top().function;
                    tasks_.pop();
                }

                /*
                 * Queue mutex has already been released.
                 * Logging and task execution happen outside
                 * the critical section.
                 */
                notFull_.notify_one();

                logger_.info("Task assigned");
                task();
            } });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stop_ = true;
    }

    // Wake workers waiting for tasks.
    condition_.notify_all();

    // Wake producers waiting for queue capacity.
    notFull_.notify_all();

    for (std::thread &worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

bool ThreadPool::TaskComesAfter::operator()(
    const PrioritizedTask &left,
    const PrioritizedTask &right) const
{
    if (left.priority != right.priority)
    {
        return left.priority < right.priority;
    }

    return left.sequence > right.sequence;
}