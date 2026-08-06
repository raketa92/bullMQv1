#pragma once

#include <string>

struct sqlite3;

class SqliteDatabase
{
public:
  explicit SqliteDatabase(const std::string &path);
  ~SqliteDatabase();
  SqliteDatabase(const SqliteDatabase &) = delete;
  SqliteDatabase &operator=(const SqliteDatabase &) = delete;

  sqlite3 *handle() noexcept;

  void execute(const std::string &sql);

private:
  sqlite3 *database_ = nullptr;
};