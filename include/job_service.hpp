#pragma once

#include "job_queue.hpp"
#include "job_store.hpp"
#include "delayed_job_scheduler.hpp"
#include <atomic>
#include <cstdint>
#include <string>

class JobService
{
public:
  JobService(
      JobStore &store,
      JobQueue &queue,
      DelayedJobScheduler &scheduler);

  std::string add(Job job);

private:
  std::string generateId();

  JobStore &store_;
  JobQueue &queue_;
  DelayedJobScheduler &scheduler_;

  std::atomic<std::uint64_t> nextId_{1};
};