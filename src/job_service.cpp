#include "job_service.hpp"

#include <utility>

JobService::JobService(
    JobStore &store,
    JobQueue &queue) : store_(store), queue_(queue) {}

void JobService::add(Job job)
{
  store_.save(job);
  queue_.push(std::move(job));
}
