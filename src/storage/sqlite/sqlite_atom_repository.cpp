#include "ccmcp/storage/sqlite/sqlite_atom_repository.h"

#include <nlohmann/json.hpp>

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteAtomRepository::SqliteAtomRepository(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

void SqliteAtomRepository::upsert(const domain::ExperienceAtom& atom) {
  // Serialize tags and evidence_refs to JSON (deterministic sort)
  nlohmann::json tags_json = atom.tags;
  nlohmann::json evidence_json = atom.evidence_refs;

  const char* sql = R"(
    INSERT INTO atoms (atom_id, domain, title, claim, tags_json, verified, evidence_refs_json)
    VALUES (?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(atom_id) DO UPDATE SET
      domain = excluded.domain,
      title = excluded.title,
      claim = excluded.claim,
      tags_json = excluded.tags_json,
      verified = excluded.verified,
      evidence_refs_json = excluded.evidence_refs_json
  )";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return;  // Silent failure for upsert (matches in-memory semantics)
  }

  sqlite3_bind_text(stmt.get(), 1, atom.atom_id.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, atom.domain.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, atom.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, atom.claim.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, tags_json.dump().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 6, atom.verified ? 1 : 0);
  sqlite3_bind_text(stmt.get(), 7, evidence_json.dump().c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_step(stmt.get());
}

std::optional<domain::ExperienceAtom> SqliteAtomRepository::get(const core::AtomId& id) const {
  const char* sql = "SELECT * FROM atoms WHERE atom_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, id.value.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    return row_to_atom(stmt.get());
  }

  return std::nullopt;
}

std::vector<domain::ExperienceAtom> SqliteAtomRepository::list_verified() const {
  const char* sql = "SELECT * FROM atoms WHERE verified = 1 ORDER BY atom_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<domain::ExperienceAtom> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_atom(stmt.get()));
  }

  return result;
}

std::vector<domain::ExperienceAtom> SqliteAtomRepository::list_all() const {
  const char* sql = "SELECT * FROM atoms ORDER BY atom_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<domain::ExperienceAtom> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    result.push_back(row_to_atom(stmt.get()));
  }

  return result;
}

domain::ExperienceAtom SqliteAtomRepository::row_to_atom(sqlite3_stmt* stmt) const {
  domain::ExperienceAtom atom;

  atom.atom_id = core::AtomId{reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))};
  atom.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  atom.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  atom.claim = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

  // Deserialize tags JSON
  std::string tags_json_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  nlohmann::json tags_json = nlohmann::json::parse(tags_json_str);
  atom.tags = tags_json.get<std::vector<std::string>>();

  atom.verified = sqlite3_column_int(stmt, 5) != 0;

  // Deserialize evidence_refs JSON
  std::string evidence_json_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  nlohmann::json evidence_json = nlohmann::json::parse(evidence_json_str);
  atom.evidence_refs = evidence_json.get<std::vector<std::string>>();

  return atom;
}

}  // namespace ccmcp::storage::sqlite
