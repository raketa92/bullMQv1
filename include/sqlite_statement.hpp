#pragma once

#include <cstdint>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

class SqliteStatement
{
public:
  SqliteStatement(sqlite3 *database, const std::string &sql);
  ~SqliteStatement();
  SqliteStatement(const SqliteStatement &) = delete;
  SqliteStatement &operator=(const SqliteStatement &) = delete;

  void bindText(int index, const std::string &value);
  void bindInt64(int index, std::int64_t value);
  void bindNull(int index);
  bool step();
  bool columnIsNull(int index) const;
  std::string columnText(int index) const;
  std::int64_t columnInt64(int index) const;

private:
  void checkBindingResult(int result) const;

  sqlite3 *database_;
  sqlite3_stmt *statement_ = nullptr;
};