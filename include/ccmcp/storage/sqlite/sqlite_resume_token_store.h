#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/tokenization/resume_token_store.h"

#include <memory>

namespace ccmcp::storage::sqlite {

// SqliteResumeTokenStore implements IResumeTokenStore with SQLite backend.
// Uses prepared statements for all operations.
// Guarantees deterministic ordering (ORDER BY token_ir_id).
class SqliteResumeTokenStore final : public tokenization::IResumeTokenStore {
 public:
  explicit SqliteResumeTokenStore(std::shared_ptr<SqliteDb> db);

  void upsert(const std::string& token_ir_id, const core::ResumeId& resume_id,
              const domain::ResumeTokenIR& token_ir) override;

  [[nodiscard]] std::optional<domain::ResumeTokenIR> get(
      const std::string& token_ir_id) const override;

  [[nodiscard]] std::optional<domain::ResumeTokenIR> get_by_resume(
      const core::ResumeId& resume_id) const override;

  [[nodiscard]] std::vector<domain::ResumeTokenIR> list_all() const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Helper to deserialize token IR from JSON
  [[nodiscard]] domain::ResumeTokenIR row_to_token_ir(sqlite3_stmt* stmt) const;
};

}  // namespace ccmcp::storage::sqlite
