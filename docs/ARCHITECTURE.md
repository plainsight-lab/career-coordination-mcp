# Purpose

career-coordination-mcp is a deterministic career coordination engine exposed through the
Model Context Protocol (MCP). It performs structured opportunity matching, resume ingestion,
token IR extraction, index build, outreach state tracking, and match decision provenance —
all with a complete, auditable trail.

The system may optionally use LLMs for text generation, but all decisions are constrained
and validated by deterministic rules. If LLM output cannot be validated, it is rejected.

**Current version:** v0.3 (complete — 6 slices, 175 tests, 0 failures)

---

# Design Principles

- Deterministic first: All selection, matching, and eligibility decisions are rule-based and testable.
- LLM as instrument: LLMs draft prose; deterministic validators enforce constraints.
- Immutable facts: "Experience atoms" are immutable capability statements with verifiers.
- Auditability: Every recommendation can be explained as: inputs → rules → outputs.
- No double outreach: Outreach is governed by a state machine and contact identity resolution.
- Error correction is explicit: Errors become tracked objects with correction paths.

---

# System Layers

```
┌─────────────────────────────────────────────────┐
│  Transport Layer                                │
│  apps/ccmcp_cli/   apps/mcp_server/             │
│  (CLI commands)    (JSON-RPC 2.0 over stdio)    │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│  Application Layer (app_service)                │
│  run_match_pipeline()   run_index_build()        │
│  run_ingest_pipeline()  record_match_decision()  │
│  apply_interaction_event()                      │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│  Domain + Rule Layer                            │
│  Matcher  ValidationEngine  ConstitutionalRules │
│  ResumeIngestor  TokenIR  DeterministicClock    │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│  Storage Interfaces                             │
│  IAtomRepository       IOpportunityRepository   │
│  IInteractionRepository  IAuditLog              │
│  IResumeStore          IResumeTokenStore        │
│  IIndexRunStore        IDecisionStore           │
│  IEmbeddingIndex       IEmbeddingProvider       │
│  IInteractionCoordinator                        │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│  Storage Implementations                        │
│  SQLite (primary)   InMemory (test/ephemeral)   │
│  Redis (optional — interaction coordination)    │
│  SqliteEmbeddingIndex (vector persistence)      │
└─────────────────────────────────────────────────┘
```

---

# Components (v0.3)

## Experience Atom Store

Canonical, versioned "atoms" describing verified capabilities.

- Interface: `IAtomRepository`
- Implementations: `SqliteAtomRepository` (persistent), `InMemoryAtomRepository` (ephemeral)
- Atoms are **immutable** — new versions create new records; old remain unchanged.
- Only verified atoms (`atom.verified == true`) are used in matching.

## Opportunity Store

Structured representations of job postings with normalized requirements.

- Interface: `IOpportunityRepository`
- Implementations: `SqliteOpportunityRepository`, `InMemoryOpportunityRepository`

## Resume Ingestion Pipeline

Converts multi-format resume files into canonical markdown for downstream processing.

- Interface: `IResumeIngestor`, `IResumeStore`
- Formats: Markdown (pass-through), TXT (wrap), DOCX (libzip + pugixml), PDF (custom text-stream parser)
- All extraction methods are deterministic and produce canonical markdown.
- Hygiene normalization: line endings, trailing whitespace, repeated blank lines.
- Produces: `resume_hash` (SHA-256 of normalized content), `source_hash` (stable_hash64_hex for drift detection).
- SQLite schema v2 (`resumes`, `resume_meta` tables).
- See: [RESUME_INGESTION.md](RESUME_INGESTION.md)

## Token IR Generation

Extracts and categorizes semantic tokens from ingested resumes.

- Primary: `DeterministicLexicalTokenizer` — lowercase ASCII, stop-word filtered, single "lexical" category.
- Testing: `StubInferenceTokenizer` — deterministic pattern-based, multi-category (skills/domains/entities/roles).
- Token IR is a **derived artifact** — always rebuildable from canonical resume markdown.
- Validated by the Constitutional Validation Engine before acceptance.
- SQLite schema v3 (`resume_token_ir` table).
- See: [TOKEN_IR.md](TOKEN_IR.md)

