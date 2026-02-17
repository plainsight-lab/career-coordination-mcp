#include "ccmcp/tokenization/stub_inference_tokenizer.h"

#include "ccmcp/core/normalization.h"

#include <algorithm>
#include <set>

namespace ccmcp::tokenization {

namespace {

// Simple pattern matching for stub categorization
bool is_likely_skill(const std::string& token) {
  // Programming languages, tools, frameworks
  static const std::set<std::string> known_skills = {
      "python", "java",    "cpp",        "c",          "javascript", "typescript", "rust",
      "go",     "sql",     "docker",     "kubernetes", "aws",        "gcp",        "azure",
      "react",  "angular", "vue",        "django",     "flask",      "spring",     "node",
      "git",    "ci",      "cd",         "terraform",  "ansible",    "jenkins",    "cmake",
      "make",   "gradle",  "maven",      "pytest",     "junit",      "testing",    "tdd",
      "agile",  "scrum",   "rest",       "api",        "graphql",    "mongodb",    "postgresql",
      "mysql",  "redis",   "kafka",      "spark",      "hadoop",     "machine",    "learning",
      "deep",   "neural",  "tensorflow", "pytorch",    "scikit",     "pandas",     "numpy"};
  return known_skills.count(token) > 0;
}

bool is_likely_domain(const std::string& token) {
  static const std::set<std::string> known_domains = {
      "backend",     "frontend",      "fullstack",     "devops",       "infrastructure",
      "security",    "mobile",        "web",           "cloud",        "distributed",
      "systems",     "architecture",  "database",      "data",         "engineering",
      "software",    "platform",      "network",       "embedded",     "performance",
      "scalability", "reliability",   "observability", "automation",   "testing",
      "quality",     "assurance",     "integration",   "deployment",   "monitoring",
      "analytics",   "visualization", "reporting",     "optimization", "ai",
      "ml"};
  return known_domains.count(token) > 0;
}

bool is_likely_role(const std::string& token) {
  static const std::set<std::string> known_roles = {
      "engineer",   "developer", "architect",  "lead",           "senior",    "staff",
      "principal",  "manager",   "director",   "head",           "vp",        "cto",
      "technical",  "software",  "platform",   "infrastructure", "site",      "reliability",
      "consultant", "analyst",   "specialist", "coordinator",    "associate", "intern"};
  return known_roles.count(token) > 0;
}

}  // namespace

domain::ResumeTokenIR StubInferenceTokenizer::tokenize(const std::string& resume_md,
                                                       const std::string& source_hash) {
  domain::ResumeTokenIR ir;

  ir.schema_version = "0.3";
  ir.source_hash = source_hash;

  // Tokenizer metadata
  ir.tokenizer.type = domain::TokenizerType::kInferenceAssisted;
  ir.tokenizer.model_id = "stub-inference-v1";
  ir.tokenizer.prompt_version = "resume-tokenizer-stub-v1";

  // Tokenize using core function
  auto all_tokens = core::tokenize_ascii(resume_md);

  // Categorize tokens using simple patterns
  std::set<std::string> skills;
  std::set<std::string> domains;
  std::set<std::string> roles;
  std::set<std::string> entities;  // Everything else

  for (const auto& token : all_tokens) {
    if (is_likely_skill(token)) {
      skills.insert(token);
    } else if (is_likely_domain(token)) {
      domains.insert(token);
    } else if (is_likely_role(token)) {
      roles.insert(token);
    } else {
      entities.insert(token);
    }
  }

  // Store categorized tokens (sorted automatically by set)
  if (!skills.empty()) {
    ir.tokens["skills"] = std::vector<std::string>(skills.begin(), skills.end());
  }
  if (!domains.empty()) {
    ir.tokens["domains"] = std::vector<std::string>(domains.begin(), domains.end());
  }
  if (!roles.empty()) {
    ir.tokens["roles"] = std::vector<std::string>(roles.begin(), roles.end());
  }
  if (!entities.empty()) {
    ir.tokens["entities"] = std::vector<std::string>(entities.begin(), entities.end());
  }

  // Spans are optional (not implemented for stub)
  // Real implementation would track line numbers during parsing

  return ir;
}

}  // namespace ccmcp::tokenization
