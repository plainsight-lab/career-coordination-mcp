# MCP Server Documentation

## Overview

The `mcp_server` is a **thin transport layer** that exposes the career-coordination-mcp pipeline as Model Context Protocol (MCP) tools. It implements JSON-RPC 2.0 over stdio and delegates all business logic to the `app_service` layer.

**Status:** ✅ Implemented in v0.3 Slice 5 (MCP Server Hardening + Persistence Wiring)

## Architecture

```
┌─────────────┐
│ MCP Client  │ (Claude Desktop, etc.)
└──────┬──────┘
       │ JSON-RPC over stdio
       ▼
┌─────────────────────┐
│   mcp_server        │ (thin transport)
│   - Protocol        │
│   - Tool routing    │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│   app_service       │ (business logic)
│   - Match pipeline  │
│   - Ingest pipeline │
│   - Index pipeline  │
│   - Interaction FSM │
│   - Audit trace     │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│   Core Services     │
│   - Repositories    │
│   - Audit log       │
│   - Vector index    │
│   - Resume store    │
│   - Index run store │
└─────────────────────┘
```

## Building and Running

### Build

```bash
cmake --build build-vcpkg
```

The MCP server executable is built at: `build-vcpkg/apps/mcp_server/mcp_server`

### Run

```bash
./build-vcpkg/apps/mcp_server/mcp_server [options]
```

The server listens on **stdin** for JSON-RPC requests and writes responses to **stdout**. Diagnostic messages are written to **stderr**.

## Configuration Flags

All flags are optional. The server prints explicit `WARNING:` lines on stderr for every ephemeral (non-persistent) backend.

| Flag | Description | Default |
|------|-------------|---------|
| `--db <path>` | SQLite database file for atoms, opportunities, interactions, resumes, index runs, audit log | in-memory (ephemeral) |
| `--redis <uri>` | Redis URI for durable interaction coordination (e.g. `redis://localhost:6379`) | in-memory coordinator (ephemeral) |
| `--vector-backend <name>` | Vector index backend: `inmemory` or `lancedb` | `inmemory` (ephemeral) |
| `--lancedb-path <dir>` | Directory for LanceDB (SQLite-backed) vector index; **required** when `--vector-backend lancedb` | — |
| `--matching-strategy <name>` | Default strategy: `lexical` or `hybrid` | `lexical` |

### Example: fully persistent

```bash
./build-vcpkg/apps/mcp_server/mcp_server \
  --db career.db \
  --redis redis://localhost:6379 \
  --vector-backend lancedb \
  --lancedb-path ./vectors \
  --matching-strategy lexical
```

### Example: ephemeral (development)

```bash
./build-vcpkg/apps/mcp_server/mcp_server
# WARNING: all subsystems ephemeral — data lost on exit
```

## Persistence Behavior

### SQLite backend (`--db`)

When `--db` is provided:
- `SqliteAtomRepository`, `SqliteOpportunityRepository`, `SqliteInteractionRepository`,
  `SqliteAuditLog`, `SqliteResumeStore`, `SqliteIndexRunStore` are all created on the same file.
- Schema v4 is applied on startup (`ensure_schema_v4()` chains migrations v1→v4; all are idempotent).

When `--db` is **not** provided:
- Atom/opportunity/interaction repositories and audit log use in-memory implementations.
- `SqliteResumeStore` and `SqliteIndexRunStore` use a dedicated `SqliteDb::open(":memory:")`.
  There are no separate in-memory implementations of these interfaces.

### Vector backend (`--vector-backend lancedb`)

LanceDB is implemented via `SqliteEmbeddingIndex` (the declared LanceDB C++ SDK is unavailable in vcpkg; `SqliteEmbeddingIndex` is the storage-layer equivalent). The vector database is stored at `<lancedb-path>/vectors.db`.

When `--vector-backend inmemory` (default), the embedding index is ephemeral and lost on restart.

---

## Supported Tools

### 1. `match_opportunity`

Run the full matching + validation pipeline for an opportunity.

**Input:**
```json
{
  "name": "match_opportunity",
  "arguments": {
    "opportunity_id": "opp-123",
    "strategy": "hybrid_lexical_embedding_v0.2",
    "k_lex": 25,
    "k_emb": 25,
    "resume_id": "resume--0",
    "trace_id": "optional-trace-id"
  }
}
```

