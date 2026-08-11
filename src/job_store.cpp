#include "job_store.hpp"
#include "sqlite_statement.hpp"

#include <stdexcept>
#include <chrono>

namespace
{
  std::string statusToDatabaseValue(
      JobStatus status)
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
    throw std::logic_error("Unknown JobStatus value");
  }

  JobStatus statusFromDatabaseValue(
      const std::string &value)
  {
    if (value == "waiting")
    {
      return JobStatus::Waiting;
    }

    if (value == "active")
    {
      return JobStatus::Active;
    }

    if (value == "completed")
    {
      return JobStatus::Completed;
    }

    if (value == "failed")
    {
      return JobStatus::Failed;
    }

    throw std::runtime_error(
        "Unknown job status in SQLite: " + value);
  }

  Job jobFromCurrentRow(
      const SqliteStatement &statement)
  {
    Job job;

    job.id = statement.columnText(0);
    job.name = statement.columnText(1);
    job.payload = statement.columnText(2);

    job.status = statusFromDatabaseValue(
        statement.columnText(3));

    job.priority = static_cast<std::uint32_t>(
        statement.columnInt64(4));

    job.delay = std::chrono::milliseconds(
        statement.columnInt64(5));

    job.retryBackoff = std::chrono::milliseconds(
        statement.columnInt64(6));

    job.attemptsMade = static_cast<std::size_t>(
        statement.columnInt64(7));

    job.maxAttempts = static_cast<std::size_t>(
        statement.columnInt64(8));

    if (!statement.columnIsNull(9))
    {
      job.failureReason = statement.columnText(9);
    }

    if (!statement.columnIsNull(10))
    {
      job.result = statement.columnText(10);
    }

    if (!statement.columnIsNull(11))
    {
      const std::chrono::milliseconds
          availableAtMilliseconds{
              statement.columnInt64(11)};

      job.availableAt =
          std::chrono::system_clock::time_point{
              std::chrono::duration_cast<
                  std::chrono::system_clock::duration>(
                  availableAtMilliseconds)};
    }

    return job;
  }
}

JobStore::JobStore(const std::string &databasePath) : database_(databasePath)
{
  initializeSchema();
}

void JobStore::initializeSchema()
{
  database_.execute(
      "PRAGMA foreign_keys = ON;");

  database_.execute(
      "PRAGMA journal_mode = WAL;");

  database_.execute(
      "PRAGMA busy_timeout = 5000;");

  database_.execute(
      R"sql(
            CREATE TABLE IF NOT EXISTS jobs
            (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                payload TEXT NOT NULL,

                status TEXT NOT NULL
                    CHECK (
                        status IN (
                            'waiting',
                            'active',
                            'completed',
                            'failed'
                        )
                    ),

                priority INTEGER NOT NULL
                    CHECK (priority >= 0),

                delay_ms INTEGER NOT NULL
                    CHECK (delay_ms >= 0),

                retry_backoff_ms INTEGER NOT NULL
                    CHECK (retry_backoff_ms >= 0),

                attempts_made INTEGER NOT NULL
                    CHECK (attempts_made >= 0),

                max_attempts INTEGER NOT NULL
                    CHECK (max_attempts >= 1),

                failure_reason TEXT,
                result TEXT,

                available_at_ms INTEGER,

                created_at TEXT NOT NULL
                    DEFAULT CURRENT_TIMESTAMP,

                updated_at TEXT NOT NULL
                    DEFAULT CURRENT_TIMESTAMP
            );
        )sql");

  database_.execute(
      R"sql(
        CREATE TABLE IF NOT EXISTS job_id_sequence
        (
            singleton INTEGER PRIMARY KEY
                CHECK (singleton = 1),

            next_value INTEGER NOT NULL
                CHECK (next_value >= 1)
        );
    )sql");

  database_.execute(
      R"sql(
        INSERT OR IGNORE INTO job_id_sequence
        (
            singleton,
            next_value
        )
        VALUES
        (
            1,
            1
        );
    )sql");
}

