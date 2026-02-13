# Build Order (v0.1)

1. Core domain model (C++ types + serialization)
2. Audit log (append-only event store)
3. Opportunity ingestion (structured parser + dedupe keys)
4. Matching engine (deterministic scoring)
5. Resume composition (atom selection + ordering + rendering)
6. Validation engine (constitutional rules)
7. MCP server (coroutines + tool routing)
8. Redis integration (state machine + queues)
9. Optional LanceDB (embeddings for retrieval—defer until value proven)

# Dependency Plan (vcpkg)

- JSON: nlohmann-json
- HTTP/server: (pick one) boost-beast or cpp-httplib
- Async: C++20 coroutines + asio (standalone) or Boost.Asio
- Redis: redis-plus-plus (or hiredis)
- LanceDB: (only if stable path exists in your environment) otherwise treat as optional adapter

# Project Layout Recommendation

- src/domain
- src/matching
- src/composition
- src/validation
- src/audit
- src/mcp
- src/storage