**Parameters:**
- `opportunity_id` (required): Opportunity to match against
- `strategy` (optional): `"hybrid_lexical_embedding_v0.2"` (default: server's configured strategy)
- `k_lex` (optional): Number of lexical candidates (default: 25)
- `k_emb` (optional): Number of embedding candidates (default: 25)
- `resume_id` (optional): Propagated to audit trail for traceability — does **not** alter matching
- `trace_id` (optional): Trace ID for audit correlation (auto-generated if not provided)

**Output:**
```json
{
  "trace_id": "trace-abc-123",
  "match_report": {
    "opportunity_id": "opp-123",
    "overall_score": 0.75,
    "strategy": "deterministic_lexical_v0.1",
    "matched_atoms": ["atom-001", "atom-002"]
  },
  "validation_report": {
    "status": "accepted",
    "finding_count": 3
  }
}
```

**Audit Events Emitted:** `RunStarted`, `MatchCompleted`, `ValidationCompleted`, `RunCompleted`

---

### 2. `validate_match_report`

Validate a match report (standalone, not part of the match pipeline).

**Input:**
```json
{
  "name": "validate_match_report",
  "arguments": {
    "match_report": { "...": "..." }
  }
}
```

**Status:** Routing registered; full standalone validation not yet implemented (returns error for unrecognized schemas).

---

### 3. `get_audit_trace`

Fetch all audit events for a given trace_id.

**Input:**
```json
{
  "name": "get_audit_trace",
  "arguments": {
    "trace_id": "trace-abc-123"
  }
}
```

**Output:**
```json
{
  "trace_id": "trace-abc-123",
  "events": [
    {
      "event_id": "evt-001",
      "trace_id": "trace-abc-123",
      "event_type": "RunStarted",
      "payload": {"source": "app_service", "operation": "match_pipeline"},
      "created_at": "2026-01-01T00:00:00Z"
    }
  ]
}
```

---

### 4. `interaction_apply_event`

Apply a state transition to an interaction atomically.

**Input:**
```json
{
  "name": "interaction_apply_event",
  "arguments": {
    "interaction_id": "int-001",
    "event": "Prepare",
    "idempotency_key": "request-xyz-789",
    "trace_id": "optional-trace-id"
  }
}
```

**Parameters:**
- `interaction_id` (required): Interaction to transition
- `event` (required): One of `"Prepare"`, `"Send"`, `"ReceiveReply"`, `"Close"`
- `idempotency_key` (required): Unique key for idempotent replay
- `trace_id` (optional): Trace ID for audit correlation

**Output:**
```json
{
  "trace_id": "trace-def-456",
  "result": {
    "outcome": "applied",
    "before_state": 0,
    "after_state": 1,
    "transition_index": 1
  }
}
```

**Possible Outcomes:** `applied`, `already_applied`, `invalid_transition`, `not_found`, `conflict`, `backend_error`

**Audit Events Emitted:** `InteractionTransitionAttempted`, `InteractionTransitionCompleted` (or `InteractionTransitionRejected`)

---

### 5. `ingest_resume`

Ingest a resume file and optionally persist it to the resume store.

**Input:**
```json
{
  "name": "ingest_resume",
  "arguments": {
    "input_path": "/absolute/path/to/resume.md",
    "persist": true,
    "trace_id": "optional-trace-id"
  }
}
```

**Parameters:**
- `input_path` (required): Absolute path to the resume file (`.md`, `.txt`, `.docx`, `.pdf`)
- `persist` (optional, default: `true`): If `true`, store the resume in `IResumeStore`
- `trace_id` (optional): Trace ID for audit correlation

**Output:**
```json
{
  "resume_id": "resume--0",
  "resume_hash": "sha256:abc123...",
  "source_hash": "a1b2c3d4e5f6...",
  "trace_id": "trace-ghi-789"
}
```

- `resume_hash`: SHA-256 of the normalized resume content
- `source_hash`: `stable_hash64_hex` of the canonical markdown text (used for index drift detection)

**Audit Events Emitted:** `IngestStarted`, `IngestCompleted`

---

### 6. `index_build`

Build or rebuild the embedding vector index for the specified artifact scope.

**Input:**
```json
{
  "name": "index_build",
  "arguments": {
    "scope": "all",
    "trace_id": "optional-trace-id"
  }
}
```

**Parameters:**
- `scope` (optional, default: `"all"`): One of `"atoms"`, `"resumes"`, `"opps"`, `"all"`
- `trace_id` (optional): Trace ID for audit correlation

**Output:**
```json
{
  "run_id": "idx-run--0",
  "counts": {
    "indexed": 12,
    "skipped": 3,
    "stale": 1
  },
  "trace_id": "trace-jkl-012"
}
```

- `indexed`: Artifacts where embedding was computed and stored (new + stale)
- `skipped`: Artifacts whose source hash is unchanged since last completed run
- `stale`: Artifacts that were reindexed because source content changed

**Audit Events Emitted:** `IndexBuildStarted`, `IndexBuildCompleted` (outer); `IndexRunStarted`, `IndexedArtifact`, `IndexRunCompleted` (inner, tracked by index run system)

**Drift detection:** Uses `source_hash` (stable hash of canonical text) compared against the last completed index run for the same provider/model/prompt scope. Stale = hash changed; skipped = hash unchanged.

---

## Protocol Details

The server implements **JSON-RPC 2.0** over **stdio**.

### Initialize

**Request:**
```json
{"jsonrpc": "2.0", "id": "1", "method": "initialize", "params": {}}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {"tools": {}},
    "serverInfo": {"name": "career-coordination-mcp", "version": "0.2.0"}
  }
}
```

### List Tools

```json
{"jsonrpc": "2.0", "id": "2", "method": "tools/list", "params": {}}
```

Returns all six tool descriptors with their JSON Schema `inputSchema`.

### Call Tool

```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "method": "tools/call",
  "params": {"name": "match_opportunity", "arguments": {"opportunity_id": "opp-123"}}
}
```

### Error Codes

| Code | Meaning |
|------|---------|
| `-32700` | Parse error (invalid JSON) |
| `-32600` | Invalid request |
| `-32601` | Method not found |
| `-32602` | Invalid params |
| `-32603` | Internal error |

---

## Testing

### Unit Tests

```bash
# All tests
./build-vcpkg/tests/ccmcp_tests

# App service tests only
./build-vcpkg/tests/ccmcp_tests "[app_service]"

# Ingest pipeline tests
./build-vcpkg/tests/ccmcp_tests "[ingest_pipeline]"

# Index build pipeline tests
./build-vcpkg/tests/ccmcp_tests "[index_build_pipeline]"
```

### Opt-in Integration Tests

Integration tests that require live backends are skipped by default:

```bash
# Enable Redis coordinator tests
CCMCP_TEST_REDIS=1 ./build-vcpkg/tests/ccmcp_tests "[redis]"

# Enable LanceDB (SQLite vector) tests
CCMCP_TEST_LANCEDB=1 ./build-vcpkg/tests/ccmcp_tests "[lancedb]"
```

### Manual Testing via stdio

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"tools/list","params":{}}' \
  | ./build-vcpkg/apps/mcp_server/mcp_server
