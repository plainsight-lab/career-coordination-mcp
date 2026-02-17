#include "ccmcp/domain/resume_token_ir_json.h"

namespace ccmcp::domain {

nlohmann::json token_ir_to_json(const ResumeTokenIR& ir) {
  nlohmann::json j;

  j["schema_version"] = ir.schema_version;
  j["source_hash"] = ir.source_hash;

  // Tokenizer metadata
  nlohmann::json tokenizer_json;
  tokenizer_json["type"] = tokenizer_type_to_string(ir.tokenizer.type);
  if (ir.tokenizer.model_id.has_value()) {
    tokenizer_json["model_id"] = ir.tokenizer.model_id.value();
  }
  if (ir.tokenizer.prompt_version.has_value()) {
    tokenizer_json["prompt_version"] = ir.tokenizer.prompt_version.value();
  }
  j["tokenizer"] = tokenizer_json;

  // Tokens (already sorted by map)
  j["tokens"] = ir.tokens;

  // Spans
  nlohmann::json spans_json = nlohmann::json::array();
  for (const auto& span : ir.spans) {
    nlohmann::json span_json;
    span_json["token"] = span.token;
    span_json["start_line"] = span.start_line;
    span_json["end_line"] = span.end_line;
    spans_json.push_back(span_json);
  }
  j["spans"] = spans_json;

  return j;
}

ResumeTokenIR token_ir_from_json(const nlohmann::json& j) {
  ResumeTokenIR ir;

  ir.schema_version = j.value("schema_version", "0.3");
  ir.source_hash = j.value("source_hash", "");

  // Tokenizer metadata
  if (j.contains("tokenizer")) {
    const auto& tokenizer_json = j["tokenizer"];
    std::string type_str = tokenizer_json.value("type", "deterministic-lexical");
    ir.tokenizer.type =
        string_to_tokenizer_type(type_str).value_or(TokenizerType::kDeterministicLexical);

    if (tokenizer_json.contains("model_id")) {
      ir.tokenizer.model_id = tokenizer_json["model_id"];
    }
    if (tokenizer_json.contains("prompt_version")) {
      ir.tokenizer.prompt_version = tokenizer_json["prompt_version"];
    }
  }

  // Tokens
  if (j.contains("tokens")) {
    ir.tokens = j["tokens"].get<std::map<std::string, std::vector<std::string>>>();
  }

  // Spans
  if (j.contains("spans")) {
    for (const auto& span_json : j["spans"]) {
      TokenSpan span;
      span.token = span_json.value("token", "");
      span.start_line = span_json.value("start_line", 1);
      span.end_line = span_json.value("end_line", 1);
      ir.spans.push_back(span);
    }
  }

  return ir;
}

std::string token_ir_to_json_string(const ResumeTokenIR& ir) {
  return token_ir_to_json(ir).dump();  // Compact JSON
}

}  // namespace ccmcp::domain