## Opportunity Matching Engine

Scores opportunities against atoms using deterministic lexical overlap or hybrid (lexical + embedding) retrieval.

- Interface entry point: `run_match_pipeline()` in `app_service`
- Lexical: `score = |R ∩ A| / |R|` per requirement; tie-break by lexicographic `atom_id`.
- Hybrid: lexical candidates ∪ embedding candidates → merged → scored with weighted blend.
- Only verified atoms are considered.
- Produces: `MatchReport` (per-requirement atom attribution, evidence tokens, scores).

## Constitutional Validation Engine (CVE)

Deterministic rule evaluation subsystem. Validates artifacts before acceptance.

- Interface: `ValidationEngine::validate(artifact_view, context)` → `ValidationReport`
- Rule severities: BLOCK (hard stop) > FAIL (reject) > WARN (flag) > PASS
- Status aggregation: BLOCKED > REJECTED > NEEDS_REVIEW > ACCEPTED
- Findings are sorted deterministically: severity descending, then rule_id ascending.

**Active rule packs (v0.3):**

| Rule ID | Severity | Applies to | Purpose |
|---------|----------|-----------|---------|
| SCHEMA-001 | BLOCK | MatchReport | Structural integrity (matched/atom_id consistency) |
| EVID-001 | FAIL | MatchReport | Evidence attribution (matched requirements must have evidence_tokens) |
| SCORE-001 | WARN | MatchReport | Degenerate scoring (zero score with non-zero requirements) |
| TOK-001 | BLOCK | Token IR | Provenance binding (source_hash must match resume) |
| TOK-002 | FAIL | Token IR | Format constraints (lowercase ASCII, length >= 2) |
| TOK-003 | FAIL | Token IR | Span bounds (line references within resume) |
| TOK-004 | FAIL | Token IR | Anti-hallucination (tokens must be derivable from resume text) |
| TOK-005 | WARN | Token IR | Excessive tokenization (>500 total or >200 per category) |

See: [CONSTITUTIONAL_RULES.md](CONSTITUTIONAL_RULES.md)

## Embedding Index (Vector Layer)

Derived similarity index for hybrid retrieval.

- Interface: `IEmbeddingIndex` (upsert, query, get)
- `InMemoryEmbeddingIndex`: ephemeral; used for testing and default server mode.
- `SqliteEmbeddingIndex`: persistent; fills the LanceDB architectural slot (no C++ LanceDB SDK available in vcpkg). Stored in a separate SQLite file (`vectors.db`).
- `LanceDBEmbeddingIndex`: reserved stub — throws on all methods. Migration target when a C++ SDK becomes available.
- `NullEmbeddingIndex`: explicit opt-out; returns empty results.
- Embeddings are **derived artifacts** — rebuildable from canonical sources at any time.
- See: [VECTORDB_BACKEND.md](VECTORDB_BACKEND.md)

## Embedding Provider

- Interface: `IEmbeddingProvider` (embed_text, dimension, provider_id)
- `DeterministicStubEmbeddingProvider`: deterministic hash-based vectors. Used in all tests and current production paths. No real ML.
- Real provider integration (OpenAI, local models) deferred to v0.4+.

## Embedding Lifecycle + Index Build Pipeline

Produces and persists embeddings with full provenance.

- Entry point: `run_index_build()` in `app_service`
- Drift detection: compares `source_hash` (stable hash of canonical text) against last completed run for the same provider/model/prompt scope. Skips unchanged artifacts; re-indexes stale ones.
- Provenance: `index_runs` table (per-run metadata) + `index_entries` table (per-artifact hashes).
- SQLite schema v4 (`index_runs`, `index_entries` tables).
- Known limitation (WARN-001): CLI `index-build` resets `DeterministicIdGenerator` on each invocation, defeating drift detection. MCP server path (shared generator across requests) is unaffected.
- See: [INDEXING.md](INDEXING.md)

