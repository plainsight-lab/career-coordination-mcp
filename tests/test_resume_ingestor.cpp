#include "ccmcp/core/clock.h"
#include "ccmcp/core/id_generator.h"
#include "ccmcp/ingest/resume_ingestor.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

using namespace ccmcp;
using namespace ccmcp::ingest;

TEST_CASE("Resume ingestor handles markdown file", "[ingest][resume_ingestor]") {
  // Create a test markdown file
  std::string test_file = "test_ingest_md.md";
  {
    std::ofstream ofs(test_file);
    ofs << "# John Doe\n\n## Experience\n- Tech Corp\n";
  }

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  auto result = ingestor->ingest_file(test_file, options, id_gen, clock);

  // Cleanup
  std::remove(test_file.c_str());

  REQUIRE(result.has_value());
  auto resume = result.value();

  REQUIRE(resume.resume_id.value == "resume--0");
  REQUIRE(!resume.resume_md.empty());
  REQUIRE(!resume.resume_hash.empty());
  REQUIRE(resume.resume_hash.starts_with("sha256:"));
  REQUIRE(resume.meta.extraction_method == "md-pass-through-v1");
  REQUIRE(resume.meta.ingestion_version == "0.3");
  REQUIRE(resume.meta.source_path.has_value());
  REQUIRE(resume.meta.source_path.value() == test_file);
}

TEST_CASE("Resume ingestor handles text file", "[ingest][resume_ingestor]") {
  // Create a test text file
  std::string test_file = "test_ingest_txt.txt";
  {
    std::ofstream ofs(test_file);
    ofs << "John Doe\nSoftware Engineer\n";
  }

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  auto result = ingestor->ingest_file(test_file, options, id_gen, clock);

  // Cleanup
  std::remove(test_file.c_str());

  REQUIRE(result.has_value());
  auto resume = result.value();

  REQUIRE(resume.meta.extraction_method == "txt-wrap-v1");
  REQUIRE(resume.resume_md.starts_with("# Resume\n\n"));
}

TEST_CASE("Resume ingestor applies hygiene by default", "[ingest][resume_ingestor]") {
  std::string input = "# Resume  \r\n\r\nExperience\t\r\n";
  std::vector<uint8_t> data(input.begin(), input.end());

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  auto result = ingestor->ingest_bytes(data, "md", options, id_gen, clock);

  REQUIRE(result.has_value());
  auto resume = result.value();

  // Should have normalized line endings and trimmed whitespace
  // Note: Input ends with \r\n, so output ends with \n (preserved)
  REQUIRE(resume.resume_md == "# Resume\n\nExperience\n");
}

TEST_CASE("Resume ingestor can disable hygiene", "[ingest][resume_ingestor]") {
  std::string input = "# Resume  \r\n\r\nExperience\t\r\n";
  std::vector<uint8_t> data(input.begin(), input.end());

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  options.enable_hygiene = false;
  auto result = ingestor->ingest_bytes(data, "md", options, id_gen, clock);

  REQUIRE(result.has_value());
  auto resume = result.value();

  // Should preserve original formatting
  REQUIRE(resume.resume_md == input);
}

TEST_CASE("Resume ingestor computes deterministic hash", "[ingest][resume_ingestor]") {
  std::string input = "# Resume\n\nExperience";
  std::vector<uint8_t> data(input.begin(), input.end());

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen1;
  core::DeterministicIdGenerator id_gen2;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  auto result1 = ingestor->ingest_bytes(data, "md", options, id_gen1, clock);
  auto result2 = ingestor->ingest_bytes(data, "md", options, id_gen2, clock);

  REQUIRE(result1.has_value());
  REQUIRE(result2.has_value());

  // Same input should produce same resume hash
  REQUIRE(result1.value().resume_hash == result2.value().resume_hash);
}

TEST_CASE("Resume ingestor uses provided timestamp", "[ingest][resume_ingestor]") {
  std::string input = "# Resume";
  std::vector<uint8_t> data(input.begin(), input.end());

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  options.extracted_at = "2025-12-31T23:59:59Z";

  auto result = ingestor->ingest_bytes(data, "md", options, id_gen, clock);

  REQUIRE(result.has_value());
  REQUIRE(result.value().meta.extracted_at.value() == "2025-12-31T23:59:59Z");
}

TEST_CASE("Resume ingestor rejects empty data", "[ingest][resume_ingestor]") {
  std::vector<uint8_t> empty_data;

  auto ingestor = create_resume_ingestor();
  core::DeterministicIdGenerator id_gen;
  core::FixedClock clock("2026-01-01T00:00:00Z");

  IngestOptions options;
  auto result = ingestor->ingest_bytes(empty_data, "md", options, id_gen, clock);

  REQUIRE(!result.has_value());
  REQUIRE(result.error() == "Empty input data");
}
