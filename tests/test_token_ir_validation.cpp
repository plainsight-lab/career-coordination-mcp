#include "ccmcp/constitution/rule.h"
#include "ccmcp/constitution/rules/tok_001.h"
#include "ccmcp/constitution/rules/tok_002.h"
#include "ccmcp/constitution/rules/tok_003.h"
#include "ccmcp/constitution/rules/tok_004.h"
#include "ccmcp/constitution/rules/tok_005.h"
#include "ccmcp/constitution/token_ir_artifact_view.h"
#include "ccmcp/domain/resume_token_ir.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;
using namespace ccmcp::constitution;

TEST_CASE("TOK-001: Source hash validation", "[validation][tok-001]") {
  domain::ResumeTokenIR token_ir;
  token_ir.source_hash = "correct-hash-123";
  token_ir.tokenizer.type = domain::TokenizerType::kDeterministicLexical;

  const std::string canonical_hash = "correct-hash-123";
  const std::string canonical_text = "Resume text";

  Tok001 rule;

  SECTION("passes when source hash matches") {
    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-1";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.empty());
  }

  SECTION("blocks when source hash does not match") {
    const std::string wrong_hash = "wrong-hash-456";
    auto view = std::make_shared<TokenIRArtifactView>(token_ir, wrong_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-2";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].rule_id == "TOK-001");
    REQUIRE(findings[0].severity == FindingSeverity::kBlock);
  }

  SECTION("has correct metadata") {
    REQUIRE(rule.rule_id() == "TOK-001");
    REQUIRE(rule.version() == "0.3.0");
    REQUIRE_FALSE(std::string(rule.description()).empty());
  }
}

TEST_CASE("TOK-002: Token format validation", "[validation][tok-002]") {
  domain::ResumeTokenIR token_ir;
  token_ir.source_hash = "hash";
  token_ir.tokenizer.type = domain::TokenizerType::kDeterministicLexical;

  const std::string canonical_hash = "hash";
  const std::string canonical_text = "Resume text";

  Tok002 rule;

  SECTION("passes with valid lowercase ASCII tokens") {
    token_ir.tokens["skills"] = {"cpp", "python", "rust"};
    token_ir.tokens["domains"] = {"architecture", "systems"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-3";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.empty());
  }

  SECTION("fails when token contains uppercase") {
    token_ir.tokens["skills"] = {"C++", "python"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-4";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE_FALSE(findings.empty());
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }

  SECTION("fails when token contains special characters") {
    token_ir.tokens["skills"] = {"c++", "python-3"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-5";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.size() >= 1);
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }

  SECTION("fails when token length < 2") {
    token_ir.tokens["skills"] = {"c", "python"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-6";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }

  SECTION("allows digits in tokens") {
    token_ir.tokens["skills"] = {"cpp20", "python3"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-7";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.empty());
  }
}

TEST_CASE("TOK-003: Token span validation", "[validation][tok-003]") {
  domain::ResumeTokenIR token_ir;
  token_ir.source_hash = "hash";
  token_ir.tokenizer.type = domain::TokenizerType::kInferenceAssisted;

  const std::string canonical_hash = "hash";
  const std::string canonical_text = "Line 1\nLine 2\nLine 3\n";

  Tok003 rule;

  SECTION("passes with valid spans") {
    token_ir.spans = {{"token1", 1, 1}, {"token2", 2, 3}, {"token3", 1, 2}};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-8";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.empty());
  }

  SECTION("fails when start_line < 1") {
    token_ir.spans = {{"token1", 0, 2}};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-9";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE_FALSE(findings.empty());
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }

  SECTION("fails when end_line < 1") {
    token_ir.spans = {{"token1", 1, 0}};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-10";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE_FALSE(findings.empty());
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }

  SECTION("fails when start_line > end_line") {
    token_ir.spans = {{"token1", 3, 1}};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-11";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE_FALSE(findings.empty());
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }

  SECTION("fails when end_line exceeds resume line count") {
    token_ir.spans = {{"token1", 1, 10}};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-12";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE_FALSE(findings.empty());
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
  }
}

TEST_CASE("TOK-004: Hallucination detection", "[validation][tok-004]") {
  domain::ResumeTokenIR token_ir;
  token_ir.source_hash = "hash";
  token_ir.tokenizer.type = domain::TokenizerType::kInferenceAssisted;

  const std::string canonical_hash = "hash";
  const std::string canonical_text =
      "Software Engineer with C++ and Python experience in distributed systems. Expert in CPP "
      "programming.";

  Tok004 rule;

  SECTION("passes when all tokens are derivable from resume") {
    token_ir.tokens["skills"] = {"cpp", "python", "software"};
    token_ir.tokens["domains"] = {"distributed", "systems", "engineer"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-13";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.empty());
  }

  SECTION("fails when token is not in resume") {
    token_ir.tokens["skills"] = {"cpp", "python", "kubernetes"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-14";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE_FALSE(findings.empty());
    REQUIRE(findings[0].severity == FindingSeverity::kFail);
    REQUIRE(findings[0].message.find("kubernetes") != std::string::npos);
  }

  SECTION("detects multiple hallucinated tokens") {
    token_ir.tokens["skills"] = {"cpp", "java", "rust", "golang"};

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-15";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.size() == 3);
    for (const auto& finding : findings) {
      REQUIRE(finding.severity == FindingSeverity::kFail);
    }
  }
}

TEST_CASE("TOK-005: Token volume thresholds", "[validation][tok-005]") {
  domain::ResumeTokenIR token_ir;
  token_ir.source_hash = "hash";
  token_ir.tokenizer.type = domain::TokenizerType::kInferenceAssisted;

  const std::string canonical_hash = "hash";
  const std::string canonical_text = "Resume text";

  Tok005 rule;

  SECTION("passes when within thresholds") {
    std::vector<std::string> tokens_50;
    for (int i = 0; i < 50; ++i) {
      tokens_50.push_back("token" + std::to_string(i));
    }
    token_ir.tokens["skills"] = tokens_50;

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-16";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.empty());
  }

  SECTION("warns when category exceeds threshold") {
    std::vector<std::string> tokens_250;
    for (int i = 0; i < 250; ++i) {
      tokens_250.push_back("token" + std::to_string(i));
    }
    token_ir.tokens["skills"] = tokens_250;

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-17";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);
    REQUIRE(findings.size() == 1);
    REQUIRE(findings[0].severity == FindingSeverity::kWarn);
    REQUIRE(findings[0].message.find("skills") != std::string::npos);
  }

  SECTION("warns when total tokens exceed threshold") {
    std::vector<std::string> tokens_150;
    for (int i = 0; i < 150; ++i) {
      tokens_150.push_back("token" + std::to_string(i));
    }

    token_ir.tokens["skills"] = tokens_150;
    token_ir.tokens["domains"] = tokens_150;
    token_ir.tokens["roles"] = tokens_150;
    token_ir.tokens["entities"] = tokens_150;

    auto view = std::make_shared<TokenIRArtifactView>(token_ir, canonical_hash, canonical_text);
    ArtifactEnvelope envelope;
    envelope.artifact_id = "test-18";
    envelope.artifact = view;
    ValidationContext context;

    auto findings = rule.Validate(envelope, context);

    bool found_total_warning = false;
    for (const auto& finding : findings) {
      if (finding.message.find("Total token count") != std::string::npos) {
        found_total_warning = true;
        REQUIRE(finding.severity == FindingSeverity::kWarn);
      }
    }
    REQUIRE(found_total_warning);
  }

  SECTION("has correct metadata") {
    REQUIRE(rule.rule_id() == "TOK-005");
    REQUIRE(rule.version() == "0.3.0");
    REQUIRE_FALSE(std::string(rule.description()).empty());
  }
}
