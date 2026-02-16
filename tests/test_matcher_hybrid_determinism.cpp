#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/matching/matcher.h"
#include "ccmcp/vector/inmemory_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("Hybrid retrieval produces deterministic output", "[matcher][hybrid][determinism]") {
  // Setup: Same inputs run twice must produce identical output
  domain::ExperienceAtom atom1{core::AtomId{"atom-1"},
                               "rust",
                               "Rust Systems",
                               "Built systems in Rust",
                               {"rust", "systems"},
                               true,
                               {}};

  domain::ExperienceAtom atom2{core::AtomId{"atom-2"},
                               "cpp",
                               "C++ Performance",
                               "Optimized C++ code",
                               {"cpp", "performance"},
                               true,
                               {}};

  domain::ExperienceAtom atom3{core::AtomId{"atom-3"},
                               "go",
                               "Go Services",
                               "Microservices in Go",
                               {"go", "microservices"},
                               true,
                               {}};

  std::vector<domain::ExperienceAtom> atoms = {atom1, atom2, atom3};

  domain::Opportunity opportunity{
      core::OpportunityId{"opp-1"},
      "TechCo",
      "Backend Engineer",
      {domain::Requirement{"systems programming experience", {"systems", "programming"}, true},
       domain::Requirement{"performance optimization", {"performance", "optimization"}, true}},
      "manual"};

  // Use deterministic stub provider
  embedding::DeterministicStubEmbeddingProvider embedding_provider;
  vector::InMemoryEmbeddingIndex vector_index;

  // Index all atoms
  for (const auto& atom : atoms) {
    std::string atom_text = atom.claim + " " + atom.title;
    auto embedding = embedding_provider.embed_text(atom_text);
    vector_index.upsert(atom.atom_id.value, embedding, "");
  }

  matching::Matcher hybrid_matcher(matching::ScoreWeights{},
                                   matching::MatchingStrategy::kHybridLexicalEmbeddingV02,
                                   matching::HybridConfig{.k_lexical = 5, .k_embedding = 5});

  // Run 1
  auto report1 = hybrid_matcher.evaluate(opportunity, atoms, &embedding_provider, &vector_index);

  // Run 2 (identical inputs)
  auto report2 = hybrid_matcher.evaluate(opportunity, atoms, &embedding_provider, &vector_index);

  // Verify determinism: outputs must be identical
  CHECK(report1.strategy == report2.strategy);
  CHECK(report1.overall_score == report2.overall_score);
  CHECK(report1.retrieval_stats.lexical_candidates == report2.retrieval_stats.lexical_candidates);
  CHECK(report1.retrieval_stats.embedding_candidates ==
        report2.retrieval_stats.embedding_candidates);
  CHECK(report1.retrieval_stats.merged_candidates == report2.retrieval_stats.merged_candidates);

  // Check requirement matches are identical
  REQUIRE(report1.requirement_matches.size() == report2.requirement_matches.size());
  for (size_t i = 0; i < report1.requirement_matches.size(); ++i) {
    CHECK(report1.requirement_matches[i].matched == report2.requirement_matches[i].matched);
    CHECK(report1.requirement_matches[i].best_score == report2.requirement_matches[i].best_score);
    CHECK(report1.requirement_matches[i].contributing_atom_id ==
          report2.requirement_matches[i].contributing_atom_id);
  }
}

TEST_CASE("Deterministic stub embedding provider produces stable vectors",
          "[embedding][determinism]") {
  embedding::DeterministicStubEmbeddingProvider provider;

  std::string text = "machine learning systems";

  // Generate embedding twice
  auto vec1 = provider.embed_text(text);
  auto vec2 = provider.embed_text(text);

  // Must be identical
  REQUIRE(vec1.size() == vec2.size());
  for (size_t i = 0; i < vec1.size(); ++i) {
    CHECK(vec1[i] == vec2[i]);
  }
}

TEST_CASE("Hybrid mode with empty query tokens falls back gracefully", "[matcher][hybrid][edge]") {
  domain::ExperienceAtom atom1{
      core::AtomId{"atom-1"}, "domain", "Title", "Claim text", {"tag"}, true, {}};

  std::vector<domain::ExperienceAtom> atoms = {atom1};

  // Opportunity with empty/whitespace requirement
  domain::Opportunity opportunity{core::OpportunityId{"opp-1"},
                                  "Company",
                                  "Role",
                                  {domain::Requirement{"   ", {}, false}},
                                  "manual"};

  embedding::DeterministicStubEmbeddingProvider embedding_provider;
  vector::InMemoryEmbeddingIndex vector_index;

  matching::Matcher hybrid_matcher(matching::ScoreWeights{},
                                   matching::MatchingStrategy::kHybridLexicalEmbeddingV02);

  auto report = hybrid_matcher.evaluate(opportunity, atoms, &embedding_provider, &vector_index);

  // Should not crash, requirement should be marked unmatched
  REQUIRE(report.requirement_matches.size() == 1);
  CHECK(report.requirement_matches[0].matched == false);
}
