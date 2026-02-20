<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./plainsight-lab-logo-dark.svg" />
    <img src="./plainsight-lab-logo.svg" alt="PlainSight Lab" width="160" />
  </picture>
</p>

# Career Coordination MCP

Deterministic inference enhanced job search coordination.

career-coordination-mcp is a C++20 reference implementation of a governance-first career coordination engine exposed via the Model Context Protocol (MCP).

It is designed to:
- Match structured opportunities against immutable experience atoms
- Compose resumes constitutionally
- Enforce outreach state machine rules
- Maintain a complete interaction audit log
- Prevent ungrounded claims
- Treat LLM output as untrusted until validated

This project is deterministic by design.

---

## Philosophy

Modern AI systems optimize for fluency.

Career coordination requires integrity.

This engine follows a strict posture:
- Deterministic rules decide structure.
- LLMs (if used) draft prose only.
- Validation gates all outputs.
- Audit logs preserve explainability.
- No autonomous actions occur without explicit human authority.

Every output must be attributable to:
- specific experience atoms,
- explicit rules,
- and a versioned constitution.

---

## Why Apache 2.0?

This project is licensed under the Apache License 2.0.

We chose Apache over MIT because:
- It includes an explicit patent grant.
- It includes patent retaliation protections.
- It remains fully permissive and commercial-friendly.
- It supports ecosystem growth without copyleft constraints.

The goal is to contribute governance-grade infrastructure back to the community while protecting contributors from patent ambiguity.

You are free to:
- Use
- Modify
- Embed
- Commercialize
- Fork

This code is infrastructure, not a trap.

---

## Architecture Overview

The system is structured around five core domains:

### 1. Experience Atoms

Immutable, verifiable capability facts.

Examples:
- Enterprise architecture leadership
- AI governance design
- Systems-level security implementation

Atoms are the only allowed source of resume claims.

---

### 2. Opportunities

Structured representations of job postings with normalized requirements.

Opportunity matching is deterministic and explainable.

---

### 3. Resume Composition

Resume generation is constitutional:
- Selected atoms must map to opportunity requirements.
- No ungrounded claims are permitted.
- Output must pass validation before acceptance.

---

### 4. Outreach State Machine

Contact interactions are governed by a finite state machine:
- No duplicate first-touch.
- No outreach if flagged “do not contact.”
- Every transition is logged.

---

### 5. Constitutional Validation Engine (CVE)

The core enforcement mechanism.

The CVE:
- Runs deterministic rule packs.
- Produces machine-readable validation reports.
- Supports override only when constitution permits.
- Emits append-only audit events.

No artifact is accepted without validation.

---

## Project Structure

```text
career-coordination-mcp/
├── include/ccmcp/
│   ├── app/               # Pipeline orchestration (app_service)
│   ├── constitution/      # Rules, ValidationEngine, Findings
│   ├── core/              # IDs, Result<T>, hashing, utilities
│   ├── domain/            # Atoms, Opportunities, Interactions, DecisionRecord
│   ├── embedding/         # IEmbeddingProvider + stub implementations
│   ├── indexing/          # IIndexRunStore, IndexBuildPipeline, drift detection
│   ├── ingest/            # IResumeIngestor, IResumeStore, IngestedResume
│   ├── interaction/       # IInteractionCoordinator, FSM, Redis coordinator
│   ├── matching/          # Lexical + hybrid retrieval, scoring
│   ├── storage/           # IAuditLog, IAtomRepository, IOpportunityRepository,
│   │   │                  # IDecisionStore
│   │   └── sqlite/        # SqliteDb, all SQLite implementations
│   ├── tokenization/      # Token IR, semantic tokenizer
│   └── vector/            # IEmbeddingIndex, InMemory + SQLite implementations
├── src/                   # All library implementations
├── apps/
│   ├── shared/            # Shared arg_parser template (used by both apps)
│   ├── ccmcp_cli/         # CLI reference app
│   │   └── commands/      # ingest-resume, tokenize-resume, index-build, match,
│   │                      # get-decision, list-decisions
│   └── mcp_server/        # MCP JSON-RPC server
│       └── handlers/      # Per-tool handler implementations
├── tests/                 # 221 deterministic unit tests
└── docs/                  # Architecture, governance, and design specs
```

