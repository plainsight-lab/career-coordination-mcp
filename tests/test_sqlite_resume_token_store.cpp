#include "ccmcp/core/ids.h"
#include "ccmcp/domain/resume_token_ir.h"
#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/storage/sqlite/sqlite_resume_token_store.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

// Helper to create a test resume
static void create_test_resume(storage::sqlite::SqliteResumeStore& store,
                               const std::string& resume_id_str) {
  ingest::IngestedResume resume;
  resume.resume_id = core::ResumeId{resume_id_str};
  resume.resume_md = "# Test Resume\n\nSample resume text.";
  resume.resume_hash = "hash-" + resume_id_str;
  resume.meta.source_hash = "source-hash-" + resume_id_str;
  resume.meta.extraction_method = "test-v1";
  resume.meta.ingestion_version = "0.3";
  resume.created_at = "2026-01-01T00:00:00Z";
  store.upsert(resume);
}

TEST_CASE("SqliteResumeTokenStore upsert and get", "[storage][sqlite][token-ir]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v3();

  storage::sqlite::SqliteResumeStore resume_store(db);
  storage::sqlite::SqliteResumeTokenStore store(db);

  // Create test resume first (required for FK constraint)
  create_test_resume(resume_store, "resume-1");

  domain::ResumeTokenIR token_ir;
  token_ir.schema_version = "0.3";
  token_ir.source_hash = "hash-123";
  token_ir.tokenizer.type = domain::TokenizerType::kDeterministicLexical;
  token_ir.tokens["skills"] = {"cpp", "python", "rust"};
  token_ir.tokens["domains"] = {"architecture", "systems"};

  const std::string token_ir_id = "test-token-ir-1";
  const core::ResumeId resume_id{"resume-1"};

  SECTION("upsert stores token IR") {
    store.upsert(token_ir_id, resume_id, token_ir);

    auto retrieved_opt = store.get(token_ir_id);
    REQUIRE(retrieved_opt.has_value());

    auto retrieved = retrieved_opt.value();
    REQUIRE(retrieved.source_hash == token_ir.source_hash);
    REQUIRE(retrieved.tokenizer.type == token_ir.tokenizer.type);
    REQUIRE(retrieved.tokens == token_ir.tokens);
  }

  SECTION("get returns nullopt for missing token IR") {
    auto result = store.get("non-existent-id");
    REQUIRE_FALSE(result.has_value());
  }

  SECTION("upsert replaces existing token IR") {
    store.upsert(token_ir_id, resume_id, token_ir);

    // Create completely new token IR
    domain::ResumeTokenIR updated_token_ir;
    updated_token_ir.source_hash = "new-hash";
    updated_token_ir.tokenizer.type = domain::TokenizerType::kInferenceAssisted;
    updated_token_ir.tokens["skills"] = {"java", "kotlin"};
    // Note: no "domains" category in updated version

    store.upsert(token_ir_id, resume_id, updated_token_ir);

    auto retrieved = store.get(token_ir_id).value();
    REQUIRE(retrieved.source_hash == "new-hash");
    REQUIRE(retrieved.tokens["skills"] == updated_token_ir.tokens["skills"]);
    REQUIRE(retrieved.tokens.count("domains") == 0);  // Old data replaced
  }

  SECTION("get_by_resume retrieves by resume ID") {
    store.upsert(token_ir_id, resume_id, token_ir);

    auto retrieved_opt = store.get_by_resume(resume_id);
    REQUIRE(retrieved_opt.has_value());

    auto retrieved = retrieved_opt.value();
    REQUIRE(retrieved.source_hash == token_ir.source_hash);
  }

  SECTION("get_by_resume returns nullopt for missing resume") {
    auto result = store.get_by_resume(core::ResumeId{"non-existent-resume"});
    REQUIRE_FALSE(result.has_value());
  }
}

TEST_CASE("SqliteResumeTokenStore list_all ordering", "[storage][sqlite][token-ir]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v3();

  storage::sqlite::SqliteResumeStore resume_store(db);
  storage::sqlite::SqliteResumeTokenStore store(db);

  // Create test resumes first
  create_test_resume(resume_store, "resume-1");
  create_test_resume(resume_store, "resume-2");
  create_test_resume(resume_store, "resume-3");

  domain::ResumeTokenIR token_ir_1;
  token_ir_1.source_hash = "hash-1";
  token_ir_1.tokens["skills"] = {"cpp"};

  domain::ResumeTokenIR token_ir_2;
  token_ir_2.source_hash = "hash-2";
  token_ir_2.tokens["skills"] = {"python"};

  domain::ResumeTokenIR token_ir_3;
  token_ir_3.source_hash = "hash-3";
  token_ir_3.tokens["skills"] = {"rust"};

  // Insert in non-alphabetical order
  store.upsert("token-c", core::ResumeId{"resume-1"}, token_ir_3);
  store.upsert("token-a", core::ResumeId{"resume-2"}, token_ir_1);
  store.upsert("token-b", core::ResumeId{"resume-3"}, token_ir_2);

  SECTION("list_all returns deterministic order") {
    auto all_1 = store.list_all();
    auto all_2 = store.list_all();

    REQUIRE(all_1.size() == 3);
    REQUIRE(all_2.size() == 3);

    // Should be ordered by token_ir_id: token-a, token-b, token-c
    REQUIRE(all_1[0].source_hash == "hash-1");
    REQUIRE(all_1[1].source_hash == "hash-2");
    REQUIRE(all_1[2].source_hash == "hash-3");

    // Multiple calls produce same order
    for (size_t i = 0; i < all_1.size(); ++i) {
      REQUIRE(all_1[i].source_hash == all_2[i].source_hash);
    }
  }
}

