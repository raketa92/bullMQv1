#include "job_store.hpp"

#include <stdexcept>
#include <utility>

bool JobStore::isTerminal(JobStatus status) const
{
  return status == JobStatus::Completed ||
         status == JobStatus::Failed;
}

void JobStore::save(const Job &job)
{
  if (job.maxAttempts == 0)
  {
    throw std::invalid_argument("Job maxAttempts must be at least 1");
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_[job.id] = job;
  }

  /*
   * Someone may already be waiting for this job ID
   * before the job is saved.
   */
  cv_.notify_all();
}

void JobStore::updateStatus(
    const std::string &id,
    JobStatus status)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    auto jobIterator = jobs_.find(id);

    if (jobIterator != jobs_.end())
    {
      jobIterator->second.status = status;
    }
  }

  /*
   * Wake threads waiting for status changes.
   *
   * Each waiter checks whether its particular job
   * reached Completed or Failed.
   */
  cv_.notify_all();
}

void JobStore::markFailed(
    const std::string &id,
    const std::string &failureReason)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto jobIterator = jobs_.find(id);
    if (jobIterator != jobs_.end())
    {
      Job &job = jobIterator->second;
      job.status = JobStatus::Failed;
      job.failureReason = failureReason;
      job.result.reset();
    }
  }

  cv_.notify_all();
}

void JobStore::markCompleted(
    const std::string &id,
    std::string result)
{

  {
    std::lock_guard<std::mutex> lock(mutex_);

    auto jobIterator = jobs_.find(id);
    if (jobIterator != jobs_.end())
    {
      Job &job = jobIterator->second;

      job.status = JobStatus::Completed;
      job.result = std::move(result);
      job.failureReason.reset();
    }
  }
  cv_.notify_all();
}

std::optional<Job> JobStore::findById(
    const std::string &id)
{
  std::lock_guard<std::mutex> lock(mutex_);

  auto jobIterator = jobs_.find(id);

  if (jobIterator == jobs_.end())
  {
    return std::nullopt;
  }

  return jobIterator->second;
}

std::vector<Job> JobStore::all()
{
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<Job> result;
  result.reserve(jobs_.size());

  for (const auto &[id, job] : jobs_)
  {
    // ID is already part of Job, so map key is unused here.
    static_cast<void>(id);

    result.push_back(job);
  }

  return result;
}

void JobStore::waitUntilFinished(
    const std::string &id)
{
  std::unique_lock<std::mutex> lock(mutex_);

  cv_.wait(lock, [this, &id]
           {
        auto jobIterator = jobs_.find(id);

        if (jobIterator == jobs_.end())
        {
            // Job does not exist yet. Continue waiting.
            return false;
        }

        return isTerminal(
            jobIterator->second.status); });
}

std::size_t JobStore::startAttempt(const std::string &id)
{
  std::size_t attemptsMade;

  {
    std::lock_guard<std::mutex> lock(mutex_);

    auto iterator = jobs_.find(id);

    if (iterator == jobs_.end())
    {
      throw std::runtime_error("Cannot start unknown job: " + id);
    }

    Job &storedJob = iterator->second;

    ++storedJob.attemptsMade;
    storedJob.status = JobStatus::Active;

    attemptsMade = storedJob.attemptsMade;
  }

  cv_.notify_all();

  return attemptsMade;
}
