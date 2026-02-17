#include "ccmcp/core/ids.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;
using namespace ccmcp::storage::sqlite;
using namespace ccmcp::ingest;

TEST_CASE("SqliteResumeStore upsert and get", "[storage][sqlite][resume_store]") {
  auto db_result = SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());

  auto db = db_result.value();
  db->ensure_schema_v2();

  SqliteResumeStore store(db);

  // Create test resume
  IngestedResume resume;
  resume.resume_id = core::ResumeId{"resume-123"};
  resume.resume_md = "# John Doe\n\n## Experience\n- Tech Corp";
  resume.resume_hash = "sha256:abc123";
  resume.meta.source_path = "/path/to/resume.md";
  resume.meta.source_hash = "sha256:def456";
  resume.meta.extraction_method = "md-pass-through-v1";
  resume.meta.extracted_at = "2026-01-01T00:00:00Z";
  resume.meta.ingestion_version = "0.3";
  resume.created_at = "2026-01-01T00:00:00Z";

  // Upsert
  store.upsert(resume);

  // Get
  auto retrieved = store.get(core::ResumeId{"resume-123"});
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->resume_id.value == "resume-123");
  REQUIRE(retrieved->resume_md == resume.resume_md);
  REQUIRE(retrieved->resume_hash == resume.resume_hash);
  REQUIRE(retrieved->meta.source_path == resume.meta.source_path);
  REQUIRE(retrieved->meta.source_hash == resume.meta.source_hash);
  REQUIRE(retrieved->meta.extraction_method == resume.meta.extraction_method);
  REQUIRE(retrieved->meta.ingestion_version == resume.meta.ingestion_version);
}

TEST_CASE("SqliteResumeStore get_by_hash", "[storage][sqlite][resume_store]") {
  auto db_result = SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());

  auto db = db_result.value();
  db->ensure_schema_v2();

  SqliteResumeStore store(db);

  IngestedResume resume;
  resume.resume_id = core::ResumeId{"resume-456"};
  resume.resume_md = "# Jane Smith";
  resume.resume_hash = "sha256:xyz789";
  resume.meta.source_hash = "sha256:source123";
  resume.meta.extraction_method = "md-pass-through-v1";
  resume.meta.ingestion_version = "0.3";
  resume.created_at = "2026-01-01T00:00:00Z";

  store.upsert(resume);

  // Get by hash
  auto retrieved = store.get_by_hash("sha256:xyz789");
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->resume_id.value == "resume-456");
  REQUIRE(retrieved->resume_hash == "sha256:xyz789");
}

TEST_CASE("SqliteResumeStore upsert replaces existing", "[storage][sqlite][resume_store]") {
  auto db_result = SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());

  auto db = db_result.value();
  db->ensure_schema_v2();

  SqliteResumeStore store(db);

  // First version
  IngestedResume resume1;
  resume1.resume_id = core::ResumeId{"resume-update"};
  resume1.resume_md = "# Version 1";
  resume1.resume_hash = "sha256:hash1";
  resume1.meta.source_hash = "sha256:source1";
  resume1.meta.extraction_method = "md-pass-through-v1";
  resume1.meta.ingestion_version = "0.3";
  resume1.created_at = "2026-01-01T00:00:00Z";

  store.upsert(resume1);

  // Updated version
  IngestedResume resume2;
  resume2.resume_id = core::ResumeId{"resume-update"};
  resume2.resume_md = "# Version 2";
  resume2.resume_hash = "sha256:hash2";
  resume2.meta.source_hash = "sha256:source2";
  resume2.meta.extraction_method = "txt-wrap-v1";
  resume2.meta.ingestion_version = "0.3";
  resume2.created_at = "2026-01-02T00:00:00Z";

  store.upsert(resume2);

  // Should have replaced
  auto retrieved = store.get(core::ResumeId{"resume-update"});
  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->resume_md == "# Version 2");
  REQUIRE(retrieved->resume_hash == "sha256:hash2");
  REQUIRE(retrieved->meta.extraction_method == "txt-wrap-v1");
}

TEST_CASE("SqliteResumeStore list_all orders deterministically",
          "[storage][sqlite][resume_store]") {
  auto db_result = SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());

  auto db = db_result.value();
  db->ensure_schema_v2();

  SqliteResumeStore store(db);

  // Insert in non-sorted order
  for (const auto& id : {"resume-c", "resume-a", "resume-b"}) {
    IngestedResume resume;
    resume.resume_id = core::ResumeId{id};
    resume.resume_md = "# Resume";
    resume.resume_hash = std::string("sha256:hash-") + id;
    resume.meta.source_hash = "sha256:source";
    resume.meta.extraction_method = "md-pass-through-v1";
    resume.meta.ingestion_version = "0.3";
    resume.created_at = "2026-01-01T00:00:00Z";
    store.upsert(resume);
  }

  auto all_resumes = store.list_all();
  REQUIRE(all_resumes.size() == 3);

  // Should be sorted by resume_id
  REQUIRE(all_resumes[0].resume_id.value == "resume-a");
  REQUIRE(all_resumes[1].resume_id.value == "resume-b");
  REQUIRE(all_resumes[2].resume_id.value == "resume-c");
}

TEST_CASE("SqliteResumeStore get returns nullopt for missing resume",
          "[storage][sqlite][resume_store]") {
  auto db_result = SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());

  auto db = db_result.value();
  db->ensure_schema_v2();

  SqliteResumeStore store(db);

  auto retrieved = store.get(core::ResumeId{"nonexistent"});
  REQUIRE(!retrieved.has_value());
}

TEST_CASE("SqliteResumeStore handles nullable source_path", "[storage][sqlite][resume_store]") {
  auto db_result = SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());

  auto db = db_result.value();
  db->ensure_schema_v2();

  SqliteResumeStore store(db);

  IngestedResume resume;
  resume.resume_id = core::ResumeId{"resume-no-path"};
  resume.resume_md = "# Resume";
  resume.resume_hash = "sha256:hash";
  resume.meta.source_path = std::nullopt;  // No source path
  resume.meta.source_hash = "sha256:source";
  resume.meta.extraction_method = "md-pass-through-v1";
  resume.meta.ingestion_version = "0.3";
  resume.created_at = "2026-01-01T00:00:00Z";

  store.upsert(resume);

  auto retrieved = store.get(core::ResumeId{"resume-no-path"});
  REQUIRE(retrieved.has_value());
  REQUIRE(!retrieved->meta.source_path.has_value());
}
