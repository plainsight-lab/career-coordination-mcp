# RESUME_INGESTION.md

## Status

**v0.3 Implementation**: Core ingestion pipeline with PDF/DOCX/MD/TXT support ✅
**v0.4+ Target**: Token IR and inference-assisted semantic tokenization (see below)

------------------------------------------------------------------------

## v0.3 Implementation Summary

### What's Implemented

✅ **Multi-format ingestion**: PDF, DOCX, Markdown, TXT
✅ **Deterministic extraction**: All formats produce canonical markdown
✅ **Hygiene normalization**: Line endings, whitespace, blank lines, headings
✅ **SQLite persistence**: Schema v2 with resume and resume_meta tables
✅ **CLI integration**: `ccmcp_cli ingest-resume <file>`
✅ **Provenance tracking**: Source hash, resume hash, extraction method, version
✅ **Comprehensive tests**: 109 passing tests

### Format Adapter Details

| Format | Extraction Method | Implementation | Limitations |
|--------|------------------|----------------|-------------|
| **Markdown** | `md-pass-through-v1` | Pass-through | None |
| **Text** | `txt-wrap-v1` | Wrap in markdown | None |
| **DOCX** | `docx-extract-v1` | libzip + pugixml<br/>Parse word/document.xml | Text only, no images/tables |
| **PDF** | `pdf-text-extract-v1` | Custom text stream parser | Basic text-layer PDFs only<br/>No compressed streams<br/>No complex encodings<br/>**Will evaluate Poppler/MuPDF when containerizing** |

### Not Yet Implemented (v0.4+)

❌ Token IR generation (inference-assisted semantic tokenization)
❌ Token validation rules (TOK-001 through TOK-005)
❌ Deterministic lexical tokenizer fallback
❌ Caching and rebuild rules
❌ Full ingestion audit events

### Rationale for PDF Implementation

The custom PDF parser is a pragmatic v0.3 choice:
- Works for typical resume PDFs (text-layer exports)
- Zero dependency overhead
- Deterministic and testable
- **Trade-off**: Poppler (industry standard) requires 20+ dependencies and complex build tooling
- **Decision**: Revisit with Poppler or MuPDF when containerizing (easier dependency management)

See: Section 3.2 below for original design spec.

------------------------------------------------------------------------

# 1. Purpose

Define the canonical pipeline for ingesting a resume into the Career
Coordination MCP system.

The pipeline must:

-   Accept multiple file formats (PDF, DOCX, Markdown, TXT)
-   Produce a canonical Markdown representation
-   Generate a derived semantic token representation
-   Preserve provenance and auditability
-   Maintain deterministic authority boundaries
-   Allow inference where advantageous, without delegating authority

This system standardizes resume handling for:

-   Opportunity matching
-   Resume composition
-   Grounding validation
-   Audit traceability

------------------------------------------------------------------------

# 2. Architectural Principle

Resume ingestion is a multi-layer pipeline:

Raw File\
↓ (deterministic extraction)\
Canonical Markdown\
↓ (deterministic hygiene)\
Normalized Text\
↓ (inference-assisted tokenization)\
Token IR (derived)\
↓ (constitutional validation)\
Approved Token Set

Key boundary:

-   Markdown is canonical.
-   Tokens are derived.
-   Tokens never create atoms automatically.
-   Inference never creates new facts.

------------------------------------------------------------------------

# 3. Canonical Format: Markdown

All resumes are converted to canonical Markdown before further
processing.

## 3.1 Supported Input Formats

-   `.md`
-   `.txt`
-   `.docx`
-   `.pdf`

## 3.2 Deterministic Extraction

Extraction must:

-   Produce UTF-8 text
-   Preserve section structure where possible
-   Avoid heuristic rewriting
-   Record extraction method

Output:

-   `resume.md`
-   `resume.meta.json`

### resume.meta.json (minimum fields)

``` json
{
  "source_path": "original_resume.pdf",
  "source_hash": "sha256:...",
  "extraction_method": "pdf-text-extract-v1",
  "extracted_at": "ISO-8601 timestamp",
  "ingestion_version": "0.1"
}
```

The `source_hash` binds downstream derived artifacts to a specific
input.

------------------------------------------------------------------------

# 4. Deterministic Hygiene Phase

After extraction, perform deterministic normalization:

-   Normalize line endings (`\n`)
-   Trim trailing whitespace
-   Collapse repeated blank lines
-   Ensure headings use Markdown format where detectable

