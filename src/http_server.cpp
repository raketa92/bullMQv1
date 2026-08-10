#include "http_server.hpp"
#include "job_store.hpp"
#include "job_service.hpp"

#include <stdexcept>
#include <nlohmann/json.hpp>
#include <chrono>
#include <utility>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cctype>
#include <exception>

namespace
{
    const char *statusToJsonValue(
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

        throw std::logic_error(
            "Unknown JobStatus value");
    }

    nlohmann::json jobToJson(
        const Job &job)
    {
        nlohmann::json json{
            {"id", job.id},
            {"name", job.name},
            {"payload", job.payload},
            {"status", statusToJsonValue(job.status)},
            {"priority", job.priority},
            {"delayMs", job.delay.count()},
            {"retryBackoffMs",
             job.retryBackoff.count()},
            {"attemptsMade", job.attemptsMade},
            {"maxAttempts", job.maxAttempts},
            {"failureReason", nullptr},
            {"result", nullptr},
            {"availableAtMs", nullptr}};

        if (job.failureReason.has_value())
        {
            json["failureReason"] =
                *job.failureReason;
        }

        if (job.result.has_value())
        {
            json["result"] = *job.result;
        }

        if (job.availableAt.has_value())
        {
            const auto availableAtMilliseconds =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    job.availableAt
                        ->time_since_epoch());

            json["availableAtMs"] =
                availableAtMilliseconds.count();
        }

        return json;
    }

    std::int64_t readIntegerField(
        const nlohmann::json &body,
        const std::string &field,
        std::int64_t defaultValue,
        std::int64_t minimum,
        std::int64_t maximum)
    {
        if (!body.contains(field))
        {
            return defaultValue;
        }

        const nlohmann::json &value =
            body.at(field);

        if (!value.is_number_integer())
        {
            throw std::invalid_argument(
                "Field '" + field +
                "' must be an integer");
        }

        std::int64_t number;

        if (value.is_number_unsigned())
        {
            const std::uint64_t unsignedNumber =
                value.get<std::uint64_t>();

            if (unsignedNumber >
                static_cast<std::uint64_t>(maximum))
            {
                throw std::invalid_argument(
                    "Field '" + field +
                    "' is out of range");
            }

            number =
                static_cast<std::int64_t>(
                    unsignedNumber);
        }
        else
        {
            number = value.get<std::int64_t>();
        }

        if (number < minimum ||
            number > maximum)
        {
            throw std::invalid_argument(
                "Field '" + field +
                "' must be between " +
                std::to_string(minimum) +
                " and " +
                std::to_string(maximum));
        }

        return number;
    }

    bool hasJsonContentType(
        const httplib::Request &request)
    {
        if (!request.has_header("Content-Type"))
        {
            return false;
        }
        std::string mediaType =
            request.get_header_value(
                "Content-Type");

        const std::size_t parameterStart = mediaType.find(";");

        if (parameterStart != std::string::npos)
        {
            mediaType.resize(parameterStart);
        }

        const std::size_t first = mediaType.find_first_not_of(" \t");

        if (first == std::string::npos)
        {
            return false;
        }

        const std::size_t last = mediaType.find_last_not_of(" \t");

        mediaType = mediaType.substr(first, last - first + 1);

        std::transform(
            mediaType.begin(),
            mediaType.end(),
            mediaType.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

        return mediaType == "application/json";
    }
}

