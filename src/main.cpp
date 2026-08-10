#include "job_processor.hpp"
#include "job_queue.hpp"
#include "job_store.hpp"
#include "logger.hpp"
#include "thread_pool.hpp"
#include "worker.hpp"
#include "job_error.hpp"
#include "job_service.hpp"
#include "delayed_job_scheduler.hpp"
#include "http_server.hpp"
#include <string>

#include <iostream>
#include <stdexcept>

namespace
{
    void registerHandlers(
        JobProcessor &processor,
        Logger &logger)
    {
        processor.registerHandler(
            "unstable_job",
            [&logger](const Job &job) -> std::string
            {
                logger.info("Executing unstable_job, attempt " + std::to_string(job.attemptsMade));

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
}

int main(int argc, char *argv[])
{
    const std::string databasePath = argc >= 2 ? argv[1] : "jobs.db";
    Logger logger("[APP] ");

    JobStore store(databasePath);
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

    HttpServer httpServer(store, jobService);

    registerHandlers(processor, logger);

    scheduler.start();
    jobService.restoreUnfinished();
    worker.start();
    httpServer.start("127.0.0.1", 8080);

    std::cout
        << "HTTP server listening on "
        << "http://127.0.0.1:8080\n"
        << "Press Enter to stop...\n";

    std::cin.get();

    httpServer.stop();
    scheduler.stop();
    worker.stop();

    return 0;
}
