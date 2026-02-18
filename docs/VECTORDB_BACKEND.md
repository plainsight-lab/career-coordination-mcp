# LanceDB Backend — v0.3 Slice 3

## Status

Implemented: v0.3 Slice 3.

## Why SQLite, Not LanceDB

LanceDB has no official C++ client library. Its supported clients are Python,
JavaScript/TypeScript, and Rust. There is no vcpkg package and no stable C++ API for
LanceDB as of v0.3.

`SqliteEmbeddingIndex` fills the LanceDB architectural slot. It provides the same
persistence, determinism, and `IEmbeddingIndex` contract that a real LanceDB backend
would deliver. The implementation is honest about what it is — SQLite, not LanceDB
protocol — and is clearly labeled in code and documentation.

`LanceDBEmbeddingIndex` (the original stub class) remains in the codebase as a reserved
boundary for when a C++ LanceDB SDK becomes available. It throws `std::runtime_error` on
all methods. Do not use it directly.

---

## What SqliteEmbeddingIndex Provides

- **Persistent vector storage**: vectors survive process restart (stored in a SQLite file).
- **Upsert semantics**: inserting the same key replaces the previous vector and metadata.
- **Cosine similarity query**: identical algorithm to `InMemoryEmbeddingIndex`.
- **Deterministic tie-breaking**: if `|score_a - score_b| <= 1e-9`, results are ordered
  by key (ascending, lexicographic). This matches `InMemoryEmbeddingIndex` exactly.
- **Rebuildable**: the vector database is a derived store. It can be deleted and rebuilt
  from canonical sources (atoms, opportunities) at any time.

---

## Table Schema

The vector index is stored in a dedicated SQLite database (separate from the canonical
`--db` database). The schema is a single table:

```sql
CREATE TABLE IF NOT EXISTS embedding_vectors (
  key           TEXT PRIMARY KEY,
  vector_blob   BLOB NOT NULL,
  dimension     INTEGER NOT NULL,
  metadata_json TEXT NOT NULL,
  created_at    TEXT NOT NULL DEFAULT (datetime('now'))
);
```

- `key`: the `VectorKey` (typically an `AtomId.value` string).
- `vector_blob`: raw `float32` bytes, native byte order. Size = `dimension * 4`.
- `dimension`: number of floats in the vector (stored for documentation; not used to
  constrain queries, since cosine_similarity handles dimension mismatch by returning 0.0).
- `metadata_json`: arbitrary JSON string supplied by the caller.
- `created_at`: ISO 8601 timestamp set at insert time; not updated on upsert.

The database is created automatically on first open. The schema is applied via
`CREATE TABLE IF NOT EXISTS`, making it idempotent.

---

## CLI Flags

### MCP Server

```bash
# Default: in-memory vector index (no persistence)
./mcp_server

# SQLite-backed vector index (persistent)
./mcp_server --vector-backend sqlite --vector-db-path /path/to/vector/dir

# Combined with SQLite canonical DB and Redis coordinator
./mcp_server \
  --db /path/to/career.db \
  --redis tcp://localhost:6379 \
  --vector-backend sqlite \
  --vector-db-path /path/to/vectors
```

### CLI

```bash
# Default: null vector index (lexical-only matching)
./ccmcp_cli

# SQLite-backed vector index
./ccmcp_cli --vector-backend sqlite --vector-db-path /path/to/vectors

# Hybrid matching with SQLite-backed vector index
./ccmcp_cli \
  --matching-strategy hybrid \
  --vector-backend sqlite \
  --vector-db-path /path/to/vectors
```

**Fail-fast rule**: specifying `--vector-backend sqlite` without `--vector-db-path` is an
error. The process exits immediately with a clear diagnostic:

```
Error: --vector-db-path <dir> is required when --vector-backend sqlite
```

The directory in `--vector-db-path` is created if it does not exist. The SQLite database
file is placed at `<vector-db-path>/vectors.db`.

---

## Determinism and Tie-Breaking

Query results are sorted by:

1. **Score descending** — higher cosine similarity ranked first.
2. **Key ascending (lexicographic)** — tie-breaking when `|score_a - score_b| <= 1e-9`.

This is identical to `InMemoryEmbeddingIndex`. The baseline SQL query uses `ORDER BY key`
to establish a deterministic scan order before sorting; the final ordering is applied in
C++ after all scores are computed.

The tie-breaking guarantee holds across open/close cycles: it depends only on the key
strings, not on insertion order or SQLite internal row order.

---

## Vector Serialisation

Vectors are stored as raw `float32` bytes using `memcpy`:

- **Write**: `memcpy(blob.data(), v.data(), v.size() * sizeof(float))`
- **Read**: `memcpy(result.data(), blob_data, size_bytes)`

This is native-endian. Since the database is local and embedded (not networked), the
same machine that writes also reads. Portability across machines with different endianness
is not a design goal for this implementation.

If cross-machine portability is required in a future version, use a portable float
encoding (e.g., IEEE 754 little-endian explicit) in the serialisation layer.

---

## Running Integration Tests

Unit tests (`:memory:` database, no file I/O) run as part of the default test suite:

```bash
cd build && ctest --test-dir . -R "sqlite_embedding"
```

File-backed integration tests are opt-in:

```bash
export CCMCP_TEST_LANCEDB=1
cd build && ctest --test-dir . -R "sqlite_embedding"
```

The integration tests:

1. Create a temporary directory under `$TMPDIR/ccmcp_test_lancedb_*`.
2. Upsert vectors in one process scope, close the database.
3. Reopen the database and verify persistence, query ordering, and tie-breaking.
4. Remove the temporary directory on completion.

No external services are required.

---

## Migration Path to Real LanceDB

When a C++ LanceDB SDK becomes available in vcpkg:

1. Add `lancedb` to `vcpkg.json` dependencies.
2. Add `find_package(lancedb CONFIG REQUIRED)` to `CMakeLists.txt`.
3. Implement `LanceDBEmbeddingIndex` using the real SDK. The interface contract
   (`upsert`, `query`, `get`) is already defined and stable.
4. Wire `--vector-backend lancedb` to `LanceDBEmbeddingIndex` instead of
   `SqliteEmbeddingIndex`.
5. Migrate existing vector data if needed (the schema is compatible in structure; a
   rebuild from canonical sources is the safest migration path since vectors are derived).
6. Keep `SqliteEmbeddingIndex` available as a lightweight fallback that requires no
   external service.

---

## Invariants

- Vectors are a **derived store**. The canonical source of truth is the atom store
  (SQLite via `IAtomRepository`). The vector database can be deleted and rebuilt at any
  time without data loss.
- `SqliteEmbeddingIndex` does not store or modify canonical atom data. It stores only
  embeddings and metadata strings.
- Scoring in `Matcher` is **not** affected by this backend. Embeddings are used for
  candidate retrieval only; final scoring remains the deterministic lexical overlap
  formula.
