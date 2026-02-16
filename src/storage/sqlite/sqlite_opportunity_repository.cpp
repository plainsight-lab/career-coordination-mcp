#include "ccmcp/storage/sqlite/sqlite_opportunity_repository.h"

#include <nlohmann/json.hpp>

#include <sqlite3.h>

namespace ccmcp::storage::sqlite {

SqliteOpportunityRepository::SqliteOpportunityRepository(std::shared_ptr<SqliteDb> db)
    : db_(std::move(db)) {}

void SqliteOpportunityRepository::upsert(const domain::Opportunity& opportunity) {
  // Begin transaction for atomic upsert
  db_->exec("BEGIN TRANSACTION");

  // Upsert opportunity
  const char* opp_sql = R"(
    INSERT INTO opportunities (opportunity_id, company, role_title, source)
    VALUES (?, ?, ?, ?)
    ON CONFLICT(opportunity_id) DO UPDATE SET
      company = excluded.company,
      role_title = excluded.role_title,
      source = excluded.source
  )";

  PreparedStatement opp_stmt(db_->connection(), opp_sql);
  if (!opp_stmt.is_valid()) {
    db_->exec("ROLLBACK");
    return;
  }

  sqlite3_bind_text(opp_stmt.get(), 1, opportunity.opportunity_id.value.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(opp_stmt.get(), 2, opportunity.company.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(opp_stmt.get(), 3, opportunity.role_title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(opp_stmt.get(), 4, opportunity.source.c_str(), -1, SQLITE_TRANSIENT);

  sqlite3_step(opp_stmt.get());

  // Delete old requirements
  const char* del_sql = "DELETE FROM requirements WHERE opportunity_id = ?";
  PreparedStatement del_stmt(db_->connection(), del_sql);
  if (!del_stmt.is_valid()) {
    db_->exec("ROLLBACK");
    return;
  }

  sqlite3_bind_text(del_stmt.get(), 1, opportunity.opportunity_id.value.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_step(del_stmt.get());

  // Insert new requirements
  const char* req_sql = R"(
    INSERT INTO requirements (opportunity_id, idx, text, tags_json, required)
    VALUES (?, ?, ?, ?, ?)
  )";

  PreparedStatement req_stmt(db_->connection(), req_sql);
  if (!req_stmt.is_valid()) {
    db_->exec("ROLLBACK");
    return;
  }

  for (size_t i = 0; i < opportunity.requirements.size(); ++i) {
    const auto& req = opportunity.requirements[i];
    nlohmann::json tags_json = req.tags;

    sqlite3_bind_text(req_stmt.get(), 1, opportunity.opportunity_id.value.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(req_stmt.get(), 2, static_cast<int>(i));
    sqlite3_bind_text(req_stmt.get(), 3, req.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(req_stmt.get(), 4, tags_json.dump().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(req_stmt.get(), 5, req.required ? 1 : 0);

    sqlite3_step(req_stmt.get());
    req_stmt.reset();
  }

  db_->exec("COMMIT");
}

std::optional<domain::Opportunity> SqliteOpportunityRepository::get(
    const core::OpportunityId& id) const {
  const char* sql = "SELECT * FROM opportunities WHERE opportunity_id = ?";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt.get(), 1, id.value.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Opportunity opp;
    opp.opportunity_id =
        core::OpportunityId{reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0))};
    opp.company = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    opp.role_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
    opp.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));

    opp.requirements = load_requirements(id);

    return opp;
  }

  return std::nullopt;
}

std::vector<domain::Opportunity> SqliteOpportunityRepository::list_all() const {
  const char* sql = "SELECT * FROM opportunities ORDER BY opportunity_id";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  std::vector<domain::Opportunity> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Opportunity opp;
    opp.opportunity_id =
        core::OpportunityId{reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0))};
    opp.company = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    opp.role_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
    opp.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));

    opp.requirements = load_requirements(opp.opportunity_id);

    result.push_back(opp);
  }

  return result;
}

std::vector<domain::Requirement> SqliteOpportunityRepository::load_requirements(
    const core::OpportunityId& id) const {
  const char* sql =
      "SELECT text, tags_json, required FROM requirements WHERE opportunity_id = ? ORDER BY idx";

  PreparedStatement stmt(db_->connection(), sql);
  if (!stmt.is_valid()) {
    return {};
  }

  sqlite3_bind_text(stmt.get(), 1, id.value.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<domain::Requirement> requirements;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Requirement req;
    req.text = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));

    std::string tags_json_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    nlohmann::json tags_json = nlohmann::json::parse(tags_json_str);
    req.tags = tags_json.get<std::vector<std::string>>();

    req.required = sqlite3_column_int(stmt.get(), 2) != 0;

    requirements.push_back(req);
  }

  return requirements;
}

}  // namespace ccmcp::storage::sqlite
