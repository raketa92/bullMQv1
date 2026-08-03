#pragma once

#include "job_queue.hpp"
#include "job_store.hpp"

class JobService
{
public:
  JobService(
      JobStore &store,
      JobQueue &queue);

  void add(Job job);

private:
  JobStore &store_;
  JobQueue &queue_;
};