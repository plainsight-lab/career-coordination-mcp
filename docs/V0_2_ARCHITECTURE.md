# v0.2 Architecture: Storage & Retrieval Interfaces

## Overview

v0.2 Slice 1 introduces storage and retrieval interfaces with in-memory implementations, establishing architectural boundaries for future persistence systems (SQLite, LanceDB, Redis).

This document describes the interface design, implementation patterns, determinism guarantees, and migration path.

## Design Principles

1. **Interfaces Before Implementation**: Define abstractions first, implementations second
2. **Determinism by Design**: Use data structures that guarantee deterministic behavior
3. **Reference Semantics**: Service container holds references, not ownership
4. **Explicit Boundaries**: Clear separation between domain, storage, and vector layers
5. **Testability First**: In-memory implementations enable fast, deterministic tests

---

## Repository Interfaces

### IAtomRepository

```cpp
class IAtomRepository {
 public:
  virtual ~IAtomRepository() = default;
  virtual void upsert(const domain::ExperienceAtom& atom) = 0;
  [[nodiscard]] virtual std::optional<domain::ExperienceAtom> get(const core::AtomId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::ExperienceAtom> list_verified() const = 0;
  [[nodiscard]] virtual std::vector<domain::ExperienceAtom> list_all() const = 0;
};
```

**Responsibilities:**
- Store and retrieve experience atoms by ID
- Filter atoms by verification status
- Maintain deterministic ordering (sorted by AtomId)

**Invariants:**
- `list_all()` returns atoms sorted by AtomId (lexicographic)
- `list_verified()` returns only verified atoms, sorted by AtomId
- `get()` returns `std::nullopt` for missing atoms, never throws
- `upsert()` is idempotent (insert-or-replace)

---

### IOpportunityRepository

```cpp
class IOpportunityRepository {
 public:
  virtual ~IOpportunityRepository() = default;
  virtual void upsert(const domain::Opportunity& opportunity) = 0;
  [[nodiscard]] virtual std::optional<domain::Opportunity> get(const core::OpportunityId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::Opportunity> list_all() const = 0;
};
```

**Responsibilities:**
- Store and retrieve opportunities by ID
- Maintain deterministic ordering (sorted by OpportunityId)

**Invariants:**
- `list_all()` returns opportunities sorted by OpportunityId (lexicographic)
- `get()` returns `std::nullopt` for missing opportunities
- `upsert()` is idempotent

---

### IInteractionRepository

```cpp
class IInteractionRepository {
 public:
  virtual ~IInteractionRepository() = default;
  virtual void upsert(const domain::Interaction& interaction) = 0;
  [[nodiscard]] virtual std::optional<domain::Interaction> get(const core::InteractionId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::Interaction> list_by_opportunity(const core::OpportunityId& id) const = 0;
  [[nodiscard]] virtual std::vector<domain::Interaction> list_all() const = 0;
};
```

**Responsibilities:**
- Store and retrieve interactions by ID
- Filter interactions by opportunity
- Maintain deterministic ordering (sorted by InteractionId)

**Invariants:**
- `list_all()` returns interactions sorted by InteractionId (lexicographic)
- `list_by_opportunity()` returns filtered interactions, sorted by InteractionId
- `get()` returns `std::nullopt` for missing interactions
- `upsert()` is idempotent

---

## In-Memory Implementations

### Design Pattern

All in-memory repositories follow the same pattern:

```cpp
class InMemoryXRepository final : public IXRepository {
 private:
  std::map<IdType, Entity> storage_;
};
```

**Key Design Decisions:**

1. **`std::map` for Storage**: Guarantees deterministic iteration order (sorted by key)
2. **Strong ID Types**: `AtomId`, `OpportunityId`, etc. have `operator<=>` for sorting
3. **Final Classes**: In-memory implementations are not intended for extension
4. **No Error Handling**: In-memory operations cannot fail (infinite memory assumption)

### Determinism Guarantee

