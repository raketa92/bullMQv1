#include "job_store.hpp"

bool JobStore::isTerminal(JobStatus status) const
{
  return status == JobStatus::Completed ||
         status == JobStatus::Failed;
}

void JobStore::save(const Job &job)
{
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