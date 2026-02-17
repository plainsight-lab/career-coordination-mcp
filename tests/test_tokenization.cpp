#include "ccmcp/domain/resume_token_ir.h"
#include "ccmcp/domain/resume_token_ir_json.h"
#include "ccmcp/tokenization/deterministic_lexical_tokenizer.h"
#include "ccmcp/tokenization/stub_inference_tokenizer.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("DeterministicLexicalTokenizer produces stable output", "[tokenization]") {
  const std::string resume_md = R"(# John Doe
Software Engineer with C++ and Python experience.
Skills: architecture, distributed systems, cloud computing.
)";
  const std::string source_hash = "test-hash-123";

  tokenization::DeterministicLexicalTokenizer tokenizer;

  SECTION("same input produces identical output") {
    auto result1 = tokenizer.tokenize(resume_md, source_hash);
    auto result2 = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result1.source_hash == result2.source_hash);
    REQUIRE(result1.tokenizer.type == result2.tokenizer.type);
    REQUIRE(result1.tokens == result2.tokens);
  }

  SECTION("produces lowercase tokens only") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result.tokens.count("lexical") == 1);
    const auto& lexical_tokens = result.tokens.at("lexical");

    for (const auto& token : lexical_tokens) {
      for (char c : token) {
        REQUIRE(((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')));
      }
    }
  }

  SECTION("respects minimum token length of 2") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    const auto& lexical_tokens = result.tokens.at("lexical");
    for (const auto& token : lexical_tokens) {
      REQUIRE(token.size() >= 2);
    }
  }

  SECTION("sets correct metadata") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result.source_hash == source_hash);
    REQUIRE(result.schema_version == "0.3");
    REQUIRE(result.tokenizer.type == domain::TokenizerType::kDeterministicLexical);
    REQUIRE_FALSE(result.tokenizer.model_id.has_value());
    REQUIRE_FALSE(result.tokenizer.prompt_version.has_value());
  }

  SECTION("produces no spans (optional for deterministic mode)") {
    auto result = tokenizer.tokenize(resume_md, source_hash);
    REQUIRE(result.spans.empty());
  }
}

TEST_CASE("StubInferenceTokenizer produces deterministic categorization", "[tokenization]") {
  const std::string resume_md = R"(# Senior Architect
Led architecture for distributed systems at Google.
Skills: C++, Python, Kubernetes, AWS, system design.
Built scalable microservices. Expert in Docker and cloud infrastructure.
)";
  const std::string source_hash = "test-hash-456";

  tokenization::StubInferenceTokenizer tokenizer;

  SECTION("same input produces identical output") {
    auto result1 = tokenizer.tokenize(resume_md, source_hash);
    auto result2 = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result1.tokens == result2.tokens);
    REQUIRE(result1.tokenizer.model_id == result2.tokenizer.model_id);
  }

  SECTION("categorizes skills correctly") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result.tokens.count("skills") == 1);
    const auto& skills = result.tokens.at("skills");

    // Should contain programming languages and tools
    REQUIRE(std::find(skills.begin(), skills.end(), "python") != skills.end());
    REQUIRE(std::find(skills.begin(), skills.end(), "kubernetes") != skills.end());
  }

  SECTION("categorizes domains correctly") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result.tokens.count("domains") == 1);
    const auto& domains = result.tokens.at("domains");

    // Should contain domain areas
    REQUIRE(std::find(domains.begin(), domains.end(), "distributed") != domains.end());
    REQUIRE(std::find(domains.begin(), domains.end(), "infrastructure") != domains.end());
  }

  SECTION("categorizes entities correctly") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result.tokens.count("entities") == 1);
    const auto& entities = result.tokens.at("entities");

    // Should contain company names
    REQUIRE(std::find(entities.begin(), entities.end(), "google") != entities.end());
  }

  SECTION("sets correct metadata") {
    auto result = tokenizer.tokenize(resume_md, source_hash);

    REQUIRE(result.source_hash == source_hash);
    REQUIRE(result.tokenizer.type == domain::TokenizerType::kInferenceAssisted);
    REQUIRE(result.tokenizer.model_id == "stub-inference-v1");
  }
}