bool JobStore::isTerminal(JobStatus status) const
{
  return status == JobStatus::Completed ||
         status == JobStatus::Failed;
}

void JobStore::save(const Job &job)
{
  if (job.maxAttempts == 0)
  {
    throw std::invalid_argument("Job maxAttempts must be at least 1");
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);

    SqliteStatement statement(
        database_.handle(),
        R"sql(
      INSERT INTO jobs
            (
                id,
                name,
                payload,
                status,
                priority,
                delay_ms,
                retry_backoff_ms,
                attempts_made,
                max_attempts,
                failure_reason,
                result,
                available_at_ms
            )
            VALUES
            (
                ?, ?, ?, ?, ?, ?,
                ?, ?, ?, ?, ?, ?
            );
      )sql");

    statement.bindText(1, job.id);
    statement.bindText(2, job.name);
    statement.bindText(3, job.payload);
    statement.bindText(4, statusToDatabaseValue(job.status));
    statement.bindInt64(5, static_cast<std::int64_t>(job.priority));
    statement.bindInt64(6, static_cast<std::int64_t>(job.delay.count()));
    statement.bindInt64(7, static_cast<std::int64_t>(job.retryBackoff.count()));
    statement.bindInt64(8, static_cast<std::int64_t>(job.attemptsMade));
    statement.bindInt64(9, static_cast<std::int64_t>(job.maxAttempts));

    if (job.failureReason.has_value())
    {
      statement.bindText(10, *job.failureReason);
    }
    else
    {
      statement.bindNull(10);
    }

    if (job.result.has_value())
    {
      statement.bindText(11, *job.result);
    }
    else
    {
      statement.bindNull(11);
    }

    if (job.availableAt.has_value())
    {
      const auto availableAtMilliseconds =
          std::chrono::duration_cast<
              std::chrono::milliseconds>(
              job.availableAt->time_since_epoch());

      statement.bindInt64(
          12,
          static_cast<std::int64_t>(
              availableAtMilliseconds.count()));
    }
    else
    {
      statement.bindNull(12);
    }

    if (statement.step())
    {
      throw std::runtime_error(
          "SQLite INSERT unexpectedly returned a row");
    }
  }

  /*
   * Someone may already be waiting for this job ID
   * before the job is saved.
   */
  cv_.notify_all();
}

void JobStore::markFailed(
    const std::string &id,
    const std::string &failureReason)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    SqliteStatement statement(
        database_.handle(),
        R"sql(
          UPDATE jobs
          SET
            status = 'failed',
            failure_reason = ?,
            result = NULL,
            available_at_ms = NULL,
            updated_at = CURRENT_TIMESTAMP
          WHERE id = ?
          RETURNING id;
        )sql");

    statement.bindText(1, failureReason);
    statement.bindText(2, id);

    if (!statement.step())
    {
      throw std::runtime_error(
          "SQLite job not found while marking failed: " +
          id);
    }

    const std::string updatedId =
        statement.columnText(0);

    if (updatedId != id)
    {
      throw std::runtime_error(
          "SQLite failed an unexpected job");
    }

    if (statement.step())
    {
      throw std::runtime_error(
          "SQLite failed multiple jobs for ID: " + id);
    }
  }

  cv_.notify_all();
}

void JobStore::markCompleted(
    const std::string &id,
    const std::string &result)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    SqliteStatement statement(
        database_.handle(),
        R"sql(
          UPDATE jobs
          SET
            status = 'completed',
            result = ?,
            failure_reason = NULL,
            available_at_ms = NULL,
            updated_at = CURRENT_TIMESTAMP
          WHERE id = ?
          RETURNING id;
        )sql");

    statement.bindText(1, result);
    statement.bindText(2, id);

    if (!statement.step())
    {
      throw std::runtime_error(
          "SQLite job not found while marking completed: " +
          id);
    }

    const std::string updatedId =
        statement.columnText(0);

    if (updatedId != id)
    {
      throw std::runtime_error(
          "SQLite completed an unexpected job");
    }

    if (statement.step())
    {
      throw std::runtime_error(
          "SQLite completed multiple jobs for ID: " +
          id);
    }
  }

  cv_.notify_all();
}

