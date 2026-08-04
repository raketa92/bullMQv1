#include "delayed_job_scheduler.hpp"

#include <stdexcept>
#include <utility>

DelayedJobScheduler::DelayedJobScheduler(JobQueue &queue) : queue_(queue) {}
DelayedJobScheduler::~DelayedJobScheduler()
{
  stop();
}

bool DelayedJobScheduler::EarlierReady::operator()(const ScheduledJob &left, const ScheduledJob &right) const
{
  return left.readyAt > right.readyAt;
}

void DelayedJobScheduler::start()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_)
  {
    return;
  }
  running_ = true;

  try
  {
    schedulerThread_ = std::thread(&DelayedJobScheduler::run, this);
  }
  catch (...)
  {
    running_ = false;
    throw;
  }
}

void DelayedJobScheduler::schedule(Job job, std::chrono::milliseconds delay)
{
  if (delay < std::chrono::milliseconds::zero())
  {
    throw std::invalid_argument("Job delay cannot be negative");
  }

  const Clock::time_point readyAt = Clock::now() + delay;

  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_)
    {
      throw std::runtime_error("Cannot schedule job on stopped scheduler");
    }

    delayedJobs_.push(ScheduledJob{readyAt, std::move(job)});
  }

  cv_.notify_one();
}

void DelayedJobScheduler::run()
{
  std::unique_lock<std::mutex> lock(mutex_);

  while (running_)
  {
    if (delayedJobs_.empty())
    {
      cv_.wait(lock, [this]
               { return !running_ || !delayedJobs_.empty(); });
      continue;
    }

    const Clock::time_point readyAt = delayedJobs_.top().readyAt;

    if (Clock::now() < readyAt)
    {
      cv_.wait_until(lock, readyAt);
      continue;
    }

    Job job = delayedJobs_.top().job;
    delayedJobs_.pop();

    lock.unlock();
    queue_.push(std::move(job));
    lock.lock();
  }
}

void DelayedJobScheduler::stop()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_)
    {
      return;
    }

    running_ = false;
  }
  cv_.notify_all();

  if (schedulerThread_.joinable())
  {
    schedulerThread_.join();
  }
}