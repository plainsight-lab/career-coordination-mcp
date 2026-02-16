#pragma once

#include "ccmcp/core/result.h"

#include <memory>
#include <string>

// Forward declare sqlite3 to avoid exposing SQLite header in public API
struct sqlite3;
struct sqlite3_stmt;

namespace ccmcp::storage::sqlite {

// SqliteDb manages a SQLite database connection and schema versioning.
// Responsibilities:
// - Open/close database connection
// - Initialize schema (migrations)
// - Provide prepared statement helpers
// - Enable foreign keys
//
// Design principles:
// - RAII: connection managed via unique_ptr with custom deleter
// - Explicit error handling via Result<T,E>
// - Thread-safe: one connection per instance (no sharing)
class SqliteDb {
 public:
  // Open or create database at path.
  // If path is ":memory:", creates in-memory database.
  [[nodiscard]] static core::Result<std::shared_ptr<SqliteDb>, std::string> open(
      const std::string& path);

  ~SqliteDb() = default;

  // Disable copy/move (unique ownership)
  SqliteDb(const SqliteDb&) = delete;
  SqliteDb& operator=(const SqliteDb&) = delete;
  SqliteDb(SqliteDb&&) = delete;
  SqliteDb& operator=(SqliteDb&&) = delete;

  // Get current schema version (0 if no schema applied)
  [[nodiscard]] int get_schema_version() const;

  // Apply schema v1 if not already applied
  [[nodiscard]] core::Result<bool, std::string> ensure_schema_v1();

  // Execute SQL statement (for non-query operations)
  [[nodiscard]] core::Result<bool, std::string> exec(const std::string& sql);

  // Get raw connection (for prepared statements)
  // Should be used only by repository implementations
  [[nodiscard]] sqlite3* connection() const { return db_.get(); }

 private:
  struct SqliteDeleter {
    void operator()(sqlite3* db) const;
  };

  explicit SqliteDb(sqlite3* db);

  std::unique_ptr<sqlite3, SqliteDeleter> db_;
};

// RAII wrapper for prepared statements
class PreparedStatement {
 public:
  PreparedStatement(sqlite3* db, const std::string& sql);
  ~PreparedStatement() = default;

  PreparedStatement(const PreparedStatement&) = delete;
  PreparedStatement& operator=(const PreparedStatement&) = delete;
  PreparedStatement(PreparedStatement&&) = delete;
  PreparedStatement& operator=(PreparedStatement&&) = delete;

  // Returns true if statement was prepared successfully
  [[nodiscard]] bool is_valid() const { return stmt_ != nullptr; }

  // Get error message if preparation failed
  [[nodiscard]] std::string error() const { return error_; }

  // Get raw statement (for binding/stepping)
  [[nodiscard]] sqlite3_stmt* get() const { return stmt_.get(); }

  // Reset statement for reuse
  void reset();

 private:
  struct StmtDeleter {
    void operator()(sqlite3_stmt* stmt) const;
  };

  std::unique_ptr<sqlite3_stmt, StmtDeleter> stmt_;
  std::string error_;
};

}  // namespace ccmcp::storage::sqlite
