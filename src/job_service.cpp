#include "job_service.hpp"

#include <utility>
#include <stdexcept>

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

  job.id = generateId();
  const std::chrono::milliseconds initialDelay = job.delay;
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

std::string JobService::generateId()
{
  const std::uint64_t numericId = nextId_.fetch_add(1);
  return "job-" + std::to_string(numericId);
}