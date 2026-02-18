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
│   ├── domain/            # Atoms, Opportunities, Interactions
│   ├── embedding/         # IEmbeddingProvider + stub implementations
│   ├── indexing/          # IIndexRunStore, IndexBuildPipeline, drift detection
│   ├── ingest/            # IResumeIngestor, IResumeStore, IngestedResume
│   ├── interaction/       # IInteractionCoordinator, FSM, Redis coordinator
│   ├── matching/          # Lexical + hybrid retrieval, scoring
│   ├── storage/           # IAuditLog, IAtomRepository, IOpportunityRepository
│   │   └── sqlite/        # SqliteDb, all SQLite implementations
│   ├── tokenization/      # Token IR, semantic tokenizer
│   └── vector/            # IEmbeddingIndex, InMemory + SQLite implementations
├── src/                   # All library implementations
├── apps/
│   ├── shared/            # Shared arg_parser template (used by both apps)
│   ├── ccmcp_cli/         # CLI reference app
│   │   └── commands/      # ingest-resume, tokenize-resume, index-build, match
│   └── mcp_server/        # MCP JSON-RPC server
│       └── handlers/      # Per-tool handler implementations
├── tests/                 # 163 deterministic unit tests
└── docs/                  # Architecture, governance, and design specs
```

## Current Scope (v0.3 — Complete)

All v0.3 slices are implemented and passing. See [Roadmap](#roadmap) below for full details.

**Implemented capabilities:**
- Deterministic lexical + hybrid (lexical + embedding) matching
- Constitutional validation engine with rule packs and machine-readable reports
- Append-only audit log with full trace retrieval
- Outreach interaction state machine (FSM) with idempotent transitions
- SQLite persistence for atoms, opportunities, interactions, audit log, resumes, and index runs
- Redis-backed interaction coordination
- Resume ingestion pipeline (Markdown, TXT, DOCX stub, PDF stub) with hygiene normalization
- Token IR generation with constitutional validation
- SQLite-backed embedding vector index with persistent storage
- Embedding lifecycle: deterministic index build/rebuild with drift detection (source hash comparison)
- MCP server exposing all tools with persistent backend wiring
- CLI commands: `ingest-resume`, `tokenize-resume`, `index-build`, `match`

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
- C++20 compatible compiler (Clang, GCC, or MSVC)
- vcpkg installed at `~/vcpkg` or `$VCPKG_ROOT`
- (Optional) clang-format and clang-tidy 14+ for code quality

### Build Instructions

The project uses vcpkg manifest mode for deterministic dependency resolution.

```bash
# Configure with vcpkg toolchain
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake

# Build
cmake --build build-vcpkg

# Run CLI demo
./build-vcpkg/apps/ccmcp_cli/ccmcp_cli

# Run tests
./build-vcpkg/tests/ccmcp_tests
```

Dependencies (resolved automatically via vcpkg.json):
- nlohmann-json (JSON serialization)
- fmt (formatting)
- Catch2 (testing)
- sqlite3 (persistence layer)

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

### v0.3 ✅ Complete (5 slices + CLI refactor)

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
- Config flags: `--db`, `--redis`, `--vector-backend inmemory|lancedb`, `--lancedb-path`, `--matching-strategy`
- Two new MCP tools: `ingest_resume`, `index_build`
- `match_opportunity` gains optional `resume_id` for audit traceability
- `ServerContext` extended with `IResumeIngestor`, `IResumeStore`, `IIndexRunStore`
- Ephemeral fallback: all subsystems announce WARNING on stderr when not persistent
- See [MCP_SERVER.md](docs/MCP_SERVER.md)

### v0.4+ (Future)
- Containerization (enables Poppler/MuPDF for real PDF extraction)
- Resume composition workflows (atom selection → draft → validation → output)
- Structured resume patching (constitutional diff and patch operations)
- Real LanceDB C++ SDK integration (currently implemented via `SqliteEmbeddingIndex`)
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
