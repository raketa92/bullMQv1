#pragma once

#include "job_queue.hpp"
#include "sqlite_database.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class JobStore
{
public:
    explicit JobStore(const std::string &databasePath);
    JobStore(const JobStore &) = delete;
    JobStore &operator=(const JobStore &) = delete;

    void save(const Job &job);

    void updateStatus(
        const std::string &id,
        JobStatus status);

    void markFailed(
        const std::string &id,
        const std::string &failureReason);

    void markCompleted(
        const std::string &id,
        const std::string &result);

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
    std::string generateJobId();

private:
    bool isTerminal(JobStatus status) const;
    void initializeSchema();

    SqliteDatabase database_;

    std::mutex mutex_;
    std::condition_variable cv_;

    std::optional<Job> findByIdLocked(const std::string &id);
};