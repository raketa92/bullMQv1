#include "job_service.hpp"

#include <utility>
#include <stdexcept>
#include <chrono>
#include <vector>

JobService::JobService(
    JobStore &store,
    JobQueue &queue,
    DelayedJobScheduler &scheduler) : store_(store), queue_(queue), scheduler_(scheduler) {}

std::string JobService::add(Job job)
{
  if (job.delay < std::chrono::milliseconds::zero())
  {
    throw std::invalid_argument("Job delay cannot be negative");
  }

  if (job.retryBackoff < std::chrono::milliseconds::zero())
  {
    throw std::invalid_argument("Job retry backoff cannot be negative");
  }

  job.id = store_.generateJobId();
  const std::chrono::milliseconds initialDelay = job.delay;

  job.availableAt.reset();

  if (initialDelay > std::chrono::milliseconds::zero())
  {
    job.availableAt = std::chrono::system_clock::now() + initialDelay;
  }
  std::string generatedId = job.id;

  store_.save(job);

  if (initialDelay > std::chrono::milliseconds::zero())
  {
    scheduler_.schedule(std::move(job), initialDelay);
  }
  else
  {
    queue_.push(std::move(job));
  }

  return generatedId;
}

void JobService::restoreUnfinished()
{
  std::vector<Job> jobs = store_.unfinished();

  for (Job &job : jobs)
  {
    if (job.attemptsMade >= job.maxAttempts)
    {
      store_.markFailed(job.id, "Job has no attempts remaining during recovery");
      continue;
    }

    if (job.status == JobStatus::Active)
    {
      job.status = JobStatus::Waiting;
      job.availableAt.reset();

      store_.markWaiting(job.id, job.availableAt);
    }

    if (job.availableAt.has_value())
    {
      const auto remainingDelay = std::chrono::duration_cast<std::chrono::milliseconds>(*job.availableAt - std::chrono::system_clock::now());

      if (remainingDelay > std::chrono::milliseconds::zero())
      {
        scheduler_.schedule(std::move(job), remainingDelay);
        continue;
      }

      job.availableAt.reset();
    }

    queue_.push(std::move(job));
  }
}