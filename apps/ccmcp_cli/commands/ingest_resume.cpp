#include "ingest_resume.h"

#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/ingest/resume_ingestor.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"

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

  std::cout << "Ingesting resume from: " << file_path << "\n";
  ccmcp::ingest::IngestOptions ingest_options;
  auto result = ingestor->ingest_file(file_path, ingest_options, id_gen, clock);

  if (!result.has_value()) {
    std::cerr << "Ingestion failed: " << result.error() << "\n";
    return 1;
  }

  const auto& ingested_resume = result.value();
  resume_store.upsert(ingested_resume);

  std::cout << "Success!\n";
  std::cout << "  Resume ID: " << ingested_resume.resume_id.value << "\n";
  std::cout << "  Resume hash: " << ingested_resume.resume_hash << "\n";
  std::cout << "  Extraction method: " << ingested_resume.meta.extraction_method << "\n";
  std::cout << "  Ingestion version: " << ingested_resume.meta.ingestion_version << "\n";
  if (ingested_resume.meta.source_path.has_value()) {
    std::cout << "  Source path: " << ingested_resume.meta.source_path.value() << "\n";
  }
  std::cout << "  Resume content length: " << ingested_resume.resume_md.size() << " bytes\n";

  return 0;
}
