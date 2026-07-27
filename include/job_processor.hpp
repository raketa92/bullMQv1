#pragma once

#include "job_queue.hpp"

#include <functional>
#include <string>
#include <unordered_map>

class JobProcessor
{
public:
  using Handler = std::function<void(const Job &)>;

  void registerHandler(
      const std::string &name,
      Handler handler);

  void process(const Job &job) const;

private:
  std::unordered_map<std::string, Handler> handlers_;
};