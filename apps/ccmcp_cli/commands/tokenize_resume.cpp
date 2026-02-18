#include "tokenize_resume.h"

#include "ccmcp/core/ids.h"
#include "ccmcp/storage/sqlite/sqlite_db.h"
#include "ccmcp/storage/sqlite/sqlite_resume_store.h"
#include "ccmcp/storage/sqlite/sqlite_resume_token_store.h"
#include "ccmcp/tokenization/deterministic_lexical_tokenizer.h"
#include "ccmcp/tokenization/stub_inference_tokenizer.h"

#include "shared/arg_parser.h"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

struct TokenizeCliConfig {
  std::optional<std::string> db_path;
  std::string mode{"deterministic"};
  bool args_valid{true};
};

}  // namespace

int cmd_tokenize_resume(int argc, char* argv[]) {  // NOLINT(modernize-avoid-c-arrays)
  if (argc < 3) {
    std::cerr << "Usage: ccmcp_cli tokenize-resume <resume-id> [--db <db-path>] "
                 "[--mode <deterministic|stub-inference>]\n";
    return 1;
  }

  const std::string resume_id_str =
      argv[2];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  const std::vector<ccmcp::apps::Option<TokenizeCliConfig>> options = {
      {"--db", true, "Path to SQLite database file",
       [](TokenizeCliConfig& c, const std::string& v) {
         c.db_path = v;
         return true;
       }},
      {"--mode", true, "Tokenizer mode (deterministic|stub-inference)",
       [](TokenizeCliConfig& c, const std::string& v) {
         if (v == "deterministic" || v == "stub-inference") {
           c.mode = v;
           return true;
         }
         std::cerr << "Invalid --mode: " << v << " (valid: deterministic, stub-inference)\n";
         c.args_valid = false;
         return false;
       }},
  };
  auto config = ccmcp::apps::parse_options(argc, argv, options, 3);

  if (!config.args_valid) {
    return 1;
  }

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
  auto schema_result = db->ensure_schema_v3();
  if (!schema_result.has_value()) {
    std::cerr << "Failed to initialize schema: " << schema_result.error() << "\n";
    return 1;
  }

  ccmcp::storage::sqlite::SqliteResumeStore resume_store(db);
  ccmcp::storage::sqlite::SqliteResumeTokenStore token_store(db);

  const ccmcp::core::ResumeId resume_id{resume_id_str};
  auto resume_opt = resume_store.get(resume_id);
  if (!resume_opt.has_value()) {
    std::cerr << "Resume not found: " << resume_id_str << "\n";
    return 1;
  }

  const auto& resume = resume_opt.value();

  std::unique_ptr<ccmcp::tokenization::ITokenizationProvider> tokenizer;
  if (config.mode == "deterministic") {
    tokenizer = std::make_unique<ccmcp::tokenization::DeterministicLexicalTokenizer>();
    std::cout << "Using deterministic lexical tokenizer\n";
  } else {
    tokenizer = std::make_unique<ccmcp::tokenization::StubInferenceTokenizer>();
    std::cout << "Using stub inference tokenizer\n";
  }

  std::cout << "Tokenizing resume: " << resume_id_str << "\n";
  auto token_ir = tokenizer->tokenize(resume.resume_md, resume.resume_hash);

  const std::string token_ir_id =
      resume_id_str + "-" + ccmcp::domain::tokenizer_type_to_string(token_ir.tokenizer.type);
  token_store.upsert(token_ir_id, resume_id, token_ir);

  std::cout << "Success!\n";
  std::cout << "  Token IR ID: " << token_ir_id << "\n";
  std::cout << "  Source hash: " << token_ir.source_hash << "\n";
  std::cout << "  Tokenizer type: "
            << ccmcp::domain::tokenizer_type_to_string(token_ir.tokenizer.type) << "\n";
  if (token_ir.tokenizer.model_id.has_value()) {
    std::cout << "  Model ID: " << token_ir.tokenizer.model_id.value() << "\n";
  }

  size_t total_tokens = 0;
  std::cout << "  Token counts by category:\n";
  for (const auto& [category, tokens] : token_ir.tokens) {
    std::cout << "    " << category << ": " << tokens.size() << "\n";
    total_tokens += tokens.size();
  }
  std::cout << "  Total tokens: " << total_tokens << "\n";
  std::cout << "  Spans: " << token_ir.spans.size() << "\n";

  return 0;
}