## Outreach State Machine

Governs contact interaction lifecycle; prevents duplicate outreach.

- Interface: `IInteractionCoordinator`
- Implementations: `InMemoryInteractionCoordinator`, `RedisInteractionCoordinator`
- States: `kDraft` → `kReady` → `kSent` → `kResponded` → `kClosed`
- Events: `kPrepare`, `kSend`, `kReceiveReply`, `kClose`
- Idempotency: transitions tracked by `idempotency_key` to prevent double-apply.
- SQLite schema v1 (`interactions` table) + optional Redis for durable coordination.

## Interaction Audit Log

Append-only record of all decisions, state transitions, and pipeline events.

- Interface: `IAuditLog`
- Implementations: `SqliteAuditLog` (persistent), `InMemoryAuditLog` (ephemeral)
- All events include: `event_id`, `trace_id`, `event_type`, `payload_json`, `created_at`.
- Events are queryable by `trace_id` for forensic reconstruction.

## Decision Records

First-class provenance artifacts capturing the full "why" of every match decision.

- Created after every `match_opportunity` pipeline run via `record_match_decision()`.
- Captures: per-requirement atom attribution, retrieval stats (lexical/embedding/merged candidate counts), validation summary (status, finding counts, top rule IDs).
- Interface: `IDecisionStore`
- Implementations: `SqliteDecisionStore` (persistent + `:memory:`), `InMemoryDecisionStore`
- Append-only; never mutated after recording.
- SQLite schema v5 (`decision_records` table, indexed by `trace_id`).
- Queryable via MCP tools (`get_decision`, `list_decisions`) and CLI (`get-decision`, `list-decisions`).
- See: [DECISION_RECORDS.md](DECISION_RECORDS.md)

---

# Application Layer

## app_service

All business logic lives in `app_service` (`include/ccmcp/app/app_service.h`, `src/app/app_service.cpp`). Both the CLI and the MCP server call into `app_service`; no business logic is duplicated in the transport layers.

Key functions:

| Function | Purpose |
|----------|---------|
| `run_match_pipeline()` | Match opportunity against atoms; validate result |
| `record_match_decision()` | Persist DecisionRecord from pipeline response |
| `run_ingest_pipeline()` | Ingest resume file to canonical markdown + SQLite |
| `run_index_build()` | Build/rebuild embedding index with drift detection |
| `apply_interaction_event()` | Apply FSM transition with idempotency and audit |
| `get_audit_trace()` | Retrieve audit events for a trace_id |
| `fetch_decision()` | Retrieve a single DecisionRecord by ID |
| `list_decisions_by_trace()` | List all DecisionRecords for a trace |

## Transport Apps

### CLI (`apps/ccmcp_cli/`)

Reference CLI with command table dispatch. Commands:

| Command | app_service function |
|---------|---------------------|
| `ingest-resume` | `run_ingest_pipeline()` |
| `tokenize-resume` | tokenize via `ITokenizationProvider` |
| `index-build` | `run_index_build()` |
| `match` | `run_match_demo()` (hardcoded fixture — does not create DecisionRecords) |
| `get-decision` | `fetch_decision()` |
| `list-decisions` | `list_decisions_by_trace()` |

### MCP Server (`apps/mcp_server/`)

Thin JSON-RPC 2.0 over stdio transport. Routes MCP tool calls to `app_service`. Exposes 8 tools:
`match_opportunity`, `validate_match_report`, `get_audit_trace`, `interaction_apply_event`,
`ingest_resume`, `index_build`, `get_decision`, `list_decisions`.

`ServerContext` holds `core::Services` (6 foundational references) plus 4 v0.3 extensions:
`IResumeIngestor`, `IResumeStore`, `IIndexRunStore`, `IDecisionStore`.

All ephemeral backends announce `WARNING:` on stderr at startup.