```

---

## Safety and Constraints

- No arbitrary file system access (only the paths explicitly passed to tools)
- No shell execution
- No network access except the optional Redis backend (`--redis`)
- All state mutations go through `app_service` with full audit trail

---

## Architecture Rationale

### Why a Thin Transport Layer?

The MCP server delegates **all business logic** to `app_service`. This ensures:

1. **No duplication**: CLI and MCP server share the same pipeline code paths
2. **Testability**: Business logic is tested independently of transport
3. **Determinism**: Same inputs → same outputs, regardless of transport

### Services vs. ServerContext

- `core::Services` holds the six foundational service references (atoms, opportunities, interactions, audit log, vector index, embedding provider). It is **unchanged** across all slices to avoid breaking test files.
- `mcp::ServerContext` extends `Services` with the three new references introduced in v0.3: `IResumeIngestor`, `IResumeStore`, `IIndexRunStore`. Handlers access these via the context.

---

## Current Limitations (v0.3)

1. **`validate_match_report`**: Routing registered but full standalone implementation pending
2. **Inline opportunity**: `match_opportunity` requires pre-existing `opportunity_id`; inline objects not yet supported
3. **Real LanceDB**: `--vector-backend lancedb` uses `SqliteEmbeddingIndex` under the hood; native LanceDB C++ SDK not available in vcpkg
4. **No authentication**: No user identity or access control (research artifact)
5. **Deterministic clock/IDs**: Server uses `DeterministicIdGenerator` and `FixedClock`; production use would inject a real wall clock and UUID generator

---

## References

- `apps/mcp_server/main.cpp` — Backend wiring and ServerContext construction
- `apps/mcp_server/handlers/` — Per-tool handler implementations
- `src/app/app_service.cpp` — Pipeline business logic
- `docs/INDEXING.md` — Index build and drift detection design
- `docs/V0_3_ARCHITECTURE.md` — v0.3 architecture overview
