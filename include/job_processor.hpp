#pragma once

#include "job_queue.hpp"

#include <functional>
#include <string>
#include <unordered_map>

class JobProcessor
{
public:
  using Handler = std::function<std::string(const Job &)>;

  void registerHandler(
      const std::string &name,
      Handler handler);

  std::string process(const Job &job) const;

private:
  std::unordered_map<std::string, Handler> handlers_;
};