`std::map` maintains keys in sorted order based on `operator<=>`. Since ID types use lexicographic comparison:

```cpp
struct AtomId {
  std::string value;
  auto operator<=>(const AtomId&) const = default;
};
```

Iteration order is deterministic and platform-independent.

**Example:**
```cpp
repo.upsert({AtomId{"atom-003"}, ...});
repo.upsert({AtomId{"atom-001"}, ...});
repo.upsert({AtomId{"atom-002"}, ...});

auto all = repo.list_all();
// Guaranteed order: atom-001, atom-002, atom-003
```

---

## Vector Index Interface

### IEmbeddingIndex

```cpp
class IEmbeddingIndex {
 public:
  virtual ~IEmbeddingIndex() = default;
  virtual void upsert(const VectorKey& key, const Vector& embedding, const std::string& metadata) = 0;
  [[nodiscard]] virtual std::vector<VectorSearchResult> query(const Vector& query_vector, size_t top_k) const = 0;
  [[nodiscard]] virtual std::optional<Vector> get(const VectorKey& key) const = 0;
};
```

**Responsibilities:**
- Store and retrieve vector embeddings by key
- Perform similarity search (query)
- Return top-k results with deterministic tie-breaking

---

### NullEmbeddingIndex

No-op implementation. All methods are safe no-ops or return empty results.

**Use Case:** When vector search is not needed (v0.2 CLI).

---

### InMemoryEmbeddingIndex

In-memory implementation using cosine similarity for query operations.

**Algorithm:**
1. Compute cosine similarity for all stored vectors
2. Sort by score (descending)
3. Tie-breaking: if scores within epsilon (1e-9), sort by key (lexicographic)
4. Return top-k results

**Determinism:** Epsilon-based floating-point comparison + lexicographic key ordering ensures reproducible results across platforms.

**Cosine Similarity:**
```
similarity(a, b) = dot(a, b) / (norm(a) * norm(b))
```

Returns 0.0 if either vector has zero magnitude or dimensions mismatch.

---

### LanceDBEmbeddingIndex (Stub)

All methods throw `std::runtime_error("LanceDB not implemented in v0.2")`.

**Purpose:** Demonstrates interface extensibility. Real LanceDB integration planned for future slice.

---

## Service Container

### Design

```cpp
struct Services {
  storage::IAtomRepository& atoms;
  storage::IOpportunityRepository& opportunities;
  storage::IInteractionRepository& interactions;
  storage::IAuditLog& audit_log;
  vector::IEmbeddingIndex& vector_index;

  Services(...);  // Constructor takes references

  ~Services() = default;

  // Prevent copying and moving
  Services(const Services&) = delete;
  Services& operator=(const Services&) = delete;
  Services(Services&&) = delete;
  Services& operator=(Services&&) = delete;
};
```

**Design Rationale:**

1. **Reference Semantics**: Services holds references, not ownership
   - Follows C++ Core Guidelines F.7 (pass by reference when ownership not transferred)
   - Lifetime management is caller's responsibility

2. **No Copy/Move**: Prevents accidental lifetime issues
   - Services is a composition root, not a value type
   - References can't be rebound after construction

3. **Explicit Dependency Injection**: All dependencies visible at construction
   - No hidden state or global variables
   - Easy to test (inject mocks)

**Usage Pattern (CLI):**
```cpp
int main() {
  // Create concrete instances on stack
  InMemoryAtomRepository atom_repo;
  InMemoryOpportunityRepository opportunity_repo;
  InMemoryInteractionRepository interaction_repo;
  InMemoryAuditLog audit_log;
  NullEmbeddingIndex vector_index;

  // Bind references via Services
  Services services{atom_repo, opportunity_repo, interaction_repo,
                    audit_log, vector_index};

  // Use services
  services.atoms.upsert(atom);
  auto verified = services.atoms.list_verified();
}
```

---

## Future Implementations

### SQLite Adapter (v0.2 Slice 2)

