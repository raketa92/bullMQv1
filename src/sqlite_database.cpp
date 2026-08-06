#include "sqlite_database.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

SqliteDatabase::SqliteDatabase(const std::string &path)
{
  const int result = sqlite3_open(path.c_str(), &database_);

  if (result != SQLITE_OK)
  {
    const std::string message = database_ != nullptr ? sqlite3_errmsg(database_) : "Unknown SQLite error";

    if (database_ != nullptr)
    {
      sqlite3_close(database_);
      database_ = nullptr;
    }

    throw std::runtime_error("Cannot open SQLite database: " + message);
  }
}

SqliteDatabase::~SqliteDatabase()
{
  if (database_ != nullptr)
  {
    sqlite3_close(database_);
  }
}

sqlite3 *SqliteDatabase::handle() noexcept
{
  return database_;
}

void SqliteDatabase::execute(
    const std::string &sql)
{
  char *errorMessage = nullptr;

  const int result = sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &errorMessage);

  if (result != SQLITE_OK)
  {
    const std::string message = errorMessage != nullptr ? errorMessage : "Unknown SQLite error";

    sqlite3_free(errorMessage);

    throw std::runtime_error("SQLite execution failed: " + message);
  }
}