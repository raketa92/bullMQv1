#pragma once

#include "job_queue.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class JobStore
{
public:
  void save(const Job &job);

  void updateStatus(
      const std::string &id,
      JobStatus status);

  void markFailed(
      const std::string &id,
      const std::string &failureReason);

  void markCompleted(
      const std::string &id,
      std::string result);

  std::optional<Job> findById(
      const std::string &id);

  std::vector<Job> all();

  /*
   * Blocks until the specified job reaches:
   *
   * Completed
   * or
   * Failed
   */
  void waitUntilFinished(const std::string &id);

  std::size_t startAttempt(const std::string &id);

private:
  bool isTerminal(JobStatus status) const;

  std::mutex mutex_;
  std::condition_variable cv_;

  std::unordered_map<std::string, Job> jobs_;
};