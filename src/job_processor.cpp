#include "job_processor.hpp"

#include <stdexcept>
#include <utility>

void JobProcessor::registerHandler(
    const std::string &name,
    Handler handler)
{
  handlers_[name] = std::move(handler);
}

std::string JobProcessor::process(const Job &job) const
{
  auto handlerIterator = handlers_.find(job.name);

  if (handlerIterator == handlers_.end())
  {
    throw std::runtime_error(
        "No handler registered for job: " + job.name);
  }

  return handlerIterator->second(job);
}