**Planned Design:**
```cpp
class SQLiteAtomRepository final : public IAtomRepository {
 private:
  sqlite3* db_;  // Connection owned by this class or passed as reference
};
```

**Key Considerations:**
- Schema design (tables, indexes)
- Transaction management (ACID guarantees)
- Error handling (Result<T,E> pattern at service layer)
- Deterministic ordering (ORDER BY in SQL queries)

---

### LanceDB Adapter (v0.2 Slice 3)

**Planned Design:**
```cpp
class LanceDBEmbeddingIndex final : public IEmbeddingIndex {
 private:
  lancedb::Connection conn_;
  std::string table_name_;
};
```

**Key Considerations:**
- Embedding dimension consistency
- Metadata serialization (JSON?)
- Bulk upsert for performance
- Query performance vs determinism tradeoffs

---

### Redis State Adapter (Future)

Planned for interaction state machine persistence.

```cpp
class RedisInteractionRepository final : public IInteractionRepository {
 private:
  redis::Connection conn_;
};
```

---

## Determinism Guarantees

### Repository Ordering

All repositories guarantee deterministic ordering via `std::map`:

| Repository              | Sort Key         | Order          |
|-------------------------|------------------|----------------|
| InMemoryAtomRepository  | AtomId           | Lexicographic  |
| InMemoryOpportunityRepo | OpportunityId    | Lexicographic  |
| InMemoryInteractionRepo | InteractionId    | Lexicographic  |

### Vector Index Ordering

InMemoryEmbeddingIndex guarantees deterministic query results:

1. **Score Comparison**: If `|score_a - score_b| > 1e-9`, higher score first
2. **Tie-Breaking**: If scores within epsilon, lexicographic key order

### CLI Output Stability

CLI output remains byte-identical to v0.1 because:
- Same DeterministicIdGenerator seed
- Same FixedClock timestamp
- std::map preserves insertion order semantics (via sorted keys)
- Matching logic unchanged

---

## Testing Strategy

### Unit Tests

Each component has dedicated test suite:

| Test File                                    | Tests | Coverage                                    |
|----------------------------------------------|-------|---------------------------------------------|
| `test_inmemory_atom_repository.cpp`          | 6     | CRUD, filtering, deterministic ordering     |
| `test_inmemory_opportunity_repository.cpp`   | 4     | CRUD, deterministic ordering                |
| `test_inmemory_interaction_repository.cpp`   | 3     | CRUD, filtering, deterministic ordering     |
| `test_null_embedding_index.cpp`              | 3     | No-op behavior                              |
| `test_inmemory_embedding_index.cpp`          | 7     | Cosine similarity, tie-breaking, edge cases |

**Total:** 23 new test cases, 66 new assertions

### Integration Tests

Existing v0.1 tests verify CLI integration:
- 22 test cases (unchanged)
- 197 assertions (unchanged)

---

## Migration Path

### Phase 1 (Current): In-Memory Adapters
- ✅ Interfaces defined
- ✅ In-memory implementations
- ✅ Service container pattern
- ✅ CLI integration (output stable)

### Phase 2: SQLite Persistence
- Define schema (atoms, opportunities, interactions tables)
- Implement SQLiteXRepository adapters
- Add migration logic (create tables, indexes)
- Swap in-memory → SQLite in CLI
- Verify determinism (tests + CLI output)

### Phase 3: LanceDB Vector Search
- Integrate LanceDB SDK
- Implement LanceDBEmbeddingIndex
- Define embedding strategy (model, dimensions)
- Add embedding generation to CLI workflow
- Swap NullEmbeddingIndex → LanceDB

### Phase 4: Redis State Machine
- Define state transition persistence model
- Implement RedisInteractionRepository
- Add state machine recovery logic
- Swap in-memory → Redis for interactions

---

## Architectural Boundaries

### Domain Layer
- Pure business logic (ExperienceAtom, Opportunity, Interaction)
- No persistence awareness
- Invariants enforced via validation

