#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

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
  std::queue<Job> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;

  bool shutdown_ = false;
};