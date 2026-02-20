#include "tokenize_resume_logic.h"

#include "ccmcp/core/ids.h"
#include "ccmcp/domain/resume_token_ir.h"
#include "ccmcp/tokenization/deterministic_lexical_tokenizer.h"
#include "ccmcp/tokenization/stub_inference_tokenizer.h"

#include <iostream>
#include <memory>
#include <string>

int execute_tokenize_resume(const std::string& resume_id_str, const std::string& mode,
                            ccmcp::ingest::IResumeStore& resume_store,
                            ccmcp::tokenization::IResumeTokenStore& token_store) {
  const ccmcp::core::ResumeId resume_id{resume_id_str};
  auto resume_opt = resume_store.get(resume_id);
  if (!resume_opt.has_value()) {
    std::cerr << "Resume not found: " << resume_id_str << "\n";
    return 1;
  }

  const auto& resume = resume_opt.value();

  std::unique_ptr<ccmcp::tokenization::ITokenizationProvider> tokenizer;
  if (mode == "deterministic") {
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