### Storage Layer
- Repository interfaces (IAtomRepository, etc.)
- Adapters (in-memory, SQLite, Redis)
- No domain logic

### Vector Layer
- Embedding index interface (IEmbeddingIndex)
- Adapters (null, in-memory, LanceDB)
- No domain logic

### Core Layer
- Service container (composition root)
- Dependency injection
- No business logic

### CLI Layer
- Composition root (creates concrete instances)
- Wires dependencies via Services
- Minimal logic (orchestration only)

---

## Performance Considerations

### In-Memory Implementations

**Strengths:**
- O(log n) upsert/get (std::map)
- O(n) list operations (iterate map)
- Zero I/O latency
- Deterministic, reproducible

**Weaknesses:**
- O(n) memory (entire dataset in RAM)
- No persistence (data lost on exit)
- Not suitable for large datasets (> 100k atoms)

**Use Case:** Testing, development, v0.2 demos

### Future SQLite Adapter

**Expected Characteristics:**
- O(log n) indexed lookups
- O(n) full scans
- Disk I/O latency
- Transaction overhead

**Optimizations:**
- Prepared statements
- Batch inserts (transactions)
- Indexes on ID columns

### Future LanceDB Adapter

**Expected Characteristics:**
- O(log n) approximate nearest neighbors (ANN)
- Embedding dimension overhead
- Network latency (if remote)

**Optimizations:**
- Bulk upserts
- Pre-computed embeddings (cache)
- Index tuning (HNSW, IVF)

---

## Error Handling

### Current (In-Memory)

No error handling required:
- `upsert()` cannot fail (infinite memory assumption)
- `get()` returns `std::nullopt` for missing entities
- No exceptions thrown

### Future (SQLite, LanceDB, Redis)

Wrap operations in try-catch at service layer:

```cpp
void SomeService::store_atom(const ExperienceAtom& atom) {
  try {
    services.atoms.upsert(atom);
  } catch (const sqlite::error& e) {
    // Log error, return Result<void, Error>
  }
}
```

Or use `Result<T,E>` pattern in interfaces:

```cpp
virtual Result<void, StorageError> upsert(...) = 0;
```

Decision deferred to SQLite integration slice.

---

## Compliance

### C++ Core Guidelines

- **C.2**: Structs for independent members (ExperienceAtom, Opportunity)
- **F.7**: Pass by reference when ownership not transferred (Services)
- **I.11**: Never transfer ownership via raw pointer (references, not pointers)
- **C.21**: Default or delete special members (Services)
- **ES.20**: Always initialize objects (all constructors initialize)

### Determinism Requirements

All components satisfy:
- **Functional Determinism**: Same inputs → same outputs
- **Environmental Determinism**: Platform-independent behavior
- **Reproducible Tests**: All tests pass consistently

---

## Summary

v0.2 Slice 1 establishes:
- ✅ Clean storage and vector index interfaces
- ✅ In-memory implementations with determinism guarantees
- ✅ Service container pattern for dependency injection
- ✅ CLI integration maintaining v0.1 output stability
- ✅ 23 new test cases verifying correctness

**Next Steps:**
1. v0.2 Slice 2: SQLite persistence
2. v0.2 Slice 3: LanceDB vector search
3. v0.2 Slice 4: Redis state machine
4. v0.2 Slice 5: MCP server integration

---

## v0.2 Slice 2: SQLite Persistence (Canonical Storage)

### Overview

SQLite is the **canonical persistence layer** for career-coordination-mcp. All atoms, opportunities, interactions, and audit events are stored in SQLite for durability and production use.

**Status:** ✅ Implemented in v0.2 Slice 2

### Architecture Decision

- **SQLite** = Canonical persistence (CRUD, relational data)
- **LanceDB** = Vector search (embeddings, similarity) - *planned for Slice 3*
- **Redis** = State machine persistence (interactions) - *planned for Slice 4*

### Schema v1

SQLite schema v1 provides tables for all canonical entities:

#### Tables

