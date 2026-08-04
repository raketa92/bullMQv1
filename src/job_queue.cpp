#include "job_queue.hpp"

#include <utility>

void JobQueue::push(Job job)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (shutdown_)
    {
      return;
    }

    queue_.push(QueuedJob{nextSequence_, std::move(job)});
    ++nextSequence_;
  }

  // Wake one dispatcher waiting in pop().
  cv_.notify_one();
}

std::optional<Job> JobQueue::pop()
{
  std::unique_lock<std::mutex> lock(mutex_);

  cv_.wait(lock, [this]
           { return shutdown_ || !queue_.empty(); });

  if (shutdown_ && queue_.empty())
  {
    return std::nullopt;
  }

  Job job = queue_.top().job;
  queue_.pop();

  return job;
}

void JobQueue::shutdown()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
  }

  /*
   * Wake the dispatcher if it is blocked in pop()
   * while the queue is empty.
   */
  cv_.notify_all();
}

bool JobQueue::JobComesAfter::operator()(
    const QueuedJob &left,
    const QueuedJob &right) const
{
  if (left.job.priority != right.job.priority)
  {
    return left.job.priority < right.job.priority;
  }

  return left.sequence > right.sequence;
}