## Current Phase — v0.4 In Progress

**Status:** ✅ v0.3 complete — v0.4 in progress (Slices 1–9 complete).
**Tests:** 221 cases · 1391 assertions · 0 failures · 7 skipped (Redis + SQLite-vector opt-in)
**v0.3 Readiness report:** [docs/V0_3_READINESS_REPORT.md](docs/V0_3_READINESS_REPORT.md)

### Feature Matrix

| Feature | Status | Persistence | CLI | MCP Tool |
|---------|--------|-------------|-----|----------|
| Lexical matching | ✅ | SQLite | `match` (demo) | `match_opportunity` |
| Hybrid (lexical + embedding) matching | ✅ | SQLite + vector | — | `match_opportunity` |
| Constitutional validation | ✅ | — | — | `match_opportunity` |
| Audit log (append-only, trace-queryable) | ✅ | SQLite | — | `get_audit_trace` |
| Interaction state machine (FSM) | ✅ | SQLite + Redis (required) | — | `interaction_apply_event` |
| Resume ingestion | ✅ | SQLite | `ingest-resume` | `ingest_resume` |
| Token IR generation | ✅ | SQLite | `tokenize-resume` | — |
| Embedding index build/rebuild | ✅ | SQLite + vector | `index-build` | `index_build` |
| Drift detection (source hash comparison) | ✅ (within session) | SQLite | — | — |
| Decision records (match provenance) | ✅ | SQLite | `get-decision`, `list-decisions` | `get_decision`, `list_decisions` |
| Constitutional BLOCK override (authorized operator) | ✅ | — (request-scoped) | `--override-rule --operator --reason` | — |