**atoms**
- `atom_id` TEXT PRIMARY KEY
- `domain` TEXT
- `title` TEXT
- `claim` TEXT
- `tags_json` TEXT (JSON array, deterministic sort)
- `verified` INTEGER (0/1)
- `evidence_refs_json` TEXT (JSON array)

**opportunities**
- `opportunity_id` TEXT PRIMARY KEY
- `company` TEXT
- `role_title` TEXT
- `source` TEXT

**requirements** (one-to-many with opportunities)
- `opportunity_id` TEXT (FK)
- `idx` INTEGER (preserves order)
- `text` TEXT
- `tags_json` TEXT (JSON array)
- `required` INTEGER (0/1)
- PRIMARY KEY: (opportunity_id, idx)

**interactions**
- `interaction_id` TEXT PRIMARY KEY
- `contact_id` TEXT
- `opportunity_id` TEXT (FK)
- `state` INTEGER

**audit_events** (append-only)
- `event_id` TEXT PRIMARY KEY
- `trace_id` TEXT
- `event_type` TEXT
- `payload` TEXT (JSON)
- `created_at` TEXT (ISO 8601)
- `entity_ids_json` TEXT (JSON array)
- `idx` INTEGER (monotonic per trace)

#### Indexes

- `idx_atoms_verified` on `atoms(verified)`
- `idx_audit_events_trace` on `audit_events(trace_id, idx)`

### Schema Versioning

**Mechanism:** `schema_version` table tracks applied migrations.

```sql
CREATE TABLE schema_version (
  version INTEGER PRIMARY KEY,
  applied_at TEXT NOT NULL
);
```

**Current version:** 1

**Migration strategy:**
- Check `schema_version` table for current version
- Apply incremental migrations (v1 → v2 → v3, etc.)
- Each migration is idempotent (CREATE IF NOT EXISTS)

### SQLite Implementations

All SQLite repositories implement the same interfaces as in-memory repositories:

| Interface                  | SQLite Implementation          | Backend           |
|----------------------------|--------------------------------|-------------------|
| IAtomRepository            | SqliteAtomRepository           | atoms table       |
| IOpportunityRepository     | SqliteOpportunityRepository    | opportunities + requirements |
| IInteractionRepository     | SqliteInteractionRepository    | interactions      |
| IAuditLog                  | SqliteAuditLog                 | audit_events      |

### Determinism Guarantees

All SQLite queries use `ORDER BY` to ensure deterministic results:

```sql
-- Atoms: ORDER BY atom_id
SELECT * FROM atoms WHERE verified = 1 ORDER BY atom_id;

-- Requirements: ORDER BY idx (preserves insertion order)
SELECT * FROM requirements WHERE opportunity_id = ? ORDER BY idx;

-- Audit events: ORDER BY idx (monotonic append index)
SELECT * FROM audit_events WHERE trace_id = ? ORDER BY idx;
```

### JSON Serialization

Tags and arrays are serialized to JSON using `nlohmann::json`:

```cpp
// Serialize
nlohmann::json tags_json = atom.tags;
sqlite3_bind_text(stmt, idx, tags_json.dump().c_str(), -1, SQLITE_TRANSIENT);

// Deserialize
std::string tags_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
nlohmann::json tags_json = nlohmann::json::parse(tags_str);
atom.tags = tags_json.get<std::vector<std::string>>();
```

**Determinism:** `nlohmann::json` produces deterministic output for arrays.

### CLI Usage

#### Default (In-Memory)

```bash
./ccmcp_cli
# Uses in-memory repositories (no persistence)
```

#### With SQLite

```bash
./ccmcp_cli --db path/to/database.sqlite
# Uses SQLite persistence
```

**File vs In-Memory:**
```bash
./ccmcp_cli --db ":memory:"
# In-memory SQLite (useful for testing)
```

**Database Creation:**
- If database file doesn't exist, it's created automatically
- Schema v1 is applied on first run

#### Example: Persistent Workflow

