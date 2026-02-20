#include "ingest_resume_logic.h"

#include <iostream>
#include <string>

int execute_ingest_resume(const std::string& file_path, ccmcp::ingest::IResumeIngestor& ingestor,
                          ccmcp::ingest::IResumeStore& resume_store,
                          ccmcp::core::IIdGenerator& id_gen, ccmcp::core::IClock& clock) {
  std::cout << "Ingesting resume from: " << file_path << "\n";

  ccmcp::ingest::IngestOptions ingest_options;
  auto result = ingestor.ingest_file(file_path, ingest_options, id_gen, clock);

  if (!result.has_value()) {
    std::cerr << "Ingestion failed: " << result.error() << "\n";
    return 1;
  }

  const auto& ingested_resume = result.value();
  resume_store.upsert(ingested_resume);

  std::cout << "Success!\n";
  std::cout << "  Resume ID: " << ingested_resume.resume_id.value << "\n";
  std::cout << "  Resume hash: " << ingested_resume.resume_hash << "\n";
  std::cout << "  Extraction method: " << ingested_resume.meta.extraction_method << "\n";
  std::cout << "  Ingestion version: " << ingested_resume.meta.ingestion_version << "\n";
  if (ingested_resume.meta.source_path.has_value()) {
    std::cout << "  Source path: " << ingested_resume.meta.source_path.value() << "\n";
  }
  std::cout << "  Resume content length: " << ingested_resume.resume_md.size() << " bytes\n";

  return 0;
}
