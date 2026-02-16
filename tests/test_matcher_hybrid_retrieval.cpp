#include "ccmcp/embedding/embedding_provider.h"
#include "ccmcp/matching/matcher.h"
#include "ccmcp/vector/inmemory_embedding_index.h"

#include <catch2/catch_test_macros.hpp>

using namespace ccmcp;

TEST_CASE("Hybrid retrieval expands recall beyond lexical", "[matcher][hybrid]") {
  // Setup: Create atoms where lexical overlap misses one, but embedding retrieval finds it
  domain::ExperienceAtom atom1{core::AtomId{"atom-1"},
                               "cpp",
                               "C++ Performance",
                               "Optimized high-performance C++ systems",
                               {"cpp", "performance"},
                               true,
                               {}};

  domain::ExperienceAtom atom2{core::AtomId{"atom-2"},
                               "systems",
                               "Low-latency Trading",
                               "Built latency-sensitive financial systems",
                               {"trading", "finance",
                                "latency"},  // No direct overlap with "performance optimization"
                               true,
                               {}};

  domain::ExperienceAtom atom3{core::AtomId{"atom-3"},
                               "web",
                               "React Frontend",
                               "Developed React single-page applications",
                               {"react", "frontend"},
                               true,
                               {}};

  std::vector<domain::ExperienceAtom> atoms = {atom1, atom2, atom3};

  // Opportunity requirement: "performance optimization" - lexically matches atom1 well, atom2
  // poorly
  domain::Opportunity opportunity{core::OpportunityId{"opp-1"},
                                  "TechCo",
                                  "Senior Engineer",
                                  {domain::Requirement{"performance optimization skills",
                                                       {"performance", "optimization"},
                                                       true}},
                                  "manual"};

  // Populate vector index with atom2 having high similarity to query
  // (simulating that "performance optimization" semantically relates to "latency-sensitive")
  vector::InMemoryEmbeddingIndex vector_index;
  embedding::DeterministicStubEmbeddingProvider embedding_provider;

  // Index all atoms
  for (const auto& atom : atoms) {
    std::string atom_text = atom.claim + " " + atom.title;
    auto embedding = embedding_provider.embed_text(atom_text);
    vector_index.upsert(atom.atom_id.value, embedding, "");
  }

  // Run matcher in hybrid mode
  matching::Matcher hybrid_matcher(matching::ScoreWeights{},
                                   matching::MatchingStrategy::kHybridLexicalEmbeddingV02,
                                   matching::HybridConfig{.k_lexical = 2, .k_embedding = 2});

  auto report = hybrid_matcher.evaluate(opportunity, atoms, &embedding_provider, &vector_index);

  // Verify hybrid mode was used
  CHECK(report.strategy == "hybrid_lexical_embedding_v0.2");

  // Verify retrieval stats show expansion
  CHECK(report.retrieval_stats.merged_candidates >= 2);
  CHECK(report.retrieval_stats.embedding_candidates > 0);

  // Verify at least one requirement matched
  CHECK(report.requirement_matches.size() == 1);
  CHECK(report.requirement_matches[0].matched == true);
}

TEST_CASE("Hybrid mode with NullEmbeddingProvider falls back to lexical", "[matcher][hybrid]") {
  domain::ExperienceAtom atom1{core::AtomId{"atom-1"}, "cpp", "C++", "C++ work", {"cpp"}, true, {}};

  std::vector<domain::ExperienceAtom> atoms = {atom1};

  domain::Opportunity opportunity{core::OpportunityId{"opp-1"},
                                  "TechCo",
                                  "Role",
                                  {domain::Requirement{"cpp experience", {"cpp"}, true}},
                                  "manual"};

  // Use NullEmbeddingProvider (returns empty vectors)
  embedding::NullEmbeddingProvider embedding_provider;
  vector::InMemoryEmbeddingIndex vector_index;

  matching::Matcher hybrid_matcher(matching::ScoreWeights{},
                                   matching::MatchingStrategy::kHybridLexicalEmbeddingV02);

  auto report = hybrid_matcher.evaluate(opportunity, atoms, &embedding_provider, &vector_index);

  // Should still work, just with lexical candidates only
  CHECK(report.strategy == "hybrid_lexical_embedding_v0.2");
  CHECK(report.retrieval_stats.embedding_candidates == 0);
  CHECK(report.retrieval_stats.merged_candidates > 0);
}

TEST_CASE("Hybrid retrieval preserves lexical scoring and tie-breaks", "[matcher][hybrid]") {
  // Two atoms with identical lexical scores
  domain::ExperienceAtom atom_a{
      core::AtomId{"atom-a"},    "domain", "Title A", "python programming",
      {"python", "programming"}, true,     {}};

  domain::ExperienceAtom atom_z{
      core::AtomId{"atom-z"},    "domain", "Title Z", "python programming",
      {"python", "programming"}, true,     {}};

  std::vector<domain::ExperienceAtom> atoms = {atom_a, atom_z};

  domain::Opportunity opportunity{
      core::OpportunityId{"opp-1"},
      "Company",
      "Role",
      {domain::Requirement{"python programming", {"python", "programming"}, true}},
      "manual"};

  embedding::DeterministicStubEmbeddingProvider embedding_provider;
  vector::InMemoryEmbeddingIndex vector_index;

  // Index atoms
  for (const auto& atom : atoms) {
    auto embedding = embedding_provider.embed_text(atom.claim);
    vector_index.upsert(atom.atom_id.value, embedding, "");
  }

  matching::Matcher hybrid_matcher(matching::ScoreWeights{},
                                   matching::MatchingStrategy::kHybridLexicalEmbeddingV02);

  auto report = hybrid_matcher.evaluate(opportunity, atoms, &embedding_provider, &vector_index);

  // Tie-break should select atom-a (lexicographically smaller)
  REQUIRE(report.requirement_matches.size() == 1);
  REQUIRE(report.requirement_matches[0].matched);
  CHECK(report.requirement_matches[0].contributing_atom_id.value().value == "atom-a");
}
