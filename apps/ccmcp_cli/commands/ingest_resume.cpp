#include "ingest_resume.h"

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"

#include "ingest_resume_logic.h"
#include "shared/arg_parser.h"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

struct IngestConfig {
  std::optional<std::string> db_path;
};

}  // namespace

int cmd_ingest_resume(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  if (argc < 3) {
    std::cerr << "Usage: ccmcp_cli ingest-resume <file-path> [--db <db-path>]\n";
    return 1;
  }

  const std::string file_path = argv[2];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  const std::vector<ccmcp::apps::Option<IngestConfig>> options = {
      {"--db", true, "Path to SQLite database file",
       [](IngestConfig& c, const std::string& v) {
         c.db_path = v;
         return true;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 3);

  const std::string db_path = config.db_path.value_or("data/ccmcp.db");
  if (!config.db_path.has_value()) {
    std::cout << "No --db specified, using default: " << db_path << "\n";
  }

  auto db_result = ccmcp::storage::sqlite::SqliteDb::open(db_path);
  if (!db_result.has_value()) {
    std::cerr << "Failed to open database: " << db_result.error() << "\n";
    return 1;
  }

  auto db = db_result.value();
  auto schema_result = db->ensure_schema_v2();
  if (!schema_result.has_value()) {
    std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
    return 1;
  }

  auto ingestor = ccmcp::ingest::create_resume_ingestor();
  ccmcp::storage::sqlite::SqliteResumeStore resume_store(db);
  ccmcp::core::DeterministicIdGenerator id_gen;
  ccmcp::core::SystemClock clock;

  return execute_ingest_resume(file_path, *ingestor, resume_store, id_gen, clock);
}