TEST_CASE("SqliteResumeTokenStore preserves token IR structure", "[storage][sqlite][token-ir]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v3();

  storage::sqlite::SqliteResumeStore resume_store(db);
  storage::sqlite::SqliteResumeTokenStore store(db);

  // Create test resumes
  create_test_resume(resume_store, "rid-1");
  create_test_resume(resume_store, "rid-2");
  create_test_resume(resume_store, "rid-3");
  create_test_resume(resume_store, "rid-4");

  SECTION("preserves tokenizer metadata") {
    domain::ResumeTokenIR token_ir;
    token_ir.source_hash = "hash";
    token_ir.tokenizer.type = domain::TokenizerType::kInferenceAssisted;
    token_ir.tokenizer.model_id = "model-123";
    token_ir.tokenizer.prompt_version = "v2";

    store.upsert("tid-1", core::ResumeId{"rid-1"}, token_ir);

    auto retrieved = store.get("tid-1").value();
    REQUIRE(retrieved.tokenizer.type == domain::TokenizerType::kInferenceAssisted);
    REQUIRE(retrieved.tokenizer.model_id.has_value());
    REQUIRE(retrieved.tokenizer.model_id.value() == "model-123");
    REQUIRE(retrieved.tokenizer.prompt_version.has_value());
    REQUIRE(retrieved.tokenizer.prompt_version.value() == "v2");
  }

  SECTION("preserves token spans") {
    domain::ResumeTokenIR token_ir;
    token_ir.source_hash = "hash";
    token_ir.spans = {{"token1", 1, 2}, {"token2", 3, 5}, {"token3", 10, 10}};

    store.upsert("tid-2", core::ResumeId{"rid-2"}, token_ir);

    auto retrieved = store.get("tid-2").value();
    REQUIRE(retrieved.spans.size() == 3);
    REQUIRE(retrieved.spans[0].token == "token1");
    REQUIRE(retrieved.spans[0].start_line == 1);
    REQUIRE(retrieved.spans[0].end_line == 2);
    REQUIRE(retrieved.spans[2].token == "token3");
    REQUIRE(retrieved.spans[2].start_line == 10);
  }

  SECTION("preserves multiple token categories") {
    domain::ResumeTokenIR token_ir;
    token_ir.source_hash = "hash";
    token_ir.tokens["skills"] = {"cpp", "python", "rust"};
    token_ir.tokens["domains"] = {"architecture", "distributed", "systems"};
    token_ir.tokens["entities"] = {"google", "microsoft"};
    token_ir.tokens["roles"] = {"engineer", "architect"};

    store.upsert("tid-3", core::ResumeId{"rid-3"}, token_ir);

    auto retrieved = store.get("tid-3").value();
    REQUIRE(retrieved.tokens.size() == 4);
    REQUIRE(retrieved.tokens["skills"] == token_ir.tokens["skills"]);
    REQUIRE(retrieved.tokens["domains"] == token_ir.tokens["domains"]);
    REQUIRE(retrieved.tokens["entities"] == token_ir.tokens["entities"]);
    REQUIRE(retrieved.tokens["roles"] == token_ir.tokens["roles"]);
  }

  SECTION("handles empty token IR") {
    domain::ResumeTokenIR token_ir;
    token_ir.source_hash = "hash";
    // No tokens, no spans

    store.upsert("tid-4", core::ResumeId{"rid-4"}, token_ir);

    auto retrieved = store.get("tid-4").value();
    REQUIRE(retrieved.tokens.empty());
    REQUIRE(retrieved.spans.empty());
  }
}

TEST_CASE("SqliteResumeTokenStore JSON stability", "[storage][sqlite][token-ir]") {
  auto db_result = storage::sqlite::SqliteDb::open(":memory:");
  REQUIRE(db_result.has_value());
  auto db = db_result.value();
  db->ensure_schema_v3();

  storage::sqlite::SqliteResumeStore resume_store(db);
  storage::sqlite::SqliteResumeTokenStore store(db);

  // Create test resume
  create_test_resume(resume_store, "rid");

  domain::ResumeTokenIR token_ir;
  token_ir.source_hash = "hash";
  token_ir.tokens["skills"] = {"cpp", "python"};

  SECTION("round-trip preserves data exactly") {
    store.upsert("tid", core::ResumeId{"rid"}, token_ir);
    auto retrieved_1 = store.get("tid").value();

    // Store the retrieved version again
    store.upsert("tid", core::ResumeId{"rid"}, retrieved_1);
    auto retrieved_2 = store.get("tid").value();

    // Should be identical
    REQUIRE(retrieved_1.source_hash == retrieved_2.source_hash);
    REQUIRE(retrieved_1.tokens == retrieved_2.tokens);
    REQUIRE(retrieved_1.tokenizer.type == retrieved_2.tokenizer.type);
  }
}
