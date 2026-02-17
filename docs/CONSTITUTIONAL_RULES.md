# Constitutional Rules (v0.1)

This document describes the active rule set for constitutional validation in career-coordination-mcp.

**Version:** v0.1 (LOCKED)
**Last Updated:** 2026-02-14

---

## Purpose

Constitutional validation ensures MatchReport integrity through deterministic rule evaluation. Rules gate data quality and prevent invalid artifacts from propagating through the system.

---

## v0.1 Active Rule Set

### SCHEMA-001 (BLOCK)

**Purpose:** Ensure MatchReport structural integrity

**Severity:** BLOCK (validation fails immediately)

**Conditions Checked:**

1. `overall_score` exists and is >= 0.0
2. `requirement_matches` exists and is an array
3. For each `RequirementMatch`:
   - `requirement_text` is non-empty
   - `best_score` >= 0.0
   - **Matched flag consistency:**
     - If `matched == true` → `contributing_atom_id` must exist and be non-empty
     - If `matched == false` → `contributing_atom_id` must be empty or null

**Violation Behavior:**

- Emit Finding with severity `BLOCK`
- Validation status: `BLOCKED`
- Execution halts (artifact rejected)

**Example Violation:**

```json
{
  "requirement_matches": [
    {
      "requirement_text": "Python experience",
      "matched": true,
      "best_score": 0.5,
      "contributing_atom_id": null  // ❌ VIOLATION: matched=true but no atom
    }
  ]
}
```

---

### EVID-001 (FAIL)

**Purpose:** Ensure evidence attribution exists for matched requirements

**Severity:** FAIL (validation fails but does not block)

**Conditions Checked:**

For each `RequirementMatch` where `matched == true`:

1. `contributing_atom_id` must not be empty (checked by SCHEMA-001)
2. `evidence_tokens` must not be empty

**Violation Behavior:**

- Emit Finding with severity `FAIL`
- Validation status: `REJECTED` (if no BLOCK findings)
- Artifact is rejected but not blocked

**Example Violation:**

```json
{
  "requirement_matches": [
    {
      "requirement_text": "Python experience",
      "matched": true,
      "best_score": 0.5,
      "contributing_atom_id": "atom-123",
      "evidence_tokens": []  // ❌ VIOLATION: matched but no evidence
    }
  ]
}
```

**Note:** Unmatched requirements (`matched == false`) are NOT checked for evidence.

---

### SCORE-001 (WARN)

**Purpose:** Warn on degenerate scoring

**Severity:** WARN (informational, does not fail validation)

**Conditions Checked:**

- `overall_score == 0.0`
- AND at least one requirement exists in `requirement_matches`

**Violation Behavior:**

- Emit Finding with severity `WARN`
- Validation status: `NEEDS_REVIEW` (if no BLOCK or FAIL findings)
- Artifact is accepted but flagged for review

**Example Warning:**

```json
{
  "overall_score": 0.0,
  "requirement_matches": [
    {
      "requirement_text": "Python experience",
      "matched": false,
      "best_score": 0.0
    }
  ]
}
// ⚠️ WARNING: All requirement scores are zero.
```

**Note:** Zero score with zero requirements does NOT trigger warning.

---

## v0.3 Token IR Rules

### TOK-001 (BLOCK)

**Purpose:** Ensure Token IR provenance binding

**Severity:** BLOCK (validation fails immediately)

**Conditions Checked:**

- `source_hash` in Token IR must match canonical resume hash

**Violation Behavior:**

- Emit Finding with severity `BLOCK`
- Validation status: `BLOCKED`
- Prevents tokens from mismatched or unknown sources

**Rationale:** Provenance binding prevents token IR from being applied to wrong resume or orphaned tokens.

---

### TOK-002 (FAIL)

**Purpose:** Ensure token format constraints

**Severity:** FAIL (validation fails but does not block)

**Conditions Checked:**

For each token in all categories:

1. Token must be lowercase ASCII only (a-z, 0-9)
2. Token length must be >= 2 characters
3. No special characters or whitespace

**Violation Behavior:**

- Emit Finding with severity `FAIL` for each violating token
- Validation status: `REJECTED`
- Artifact is rejected

**Rationale:** Enforces deterministic tokenization format for matching and retrieval.

**Example Violation:**

```json
{
  "tokens": {
    "skills": ["C++", "python"]  // ❌ VIOLATION: "C++" contains uppercase and special chars
  }
}
```

---

### TOK-003 (FAIL)

**Purpose:** Ensure token span bounds

**Severity:** FAIL (validation fails but does not block)

**Conditions Checked:**

For each `TokenSpan`:

1. `start_line >= 1`
2. `end_line >= 1`
3. `start_line <= end_line`
4. `end_line <= canonical resume line count` (if canonical text available)

**Violation Behavior:**

- Emit Finding with severity `FAIL` for each invalid span
- Validation status: `REJECTED`

**Rationale:** Prevents invalid line references that break source tracing.

**Example Violation:**

```json
{
  "spans": [
    {"token": "python", "start_line": 5, "end_line": 3}  // ❌ start > end
  ]
}
```

---

### TOK-004 (FAIL)

**Purpose:** Prevent hallucinated tokens (anti-hallucination gate)

