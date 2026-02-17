# MCP Server Documentation

## Overview

The `mcp_server` is a **thin transport layer** that exposes the career-coordination-mcp pipeline as Model Context Protocol (MCP) tools. It implements JSON-RPC 2.0 over stdio and delegates all business logic to the `app_service` layer.

**Status:** ✅ Implemented in v0.2 Slice 5

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
│   - Validation      │
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
│   - Coordinators    │
└─────────────────────┘
```

## Building and Running

### Build

```bash
cmake --build build
```

The MCP server executable is built at: `build/apps/mcp_server/mcp_server`

### Run

```bash
./build/apps/mcp_server/mcp_server
```

The server listens on **stdin** for JSON-RPC requests and writes responses to **stdout**. Diagnostic messages are written to **stderr**.

## Supported Tools

### 1. `match_opportunity`

Run the full matching + validation pipeline for an opportunity.

**Input:**
```json
{
  "name": "match_opportunity",
  "arguments": {
    "opportunity_id": "opp-123",
    "strategy": "deterministic_lexical_v0.1",
    "k_lex": 25,
    "k_emb": 25,
    "trace_id": "optional-trace-id"
  }
}
```

**Parameters:**
- `opportunity_id` (required): Opportunity to match against
- `strategy` (optional): `"deterministic_lexical_v0.1"` (default) or `"hybrid_lexical_embedding_v0.2"`
- `k_lex` (optional): Number of lexical results (default: 25)
- `k_emb` (optional): Number of embedding results (default: 25)
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

**Audit Events Emitted:**
1. `RunStarted`
2. `MatchCompleted`
3. `ValidationCompleted`
4. `RunCompleted`

---

### 2. `validate_match_report`

Validate a match report (standalone validation, not part of match pipeline).

**Input:**
```json
{
  "name": "validate_match_report",
  "arguments": {
    "match_report": { ... },
    "trace_id": "optional-trace-id"
  }
}
```

**Status:** Not yet implemented in v0.2 Slice 5 (returns error).

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
    },
    ...
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

**Possible Outcomes:**
- `"applied"`: Transition successfully applied
- `"already_applied"`: Idempotency key already used (safe replay)
- `"invalid_transition"`: Event not allowed from current state
- `"not_found"`: Interaction does not exist
- `"conflict"`: Concurrent modification detected
- `"backend_error"`: Redis or storage error

**Audit Events Emitted:**
1. `InteractionTransitionAttempted`
2. `InteractionTransitionCompleted` (or `InteractionTransitionRejected`)

---

## Protocol Details

The server implements **JSON-RPC 2.0** over **stdio**.

### Initialize

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "initialize",
  "params": {}
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {
      "tools": {}
    },
    "serverInfo": {
      "name": "career-coordination-mcp",
      "version": "0.2.0"
    }
  }
}
```

### List Tools

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "method": "tools/list",
  "params": {}
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "result": {
    "tools": [
      {
        "name": "match_opportunity",
        "description": "Run matching + validation pipeline for an opportunity",
        "inputSchema": { ... }
      },
      ...
    ]
  }
}
```

### Call Tool

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "method": "tools/call",
  "params": {
    "name": "match_opportunity",
    "arguments": {
      "opportunity_id": "opp-123"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "result": {
    "trace_id": "...",
    "match_report": { ... },
    "validation_report": { ... }
  }
}
```

### Error Responses

**Format:**
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "error": {
    "code": -32601,
    "message": "Method not found",
    "data": {}
  }
}
```

**Error Codes:**
- `-32700`: Parse error (invalid JSON)
- `-32600`: Invalid request
- `-32601`: Method not found
- `-32602`: Invalid params
- `-32603`: Internal error

---

## Safety and Constraints

### What the MCP Server Does NOT Do

- ❌ No file system access (read/write arbitrary files)
- ❌ No shell command execution
- ❌ No network access (except stdin/stdout)
- ❌ No database mutations beyond what the tools explicitly expose

### Least-Privilege Design

The server operates on **in-memory services** by default. Future work:
- Add `--db` flag for SQLite persistence
- Add `--redis` flag for Redis coordination
- Add configuration file support

---

## Current Limitations (v0.2)

1. **In-memory only**: No persistence flags implemented yet (services are transient)
2. **Inline opportunity**: `match_opportunity` requires pre-existing `opportunity_id`, does not support inline opportunity objects
3. **Validate tool**: `validate_match_report` not yet implemented
4. **No authentication**: No user identity or access control (research artifact)

---

## Testing

### Unit Tests (app_service layer)

```bash
./build/tests/ccmcp_tests "[app_service]"
```

Verifies:
- Match pipeline determinism
- Validation integration
- Interaction idempotency
- Audit trace correctness

### Integration Tests (MCP protocol)

**Not yet implemented** (opt-in with `CCMCP_TEST_MCP=1` planned for future).

Manual testing via stdio:

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"tools/list","params":{}}' | ./build/apps/mcp_server/mcp_server
```

---

## Tracing and Observability

All tool invocations emit **audit events** to the configured audit log (in-memory or SQLite). Use `get_audit_trace` to retrieve the full event sequence for a `trace_id`.

**Example workflow:**
1. Call `match_opportunity` → receive `trace_id: "trace-abc-123"`
2. Call `get_audit_trace` with `trace_id: "trace-abc-123"`
3. Receive full audit trail: `[RunStarted, MatchCompleted, ValidationCompleted, RunCompleted]`

---

## Future Enhancements (Post-v0.2)

**v0.3: Configuration**
- Add `--db`, `--redis`, `--vector-backend` CLI flags
- Support config file (JSON/TOML)

**v0.4: Full Tool Support**
- Implement `validate_match_report` standalone tool
- Add `create_interaction` tool

**v0.5: Authentication**
- API key validation
- User identity propagation to audit log

**v0.6: Streaming**
- Support streaming responses for long-running operations
- Progress updates

---

## Architecture Rationale

### Why a Thin Transport Layer?

The MCP server delegates **all business logic** to `app_service`. This ensures:

1. **No duplication**: CLI and MCP server share the same code paths
2. **Testability**: Business logic is tested independently of transport
3. **Maintainability**: Protocol changes don't require rewriting business logic
4. **Determinism**: Same inputs → same outputs, regardless of transport

### Why app_service Exists

The `app_service` layer was introduced in v0.2 Slice 5 to:
- Extract reusable pipeline logic from CLI
- Provide a clean boundary between transport (CLI/MCP/future HTTP) and business logic
- Enable deterministic testing without transport concerns
- Encapsulate audit event emission

---

## Summary

The MCP server is a **minimal, safe, read-only-by-default** transport layer that exposes the career-coordination-mcp pipeline via MCP tools. It follows the principle of least privilege, delegates all logic to `app_service`, and maintains full audit traceability.

For implementation details, see:
- `apps/mcp_server/main.cpp` - Server loop and tool handlers
- `apps/mcp_server/mcp_protocol.{h,cpp}` - JSON-RPC helpers
- `src/app/app_service.cpp` - Business logic
- `docs/V0_2_ARCHITECTURE.md` - Architecture overview