TEST_CASE("Token IR JSON serialization is deterministic", "[tokenization][json]") {
  domain::ResumeTokenIR token_ir;
  token_ir.schema_version = "0.3";
  token_ir.source_hash = "abc123";
  token_ir.tokenizer.type = domain::TokenizerType::kDeterministicLexical;
  token_ir.tokens["skills"] = {"cpp", "python", "rust"};
  token_ir.tokens["domains"] = {"architecture", "systems"};

  SECTION("same token IR produces identical JSON") {
    auto json1 = domain::token_ir_to_json_string(token_ir);
    auto json2 = domain::token_ir_to_json_string(token_ir);

    REQUIRE(json1 == json2);
  }

  SECTION("JSON round-trip preserves data") {
    auto json_str = domain::token_ir_to_json_string(token_ir);
    auto parsed_json = nlohmann::json::parse(json_str);
    auto reconstructed = domain::token_ir_from_json(parsed_json);

    REQUIRE(reconstructed.schema_version == token_ir.schema_version);
    REQUIRE(reconstructed.source_hash == token_ir.source_hash);
    REQUIRE(reconstructed.tokenizer.type == token_ir.tokenizer.type);
    REQUIRE(reconstructed.tokens == token_ir.tokens);
  }

  SECTION("tokens are sorted by key in JSON") {
    auto json_obj = domain::token_ir_to_json(token_ir);
    auto tokens_obj = json_obj["tokens"];

    // Map iteration in C++ should be sorted, verify JSON preserves order
    std::vector<std::string> keys;
    for (auto it = tokens_obj.begin(); it != tokens_obj.end(); ++it) {
      keys.push_back(it.key());
    }

    REQUIRE(keys.size() == 2);
    REQUIRE(keys[0] == "domains");  // 'd' comes before 's'
    REQUIRE(keys[1] == "skills");
  }
}

TEST_CASE("DeterministicLexicalTokenizer filters stop words", "[tokenization]") {
  const std::string resume_md = R"(
The software engineer has experience with the Python programming language.
She is an expert in the field of distributed systems and has worked with the team.
)";
  const std::string source_hash = "test-hash-stop-words";

  SECTION("filters stop words by default") {
    tokenization::DeterministicLexicalTokenizer tokenizer;  // default: filter_stop_words=true
    auto result = tokenizer.tokenize(resume_md, source_hash);

    const auto& tokens = result.tokens.at("lexical");

    // Should contain technical terms
    REQUIRE(std::find(tokens.begin(), tokens.end(), "software") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "engineer") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "python") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "distributed") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "systems") != tokens.end());

    // Should NOT contain stop words
    REQUIRE(std::find(tokens.begin(), tokens.end(), "the") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "an") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "in") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "of") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "has") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "with") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "and") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "she") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "is") == tokens.end());
  }

  SECTION("can disable stop word filtering") {
    tokenization::DeterministicLexicalTokenizer tokenizer(false);  // filter_stop_words=false
    auto result = tokenizer.tokenize(resume_md, source_hash);

    const auto& tokens = result.tokens.at("lexical");

    // Should contain both technical terms AND stop words
    REQUIRE(std::find(tokens.begin(), tokens.end(), "software") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "the") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "an") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "in") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "of") != tokens.end());
  }

  SECTION("stop word filtering is deterministic") {
    tokenization::DeterministicLexicalTokenizer tokenizer1;
    tokenization::DeterministicLexicalTokenizer tokenizer2;

    auto result1 = tokenizer1.tokenize(resume_md, source_hash);
    auto result2 = tokenizer2.tokenize(resume_md, source_hash);

    REQUIRE(result1.tokens == result2.tokens);
  }

  SECTION("filters common prepositions and conjunctions") {
    const std::string text = "Experience with Python and C++ in cloud computing for AWS.";
    tokenization::DeterministicLexicalTokenizer tokenizer;
    auto result = tokenizer.tokenize(text, source_hash);

    const auto& tokens = result.tokens.at("lexical");

    // Keep: technical terms
    REQUIRE(std::find(tokens.begin(), tokens.end(), "experience") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "python") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "cloud") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "computing") != tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "aws") != tokens.end());

    // Filter: stop words
    REQUIRE(std::find(tokens.begin(), tokens.end(), "with") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "and") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "in") == tokens.end());
    REQUIRE(std::find(tokens.begin(), tokens.end(), "for") == tokens.end());
  }
}

TEST_CASE("TokenizerType enum conversions", "[tokenization]") {
  SECTION("deterministic-lexical conversion") {
    auto type_opt = domain::string_to_tokenizer_type("deterministic-lexical");
    REQUIRE(type_opt.has_value());
    REQUIRE(type_opt.value() == domain::TokenizerType::kDeterministicLexical);

    auto str = domain::tokenizer_type_to_string(domain::TokenizerType::kDeterministicLexical);
    REQUIRE(str == "deterministic-lexical");
  }

  SECTION("inference-assisted conversion") {
    auto type_opt = domain::string_to_tokenizer_type("inference-assisted");
    REQUIRE(type_opt.has_value());
    REQUIRE(type_opt.value() == domain::TokenizerType::kInferenceAssisted);

    auto str = domain::tokenizer_type_to_string(domain::TokenizerType::kInferenceAssisted);
    REQUIRE(str == "inference-assisted");
  }

  SECTION("invalid string returns nullopt") {
    auto type_opt = domain::string_to_tokenizer_type("invalid-type");
    REQUIRE_FALSE(type_opt.has_value());
  }

  SECTION("round-trip conversion") {
    auto original = domain::TokenizerType::kDeterministicLexical;
    auto str = domain::tokenizer_type_to_string(original);
    auto reconstructed = domain::string_to_tokenizer_type(str);

    REQUIRE(reconstructed.has_value());
    REQUIRE(reconstructed.value() == original);
  }
}
