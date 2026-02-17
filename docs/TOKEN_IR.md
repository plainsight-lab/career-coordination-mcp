# Token IR (Intermediate Representation)

**Version:** v0.3
**Status:** Implemented

## Overview

The Token IR layer extracts and categorizes semantic tokens from resume markdown to enable:
- **Constitutional validation** of token derivation (anti-hallucination)
- **Structured semantic representation** for future embedding and retrieval
- **Provenance binding** to source resume via hash
- **Deterministic reproducibility** of token extraction

Token IR is an **intermediate artifact** between raw resume ingestion and eventual ML-based semantic analysis. In v0.3, it uses **deterministic lexical tokenization** as the primary method, with a **stub inference tokenizer** for testing future inference-assisted workflows.

---

## Schema (v0.3)

### ResumeTokenIR

```cpp
struct ResumeTokenIR {
  std::string schema_version = "0.3";
  std::string source_hash;              // SHA-256 hash of canonical resume markdown
  TokenizerMetadata tokenizer;
  std::map<std::string, std::vector<std::string>> tokens;  // category -> tokens
  std::vector<TokenSpan> spans;         // optional line span references
};
```

### TokenizerMetadata

```cpp
struct TokenizerMetadata {
  TokenizerType type;                   // kDeterministicLexical | kInferenceAssisted
  std::optional<std::string> model_id;  // for inference-assisted only
  std::optional<std::string> prompt_version;  // for inference-assisted only
};
```

### TokenSpan

```cpp
struct TokenSpan {
  std::string token;
  uint32_t start_line;  // 1-indexed
  uint32_t end_line;    // 1-indexed, inclusive
};
```

---

## Tokenization Modes

### 1. Deterministic Lexical (Primary - v0.3)

**Class:** `DeterministicLexicalTokenizer`
**Type:** `TokenizerType::kDeterministicLexical`

- Uses existing `core::tokenize_ascii()` function
- **Lowercase ASCII only** (a-z, 0-9)
- **Minimum token length:** 2 characters
- **Stop word filtering:** Removes ~150 common English words (configurable)
  - Articles: a, an, the
  - Prepositions: in, on, at, to, from, of, for, with, etc.
  - Conjunctions: and, or, but, etc.
  - Pronouns: i, you, he, she, it, we, they, etc.
  - Common verbs: is, are, was, were, be, have, has, had, etc.
- **No spans** (optional for deterministic mode)
- **Single category:** "lexical"
- **Fully deterministic and reproducible**

**Stop Word Filtering:**
By default, common English stop words are filtered to improve semantic quality. This can be disabled by passing `filter_stop_words=false` to the constructor:

```cpp
// Default: stop words filtered
DeterministicLexicalTokenizer tokenizer1;

// Disable stop word filtering
DeterministicLexicalTokenizer tokenizer2(false);
```

**Rationale:**
Deterministic tokenization provides a **provable baseline** for constitutional validation. All tokens can be verified against the canonical resume without relying on ML inference, ensuring zero hallucination risk. Stop word removal improves semantic quality by filtering out low-value words (articles, prepositions, common verbs) that don't contribute to matching or retrieval.

### 2. Stub Inference (Testing Only - v0.3)

**Class:** `StubInferenceTokenizer`
**Type:** `TokenizerType::kInferenceAssisted`
**Model ID:** `"stub-inference-v1"`

- **Deterministic pattern-based categorization** (no real ML)
- Categories: `skills`, `domains`, `entities`, `roles`
- Produces reproducible output for testing validation rules
- **Not for production use** - placeholder for future LLM-based tokenization

**Categories:**
- **skills:** Programming languages, tools (e.g., python, kubernetes, docker)
- **domains:** Technical domains (e.g., distributed, architecture, infrastructure)
- **entities:** Organizations, products (e.g., google, aws)
- **roles:** Job titles, responsibilities (e.g., architect, engineer)

**Future:** Will be replaced with actual LLM-based inference in v0.4+, using constitutional validation to prevent hallucinations.

---

## JSON Serialization

Token IR is serialized to JSON with **deterministic, stable ordering**:

```json
{
  "schema_version": "0.3",
  "source_hash": "sha256:abc123...",
  "tokenizer": {
    "type": "deterministic-lexical"
  },
  "tokens": {
    "lexical": ["architecture", "cpp", "distributed", "python", "systems"]
  },
  "spans": []
}
```

**Guarantees:**
- **Sorted keys** in all objects (map iteration order)
- **Compact format** (no whitespace)
- **Round-trip stability** (serialize → parse → serialize = identical)

---

## Storage

### SQLite Schema (v3)

```sql
CREATE TABLE resume_token_ir (
  token_ir_id TEXT PRIMARY KEY,
  resume_id TEXT NOT NULL,
  token_ir_json TEXT NOT NULL,
  created_at TEXT NOT NULL,
  FOREIGN KEY(resume_id) REFERENCES resumes(resume_id)
    ON DELETE CASCADE
);

CREATE INDEX idx_resume_token_ir_resume ON resume_token_ir(resume_id);
```

### Storage Interface

**Interface:** `IResumeTokenStore`
**Implementation:** `SqliteResumeTokenStore`

**Operations:**
- `upsert(token_ir_id, resume_id, token_ir)` - Store/update token IR
- `get(token_ir_id)` - Retrieve by token IR ID
- `get_by_resume(resume_id)` - Retrieve by resume ID
- `list_all()` - List all token IRs (deterministic order: ORDER BY token_ir_id)

