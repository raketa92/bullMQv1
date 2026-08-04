#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <chrono>
#include <cstdint>
#include <vector>

enum class JobStatus
{
  Waiting,
  Active,
  Completed,
  Failed
};

struct Job
{
  std::string id;
  std::string name;
  std::string payload;
  JobStatus status = JobStatus::Waiting;

  std::chrono::milliseconds delay{0};
  std::chrono::milliseconds retryBackoff{0};
  std::uint32_t priority = 0;

  std::size_t attemptsMade = 0;
  std::size_t maxAttempts = 1;
  std::optional<std::string> failureReason = std::nullopt;
  std::optional<std::string> result = std::nullopt;
};

class JobQueue
{
public:
  void push(Job job);

  /*
   * Returns:
   *
   * Job             when work is available
   * std::nullopt    when queue is shut down and empty
   */
  std::optional<Job> pop();

  void shutdown();

private:
  struct QueuedJob
  {
    std::uint64_t sequence;
    Job job;
  };

  struct JobComesAfter
  {
    bool operator()(
        const QueuedJob &left,
        const QueuedJob &right) const;
  };

  std::priority_queue<QueuedJob, std::vector<QueuedJob>, JobComesAfter> queue_;
  std::uint64_t nextSequence_ = 0;
  std::mutex mutex_;
  std::condition_variable cv_;

  bool shutdown_ = false;
};