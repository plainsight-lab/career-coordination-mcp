#include "ccmcp/ingest/resume_ingestor.h"

#include "ccmcp/core/hashing.h"
#include "ccmcp/ingest/format_adapter.h"
#include "ccmcp/ingest/hygiene.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace ccmcp::ingest {

namespace {

/// Detect format from file extension
std::string detect_format_from_path(const std::string& path) {
  std::filesystem::path p(path);
  std::string ext = p.extension().string();

  // Normalize to lowercase
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (ext == ".md" || ext == ".markdown") {
    return "md";
  }
  if (ext == ".txt" || ext == ".text") {
    return "txt";
  }
  if (ext == ".pdf") {
    return "pdf";
  }
  if (ext == ".docx") {
    return "docx";
  }

  // Default to txt for unknown extensions
  return "txt";
}

/// Read file into byte vector
core::Result<std::vector<uint8_t>, std::string> read_file_bytes(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return core::Result<std::vector<uint8_t>, std::string>::err("Failed to open file: " + path);
  }

  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
    return core::Result<std::vector<uint8_t>, std::string>::err("Failed to read file: " + path);
  }

  return core::Result<std::vector<uint8_t>, std::string>::ok(std::move(data));
}

/// Create format adapter for given format
std::unique_ptr<IFormatAdapter> create_adapter(const std::string& format) {
  if (format == "md" || format == "markdown") {
    return std::make_unique<MarkdownAdapter>();
  }
  if (format == "txt" || format == "text") {
    return std::make_unique<TextAdapter>();
  }
  if (format == "pdf") {
    return std::make_unique<PdfAdapter>();
  }
  if (format == "docx") {
    return std::make_unique<DocxAdapter>();
  }

  // Default to text adapter for unknown formats
  return std::make_unique<TextAdapter>();
}

}  // namespace

/// Default implementation of resume ingestor
class ResumeIngestor : public IResumeIngestor {
 public:
  IngestResult ingest_file(const std::string& file_path, const IngestOptions& options,
                           core::IIdGenerator& id_gen, core::IClock& clock) override {
    // Read file bytes
    auto bytes_result = read_file_bytes(file_path);
    if (!bytes_result.has_value()) {
      return IngestResult::err(bytes_result.error());
    }

    // Detect format
    std::string format = detect_format_from_path(file_path);

    // Build options with source path
    IngestOptions opts = options;
    if (!opts.source_path.has_value()) {
      opts.source_path = file_path;
    }

    return ingest_bytes(bytes_result.value(), format, opts, id_gen, clock);
  }

  IngestResult ingest_bytes(const std::vector<uint8_t>& data, const std::string& format,
                            const IngestOptions& options, core::IIdGenerator& id_gen,
                            core::IClock& clock) override {
    if (data.empty()) {
      return IngestResult::err("Empty input data");
    }

    // Compute source hash (deterministic hash of input bytes)
    std::string source_hash =
        "sha256:" + core::stable_hash64_hex(std::string(data.begin(), data.end()));

    // Create format adapter
    auto adapter = create_adapter(format);

    // Extract markdown
    auto extract_result = adapter->extract(data);
    if (!extract_result.has_value()) {
      return IngestResult::err("Extraction failed: " + extract_result.error().message);
    }

    std::string resume_md = extract_result.value();

    // Apply hygiene normalization (if enabled)
    if (options.enable_hygiene) {
      resume_md = hygiene::apply_hygiene(resume_md);
    }

    // Compute resume hash (hash of normalized markdown)
    std::string resume_hash = "sha256:" + core::stable_hash64_hex(resume_md);

    // Build metadata
    ResumeMeta meta;
    meta.source_path = options.source_path;
    meta.source_hash = source_hash;
    meta.extraction_method = adapter->extraction_method();
    meta.ingestion_version = "0.3";

    // Use provided timestamp or generate one
    if (options.extracted_at.has_value()) {
      meta.extracted_at = options.extracted_at;
    } else {
      meta.extracted_at = clock.now_iso8601();
    }

    // Generate resume ID
    core::ResumeId resume_id{id_gen.next("resume-")};

    // Build result
    IngestedResume resume;
    resume.resume_id = resume_id;
    resume.resume_md = resume_md;
    resume.resume_hash = resume_hash;
    resume.meta = meta;
    resume.created_at = std::nullopt;  // Will be set by store when persisted

    return IngestResult::ok(resume);
  }
};

/// Factory function to create default ingestor
std::unique_ptr<IResumeIngestor> create_resume_ingestor() {
  return std::make_unique<ResumeIngestor>();
}

}  // namespace ccmcp::ingest