---

## Constitutional Validation

Token IR artifacts are validated using **5 constitutional rules** (TOK-001 through TOK-005). See [CONSTITUTIONAL_RULES.md](CONSTITUTIONAL_RULES.md#token-ir-rules) for details.

**Key Validations:**
- **Provenance binding** (source_hash must match canonical resume)
- **Format constraints** (lowercase ASCII, length >= 2)
- **Span bounds checking** (line references within resume)
- **Anti-hallucination** (all tokens derivable via tokenize_ascii)
- **Volume thresholds** (warn on excessive tokenization)

---

## CLI Usage

### Tokenize Resume

```bash
ccmcp_cli tokenize-resume <resume-id> [--db <db-path>] [--mode <deterministic|stub-inference>]
```

**Example:**
```bash
# Tokenize resume with deterministic lexical mode (default)
ccmcp_cli tokenize-resume resume-123 --db data/ccmcp.db --mode deterministic

# Output:
# Success!
#   Token IR ID: resume-123-deterministic-lexical
#   Source hash: sha256:abc123...
#   Tokenizer type: deterministic-lexical
#   Token counts by category:
#     lexical: 42
#   Total tokens: 42
#   Spans: 0
```

---

## Design Rationale

### Why Token IR?

1. **Constitutional Validation Gate:**
   Token IR provides a **typed artifact layer** for validating semantic extraction before ML processing. This prevents hallucinations from propagating downstream.

2. **Deterministic Baseline:**
   Deterministic lexical tokenization establishes a **provable ground truth** that can be used to validate inference-assisted results.

3. **Separation of Concerns:**
   Tokenization is separated from embedding/retrieval, allowing independent evolution and testing of each layer.

4. **Future-Proof:**
   The schema supports both deterministic and inference-assisted modes, enabling smooth transition to LLM-based tokenization in v0.4+.

### Why Not Extract Tokens During Ingestion?

Token IR is **derived from** the canonical resume markdown, not stored during ingestion, because:
- **Multiple tokenization modes** may be needed (deterministic, inference, hybrid)
- **Tokenization is cheaper to recompute** than re-ingest from source files
- **Resume hash provides stable provenance** regardless of tokenization method

### Why Store Token IR in SQLite?

- **Audit trail** of tokenization runs
- **Comparison** between deterministic and inference results
- **Retrieval optimization** (precomputed tokens for matching)
- **Testing** of constitutional validation rules

---

## Testing

### Test Coverage

- **Deterministic tokenization stability** (same input = same output)
- **JSON serialization round-trip** (serialize → parse → serialize)
- **SQLite persistence** (upsert, get, list_all, ordering)
- **Constitutional validation** (TOK-001 through TOK-005)
- **Tokenizer mode switching** (deterministic ↔ stub-inference)

### Test Files

- `tests/test_tokenization.cpp` - Tokenization stability and enum conversions
- `tests/test_token_ir_validation.cpp` - Constitutional rule validation
- `tests/test_sqlite_resume_token_store.cpp` - SQLite round-trip and ordering

---

## Production Workflow

The recommended production tokenization flow is **two-pass**:

### Pass 1: Deterministic Baseline
Run `DeterministicLexicalTokenizer` (with stop word filtering) to establish a **provable ground truth**:
- Generates high-quality semantic tokens (technical terms, domain-specific vocabulary)
- Passes constitutional validation (TOK-004 anti-hallucination gate)
- Provides baseline for matching and retrieval

### Pass 2: Probabilistic Enrichment (v0.4+)
Run inference-based tokenizer to **enrich** the baseline:
- Categorizes tokens (skills, domains, entities, roles)
- Extracts additional semantic information (achievements, certifications)
- Adds context and relationships
- **Validated against deterministic baseline** (anti-hallucination)

This approach provides:
- **Safety:** Deterministic baseline prevents hallucinations
- **Quality:** Inference enrichment adds semantic depth
- **Auditability:** Both passes stored and comparable
- **Graceful degradation:** Can fall back to deterministic if inference fails

---

## Future Work (v0.4+)

1. **LLM-Based Inference Tokenization:**
   - Replace stub tokenizer with real LLM-based categorization
   - Use constitutional validation (TOK-004) to prevent hallucinations
   - Support prompt versioning and model ID tracking

2. **Hybrid Tokenization:**
   - Combine deterministic baseline with inference enhancement
   - Use deterministic tokens as "required" and inference as "enrichment"

3. **Token Spans:**
   - Implement line span tracking for inference mode
   - Enable source highlighting and provenance tracing

4. **Category Expansion:**
   - Add categories: `achievements`, `certifications`, `education`
   - Support custom category definitions via configuration

5. **Token Deduplication:**
   - Across categories (same token in multiple categories)
   - Canonical form selection (e.g., "C++" vs "cpp")

---

## References

- [CONSTITUTIONAL_RULES.md](CONSTITUTIONAL_RULES.md#token-ir-rules) - TOK-001 through TOK-005 validation rules
- [RESUME_INGESTION.md](RESUME_INGESTION.md) - Upstream resume ingestion pipeline
- `include/ccmcp/domain/resume_token_ir.h` - Token IR schema types
- `include/ccmcp/tokenization/tokenization_provider.h` - Tokenization interface