HttpServer::HttpServer(
    JobStore &store,
    JobService &jobService)
    : store_(store), jobService_(jobService)
{
    registerRoutes();
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::registerRoutes()
{
    server_.set_exception_handler(
        [](
            const httplib::Request &,
            httplib::Response &response,
            std::exception_ptr exception)
        {
            std::string details =
                "Unknown non-standard exception";

            if (exception)
            {
                try
                {
                    std::rethrow_exception(
                        exception);
                }
                catch (
                    const std::exception &error)
                {
                    details = error.what();
                }
                catch (...)
                {
                    // Keep the default details.
                }
            }

            response.status = 500;
            const nlohmann::json responseBody{
                {"error",
                 "Internal server error"},
                {"details", details}};

            response.set_content(
                responseBody.dump(),
                "application/json");
        });

    server_.Get(
        "/health",
        [](const httplib::Request &,
           httplib::Response &response)
        {
            response.status = 200;

            response.set_content(R"({"status":"ok"})", "application/json");
        });

    server_.Get(
        "/jobs/:id",
        [this](
            const httplib::Request &request,
            httplib::Response &response)
        {
            const std::string &id =
                request.path_params.at("id");

            const std::optional<Job> job =
                store_.findById(id);

            if (!job.has_value())
            {
                response.status = 404;

                const nlohmann::json error{
                    {"error", "Job not found"},
                    {"id", id}};

                response.set_content(
                    error.dump(),
                    "application/json");

                return;
            }

            response.status = 200;

            response.set_content(
                jobToJson(*job).dump(),
                "application/json");
        });

    server_.Get(
        "/jobs",
        [this](
            const httplib::Request &,
            httplib::Response &response)
        {
            const auto jobs =
                store_.all();

            nlohmann::json body{
                {"count", jobs.size()},
                {"jobs", nlohmann::json::array()}};

            for (const Job &job : jobs)
            {
                body["jobs"].push_back(
                    jobToJson(job));
            }

            response.status = 200;

            response.set_content(
                body.dump(),
                "application/json");
        });

    server_.Post(
        "/jobs",
        [this](
            const httplib::Request &request,
            httplib::Response &response)
        {
            if (!hasJsonContentType(request))
            {
                response.status = 415;

                const nlohmann::json responseBody{
                    {"error",
                     "Content-Type must be application/json"}};

                response.set_content(responseBody.dump(), "application/json");
                return;
            }
            try
            {
                const nlohmann::json body =
                    nlohmann::json::parse(
                        request.body);

                if (!body.is_object())
                {
                    throw std::invalid_argument(
                        "Request body must be a JSON object");
                }

                if (!body.contains("name") ||
                    !body.at("name").is_string())
                {
                    throw std::invalid_argument(
                        "Field 'name' must be a string");
                }

                const std::string name =
                    body.at("name")
                        .get<std::string>();

                if (name.empty())
                {
                    throw std::invalid_argument(
                        "Field 'name' cannot be empty");
                }

                constexpr std::int64_t maximumAttempts =
                    1000;

                constexpr std::int64_t maximumDelayMs =
                    365LL * 24 * 60 * 60 * 1000;

                const std::int64_t maxAttempts =
                    readIntegerField(
                        body,
                        "maxAttempts",
                        1,
                        1,
                        maximumAttempts);

                const std::int64_t delayMs =
                    readIntegerField(
                        body,
                        "delayMs",
                        0,
                        0,
                        maximumDelayMs);

                const std::int64_t retryBackoffMs =
                    readIntegerField(
                        body,
                        "retryBackoffMs",
                        0,
                        0,
                        maximumDelayMs);

                const std::int64_t priority =
                    readIntegerField(
                        body,
                        "priority",
                        0,
                        0,
                        std::numeric_limits<
                            std::uint32_t>::max());

                std::string payload;

                if (body.contains("payload"))
                {
                    if (!body.at("payload").is_string())
                    {
                        throw std::invalid_argument(
                            "Field 'payload' must be a string");
                    }

                    payload =
                        body.at("payload")
                            .get<std::string>();
                }

                Job job;
                job.name = name;
                job.payload = payload;

                job.maxAttempts =
                    static_cast<std::size_t>(
                        maxAttempts);

                job.delay =
                    std::chrono::milliseconds{
                        delayMs};

                job.retryBackoff =
                    std::chrono::milliseconds{
                        retryBackoffMs};

                job.priority =
                    static_cast<std::uint32_t>(
                        priority);

                const std::string id =
                    jobService_.add(
                        std::move(job));

                response.status = 201;

                response.set_header(
                    "Location",
                    "/jobs/" + id);

                const nlohmann::json responseBody{
                    {"id", id}};

                response.set_content(
                    responseBody.dump(),
                    "application/json");
            }
            catch (
                const nlohmann::json::parse_error
                    &error)
            {
                response.status = 400;

                const nlohmann::json responseBody{
                    {"error", "Invalid JSON"},
                    {"details", error.what()}};

                response.set_content(
                    responseBody.dump(),
                    "application/json");
            }
            catch (
                const std::invalid_argument &error)
            {
                response.status = 400;

                const nlohmann::json responseBody{
                    {"error", error.what()}};

                response.set_content(
                    responseBody.dump(),
                    "application/json");
            }
        });
}

void HttpServer::start(
    const std::string &host,
    int port)
{
    if (running_)
    {
        throw std::logic_error("HTTP server is already running");
    }

    if (host.empty())
    {
        throw std::invalid_argument("HTTP host cannot be empty");
    }

    if (port < 1 || port > 65535)
    {
        throw std::invalid_argument("HTTP port must be between 1 and 65535");
    }

    if (!server_.bind_to_port(host, port))
    {
        throw std::runtime_error(
            "Cannot bind HTTP server to " + host + ":" + std::to_string(port));
    }

    try
    {
        serverThread_ = std::thread([this]
                                    { server_.listen_after_bind(); });

        running_ = true;
    }
    catch (...)
    {
        server_.stop();
        throw;
    }
}

void HttpServer::stop()
{
    if (!running_)
    {
        return;
    }

    server_.stop();

    if (serverThread_.joinable())
    {
        serverThread_.join();
    }

    running_ = false;
}