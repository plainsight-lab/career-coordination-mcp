#pragma once

#ifdef CCMCP_TRANSPORT_BOUNDARY_GUARD
#error "Concrete storage/redis header included in a guarded translation unit — use interfaces only."
#endif

#include "ccmcp/storage/repositories.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"

#include <memory>

namespace ccmcp::storage::sqlite {

// SqliteAtomRepository implements IAtomRepository with SQLite backend.
// Uses prepared statements for all operations.
// Guarantees deterministic ordering (ORDER BY atom_id).
class SqliteAtomRepository final : public IAtomRepository {
 public:
  explicit SqliteAtomRepository(std::shared_ptr<SqliteDb> db);

  void upsert(const domain::ExperienceAtom& atom) override;
  [[nodiscard]] std::optional<domain::ExperienceAtom> get(const core::AtomId& id) const override;
  [[nodiscard]] std::vector<domain::ExperienceAtom> list_verified() const override;
  [[nodiscard]] std::vector<domain::ExperienceAtom> list_all() const override;

 private:
  std::shared_ptr<SqliteDb> db_;

  // Helper to deserialize atom from prepared statement row
  [[nodiscard]] domain::ExperienceAtom row_to_atom(sqlite3_stmt* stmt) const;
};

}  // namespace ccmcp::storage::sqlite
