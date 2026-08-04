#include "job_processor.hpp"
#include "job_queue.hpp"
#include "job_store.hpp"
#include "logger.hpp"
#include "thread_pool.hpp"
#include "worker.hpp"
#include "job_error.hpp"
#include "job_service.hpp"
#include "delayed_job_scheduler.hpp"
#include <string>

#include <iostream>
#include <stdexcept>

namespace
{
    const char *statusToString(JobStatus status)
    {
        switch (status)
        {
        case JobStatus::Waiting:
            return "waiting";

        case JobStatus::Active:
            return "active";

        case JobStatus::Completed:
            return "completed";

        case JobStatus::Failed:
            return "failed";
        }

        return "unknown";
    }

    void registerDemoHandlers(
        JobProcessor &processor,
        Logger &logger)
    {
        processor.registerHandler(
            "unstable_job",
            [&logger](const Job &job) -> std::string
            {
                logger.info("Execution unstable_job, attempt " + std::to_string(job.attemptsMade));

                if (job.attemptsMade < 3)
                {
                    throw RetryableJobError("Temporary service failure");
                }

                return "Temporary operation eventually succeeded";
            });

        processor.registerHandler(
            "invalid_job",
            [&logger](const Job &job) -> std::string
            {
                logger.info(
                    "Executing invalid_job, attempt " +
                    std::to_string(job.attemptsMade));

                throw std::runtime_error(
                    "Invalid payload");
            });
    }

    void printJob(const Job &job)
    {
        std::cout
            << "Job: " << job.id
            << ", status: "
            << statusToString(job.status)
            << ", priority: "
            << job.priority
            << ", attempts: "
            << job.attemptsMade
            << ", max attempts: "
            << job.maxAttempts
            << ", failure reason: "
            << job.failureReason.value_or("none")
            << ", result: "
            << job.result.value_or("none")
            << '\n';
    }
}

int main()
{
    Logger logger("[APP] ");

    JobStore store;
    JobQueue queue;
    JobProcessor processor;

    DelayedJobScheduler scheduler(queue);

    JobService jobService(store, queue, scheduler);

    ThreadPool pool(4, logger, 100);

    Worker worker(
        queue,
        scheduler,
        processor,
        pool,
        store);

    registerDemoHandlers(processor, logger);

    scheduler.start();

    Job retryJob{
        "",
        "unstable_job",
        "temporary operation"};
    retryJob.maxAttempts = 3;
    retryJob.delay = std::chrono::milliseconds{500};
    retryJob.retryBackoff = std::chrono::milliseconds{100};
    retryJob.priority = 100;

    Job permanentFailureJob{
        "",
        "invalid_job",
        "bad payload"};
    permanentFailureJob.maxAttempts = 5;
    permanentFailureJob.priority = 10;

    Job exhaustedRetryJob{"", "unstable_job", "temporary operation"};
    exhaustedRetryJob.maxAttempts = 2;
    exhaustedRetryJob.retryBackoff = std::chrono::milliseconds{100};
    exhaustedRetryJob.priority = 1;

    /*
     * JobService saves each job before exposing it
     * to workers through JobQueue.
     */
    const std::string retryJobId = jobService.add(retryJob);
    const std::string exhaustedRetryJobId = jobService.add(exhaustedRetryJob);
    const std::string permanentFailureJobId = jobService.add(permanentFailureJob);

    worker.start();

    /*
     * Proper synchronization.
     *
     * No sleep_for() and no timing assumption.
     */
    store.waitUntilFinished(retryJobId);
    store.waitUntilFinished(permanentFailureJobId);
    store.waitUntilFinished(exhaustedRetryJobId);

    for (const Job &job : store.all())
    {
        printJob(job);
    }

    scheduler.stop();
    worker.stop();

    return 0;
}
