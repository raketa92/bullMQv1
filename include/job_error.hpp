#pragma once

#include <stdexcept>
#include <string>

class RetryableJobError : public std::runtime_error
{
public:
  explicit RetryableJobError(const std::string &message)
      : std::runtime_error(message)
  {
  }
};