void JobStore::markWaiting(
    const std::string &id,
    const std::optional<std::chrono::system_clock::time_point> &availableAt)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    SqliteStatement statement(
        database_.handle(),
        R"sql(
    UPDATE jobs
          SET
            status = 'waiting',
            failure_reason = NULL,
            result = NULL,
            available_at_ms = ?,
            updated_at = CURRENT_TIMESTAMP
          WHERE id = ?
            AND status = 'active'
          RETURNING id;
    )sql");

    if (availableAt.has_value())
    {
      const auto availableAtMilliseconds =
          std::chrono::duration_cast<
              std::chrono::milliseconds>(
              availableAt->time_since_epoch());

      statement.bindInt64(
          1,
          static_cast<std::int64_t>(
              availableAtMilliseconds.count()));
    }
    else
    {
      statement.bindNull(1);
    }

    statement.bindText(2, id);

    if (!statement.step())
    {
      throw std::runtime_error(
          "Cannot mark job waiting because it is "
          "unknown or not active: " +
          id);
    }

    const std::string updatedId =
        statement.columnText(0);

    if (updatedId != id)
    {
      throw std::runtime_error(
          "SQLite marked an unexpected job waiting");
    }

    if (statement.step())
    {
      throw std::runtime_error(
          "SQLite marked multiple jobs waiting for ID: " +
          id);
    }
  }

  cv_.notify_all();
}

std::optional<Job> JobStore::findById(
    const std::string &id)
{
  std::lock_guard<std::mutex> lock(mutex_);

  return findByIdLocked(id);
}

std::optional<Job> JobStore::findByIdLocked(
    const std::string &id)
{
  SqliteStatement statement(
      database_.handle(),
      R"sql(
        SELECT
          id,
          name,
          payload,
          status,
          priority,
          delay_ms,
          retry_backoff_ms,
          attempts_made,
          max_attempts,
          failure_reason,
          result,
          available_at_ms
        FROM jobs
        WHERE id = ?;
      )sql");

  statement.bindText(1, id);

  if (!statement.step())
  {
    return std::nullopt;
  }

  Job job = jobFromCurrentRow(statement);

  if (statement.step())
  {
    throw std::runtime_error(
        "SQLite returned multiple jobs for ID: " + id);
  }

  return job;
}

std::vector<Job> JobStore::all()
{
  std::lock_guard<std::mutex> lock(mutex_);

  SqliteStatement statement(
      database_.handle(),
      R"sql(
        SELECT
          id,
          name,
          payload,
          status,
          priority,
          delay_ms,
          retry_backoff_ms,
          attempts_made,
          max_attempts,
          failure_reason,
          result,
          available_at_ms
        FROM jobs
        ORDER BY created_at, id;
      )sql");

  std::vector<Job> jobs;

  while (statement.step())
  {
    jobs.push_back(
        jobFromCurrentRow(statement));
  }

  return jobs;
}

