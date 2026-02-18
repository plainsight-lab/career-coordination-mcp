# Decision Records — v0.3 Slice 6

Decision records make match decisions **first-class queryable artifacts**. After every
`match_opportunity` pipeline run, a `DecisionRecord` is persisted that captures the full
"why" of the decision: which requirements were matched, by which atoms, with what evidence,
how many candidates were retrieved, and what the constitutional validation concluded.

---

## 1. Purpose

The matching pipeline produces a score and a validation status, but that result alone is
insufficient for audit, compliance, or iterative improvement. Decision records answer:

- *Why* was a candidate scored the way it was?
- *Which atoms* contributed to each requirement?
- *How many* embedding vs. lexical candidates were retrieved?
- *What* did the validation engine conclude, and which rules fired?

Decision records are **append-only derived artifacts**. They are never mutated after
recording. The canonical truth remains in the atoms and opportunities tables; decision
records are a durable snapshot of a pipeline run's provenance.

---

## 2. Schema (v5)

```sql
CREATE TABLE IF NOT EXISTS decision_records (
  decision_id    TEXT PRIMARY KEY,
  trace_id       TEXT NOT NULL,
  opportunity_id TEXT NOT NULL,
  artifact_id    TEXT NOT NULL,
  decision_json  TEXT NOT NULL,   -- full DecisionRecord JSON blob
  created_at     TEXT             -- nullable: null in deterministic test scenarios
);

CREATE INDEX IF NOT EXISTS idx_decision_records_trace
  ON decision_records(trace_id);
```

The `decision_json` column stores the full record as a stable, deterministically serialized
JSON blob. On read, the blob is deserialized and `created_at` is overridden from the
dedicated nullable column to preserve nullability.

---

## 3. Domain Type

```
DecisionRecord {
  decision_id:            string          // generated: "decision-<uuid>"
  trace_id:               string          // shared with audit trail
  artifact_id:            string          // "match-report-<opportunity_id>"
  created_at:             string|null     // ISO-8601; null in deterministic tests
  opportunity_id:         string
  requirement_decisions:  RequirementDecision[]
  retrieval_stats:        RetrievalStatsSummary
  validation_summary:     ValidationSummary
  version:                string          // "0.3"
}

RequirementDecision {
  requirement_text:  string
  atom_id:           string|null     // null if requirement was not matched
  evidence_tokens:   string[]
}

RetrievalStatsSummary {
  lexical_candidates:    number
  embedding_candidates:  number
  merged_candidates:     number
}

ValidationSummary {
  status:         string          // "accepted" | "rejected" | "blocked" | "needs_review"
  finding_count:  number          // total findings (all severities)
  fail_count:     number          // kFail + kBlock findings
  warn_count:     number          // kWarn findings
  top_rule_ids:   string[]        // sorted rule IDs from fail/block/warn findings
}
```

---

## 4. Serialization Invariants

- **Deterministic key ordering**: `nlohmann::json` uses `std::map` by default, so all
  object keys are sorted alphabetically. The same input always produces the same JSON string.
- **Stable `dump()`**: All JSON output can be compared as strings across identical inputs.
- **Null roundtrip**: `created_at` and `atom_id` are stored as JSON `null` when absent,
  and deserialized back to `std::nullopt`.

---

## 5. API

### app_service

```cpp
// Record a decision from a completed pipeline response.
// Emits DecisionRecorded audit event. Returns the generated decision_id.
std::string record_match_decision(
    const MatchPipelineResponse& pipeline_response,
    storage::IDecisionStore& decision_store,
    core::Services& services,
    core::IIdGenerator& id_gen,
    core::IClock& clock);

// Fetch a single decision by ID. Returns nullopt if not found.
std::optional<domain::DecisionRecord> fetch_decision(
    const std::string& decision_id,
    storage::IDecisionStore& decision_store);

// List all decisions for a trace, ordered by decision_id ASC.
std::vector<domain::DecisionRecord> list_decisions_by_trace(
    const std::string& trace_id,
    storage::IDecisionStore& decision_store);
```

### Wiring Note

`record_match_decision()` is a separate top-level function, not a parameter to
`run_match_pipeline()`. This keeps the `run_match_pipeline` signature unchanged and
preserves all 37+ existing callers. The MCP `match_opportunity` handler calls both
functions in sequence.

---

## 6. MCP Tools

### `get_decision`

```json
{
  "name": "get_decision",
  "inputSchema": {
    "type": "object",
    "properties": { "decision_id": { "type": "string" } },
    "required": ["decision_id"]
  }
}
```

Returns the full `DecisionRecord` JSON, or `{"error": "Decision not found: <id>"}`.

### `list_decisions`

```json
{
  "name": "list_decisions",
  "inputSchema": {
    "type": "object",
    "properties": { "trace_id": { "type": "string" } },
    "required": ["trace_id"]
  }
}
```

Returns:
```json
{
  "trace_id": "<trace_id>",
  "decisions": [ /* DecisionRecord[] ordered by decision_id ASC */ ]
}
```

---

## 7. CLI Commands

```bash
# Fetch a single decision record
ccmcp_cli get-decision --db career.db --decision-id decision-abc123

# List all decisions for a trace
ccmcp_cli list-decisions --db career.db --trace-id trace-xyz789
```

Output is pretty-printed JSON (2-space indent).

---

## 8. Audit Events

| Event type         | When                                    | Payload                                     |
|--------------------|-----------------------------------------|---------------------------------------------|
| `DecisionRecorded` | After `record_match_decision()` returns | `decision_id`, `opportunity_id`             |

The `DecisionRecorded` event is appended to the same audit log trace as the preceding
`MatchCompleted` and `ValidationCompleted` events, enabling full trace reconstruction.

---

## 9. Storage Architecture

```
match_opportunity (MCP tool)
  → run_match_pipeline()    → MatchReport + ValidationReport
  → record_match_decision() → DecisionRecord → decision_records table
                                             → audit_events (DecisionRecorded)
```

- **SQLite path** (`--db <path>`): uses `SqliteDecisionStore` backed by on-disk DB.
- **Ephemeral path** (no `--db`): uses `SqliteDecisionStore` backed by `:memory:` DB.
  Data is lost on process exit (same behavior as resume and index stores in this mode).
- **Tests**: use `InMemoryDecisionStore` or `SqliteDecisionStore(":memory:")` via
  `ensure_schema_v5()`.

---

## 10. Rebuild / Replay

Decision records are not automatically regenerated on schema upgrade. They represent
the provenance of past pipeline runs and must not be modified retroactively.

If a rebuild is required (e.g., after a schema migration error), use:
```bash
# Re-run the match pipeline — a new DecisionRecord will be created for the new run.
ccmcp_cli match --db career.db
# or via MCP:
match_opportunity { "opportunity_id": "<id>" }
```

Decision records for prior runs remain queryable unless the table is explicitly cleared.