**Severity:** FAIL (validation fails but does not block)

**Conditions Checked:**

For each token in all categories:

- Token must appear in canonical resume markdown **OR**
- Token must be derivable via `core::tokenize_ascii(canonical_resume_text)`

**Violation Behavior:**

- Emit Finding with severity `FAIL` for each hallucinated token
- Validation status: `REJECTED`
- Prevents LLM hallucinations from propagating

**Rationale:** Critical anti-hallucination gate. Ensures all tokens are grounded in actual resume content, not invented by inference models.

**Example Violation:**

```json
{
  "tokens": {
    "skills": ["cpp", "kubernetes"]  // ❌ "kubernetes" not in resume
  }
}
// Canonical resume: "Software Engineer with C++ experience"
// Derivable tokens: ["software", "engineer", "with", "cpp", "experience"]
```

**Note:** This rule is the **primary defense** against LLM hallucinations in inference-assisted tokenization.

---

### TOK-005 (WARN)

**Purpose:** Warn on excessive tokenization

**Severity:** WARN (informational, does not fail validation)

**Conditions Checked:**

1. Total token count across all categories <= 500
2. Per-category token count <= 200

**Violation Behavior:**

- Emit Finding with severity `WARN`
- Validation status: `NEEDS_REVIEW` (if no BLOCK or FAIL)
- Artifact is accepted but flagged

**Rationale:** Excessive tokenization may indicate quality issues (over-extraction, noise).

**Example Warning:**

```json
{
  "tokens": {
    "skills": [/* 250 tokens */]  // ⚠️ WARNING: Exceeds per-category threshold
  }
}
```

---

## Rule Evaluation Order

Rules are evaluated in **fixed, deterministic order**:

### MatchReport Rules (v0.1):
1. **SCHEMA-001** (structural integrity)
2. **EVID-001** (evidence attribution)
3. **SCORE-001** (degenerate scoring)

### Token IR Rules (v0.3):
1. **TOK-001** (provenance binding)
2. **TOK-002** (format constraints)
3. **TOK-003** (span bounds)
4. **TOK-004** (anti-hallucination)
5. **TOK-005** (volume thresholds)

This order ensures:
- Structural checks happen first (fail fast)
- Evidence checks only run on structurally valid data
- Warnings are added after all critical checks

---

## Status Aggregation Logic

Final `ValidationReport.status` is determined by the **most severe finding**:

| Findings Present | Status | Description |
|-----------------|--------|-------------|
| Any BLOCK | `BLOCKED` | Critical structural violation |
| Any FAIL (no BLOCK) | `REJECTED` | Validation failed |
| Only WARN | `NEEDS_REVIEW` | Accepted with warnings |
| No findings | `ACCEPTED` | Fully valid |

**Aggregation Algorithm:**

```cpp
for each finding:
  if severity == BLOCK:
    status = BLOCKED
  else if severity == FAIL and status != BLOCKED:
    status = REJECTED
  else if severity == WARN and status == ACCEPTED:
    status = NEEDS_REVIEW
```

---

## Deterministic Finding Ordering

Findings are sorted before returning the `ValidationReport`:

**Sort Key 1:** Severity (descending)
- `BLOCK` (highest)
- `FAIL`
- `WARN`
- `PASS` (lowest)

**Sort Key 2:** Rule ID (lexicographic ascending)
- `EVID-001`
- `SCHEMA-001`
- `SCORE-001`

**Example Sorted Output:**

```
BLOCK: SCHEMA-001 - "RequirementMatch[0] is matched=true but missing contributing_atom_id"
FAIL:  EVID-001   - "Matched requirement 'Python' has no evidence_tokens"
WARN:  SCORE-001  - "All requirement scores are zero."
```

---

## Implementation

**Rule Definitions:** `src/constitution/validation_engine.cpp`

**Test Coverage:**

*MatchReport Rules (v0.1):*
- `test_validation_schema_block.cpp` - SCHEMA-001 violations
- `test_validation_evidence_fail.cpp` - EVID-001 violations
- `test_validation_warn.cpp` - SCORE-001 warnings
- `test_validation_pass.cpp` - Valid reports and deterministic sorting

*Token IR Rules (v0.3):*
- `test_token_ir_validation.cpp` - TOK-001 through TOK-005 validation
- `test_tokenization.cpp` - Tokenization stability and determinism
- `test_sqlite_resume_token_store.cpp` - SQLite persistence and round-trip

**Guarantees:**

1. **Determinism:** Same input produces identical output across runs
2. **Stable Ordering:** Findings always sorted by severity, then rule_id
3. **No Side Effects:** Validation is pure evaluation (does not modify artifacts)
4. **Thread Safety:** Validation engine is const after construction

---

## Future Extensions (Post-v0.1)

The following rules are **deferred** beyond v0.1:

- `GND-001`: Grounding validation (all claims traceable to atoms)
- `ATOM-001`: Atom verification requirements
- `TRACE-001`: Audit trail completeness

Future versions may add optional rules but must not break v0.1 invariants without explicit migration.

---

## Version History

- **v0.3** (2026-02-17): Added Token IR rules (TOK-001 through TOK-005)
- **v0.1** (2026-02-14): Initial rule set (SCHEMA-001, EVID-001, SCORE-001)
