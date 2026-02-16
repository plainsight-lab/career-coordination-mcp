#include "ccmcp/core/id_generator.h"
#include "ccmcp/core/ids.h"
#include "ccmcp/core/normalization.h"
#include "ccmcp/domain/experience_atom.h"
#include "ccmcp/domain/opportunity.h"
#include "ccmcp/domain/requirement.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

// ============================================================================
// Normalization Utility Tests
// ============================================================================

TEST_CASE("normalize_ascii_lower is deterministic and locale-independent", "[normalization]") {
  SECTION("lowercases ASCII uppercase") {
    auto result = core::normalize_ascii_lower("HELLO WORLD");
    REQUIRE(result == "hello world");
  }

  SECTION("preserves lowercase") {
    auto result = core::normalize_ascii_lower("already lowercase");
    REQUIRE(result == "already lowercase");
  }

  SECTION("preserves non-ASCII unchanged") {
    auto result = core::normalize_ascii_lower("Café");
    REQUIRE(result == "café");  // Only 'C' → 'c', é unchanged
  }

  SECTION("determinism: repeated calls produce identical output") {
    const std::string input = "Mixed CASE Input 123!@#";
    auto result1 = core::normalize_ascii_lower(input);
    auto result2 = core::normalize_ascii_lower(input);
    REQUIRE(result1 == result2);
    REQUIRE(result1 == "mixed case input 123!@#");
  }
}

TEST_CASE("tokenize_ascii is deterministic and locale-independent", "[normalization]") {
  SECTION("splits on punctuation and whitespace") {
    auto tokens = core::tokenize_ascii("Hello, World! This-is-a-test.");
    // Tokens: hello, world, this, is, test (min length 2, so 'a' dropped)
    REQUIRE(tokens.size() == 5);
    REQUIRE(tokens[0] == "hello");
    REQUIRE(tokens[1] == "world");
    REQUIRE(tokens[2] == "this");
    REQUIRE(tokens[3] == "is");
    REQUIRE(tokens[4] == "test");
  }

  SECTION("drops tokens shorter than min_length") {
    auto tokens = core::tokenize_ascii("a bb ccc", 2);
    REQUIRE(tokens.size() == 2);  // "a" dropped
    REQUIRE(tokens[0] == "bb");
    REQUIRE(tokens[1] == "ccc");
  }

  SECTION("handles repeated delimiters") {
    auto tokens = core::tokenize_ascii("one!!!two###three", 2);
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0] == "one");
    REQUIRE(tokens[1] == "two");
    REQUIRE(tokens[2] == "three");
  }

  SECTION("lowercases during tokenization") {
    auto tokens = core::tokenize_ascii("UPPER case MiXeD", 2);
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0] == "upper");
    REQUIRE(tokens[1] == "case");
    REQUIRE(tokens[2] == "mixed");
  }

  SECTION("determinism: repeated calls produce identical output") {
    const std::string input = "Test!@#Input$%^With&&*Punctuation";
    auto tokens1 = core::tokenize_ascii(input);
    auto tokens2 = core::tokenize_ascii(input);
    REQUIRE(tokens1 == tokens2);
  }
}

TEST_CASE("normalize_tags produces sorted, deduplicated lowercase tags", "[normalization]") {
  SECTION("normalizes mixed case tags") {
    std::vector<std::string> input = {"Python", "JAVA", "python", "Go"};
    auto result = core::normalize_tags(input);
    // Expected: go, java, python (sorted, deduped)
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "go");
    REQUIRE(result[1] == "java");
    REQUIRE(result[2] == "python");
  }

  SECTION("removes duplicates case-insensitively") {
    std::vector<std::string> input = {"rust", "Rust", "RUST", "Go", "go"};
    auto result = core::normalize_tags(input);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == "go");
    REQUIRE(result[1] == "rust");
  }

  SECTION("tokenizes multi-word tags") {
    std::vector<std::string> input = {"C++ Programming", "python-dev", "machine learning"};
    auto result = core::normalize_tags(input);
    // Tokens: programming, python, dev, machine, learning (sorted)
    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == "dev");
    REQUIRE(result[1] == "learning");
    REQUIRE(result[2] == "machine");
    REQUIRE(result[3] == "programming");
    REQUIRE(result[4] == "python");
  }

  SECTION("golden stability: output is byte-stable") {
    std::vector<std::string> input = {"Kubernetes", "Docker", "AWS", "Azure"};
    auto result = core::normalize_tags(input);
    // Join for golden comparison
    std::string joined;
    for (const auto& tag : result) {
      if (!joined.empty())
        joined += ",";
      joined += tag;
    }
    REQUIRE(joined == "aws,azure,docker,kubernetes");
  }
}

