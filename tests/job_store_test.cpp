#include "job_store.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
  void expectEqual(
      std::size_t actual,
      std::size_t expected,
      const std::string &field)
  {
    if (actual == expected)
    {
      return;
    }

    throw std::runtime_error(
        field +
        ": expected " +
        std::to_string(expected) +
        ", received " +
        std::to_string(actual));
  }

  void expectTrue(
      bool condition,
      const std::string &message)
  {
    if (!condition)
    {
      throw std::runtime_error(message);
    }
  }

  void expectMetrics(
      const JobMetrics &actual,
      std::size_t total,
      std::size_t waiting,
      std::size_t active,
      std::size_t completed,
      std::size_t failed,
      const std::string &stage)
  {
    expectEqual(
        actual.total,
        total,
        stage + " total");

    expectEqual(
        actual.waiting,
        waiting,
        stage + " waiting");

    expectEqual(
        actual.active,
        active,
        stage + " active");

    expectEqual(
        actual.completed,
        completed,
        stage + " completed");

    expectEqual(
        actual.failed,
        failed,
        stage + " failed");
  }

  void testEmptyStoreMetrics()
  {
    JobStore store(":memory:");

    const JobMetrics metrics =
        store.metrics();

    expectEqual(
        metrics.total,
        0,
        "total");

    expectEqual(
        metrics.waiting,
        0,
        "waiting");

    expectEqual(
        metrics.active,
        0,
        "active");

    expectEqual(
        metrics.completed,
        0,
        "completed");

    expectEqual(
        metrics.failed,
        0,
        "failed");
  }

  void testMetricsFollowJobStates()
  {
    JobStore store(":memory:");

    Job waitingJob;
    waitingJob.id = "job-waiting";
    waitingJob.name = "test_job";

    store.save(waitingJob);

    Job completedJob;
    completedJob.id = "job-completed";
    completedJob.name = "test_job";
    completedJob.maxAttempts = 2;

    store.save(completedJob);

    expectMetrics(
        store.metrics(),
        2,
        2,
        0,
        0,
        0,
        "after saving");

    const std::size_t completedAttempts =
        store.startAttempt(
            completedJob.id);

    expectEqual(
        completedAttempts,
        1,
        "completed job attempts");

    expectMetrics(
        store.metrics(),
        2,
        1,
        1,
        0,
        0,
        "while active");

    store.markCompleted(
        completedJob.id,
        "test result");

    expectMetrics(
        store.metrics(),
        2,
        1,
        0,
        1,
        0,
        "after completion");

    Job failedJob;
    failedJob.id = "job-failed";
    failedJob.name = "test_job";

    store.save(failedJob);
    store.startAttempt(failedJob.id);

    store.markFailed(
        failedJob.id,
        "test failure");

    expectMetrics(
        store.metrics(),
        3,
        1,
        0,
        1,
        1,
        "after failure");
  }

  void testFindByIdPreservesJobData()
  {
    JobStore store(":memory:");

    expectTrue(
        !store.findById("missing").has_value(),
        "unknown job should not be found");

    Job original;
    original.id = "job-persisted";
    original.name = "test_job";
    original.payload = "test payload";
    original.priority = 42;
    original.delay =
        std::chrono::milliseconds{1500};
    original.retryBackoff =
        std::chrono::milliseconds{750};
    original.maxAttempts = 3;

    original.availableAt =
        std::chrono::system_clock::time_point{
            std::chrono::milliseconds{
                1234567890}};

    store.save(original);

    const std::optional<Job> stored =
        store.findById(original.id);

    expectTrue(
        stored.has_value(),
        "saved job should be found");

    expectTrue(
        stored->id == original.id,
        "stored ID differs");

    expectTrue(
        stored->name == original.name,
        "stored name differs");

    expectTrue(
        stored->payload == original.payload,
        "stored payload differs");

    expectTrue(
        stored->status == JobStatus::Waiting,
        "stored status should be waiting");

    expectTrue(
        stored->priority == original.priority,
        "stored priority differs");

    expectTrue(
        stored->delay == original.delay,
        "stored delay differs");

    expectTrue(
        stored->retryBackoff ==
            original.retryBackoff,
        "stored retry backoff differs");

    expectTrue(
        stored->attemptsMade == 0,
        "new job should have zero attempts");

    expectTrue(
        stored->maxAttempts ==
            original.maxAttempts,
        "stored maximum attempts differs");

    expectTrue(
        stored->availableAt ==
            original.availableAt,
        "stored available time differs");

    expectTrue(
        !stored->failureReason.has_value(),
        "new job should not have a failure reason");

    expectTrue(
        !stored->result.has_value(),
        "new job should not have a result");

    store.startAttempt(original.id);

    store.markCompleted(
        original.id,
        "persisted result");

    const std::optional<Job> completed =
        store.findById(original.id);

    expectTrue(
        completed.has_value(),
        "completed job should be found");

    expectTrue(
        completed->status ==
            JobStatus::Completed,
        "job should be completed");

    expectTrue(
        completed->attemptsMade == 1,
        "completed job should have one attempt");

    expectTrue(
        completed->result ==
            std::optional<std::string>{
                "persisted result"},
        "completed result differs");

    expectTrue(
        !completed->failureReason.has_value(),
        "completed job should not have a failure");

    expectTrue(
        !completed->availableAt.has_value(),
        "completed job should not remain scheduled");
  }
}

int main()
{
  try
  {
    testEmptyStoreMetrics();

    std::cout
        << "[PASS] empty store metrics\n";

    testFindByIdPreservesJobData();

    std::cout
        << "[PASS] findById preserves job data\n";

    testMetricsFollowJobStates();

    std::cout
        << "[PASS] metrics follow job states\n";

    return EXIT_SUCCESS;
  }
  catch (const std::exception &error)
  {
    std::cerr
        << "[FAIL] "
        << error.what()
        << '\n';

    return EXIT_FAILURE;
  }
}