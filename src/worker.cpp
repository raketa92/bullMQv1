#include "worker.hpp"

#include <exception>
#include <utility>

#include "job_error.hpp"

Worker::Worker(
    JobQueue &queue,
    JobProcessor &processor,
    ThreadPool &pool,
    JobStore &store)
    : queue_(queue),
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

            /*
             * Move the Job into the ThreadPool task.
             *
             * The dispatcher can immediately return to
             * JobQueue::pop() while pool workers perform
             * the actual processing.
             */
            pool_.enqueue(
                [this, job = std::move(*optionalJob)]() mutable
                {
                    job.attemptsMade = store_.startAttempt(job.id);

                    job.status = JobStatus::Active;

                    try
                    {
                        std::string result = processor_.process(job);
                        store_.markCompleted(job.id, std::move(result));
                    }
                    catch (const RetryableJobError &error)
                    {
                        const bool hasAttemptsRemaining =
                            job.attemptsMade < job.maxAttempts;

                        if (hasAttemptsRemaining)
                        {
                            job.status = JobStatus::Waiting;
                            store_.updateStatus(job.id, JobStatus::Waiting);
                            queue_.push(std::move(job));
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
