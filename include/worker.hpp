#pragma once

#include "job_processor.hpp"
#include "job_queue.hpp"
#include "job_store.hpp"
#include "thread_pool.hpp"
#include "delayed_job_scheduler.hpp"

#include <atomic>
#include <thread>

class Worker
{
public:
  Worker(
      JobQueue &queue,
      DelayedJobScheduler &scheduler,
      JobProcessor &processor,
      ThreadPool &pool,
      JobStore &store);

  ~Worker();

  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;

  void start();
  void stop();

private:
  JobQueue &queue_;
  DelayedJobScheduler &scheduler_;
  JobProcessor &processor_;
  ThreadPool &pool_;
  JobStore &store_;

  std::atomic<bool> running_;
  std::thread dispatcher_;

  std::chrono::milliseconds calculateRetryDelay(
      const Job &job) const;
};