#include "worker.hpp"

#include <utility>

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
                [this, job = std::move(*optionalJob)]
                {
                    store_.updateStatus(
                        job.id,
                        JobStatus::Active);

                    try
                    {
                        processor_.process(job);

                        store_.updateStatus(
                            job.id,
                            JobStatus::Completed);
                    }
                    catch (...)
                    {
                        store_.updateStatus(
                            job.id,
                            JobStatus::Failed);
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