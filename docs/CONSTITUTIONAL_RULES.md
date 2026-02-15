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

## Rule Evaluation Order

Rules are evaluated in **fixed, deterministic order**:

1. **SCHEMA-001** (structural integrity)
2. **EVID-001** (evidence attribution)
3. **SCORE-001** (degenerate scoring)

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
- `test_validation_schema_block.cpp` - SCHEMA-001 violations
- `test_validation_evidence_fail.cpp` - EVID-001 violations
- `test_validation_warn.cpp` - SCORE-001 warnings
- `test_validation_pass.cpp` - Valid reports and deterministic sorting

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

- **v0.1** (2026-02-14): Initial rule set (SCHEMA-001, EVID-001, SCORE-001)