All v0.3 slices are implemented and passing. See [Roadmap](#roadmap) below for full details.

---

## Non-Goals

- Autonomous job application bots
- Automated emailing without approval
- Agentic browsing loops
- Undocumented state transitions
- Vector-only matching

This is governance infrastructure, not an automation toy.

---

## Building

### Prerequisites

- CMake 3.21+
- C++20 compatible compiler (Clang 14+, GCC 12+, or MSVC 19.30+)
- vcpkg at commit `e6ebaa9c3ca8fca90c63af62fc895c2486609580` or later (`$VCPKG_ROOT` or `~/vcpkg`)
- Python 3 (for baseline verification in `scripts/build.sh`)
- (Optional) clang-format and clang-tidy 14+ for code quality

### Dependency Governance

Dependency versions are pinned via the `builtin-baseline` field in `vcpkg.json`. This
guarantees that a clean checkout resolves the exact same package graph regardless of
when or where the build runs. The build script verifies the baseline before configuring
CMake and fails fast if the local vcpkg installation is older than the pinned commit.

`vcpkg_installed/` is excluded from version control — artifacts are always resolved
from the pinned baseline, never from committed binaries.

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for full dependency governance details,
clean-room validation steps, and baseline update procedure.

### Build Instructions

```bash
# Recommended: deterministic build with baseline verification
./scripts/build.sh

# Build without tests
./scripts/build.sh --skip-tests

# Clean build (wipe build dir, then build and test)
./scripts/build.sh --clean
```

Or manually:

```bash
# Configure with vcpkg toolchain
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake

# Build
cmake --build build-vcpkg --parallel

# Run CLI demo
./build-vcpkg/apps/ccmcp_cli/ccmcp_cli

# Run tests
./build-vcpkg/tests/ccmcp_tests
```

Dependencies (pinned via vcpkg.json `builtin-baseline`, resolved automatically):
- nlohmann-json 3.12.0 (JSON serialization)
- fmt 12.1.0 (formatting)
- Catch2 3.12.0 (testing)
- sqlite3 3.51.2 (persistence layer)
- redis-plus-plus 1.3.15 (interaction coordination)
- libzip 1.11.4 (DOCX resume ingestion)
- pugixml 1.15 (DOCX resume ingestion)

### Code Quality

Format code and run static analysis:

```bash
# Format all C++ files (modifies in-place)
./scripts/format.sh

# Check formatting without modifying
./scripts/format.sh --check

# Run clang-tidy static analysis
./scripts/lint.sh
```

See [docs/LINTING.md](docs/LINTING.md) for detailed linting and formatting documentation.

## Deterministic Inference Enhanced

This project embodies a simple principle:

Deterministic structure first.
Model assistance second.
Human authority always.

The engine can integrate LLM providers later, but:
- LLM output is treated as untrusted input.
- Validation is mandatory.
- No new facts may be introduced without grounding.
- All claims must trace back to canonical atoms.

---

## Roadmap

### v0.1 ✅ Complete
- Deterministic matcher
- Constitutional validation scaffolding
- Audit logging
- CLI demo

### v0.2 ✅ Complete
- Hybrid lexical + embedding retrieval
- SQLite persistence (atoms, opportunities, interactions, audit log)
- Redis-backed interaction state machine coordination
- MCP protocol server with app_service layer

### v0.3 ✅ Complete (6 slices + CLI refactor)

**Slice 1 — Resume Ingestion Pipeline**
- Multi-format ingestion (Markdown, TXT; DOCX and PDF stubs)
- Deterministic text extraction and hygiene normalization
- SQLite schema v2 (resumes + resume_meta tables)
- CLI: `ccmcp_cli ingest-resume`
- See [RESUME_INGESTION.md](docs/RESUME_INGESTION.md)

**Slice 2 — Token IR Generation**
- Semantic token extraction from ingested resumes
- Constitutional validation of token IR (no ungrounded claims)
- SQLite schema v3 (resume_tokens table)
- CLI: `ccmcp_cli tokenize-resume`
- See [TOKEN_IR.md](docs/TOKEN_IR.md)

**Slice 3 — SQLite-backed Persistent Vector Index**
- `SqliteEmbeddingIndex`: upsert, similarity search, persistent storage
- Replaces ephemeral in-memory-only vector index for production use
- See [VECTORDB_BACKEND.md](docs/VECTORDB_BACKEND.md)

**Slice 4 — Embedding Lifecycle + Index Build/Rebuild**
- SQLite schema v4 (index_runs + index_entries tables)
- `IIndexRunStore` + `SqliteIndexRunStore` for provenance tracking
- `run_index_build()` pipeline with per-artifact drift detection (source hash comparison)
- Deterministic index rebuild: skip unchanged, reindex stale
- CLI: `ccmcp_cli index-build`
- Audit events: `IndexRunStarted`, `IndexedArtifact`, `IndexRunCompleted`
- See [INDEXING.md](docs/INDEXING.md)

**CLI Hardening Refactor**
- Shared `arg_parser` template extracted to `apps/shared/`
- CLI decomposed from monolithic 557-line main into command table dispatch + per-command files

**Slice 5 — MCP Server Hardening + Persistence Wiring**
- All six backends wired in `main.cpp`: SQLite stores, Redis coordinator, LanceDB vector index
- Config flags: `--db`, `--redis`, `--vector-backend inmemory|sqlite`, `--vector-db-path`, `--matching-strategy`
- Two new MCP tools: `ingest_resume`, `index_build`
- `match_opportunity` gains optional `resume_id` for audit traceability
- `ServerContext` extended with `IResumeIngestor`, `IResumeStore`, `IIndexRunStore`
- Ephemeral fallback: all subsystems announce WARNING on stderr when not persistent
- See [MCP_SERVER.md](docs/MCP_SERVER.md)

**Slice 6 — Decision Records**
- SQLite schema v5 (`decision_records` table, indexed by `trace_id`)
- `DecisionRecord`: first-class provenance artifact capturing the full "why" of a match decision
  - Per-requirement atom attribution (`atom_id`, `evidence_tokens`)
  - Retrieval stats snapshot (lexical, embedding, merged candidate counts)
  - Validation summary (status, finding counts, top rule IDs from fail/block/warn findings)
- `IDecisionStore` interface + `SqliteDecisionStore` + `InMemoryDecisionStore`
- `record_match_decision()`: called after every `match_opportunity` run; emits `DecisionRecorded` audit event
- `match_opportunity` response now includes `decision_id` for immediate lookup
- Two new MCP tools: `get_decision`, `list_decisions`
- Two new CLI commands: `ccmcp_cli get-decision`, `ccmcp_cli list-decisions`
- Deterministic JSON serialization (alphabetically sorted keys via `nlohmann::json` std::map default)
- See [DECISION_RECORDS.md](docs/DECISION_RECORDS.md)

### v0.4 (In Progress)

**Slice 1 — Deterministic Index Run Identity** ✅
- Schema v6 (`id_counters` table): monotonically increasing run IDs persistent across restarts
- `IIndexRunStore::next_index_run_id()` sourced from SQLite counter (fixes WARN-001)
- Drift detection correctness: run IDs no longer collide across CLI invocations

**Slice 2 — Vector Backend Semantics Normalization** ✅
- `VectorBackend` enum: shared authoritative vocabulary for `--vector-backend` flag
- Aligned CLI and MCP server to identical valid values (`inmemory`, `sqlite`)
- MCP server `--lancedb-path` renamed to `--vector-db-path`; `lancedb` value now fails fast
- Removed silent aliasing: flag → implementation wiring is explicit and exhaustive

**Slice 3 — Reproducible Dependency Resolution** ✅
- `builtin-baseline` pinned in `vcpkg.json` — same dependency graph on every clean checkout
- `scripts/build.sh` — deterministic build entrypoint with baseline verification
- `docs/DEVELOPMENT.md` — clean-room build procedure and dependency governance
- `docs/ARCHITECTURE.md` — Dependency Governance section

**Slice 4 — Constitutional BLOCK Override Rail** ✅
- `ConstitutionOverrideRequest` domain type (`rule_id`, `operator_id`, `reason`, `payload_hash`)
- `ValidationStatus::kOverridden` — 5th distinct terminal status; BLOCK present but explicitly overridden
- `payload_hash` binding: `stable_hash64_hex(envelope.artifact_id)` — override is artifact-bound; mismatch silently rejects
- CVE: override applied post-findings-sort; BLOCK findings preserved; only `status` changes to `kOverridden`
- `ConstitutionOverrideApplied` audit event emitted after `ValidationCompleted` when override applied
- CLI: `--override-rule`, `--operator`, `--reason` — all-or-nothing; partial set fails fast
- Override logic confined to `app_service`/CVE layers — no storage adapter participation
- See [CONSTITUTIONAL_RULES.md](docs/CONSTITUTIONAL_RULES.md) — BLOCK Override Rail section

**Slice 5 — Redis-First Operational Posture** ✅
- `--redis <uri>` is required at MCP server startup; no in-memory fallback in production paths
- `InMemoryInteractionCoordinator` removed from all real code paths; `RedisInteractionCoordinator` only
- `redis-health` CLI command verifies liveness and round-trip before use
- Startup fail-fast on missing or unresolvable Redis URI

**Slice 6 — Transport/Storage Boundary Hardening** ✅
- `CCMCP_TRANSPORT_BOUNDARY_GUARD` `#error` on 11 concrete sqlite/redis headers
- `ccmcp_cli_logic` and `mcp_transport_logic` OBJECT libraries compiled with the guard flag
- Logic layers include interfaces only; concrete objects constructed only in wiring `.cpp` + `main.cpp`
- Compile-time enforcement: cross-boundary coupling fails the build immediately

**Slice 7 — Deterministic Runtime Configuration Snapshotting** ✅
- `sha256_hex()` — pure C++ FIPS 180-4 SHA-256, no new dependencies
- `core::kBuildVersion` constant (`"0.4"`) for provenance labeling
- `RedisConfig::redis_db` field; `redis://host:port/N` URI parsing for database index
- `RuntimeConfigSnapshot` domain type + deterministic JSON serialization (alphabetically sorted keys)
- `IRuntimeSnapshotStore` interface + `SqliteRuntimeSnapshotStore` (schema v7, boundary-guarded)
- Schema v7: `runtime_snapshots` table — `run_id`, `snapshot_json`, `snapshot_hash`, `created_at`
- MCP server emits one snapshot per process launch, before `run_server_loop`, in both code paths
- New invariant: every run with a persisted DB has a matching `runtime_snapshots` row

**Slice 8 — Deterministic Audit Hash Chain (Event Integrity Precursor to Crypto)** ✅
- `AuditEvent` extended: `previous_hash` + `event_hash` fields (default `""`, computed on append)
- `kGenesisHash` — 64 zero hex digits, deterministic chain anchor for each trace's first event
- `compute_event_hash()` — SHA-256(stable JSON of event fields + previous_hash), alphabetically sorted keys
- `verify_audit_chain()` — recomputes every link; returns `AuditChainVerificationResult` (valid, index, error)
- Both `InMemoryAuditLog::append` and `SqliteAuditLog::append` compute and store the chain
- `SqliteAuditLog` fetches `(idx, previous_hash)` atomically under a single lock acquisition
- Schema v8: `ALTER TABLE audit_events ADD COLUMN previous_hash / event_hash TEXT NOT NULL DEFAULT ''`
- New invariant: every appended audit event carries a tamper-evident SHA-256 hash linking it to its predecessor

**Slice 9 — Dockerization Readiness (Local-Only Containerized Deployment Blueprint)** ✅
- Multi-stage `Dockerfile`: `ubuntu:22.04` builder → `debian:bookworm-slim` runtime
- Static vcpkg linkage via custom overlay triplet (`docker/triplets/x64-linux-static-release.cmake`)
- `scripts/docker-entrypoint.sh` — validates `REDIS_HOST`, constructs CLI args, `exec`s `mcp_server`
- `docker-compose.yml` — `redis:7-alpine` + app services, bind-mounted `./data:/data` for SQLite persistence
- Non-root container execution (`ccmcp` UID 10001); Redis isolated to bridge network; no host ports exposed
- Zero C++ changes — entrypoint script handles env var translation; all startup validation preserved
- See [DEPLOYMENT.md](docs/DEPLOYMENT.md)

**Planned:**
- Real PDF extraction (Poppler/MuPDF) enabled by containerized toolchain
- Resume composition workflows (atom selection → draft → validation → output)
- Structured resume patching (constitutional diff and patch operations)
- Real LanceDB C++ SDK integration (currently `SqliteEmbeddingIndex` fills this slot)
- Token validation rule packs
- Real embedding provider integration (OpenAI, local models)

---

## Contributing

Contributions are welcome if they preserve:
- Determinism
- Auditability
- Explicit governance boundaries
- Clear error surfaces
- Reproducibility

Please avoid adding:
- Hidden network calls
- Agent loops
- Non-deterministic decision paths

All contributions must align with the constitutional posture of the project.

---

## License


This project is licensed under the Apache License, Version 2.0.

Copyright (c) 2026 PlainSight Lab

See LICENSE and NOTICE for full terms.

**Asset Exception**

The following assets are not licensed under Apache 2.0 and remain copyrighted:
	•	plainsight-lab-logo.svg
	•	plainsight-lab-logo-dark.svg

These files are provided for use within this repository only and may not be redistributed, modified, or used to imply affiliation without explicit permission.

All other source code and documentation in this repository are licensed under Apache 2.0 unless otherwise stated.
---

## Final Note

This is not just a job search tool.

It is a reference implementation of:
- Deterministic inference
- Constitutional validation
- Auditable decision systems

It is infrastructure for reasoning under constraint.