TEST_CASE("trim removes leading and trailing whitespace", "[normalization]") {
  SECTION("trims spaces") {
    REQUIRE(core::trim("  hello  ") == "hello");
  }

  SECTION("trims mixed whitespace") {
    REQUIRE(core::trim("\t\n  test  \r\n") == "test");
  }

  SECTION("handles all-whitespace input") {
    REQUIRE(core::trim("   \t\n   ").empty());
  }

  SECTION("preserves internal whitespace") {
    REQUIRE(core::trim("  hello   world  ") == "hello   world");
  }
}

// ============================================================================
// ExperienceAtom Schema Tests
// ============================================================================

TEST_CASE("ExperienceAtom normalization is deterministic", "[domain][atom]") {
  SECTION("normalizes domain to lowercase") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "  Enterprise ARCHITECTURE  ";
    atom.claim = "Test claim";
    atom.tags = {"Python", "AWS"};

    auto normalized = domain::normalize_atom(atom);
    REQUIRE(normalized.domain == "enterprise architecture");
  }

  SECTION("trims title and claim") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "security";
    atom.title = "  Security Lead  ";
    atom.claim = "  Designed secure systems  ";
    atom.tags = {};

    auto normalized = domain::normalize_atom(atom);
    REQUIRE(normalized.title == "Security Lead");
    REQUIRE(normalized.claim == "Designed secure systems");
  }

  SECTION("normalizes tags: lowercase, sorted, deduplicated") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "ai";
    atom.claim = "Built ML models";
    atom.tags = {"Python", "TensorFlow", "python", "AWS"};

    auto normalized = domain::normalize_atom(atom);
    REQUIRE(normalized.tags.size() == 3);
    REQUIRE(normalized.tags[0] == "aws");
    REQUIRE(normalized.tags[1] == "python");
    REQUIRE(normalized.tags[2] == "tensorflow");
  }

  SECTION("trims and filters empty evidence_refs") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "backend";
    atom.claim = "Built scalable APIs";
    atom.evidence_refs = {"  https://example.com  ", "  ", "github.com/project"};

    auto normalized = domain::normalize_atom(atom);
    REQUIRE(normalized.evidence_refs.size() == 2);
    REQUIRE(normalized.evidence_refs[0] == "https://example.com");
    REQUIRE(normalized.evidence_refs[1] == "github.com/project");
  }

  SECTION("preserves atom_id and verified flag") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "data";
    atom.claim = "Analyzed large datasets";
    atom.verified = true;

    auto normalized = domain::normalize_atom(atom);
    REQUIRE(normalized.atom_id.value == atom.atom_id.value);
    REQUIRE(normalized.verified == true);
  }
}

TEST_CASE("ExperienceAtom validation enforces invariants", "[domain][atom]") {
  SECTION("valid normalized atom passes validation") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "backend";  // already lowercase
    atom.claim = "Built APIs";
    atom.tags = {"go", "python"};  // already normalized

    auto result = atom.validate();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == true);
  }

  SECTION("rejects empty atom_id") {
    domain::ExperienceAtom atom;
    atom.atom_id.value = "";
    atom.domain = "test";
    atom.claim = "Test claim";

    auto result = atom.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "atom_id must not be empty");
  }

  SECTION("rejects empty claim") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "test";
    atom.claim = "";

    auto result = atom.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "claim must not be empty");
  }

  SECTION("rejects unnormalized domain") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "BACKEND";  // uppercase
    atom.claim = "Test";

    auto result = atom.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "domain must be normalized (lowercase)");
  }

  SECTION("rejects unnormalized tags") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "backend";
    atom.claim = "Test";
    atom.tags = {"Python", "Go"};  // Not lowercase

    auto result = atom.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "tags must be normalized (lowercase, sorted, deduplicated)");
  }
}

// ============================================================================
// Requirement Schema Tests
// ============================================================================

TEST_CASE("Requirement normalization is deterministic", "[domain][requirement]") {
  SECTION("trims text") {
    domain::Requirement req;
    req.text = "  5+ years Python experience  ";
    req.tags = {"Python"};

    auto normalized = domain::normalize_requirement(req);
    REQUIRE(normalized.text == "5+ years Python experience");
  }

  SECTION("normalizes tags") {
    domain::Requirement req;
    req.text = "Cloud experience required";
    req.tags = {"AWS", "azure", "GCP", "aws"};

    auto normalized = domain::normalize_requirement(req);
    REQUIRE(normalized.tags.size() == 3);
    REQUIRE(normalized.tags[0] == "aws");
    REQUIRE(normalized.tags[1] == "azure");
    REQUIRE(normalized.tags[2] == "gcp");
  }
}

