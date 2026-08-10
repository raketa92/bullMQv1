#pragma once

#include <httplib.h>
#include <string>
#include <thread>

class JobStore;
class JobService;

class HttpServer
{
public:
  explicit HttpServer(JobStore &store, JobService &jobService);
  ~HttpServer();

  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(
      const HttpServer &) = delete;

  void start(
      const std::string &host,
      int port);

  void stop();

private:
  void registerRoutes();

  JobStore &store_;
  JobService &jobService_;
  httplib::Server server_;
  std::thread serverThread_;
  bool running_ = false;
};