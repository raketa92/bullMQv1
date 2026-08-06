#pragma once

#include "job_queue.hpp"
#include "job_store.hpp"
#include "delayed_job_scheduler.hpp"

class JobService
{
public:
  JobService(
      JobStore &store,
      JobQueue &queue,
      DelayedJobScheduler &scheduler);

  std::string add(Job job);
  void restoreUnfinished();

private:
  JobStore &store_;
  JobQueue &queue_;
  DelayedJobScheduler &scheduler_;
};