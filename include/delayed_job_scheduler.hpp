#pragma once

#include "job_queue.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class DelayedJobScheduler
{
public:
  explicit DelayedJobScheduler(JobQueue &queue);
  ~DelayedJobScheduler();
  DelayedJobScheduler(const DelayedJobScheduler &) = delete;
  DelayedJobScheduler &operator=(const DelayedJobScheduler &) = delete;

  void start();
  void schedule(Job job, std::chrono::milliseconds delay);
  void stop();

private:
  using Clock = std::chrono::steady_clock;

  struct ScheduledJob
  {
    Clock::time_point readyAt;
    Job job;
  };

  struct EarlierReady
  {
    bool operator()(const ScheduledJob &left, const ScheduledJob &right) const;
  };

  void run();

  JobQueue &queue_;
  std::priority_queue<ScheduledJob, std::vector<ScheduledJob>, EarlierReady> delayedJobs_;

  std::mutex mutex_;
  std::condition_variable cv_;

  bool running_ = false;
  std::thread schedulerThread_;
};