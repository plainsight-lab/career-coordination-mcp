#include "ccmcp/vector/sqlite_embedding_index.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>

namespace ccmcp::vector {

namespace {

constexpr const char* kSchema = R"(
CREATE TABLE IF NOT EXISTS embedding_vectors (
  key          TEXT PRIMARY KEY,
  vector_blob  BLOB NOT NULL,
  dimension    INTEGER NOT NULL,
  metadata_json TEXT NOT NULL,
  created_at   TEXT NOT NULL DEFAULT (datetime('now'))
);
)";

// RAII guard for prepared statements, local to this translation unit.
struct StmtGuard {
  sqlite3_stmt* stmt = nullptr;
  StmtGuard() = default;
  ~StmtGuard() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
    }
  }
  StmtGuard(const StmtGuard&) = delete;
  StmtGuard& operator=(const StmtGuard&) = delete;
};

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// DbDeleter
// ─────────────────────────────────────────────────────────────────────────────

void SqliteEmbeddingIndex::DbDeleter::operator()(sqlite3* db) const {
  if (db != nullptr) {
    sqlite3_close(db);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

SqliteEmbeddingIndex::SqliteEmbeddingIndex(const std::string& db_path) {
  sqlite3* raw_db = nullptr;
  int rc = sqlite3_open(db_path.c_str(), &raw_db);
  if (rc != SQLITE_OK) {
    std::string err = (raw_db != nullptr) ? sqlite3_errmsg(raw_db) : "unknown error";
    sqlite3_close(raw_db);
    throw std::runtime_error("SqliteEmbeddingIndex: cannot open '" + db_path + "': " + err);
  }
  db_.reset(raw_db);
  ensure_schema();
}

// Destructor must be defined here so the sqlite3 deleter runs where sqlite3 is a complete type.
SqliteEmbeddingIndex::~SqliteEmbeddingIndex() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Schema
// ─────────────────────────────────────────────────────────────────────────────

void SqliteEmbeddingIndex::ensure_schema() {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db_.get(), kSchema, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string err = (err_msg != nullptr) ? err_msg : "unknown error";
    sqlite3_free(err_msg);
    throw std::runtime_error("SqliteEmbeddingIndex: schema setup failed: " + err);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// IEmbeddingIndex interface
// ─────────────────────────────────────────────────────────────────────────────

void SqliteEmbeddingIndex::upsert(const VectorKey& key, const Vector& embedding,
                                  const std::string& metadata) {
  constexpr const char* sql = R"(
    INSERT INTO embedding_vectors (key, vector_blob, dimension, metadata_json)
    VALUES (?, ?, ?, ?)
    ON CONFLICT(key) DO UPDATE SET
      vector_blob   = excluded.vector_blob,
      dimension     = excluded.dimension,
      metadata_json = excluded.metadata_json
  )";

  StmtGuard guard;
  int rc = sqlite3_prepare_v2(db_.get(), sql, -1, &guard.stmt, nullptr);
  if (rc != SQLITE_OK) {
    return;  // Silent failure for upsert (matches in-memory semantics)
  }

  auto blob = to_blob(embedding);
  const int dim = static_cast<int>(embedding.size());

  sqlite3_bind_text(guard.stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(guard.stmt, 2, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
  sqlite3_bind_int(guard.stmt, 3, dim);
  sqlite3_bind_text(guard.stmt, 4, metadata.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_step(guard.stmt);
}

std::vector<VectorSearchResult> SqliteEmbeddingIndex::query(const Vector& query_vector,
                                                            size_t top_k) const {
  // Load all rows ordered by key to ensure a deterministic baseline before sorting.
  // The final ordering is by score (desc) then key (asc), applied in-process below.
  constexpr const char* sql =
      "SELECT key, vector_blob, metadata_json FROM embedding_vectors ORDER BY key";

  StmtGuard guard;
  int rc = sqlite3_prepare_v2(db_.get(), sql, -1, &guard.stmt, nullptr);
  if (rc != SQLITE_OK) {
    return {};
  }

  std::vector<VectorSearchResult> results;
  while (sqlite3_step(guard.stmt) == SQLITE_ROW) {
    const char* raw_key = reinterpret_cast<const char*>(sqlite3_column_text(guard.stmt, 0));
    const void* blob_data = sqlite3_column_blob(guard.stmt, 1);
    const int blob_size = sqlite3_column_bytes(guard.stmt, 1);
    const char* raw_meta = reinterpret_cast<const char*>(sqlite3_column_text(guard.stmt, 2));

    Vector stored = from_blob(blob_data, blob_size);
    double score = cosine_similarity(query_vector, stored);

    results.push_back(VectorSearchResult{
        .key = raw_key != nullptr ? std::string(raw_key) : std::string{},
        .score = score,
        .metadata = raw_meta != nullptr ? std::string(raw_meta) : std::string{},
    });
  }

  // Sort: score descending, tie-break by key ascending (lexicographic).
  // Epsilon 1e-9 matches InMemoryEmbeddingIndex exactly.
  std::sort(results.begin(), results.end(),
            [](const VectorSearchResult& a, const VectorSearchResult& b) {
              constexpr double kEpsilon = 1e-9;
              if (std::abs(a.score - b.score) > kEpsilon) {
                return a.score > b.score;
              }
              return a.key < b.key;
            });

  if (results.size() > top_k) {
    results.resize(top_k);
  }

  return results;
}

std::optional<Vector> SqliteEmbeddingIndex::get(const VectorKey& key) const {
  constexpr const char* sql = "SELECT vector_blob FROM embedding_vectors WHERE key = ?";

  StmtGuard guard;
  int rc = sqlite3_prepare_v2(db_.get(), sql, -1, &guard.stmt, nullptr);
  if (rc != SQLITE_OK) {
    return std::nullopt;
  }

  sqlite3_bind_text(guard.stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(guard.stmt) == SQLITE_ROW) {
    const void* blob_data = sqlite3_column_blob(guard.stmt, 0);
    const int blob_size = sqlite3_column_bytes(guard.stmt, 0);
    return from_blob(blob_data, blob_size);
  }

  return std::nullopt;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

double SqliteEmbeddingIndex::cosine_similarity(const Vector& a, const Vector& b) {
  if (a.size() != b.size() || a.empty()) {
    return 0.0;
  }

  double dot_product = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;

  for (size_t i = 0; i < a.size(); ++i) {
    dot_product += static_cast<double>(a[i]) * static_cast<double>(b[i]);
    norm_a += static_cast<double>(a[i]) * static_cast<double>(a[i]);
    norm_b += static_cast<double>(b[i]) * static_cast<double>(b[i]);
  }

  norm_a = std::sqrt(norm_a);
  norm_b = std::sqrt(norm_b);

  if (norm_a == 0.0 || norm_b == 0.0) {
    return 0.0;
  }

  return dot_product / (norm_a * norm_b);
}

std::vector<std::byte> SqliteEmbeddingIndex::to_blob(const Vector& v) {
  std::vector<std::byte> blob(v.size() * sizeof(float));
  std::memcpy(blob.data(), v.data(), blob.size());
  return blob;
}

Vector SqliteEmbeddingIndex::from_blob(const void* data, int size_bytes) {
  if (data == nullptr || size_bytes <= 0) {
    return {};
  }
  const size_t n = static_cast<size_t>(size_bytes) / sizeof(float);
  Vector result(n);
  std::memcpy(result.data(), data, static_cast<size_t>(size_bytes));
  return result;
}

}  // namespace ccmcp::vector