TEST_CASE("Requirement validation enforces invariants", "[domain][requirement]") {
  SECTION("valid requirement passes") {
    domain::Requirement req;
    req.text = "Python experience";
    req.tags = {"python"};
    req.required = true;

    auto result = req.validate();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == true);
  }

  SECTION("rejects empty text") {
    domain::Requirement req;
    req.text = "";

    auto result = req.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "requirement text must not be empty");
  }

  SECTION("rejects unnormalized tags") {
    domain::Requirement req;
    req.text = "Test requirement";
    req.tags = {"Python", "Go"};  // Not normalized

    auto result = req.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "tags must be normalized (lowercase, sorted, deduplicated)");
  }
}

// ============================================================================
// Opportunity Schema Tests
// ============================================================================

TEST_CASE("Opportunity normalization is deterministic", "[domain][opportunity]") {
  SECTION("trims company and role_title") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "  Acme Corp  ";
    opp.role_title = "  Senior Engineer  ";

    auto normalized = domain::normalize_opportunity(opp);
    REQUIRE(normalized.company == "Acme Corp");
    REQUIRE(normalized.role_title == "Senior Engineer");
  }

  SECTION("normalizes requirements in order") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "Test";
    opp.role_title = "Engineer";

    domain::Requirement req1;
    req1.text = "  Python experience  ";
    req1.tags = {"Python", "AWS"};

    domain::Requirement req2;
    req2.text = "  Go experience  ";
    req2.tags = {"Go"};

    opp.requirements = {req1, req2};

    auto normalized = domain::normalize_opportunity(opp);
    REQUIRE(normalized.requirements.size() == 2);
    REQUIRE(normalized.requirements[0].text == "Python experience");
    REQUIRE(normalized.requirements[1].text == "Go experience");
    // Verify tags normalized
    REQUIRE(normalized.requirements[0].tags[0] == "aws");
    REQUIRE(normalized.requirements[0].tags[1] == "python");
  }

  SECTION("preserves requirements order (does not sort)") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "Test";
    opp.role_title = "Engineer";

    domain::Requirement req1;
    req1.text = "Zebra requirement";  // Would sort last
    domain::Requirement req2;
    req2.text = "Alpha requirement";  // Would sort first

    opp.requirements = {req1, req2};

    auto normalized = domain::normalize_opportunity(opp);
    REQUIRE(normalized.requirements.size() == 2);
    REQUIRE(normalized.requirements[0].text == "Zebra requirement");
    REQUIRE(normalized.requirements[1].text == "Alpha requirement");
  }
}

TEST_CASE("Opportunity validation enforces invariants", "[domain][opportunity]") {
  SECTION("valid opportunity passes") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "Acme";
    opp.role_title = "Engineer";

    domain::Requirement req;
    req.text = "Python";
    opp.requirements = {req};

    auto result = opp.validate();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == true);
  }

  SECTION("rejects empty opportunity_id") {
    domain::Opportunity opp;
    opp.opportunity_id.value = "";
    opp.company = "Test";
    opp.role_title = "Test";

    auto result = opp.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "opportunity_id must not be empty");
  }

  SECTION("rejects empty company") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "";
    opp.role_title = "Test";

    auto result = opp.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "company must not be empty");
  }

  SECTION("rejects empty role_title") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "Test";
    opp.role_title = "";

    auto result = opp.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == "role_title must not be empty");
  }

  SECTION("rejects invalid requirement") {
    core::DeterministicIdGenerator gen;

    domain::Opportunity opp;
    opp.opportunity_id = core::new_opportunity_id(gen);
    opp.company = "Test";
    opp.role_title = "Test";

    domain::Requirement invalid_req;
    invalid_req.text = "";  // Invalid
    opp.requirements = {invalid_req};

    auto result = opp.validate();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().find("invalid requirement") != std::string::npos);
  }
}

// ============================================================================
// Golden Stability Tests
// ============================================================================

TEST_CASE("Schema normalization produces stable output", "[golden]") {
  SECTION("atom tag serialization is stable") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "AI/ML";
    atom.claim = "Built recommendation systems";
    atom.tags = {"Python", "TensorFlow", "AWS", "docker"};

    auto normalized = domain::normalize_atom(atom);

    // Join tags for golden comparison
    std::string joined;
    for (const auto& tag : normalized.tags) {
      if (!joined.empty())
        joined += ",";
      joined += tag;
    }

    // This output must be byte-stable across runs
    REQUIRE(joined == "aws,docker,python,tensorflow");
  }

  SECTION("repeated normalization is idempotent") {
    core::DeterministicIdGenerator gen;

    domain::ExperienceAtom atom;
    atom.atom_id = core::new_atom_id(gen);
    atom.domain = "Security";
    atom.claim = "Implemented auth systems";
    atom.tags = {"OAuth", "JWT", "SAML"};

    auto normalized1 = domain::normalize_atom(atom);
    auto normalized2 = domain::normalize_atom(normalized1);

    REQUIRE(normalized1.domain == normalized2.domain);
    REQUIRE(normalized1.tags == normalized2.tags);
  }
}
