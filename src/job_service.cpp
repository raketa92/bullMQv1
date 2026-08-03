#include "job_service.hpp"

#include <utility>

JobService::JobService(
    JobStore &store,
    JobQueue &queue) : store_(store), queue_(queue) {}

std::string JobService::add(Job job)
{
  job.id = generateId();
  std::string generatedId = job.id;

  store_.save(job);
  queue_.push(std::move(job));

  return generatedId;
}

std::string JobService::generateId()
{
  const std::uint64_t numericId = nextId_.fetch_add(1);
  return "job-" + std::to_string(numericId);
}