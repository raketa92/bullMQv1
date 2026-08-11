#include "delayed_job_scheduler.hpp"
#include "job_error.hpp"
#include "job_processor.hpp"
#include "job_queue.hpp"
#include "job_service.hpp"
#include "job_store.hpp"
#include "logger.hpp"
#include "thread_pool.hpp"
#include "worker.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
    void expectTrue(
        bool condition,
        const std::string &message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    Job waitForTerminalJob(
        JobStore &store,
        const std::string &id,
        std::chrono::milliseconds timeout)
    {
        const auto deadline =
            std::chrono::steady_clock::now() +
            timeout;

        while (
            std::chrono::steady_clock::now() <
            deadline)
        {
            const std::optional<Job> job =
                store.findById(id);

            if (job.has_value() &&
                (job->status ==
                     JobStatus::Completed ||
                 job->status ==
                     JobStatus::Failed))
            {
                return *job;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds{5});
        }

        throw std::runtime_error(
            "Timed out waiting for job: " + id);
    }

    void testRetryableJobEventuallyCompletes()
    {
        Logger logger("[TEST] ");
        JobStore store(":memory:");
        JobQueue queue;
        JobProcessor processor;
        DelayedJobScheduler scheduler(queue);

        JobService service(
            store,
            queue,
            scheduler);

        ThreadPool pool(
            2,
            logger,
            16);

        Worker worker(
            queue,
            scheduler,
            processor,
            pool,
            store);

        processor.registerHandler(
            "retry_job",
            [](const Job &job)
                -> std::string
            {
                if (job.attemptsMade < 3)
                {
                    throw RetryableJobError(
                        "temporary test failure");
                }

                return "retry succeeded";
            });

        processor.registerHandler(
            "permanent_job",
            [](const Job &) -> std::string
            {
                throw std::runtime_error(
                    "permanent test failure");
            });

        processor.registerHandler(
            "exhausted_job",
            [](const Job &) -> std::string
            {
                throw RetryableJobError(
                    "retry attempts exhausted");
            });

        scheduler.start();
        worker.start();

        Job job;
        job.name = "retry_job";
        job.maxAttempts = 3;
        job.retryBackoff =
            std::chrono::milliseconds{10};

        const std::string id =
            service.add(std::move(job));

        const Job finished =
            waitForTerminalJob(
                store,
                id,
                std::chrono::seconds{2});

        Job permanentJob;
        permanentJob.name = "permanent_job";
        permanentJob.maxAttempts = 5;

        const std::string permanentId =
            service.add(
                std::move(permanentJob));

        const Job permanentFinished =
            waitForTerminalJob(
                store,
                permanentId,
                std::chrono::seconds{2});

        Job exhaustedJob;
        exhaustedJob.name = "exhausted_job";
        exhaustedJob.maxAttempts = 2;
        exhaustedJob.retryBackoff =
            std::chrono::milliseconds{10};

        const std::string exhaustedId =
            service.add(
                std::move(exhaustedJob));

        const Job exhaustedFinished =
            waitForTerminalJob(
                store,
                exhaustedId,
                std::chrono::seconds{2});

        worker.stop();
        pool.stop();
        scheduler.stop();

        expectTrue(
            finished.status ==
                JobStatus::Completed,
            "retryable job should complete");

        expectTrue(
            finished.attemptsMade == 3,
            "retryable job should use three attempts");

        expectTrue(
            finished.result.has_value() &&
                *finished.result ==
                    "retry succeeded",
            "retryable job result differs");

        expectTrue(
            !finished.failureReason.has_value(),
            "completed job should not have a failure");

        expectTrue(
            permanentFinished.status ==
                JobStatus::Failed,
            "permanent job should fail");

        expectTrue(
            permanentFinished.attemptsMade == 1,
            "permanent job should run once");

        expectTrue(
            permanentFinished.failureReason
                    .has_value() &&
                *permanentFinished.failureReason ==
                    "permanent test failure",
            "permanent failure reason differs");

        expectTrue(
            !permanentFinished.result.has_value(),
            "permanent failure should not have a result");

        expectTrue(
            exhaustedFinished.status ==
                JobStatus::Failed,
            "exhausted job should fail");

        expectTrue(
            exhaustedFinished.attemptsMade == 2,
            "exhausted job should use two attempts");

        expectTrue(
            exhaustedFinished.failureReason
                    .has_value() &&
                *exhaustedFinished.failureReason ==
                    "retry attempts exhausted",
            "exhausted failure reason differs");

        expectTrue(
            !exhaustedFinished.result.has_value(),
            "exhausted job should not have a result");
    }

}

int main()
{
    try
    {
        testRetryableJobEventuallyCompletes();

        std::cout
            << "[PASS] worker outcomes\n";

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
