#include "job_processor.hpp"
#include "job_queue.hpp"
#include "job_store.hpp"
#include "logger.hpp"
#include "thread_pool.hpp"
#include "worker.hpp"

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
}

int main()
{
    Logger logger("[APP] ");

    JobStore store;
    JobQueue queue;
    JobProcessor processor;

    ThreadPool pool(
        4, // worker thread count
        logger,
        100); // maximum ThreadPool task queue size

    Worker worker(
        queue,
        processor,
        pool,
        store);

    processor.registerHandler(
        "send_email",
        [](const Job &job)
        {
            std::cout
                << "Sending email to: "
                << job.payload
                << '\n';
        });

    processor.registerHandler(
        "fail_job",
        [](const Job &job)
        {
            throw std::runtime_error(
                "Intentional failure for payload: " +
                job.payload);
        });

    worker.start();

    Job job1{
        "job-1",
        "send_email",
        "test@example.com"};

    Job job2{
        "job-2",
        "fail_job",
        "bad payload"};

    /*
     * At this stage, caller must perform two operations:
     *
     * 1. save job for status tracking
     * 2. push job for execution
     *
     * A future QueueService will combine these operations.
     */
    store.save(job1);
    queue.push(job1);

    store.save(job2);
    queue.push(job2);

    /*
     * Proper synchronization.
     *
     * No sleep_for() and no timing assumption.
     */
    store.waitUntilFinished(job1.id);
    store.waitUntilFinished(job2.id);

    for (const Job &job : store.all())
    {
        std::cout
            << "id=" << job.id
            << " name=" << job.name
            << " payload=" << job.payload
            << " status=" << statusToString(job.status)
            << '\n';
    }

    worker.stop();

    return 0;
}