```bash
# Run 1: Create database and populate
./ccmcp_cli --db career.db

# Run 2: Database persists, can query existing data
./ccmcp_cli --db career.db
```

### Transaction Handling

**OpportunityRepository:** Uses transactions for atomic upserts (opportunity + requirements):

```cpp
db->exec("BEGIN TRANSACTION");
// Upsert opportunity
// Delete old requirements
// Insert new requirements
db->exec("COMMIT");
```

**Rollback on error:** If any step fails, transaction is rolled back.

### Error Handling

SQLite operations can fail (disk full, permissions, corruption). Error handling strategy:

**Current (v0.2 Slice 2):**
- Upsert operations: silent failure (matches in-memory semantics)
- Query operations: return empty results on error

**Future (v0.3+):**
- Wrap operations in Result<T, StorageError>
- Surface errors to caller
- Add retry logic for transient failures

### Testing Strategy

**Integration Tests:**
- `test_sqlite_atom_repository.cpp`: Roundtrip, ordering, upsert replace
- `test_sqlite_opportunity_repository.cpp`: Requirements order preservation
- `test_sqlite_audit_log.cpp`: Append-only, deterministic query

**Test Database:** All tests use `:memory:` for fast, isolated testing.

**Test Coverage:**
- CRUD operations
- Deterministic ordering (ORDER BY)
- Edge cases (empty, missing, duplicates)
- Transaction semantics (opportunity + requirements)

### Performance Characteristics

**In-Memory (Default):**
- O(log n) upsert/get (std::map)
- O(n) list operations
- Zero I/O latency
- No persistence

**SQLite:**
- O(log n) indexed lookups
- O(n) full scans
- Disk I/O latency
- Durable persistence

**Trade-off:** SQLite adds I/O overhead but provides durability.

**Optimization opportunities (future):**
- Prepared statement caching
- Batch inserts (BEGIN/COMMIT around loops)
- Index tuning

### File Layout

```
include/ccmcp/storage/sqlite/
├── sqlite_db.h                      # Database management, schema versioning
├── sqlite_atom_repository.h
├── sqlite_opportunity_repository.h
├── sqlite_interaction_repository.h
└── sqlite_audit_log.h

src/storage/sqlite/
├── sqlite_db.cpp                    # Schema v1 embedded as const char*
├── sqlite_atom_repository.cpp
├── sqlite_opportunity_repository.cpp
├── sqlite_interaction_repository.cpp
└── sqlite_audit_log.cpp

resources/sqlite/
└── schema_v1.sql                    # SQL schema (reference, not used at runtime)
```

### Dependencies

**vcpkg.json:**
```json
"dependencies": [
  "sqlite3"
]
```

**CMake:**
```cmake
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(ccmcp PUBLIC unofficial::sqlite3::sqlite3)
```

### Future Enhancements (Post-v0.2)

**v0.3: Advanced SQLite Features**
- Connection pooling (multi-threaded)
- WAL mode (Write-Ahead Logging)
- Prepared statement cache
- Batch insert optimization

**v0.4: Migrations**
- Schema v2, v3, etc.
- Incremental migration system
- Rollback support

**v0.5: Backup/Export**
- SQLite backup API
- Export to JSON
- Import from JSON

---

## Summary: v0.2 Complete Architecture

| Component          | Implementation   | Purpose                          | Status |
|--------------------|------------------|----------------------------------|--------|
| Atoms              | SQLite           | Canonical persistence            | ✅     |
| Opportunities      | SQLite           | Canonical persistence            | ✅     |
| Interactions       | SQLite           | Canonical persistence            | ✅     |
| Audit Events       | SQLite           | Append-only log                  | ✅     |
| Vector Index       | NullEmbeddingIndex | Placeholder for LanceDB        | ✅     |
| In-Memory          | Available        | Testing, development             | ✅     |

**Next Steps:**
- v0.2 Slice 3: LanceDB vector search
- v0.2 Slice 4: Redis state machine
- v0.2 Slice 5: MCP server