JobMetrics JobStore::metrics()
{
  std::lock_guard<std::mutex> lock(mutex_);

  SqliteStatement statement(
      database_.handle(),
      R"sql(
        SELECT
          COUNT(*),

          COUNT(
            CASE
              WHEN status = 'waiting'
              THEN 1
            END
          ),

          COUNT(
            CASE
              WHEN status = 'active'
              THEN 1
            END
          ),

          COUNT(
            CASE
              WHEN status = 'completed'
              THEN 1
            END
          ),

          COUNT(
            CASE
              WHEN status = 'failed'
              THEN 1
            END
          )
        FROM jobs;
      )sql");

  if (!statement.step())
  {
    throw std::runtime_error(
        "SQLite metrics query returned no row");
  }

  const auto readCount =
      [&statement](int column)
  {
    const std::int64_t value =
        statement.columnInt64(column);

    if (value < 0)
    {
      throw std::runtime_error(
          "SQLite returned a negative job count");
    }

    return static_cast<std::size_t>(
        value);
  };

  JobMetrics result;

  result.total = readCount(0);
  result.waiting = readCount(1);
  result.active = readCount(2);
  result.completed = readCount(3);
  result.failed = readCount(4);

  if (result.total !=
      result.waiting +
          result.active +
          result.completed +
          result.failed)
  {
    throw std::runtime_error(
        "SQLite returned inconsistent job metrics");
  }

  if (statement.step())
  {
    throw std::runtime_error(
        "SQLite metrics query returned multiple rows");
  }

  return result;
}

void JobStore::waitUntilFinished(
    const std::string &id)
{
  std::unique_lock<std::mutex> lock(mutex_);

  cv_.wait(lock, [this, &id]
           {
        const std::optional<Job> job = findByIdLocked(id);

        if (!job.has_value())
        {
            return false;
        }

        return isTerminal(
            job->status); });
}

std::size_t JobStore::startAttempt(const std::string &id)
{
  std::size_t attemptsMade;

  {
    std::lock_guard<std::mutex> lock(mutex_);

    SqliteStatement statement(
        database_.handle(),
        R"sql(
        UPDATE jobs
        SET
          attempts_made = attempts_made + 1,
          status = 'active',
          failure_reason = NULL,
          result = NULL,
          available_at_ms = NULL,
          updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
          AND status = 'waiting'
          AND attempts_made < max_attempts

        RETURNING attempts_made;
      )sql");

    statement.bindText(1, id);

    if (!statement.step())
    {
      throw std::runtime_error(
          "Job cannot start because it is unknown, "
          "not waiting, or has exhausted its attempts: " +
          id);
    }

    const std::int64_t storedAttempts = statement.columnInt64(0);

    if (storedAttempts < 0)
    {
      throw std::runtime_error("SQLite returned a negative "
                               "attempt count for job: " +
                               id);
    }

    if (statement.step())
    {
      throw std::runtime_error(
          "SQLite updated multiple jobs for ID: " +
          id);
    }

    attemptsMade = static_cast<std::size_t>(storedAttempts);
  }

  cv_.notify_all();

  return attemptsMade;
}

std::string JobStore::generateJobId()
{
  std::lock_guard<std::mutex> lock(mutex_);

  SqliteStatement statement(
      database_.handle(),
      R"sql(
      UPDATE job_id_sequence
      SET next_value = next_value + 1
      WHERE singleton = 1
      RETURNING next_value - 1;
  )sql");

  if (!statement.step())
  {
    throw std::runtime_error("SQLite ID sequence returned no value");
  }

  const std::int64_t numericId = statement.columnInt64(0);

  if (statement.step())
  {
    throw std::runtime_error(
        "SQLite ID sequence returned multiple values");
  }

  return "job-" + std::to_string(numericId);
}

std::vector<Job> JobStore::unfinished()
{
  std::lock_guard<std::mutex> lock(mutex_);

  SqliteStatement statement(
      database_.handle(),
      R"sql(
      SELECT
          id,
          name,
          payload,
          status,
          priority,
          delay_ms,
          retry_backoff_ms,
          attempts_made,
          max_attempts,
          failure_reason,
          result,
          available_at_ms
        FROM jobs
        WHERE status IN ('waiting', 'active')
        ORDER BY created_at, id;
    )sql");

  std::vector<Job> jobs;

  while (statement.step())
  {
    jobs.push_back(jobFromCurrentRow(statement));
  }

  return jobs;
}