Do NOT:

-   Rewrite claims
-   Rephrase text
-   Reorder sections
-   Remove semantic content

A `resume_hash` is computed from normalized Markdown.

------------------------------------------------------------------------

# 5. Tokenization Strategy

Tokenization is a derived semantic layer.

Two tokenization modes:

1.  Deterministic lexical tokenizer (fallback)
2.  Inference-assisted semantic tokenizer (primary)

------------------------------------------------------------------------

# 6. Deterministic Lexical Tokenizer (Fallback)

Purpose:

-   Provide stable baseline token set
-   Used if inference tokenization fails validation

Rules:

-   ASCII-only lowercase
-   Replace non-alphanumeric ASCII characters with delimiters
-   Split on delimiters
-   Drop tokens shorter than 2 characters
-   Deduplicate
-   Sort lexicographically

Output example:

``` json
{
  "tokenizer_type": "deterministic",
  "tokens": ["architecture", "cxx20", "cmake", "governance"]
}
```

------------------------------------------------------------------------

# 7. Inference-Assisted Semantic Tokenizer

## 7.1 Purpose

Leverage model reasoning to extract semantic compiler-like tokens:

-   Skills
-   Domains
-   Tools
-   Entities
-   Roles
-   Artifacts
-   Outcomes

The model extracts structured lexemes --- it does not summarize.

------------------------------------------------------------------------

## 7.2 Token IR Schema

``` json
{
  "schema_version": "0.1",
  "source_hash": "sha256:...",
  "tokenizer": {
    "type": "llm",
    "model": "claude-x",
    "prompt_version": "resume-tokenizer-v1"
  },
  "tokens": {
    "skills": [],
    "domains": [],
    "entities": [],
    "roles": [],
    "artifacts": [],
    "outcomes": []
  },
  "spans": [
    {
      "token": "cmake",
      "start_line": 42,
      "end_line": 42
    }
  ]
}
```

------------------------------------------------------------------------

# 8. Validation of Token IR (CVE Gate)

Validation rules:

-   TOK-001 (BLOCK): `source_hash` must match canonical resume hash.
-   TOK-002 (FAIL): All tokens must be lowercase ASCII.
-   TOK-003 (FAIL): Tokens must reference valid line spans.
-   TOK-004 (FAIL): No hallucinated tokens (must appear in canonical
    text).
-   TOK-005 (WARN): Excessive token volume.

If validation fails:

-   Reject LLM tokens
-   Fall back to deterministic lexical tokens
-   Emit audit event

------------------------------------------------------------------------

# 9. Token Authority Model

Tokens are:

-   Derived
-   Rebuildable
-   Versioned
-   Non-authoritative

Tokens cannot:

-   Create ExperienceAtoms automatically
-   Modify resume text
-   Override canonical content

Atom creation requires explicit human authority.

------------------------------------------------------------------------

# 10. Provenance & Audit

Each ingestion run emits:

-   IngestionStarted
-   ExtractionCompleted
-   HygieneCompleted
-   TokenizationAttempted
-   TokenValidationPassed / Failed
-   IngestionCompleted

Each event includes:

-   trace_id
-   source_hash
-   resume_hash
-   tokenizer metadata

------------------------------------------------------------------------

# 11. Caching and Rebuild Rules

Derived artifacts are tied to `resume_hash`.

If unchanged:

-   Skip re-extraction
-   Skip re-tokenization
-   Reuse prior Token IR

If tokenizer model or prompt changes:

-   Re-tokenize
-   Retain prior versions if desired

------------------------------------------------------------------------

# 12. Explicit Non-Goals

The ingestion pipeline will not:

-   Rewrite resume content
-   Generate resume improvements automatically
-   Modify canonical markdown silently
-   Create atoms automatically

Resume modification requires explicit workflow.

------------------------------------------------------------------------

# 13. Integration with Career Coordinator

In v0.2+, matching may use:

-   ExperienceAtoms (canonical)
-   Opportunity requirements
-   Resume Token IR (derived)

Matching must:

-   Prefer canonical atoms
-   Use tokens for recall expansion only
-   Remain explainable (token → span → source)

------------------------------------------------------------------------

# 14. Alignment with PlainSight Governance

This pipeline enforces:

-   Canonical truth in files
-   Derived semantic layers
-   Explicit validation gates
-   Auditability
-   Human authority
-   Deterministic fallback behavior

It embodies:

Deterministic inference enhanced systems\
Not inference-governed systems.
