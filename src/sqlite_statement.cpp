#include "sqlite_statement.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

SqliteStatement::SqliteStatement(
    sqlite3 *database,
    const std::string &sql) : database_(database)
{
  const int result = sqlite3_prepare_v2(
      database_,
      sql.c_str(),
      -1,
      &statement_,
      nullptr);

  if (result != SQLITE_OK)
  {
    throw std::runtime_error("Cannot prepare SQL statement: " + std::string(sqlite3_errmsg(database_)));
  }
}

SqliteStatement::~SqliteStatement()
{
  if (statement_ != nullptr)
  {
    sqlite3_finalize(statement_);
  }
}

void SqliteStatement::checkBindingResult(int result) const
{
  if (result != SQLITE_OK)
  {
    throw std::runtime_error("Cannot bind SQLite value: " + std::string(sqlite3_errmsg(database_)));
  }
}

void SqliteStatement::bindText(
    int index,
    const std::string &value)
{
  const int result = sqlite3_bind_text(
      statement_,
      index,
      value.c_str(),
      -1,
      SQLITE_TRANSIENT);

  checkBindingResult(result);
}

void SqliteStatement::bindInt64(
    int index,
    std::int64_t value)
{
  const int result = sqlite3_bind_int64(
      statement_,
      index,
      value);

  checkBindingResult(result);
}

void SqliteStatement::bindNull(int index)
{
  const int result = sqlite3_bind_null(
      statement_,
      index);

  checkBindingResult(result);
}

bool SqliteStatement::step()
{
  const int result =
      sqlite3_step(statement_);

  if (result == SQLITE_ROW)
  {
    return true;
  }

  if (result == SQLITE_DONE)
  {
    return false;
  }

  throw std::runtime_error(
      "Cannot execute SQLite statement: " +
      std::string(sqlite3_errmsg(database_)));
}

bool SqliteStatement::columnIsNull(
    int index) const
{
  return sqlite3_column_type(
             statement_,
             index) == SQLITE_NULL;
}

std::string SqliteStatement::columnText(
    int index) const
{
  const unsigned char *text =
      sqlite3_column_text(
          statement_,
          index);

  if (text == nullptr)
  {
    return {};
  }

  const int size = sqlite3_column_bytes(
      statement_,
      index);

  return std::string(
      reinterpret_cast<const char *>(text),
      static_cast<std::size_t>(size));
}

std::int64_t SqliteStatement::columnInt64(
    int index) const
{
  return static_cast<std::int64_t>(
      sqlite3_column_int64(
          statement_,
          index));
}