See: [MCP_SERVER.md](MCP_SERVER.md)

---

# Storage Architecture (v0.3)

## SQLite — Primary Store

Single file (e.g., `career.db`). Schema migrations are chained and idempotent.

| Schema version | Tables added | Introduced |
|---------------|-------------|-----------|
| v1 | atoms, opportunities, interactions, audit_events | v0.1 |
| v2 | resumes, resume_meta | v0.3 Slice 1 |
| v3 | resume_token_ir | v0.3 Slice 2 |
| v4 | index_runs, index_entries | v0.3 Slice 4 |
| v5 | decision_records | v0.3 Slice 6 |

`ensure_schema_v5()` applies all migrations in sequence on startup. All are safe to run on an existing database.

## Vector Store — Derived, Separate File

`SqliteEmbeddingIndex` stores vectors in a separate file (`vectors.db`). Isolated from the canonical store by design — vectors are rebuildable from canonical sources and must not corrupt canonical data on failure.

## Redis — Optional, Interaction Coordination

Used by `RedisInteractionCoordinator` for durable, atomic FSM transitions. Optional — falls back to `InMemoryInteractionCoordinator` when not configured. Tests opt-in via `CCMCP_TEST_REDIS=1`.

## Ephemeral Fallback

When `--db` is not provided to the MCP server, all repositories use in-memory implementations except `IResumeStore` and `IIndexRunStore` (which use `SqliteDb::open(":memory:")`). All ephemeral backends print an explicit `WARNING:` on stderr. Data is lost on process exit.

---

# Data Flows (v0.3)

## Resume Ingestion

```
Input file (MD/TXT/DOCX/PDF)
  → FormatAdapter           [deterministic text extraction]
  → HygieneNormalizer       [line endings, whitespace]
  → Canonical Markdown       [stored in IResumeStore via SQLite]
  → source_hash computed     [stable_hash64_hex of markdown]
  → resume_hash computed     [SHA-256 of normalized content]
  → Audit: IngestStarted, IngestCompleted
```

## Token IR Generation

```
Canonical Markdown (from IResumeStore)
  → ITokenizationProvider   [DeterministicLexicalTokenizer or StubInferenceTokenizer]
  → ResumeTokenIR            [tokens by category, source_hash binding]
  → ValidationEngine        [TOK-001..TOK-005]
  → IResumeTokenStore        [SQLite v3]
```

## Index Build

```
Canonical sources (atoms, resumes, opportunities from SQLite)
  → run_index_build()
  → For each artifact:
      get_last_source_hash()  [drift detection vs. last completed run]
      if unchanged → skip     [skipped_count++]
      if stale/new → embed    [IEmbeddingProvider.embed_text()]
        → upsert vector       [IEmbeddingIndex]
        → upsert index_entry  [IIndexRunStore]
        → emit IndexedArtifact audit event
  → mark run completed        [status = 'completed' in index_runs]
  → emit IndexRunCompleted audit event
```

## Match Pipeline

```
opportunity_id
  → IOpportunityRepository.get()
  → Matcher.match(opportunity, atoms, strategy)
      Lexical: tokenize requirements + atoms → overlap scoring
      Hybrid:  lexical candidates ∪ embedding candidates → merge → score
  → MatchReport
  → ValidationEngine.validate(match_report)  [SCHEMA-001, EVID-001, SCORE-001]
  → ValidationReport
  → Audit: RunStarted, MatchCompleted, ValidationCompleted, RunCompleted
  → record_match_decision()
      → DecisionRecord
      → IDecisionStore.upsert()
      → Audit: DecisionRecorded
  → Response: {trace_id, decision_id, match_report, validation_report}
```

---

# Non-Goals

- Autonomous emailing / messaging
- Auto-apply to jobs
- Unbounded web scraping
- "Agentic" loops without deterministic gates
- Vector-only matching (embeddings are a retrieval signal, not a decision authority)
- Autonomous LLM calls (LLM integration is deferred; when added, output is untrusted input to the CVE)
