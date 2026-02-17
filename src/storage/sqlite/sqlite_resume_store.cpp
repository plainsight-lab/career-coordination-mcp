#include "ccmcp/storage/sqlite/sqlite_resume_store.h"

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteResumeStore::SqliteResumeStore(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteResumeStore::upsert(const ingest::IngestedResume& resume) {
  // Begin transaction
  db_->exec("BEGIN TRANSACTION");

  // Upsert resume
  const char* resume_sql = R"(
    INSERT INTO resumes (resume_id, resume_md, resume_hash, created_at)
    VALUES (?, ?, ?, ?)
    ON CONFLICT(resume_id) DO UPDATE SET
      resume_md = excluded.resume_md,
      resume_hash = excluded.resume_hash,
      created_at = excluded.created_at
  )";

  PreparedStatement resume_stmt(db_->connection(), resume_sql);
  if (!resume_stmt.is_valid()) {
    db_->exec("ROLLBACK");
    return;  // Silent failure for upsert
  }

  sqlite3_bind_text(resume_stmt.get(), 1, resume.resume_id.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(resume_stmt.get(), 2, resume.resume_md.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(resume_stmt.get(), 3, resume.resume_hash.c_str(), -1, SQLITE_TRANSIENT);
  std::string created_at = resume.created_at.value_or("CURRENT_TIMESTAMP");
  sqlite3_bind_text(resume_stmt.get(), 4, created_at.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(resume_stmt.get()) != SQLITE_DONE) {
    db_->exec("ROLLBACK");
    return;
  }

  // Upsert resume_meta
  const char* meta_sql = R"(
    INSERT INTO resume_meta (resume_id, source_path, source_hash, extraction_method, extracted_at, ingestion_version)
    VALUES (?, ?, ?, ?, ?, ?)
    ON CONFLICT(resume_id) DO UPDATE SET
      source_path = excluded.source_path,
      source_hash = excluded.source_hash,
      extraction_method = excluded.extraction_method,
      extracted_at = excluded.extracted_at,
      ingestion_version = excluded.ingestion_version
  )";

  PreparedStatement meta_stmt(db_->connection(), meta_sql);
  if (!meta_stmt.is_valid()) {
    db_->exec("ROLLBACK");
    return;
  }

  sqlite3_bind_text(meta_stmt.get(), 1, resume.resume_id.value.c_str(), -1, SQLITE_TRANSIENT);
  if (resume.meta.source_path.has_value()) {
    sqlite3_bind_text(meta_stmt.get(), 2, resume.meta.source_path->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(meta_stmt.get(), 2);
  }
  sqlite3_bind_text(meta_stmt.get(), 3, resume.meta.source_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(meta_stmt.get(), 4, resume.meta.extraction_method.c_str(), -1,
                    SQLITE_TRANSIENT);
  if (resume.meta.extracted_at.has_value()) {
    sqlite3_bind_text(meta_stmt.get(), 5, resume.meta.extracted_at->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(meta_stmt.get(), 5);
  }
  sqlite3_bind_text(meta_stmt.get(), 6, resume.meta.ingestion_version.c_str(), -1,
                    SQLITE_TRANSIENT);

  if (sqlite3_step(meta_stmt.get()) != SQLITE_DONE) {
    db_->exec("ROLLBACK");
    return;
  }

  // Commit transaction
  db_->exec("COMMIT");
}

std::optional<ingest::IngestedResume> SqliteResumeStore::get(const core::ResumeId& id) const {
  const char* sql = "SELECT * FROM resumes WHERE resume_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, id.value.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_resume(stmt.get());
  }

  return std::nullopt;
}

std::optional<ingest::IngestedResume> SqliteResumeStore::get_by_hash(
    const std::string& resume_hash) const {
  const char* sql = "SELECT * FROM resumes WHERE resume_hash = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, resume_hash.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_resume(stmt.get());
  }

  return std::nullopt;
}

std::vector<ingest::IngestedResume> SqliteResumeStore::list_all() const {
  const char* sql = "SELECT * FROM resumes ORDER BY resume_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<ingest::IngestedResume> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_resume(stmt.get()));
  }

  return result;
}

ingest::IngestedResume SqliteResumeStore::row_to_resume(sqlite3_stmt* stmt) const {
  ingest::IngestedResume resume;

  // Extract resume_id from column 0
  const unsigned char* resume_id_text = sqlite3_column_text(stmt, 0);
  resume.resume_id = core::ResumeId{std::string(reinterpret_cast<const char*>(resume_id_text))};

  // Extract resume_md from column 1
  const unsigned char* resume_md_text = sqlite3_column_text(stmt, 1);
  resume.resume_md = std::string(reinterpret_cast<const char*>(resume_md_text));

  // Extract resume_hash from column 2
  const unsigned char* resume_hash_text = sqlite3_column_text(stmt, 2);
  resume.resume_hash = std::string(reinterpret_cast<const char*>(resume_hash_text));

  // Extract created_at from column 3
  const unsigned char* created_at_text = sqlite3_column_text(stmt, 3);
  resume.created_at = std::string(reinterpret_cast<const char*>(created_at_text));

  // Fetch meta separately
  resume.meta = get_meta(resume.resume_id.value);

  return resume;
}

ingest::ResumeMeta SqliteResumeStore::get_meta(const std::string& resume_id) const {
  const char* sql = "SELECT * FROM resume_meta WHERE resume_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  ingest::ResumeMeta meta;

  if (!stmt.is_valid()) {
    return meta;
  }

  sqlite3_bind_text(stmt.get(), 1, resume_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    // Column 0: resume_id (skip, already known)

    // Column 1: source_path (nullable)
    if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
      const unsigned char* source_path_text = sqlite3_column_text(stmt.get(), 1);
      meta.source_path = std::string(reinterpret_cast<const char*>(source_path_text));
    }

    // Column 2: source_hash
    const unsigned char* source_hash_text = sqlite3_column_text(stmt.get(), 2);
    meta.source_hash = std::string(reinterpret_cast<const char*>(source_hash_text));

    // Column 3: extraction_method
    const unsigned char* extraction_method_text = sqlite3_column_text(stmt.get(), 3);
    meta.extraction_method = std::string(reinterpret_cast<const char*>(extraction_method_text));

    // Column 4: extracted_at (nullable)
    if (sqlite3_column_type(stmt.get(), 4) != SQLITE_NULL) {
      const unsigned char* extracted_at_text = sqlite3_column_text(stmt.get(), 4);
      meta.extracted_at = std::string(reinterpret_cast<const char*>(extracted_at_text));
    }

    // Column 5: ingestion_version
    const unsigned char* ingestion_version_text = sqlite3_column_text(stmt.get(), 5);
    meta.ingestion_version = std::string(reinterpret_cast<const char*>(ingestion_version_text));
  }

  return meta;
}

}  // namespace ccmcp::storage::sqlite
