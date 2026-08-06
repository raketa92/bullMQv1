#include "worker.hpp"

#include <exception>
#include <utility>
#include <chrono>

#include "job_error.hpp"

Worker::Worker(
    JobQueue &queue,
    DelayedJobScheduler &scheduler,
    JobProcessor &processor,
    ThreadPool &pool,
    JobStore &store)
    : queue_(queue),
      scheduler_(scheduler),
      processor_(processor),
      pool_(pool),
      store_(store),
      running_(false)
{
}

Worker::~Worker()
{
    stop();
}

void Worker::start()
{
    running_.store(true);

    dispatcher_ = std::thread([this]
                              {
        while (running_.load())
        {
            auto optionalJob = queue_.pop();

            /*
             * nullopt means:
             *
             * queue has been shut down
             * and there are no jobs remaining.
             */
            if (!optionalJob.has_value())
            {
                break;
            }

            const auto jobPriority = optionalJob->priority;

            /*
             * Move the Job into the ThreadPool task.
             *
             * The dispatcher can immediately return to
             * JobQueue::pop() while pool workers perform
             * the actual processing.
             */
            pool_.enqueueWithPriority(
                jobPriority,
                [this, job = std::move(*optionalJob)]() mutable
                {
                    try
                    {
                        job.attemptsMade = store_.startAttempt(job.id);

                        job.status = JobStatus::Active;
                        job.availableAt.reset();

                    
                        std::string result = processor_.process(job);
                        store_.markCompleted(job.id, result);
                    }
                    catch (const RetryableJobError &error)
                    {
                        const bool hasAttemptsRemaining =
                            job.attemptsMade < job.maxAttempts;

                        if (hasAttemptsRemaining)
                        {
                            const std::chrono::milliseconds retryDelay =
                                calculateRetryDelay(job);

                            job.status = JobStatus::Waiting;
                            job.availableAt.reset();

                            if (retryDelay >
                                std::chrono::milliseconds::zero())
                            {
                                job.availableAt =
                                    std::chrono::system_clock::now() +
                                    retryDelay;
                            }

                            store_.markWaiting(
                                job.id,
                                job.availableAt);

                            if (retryDelay >
                                std::chrono::milliseconds::zero())
                            {
                                scheduler_.schedule(
                                    std::move(job),
                                    retryDelay);
                            }
                            else
                            {
                                queue_.push(std::move(job));
                            }
                        }
                        else
                        {
                            store_.markFailed(job.id, error.what());
                        }
                    }
                    catch (const std::exception &error)
                    {
                        store_.markFailed(job.id, error.what());
                    }
                    catch (...)
                    {
                        store_.markFailed(job.id, "Unknown non-standard exception");
                    }
                });
        } });
}

void Worker::stop()
{
    running_.store(false);

    /*
     * Wake dispatcher if it is sleeping in queue_.pop().
     */
    queue_.shutdown();

    if (dispatcher_.joinable())
    {
        dispatcher_.join();
    }
}

std::chrono::milliseconds Worker::calculateRetryDelay(
    const Job &job) const
{
    std::chrono::milliseconds delay = job.retryBackoff;

    for (std::size_t attempt = 1; attempt < job.attemptsMade; ++attempt)
    {
        const std::chrono::milliseconds maximum = std::chrono::milliseconds::max();

        if (delay > maximum / 2)
        {
            return maximum;
        }

        delay *= 2;
    }

    return delay;
}
