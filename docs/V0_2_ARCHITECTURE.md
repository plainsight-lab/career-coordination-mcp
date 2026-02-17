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

---

## v0.2 Slice 3: Hybrid Lexical + Embedding Retrieval

### Overview

Hybrid retrieval expands candidate atom selection by combining:
1. **Lexical retrieval** (token overlap)
2. **Embedding retrieval** (semantic similarity via IVectorIndex)

**Key principle**: Embeddings are for **retrieval only**, not scoring. Final scoring remains deterministic lexical overlap.

**Status:** ✅ Implemented in v0.2 Slice 3

### Architecture Decision

**Retrieval ≠ Scoring**

- Embeddings expand recall (find more candidate atoms)
- Lexical scoring remains the source of truth
- Determinism preserved via:
  - Stable embedding generation (DeterministicStubEmbeddingProvider)
  - Deterministic vector search (InMemoryVectorIndex sorts by score, then key)
  - Deterministic merge (std::set preserves lexicographic order)

### Hybrid Retrieval Algorithm

#### Input
- Opportunity requirements (R₁, R₂, ..., Rₙ)
- All verified ExperienceAtoms

#### Stage 1: Lexical Candidate Selection

1. Combine all requirement texts into query tokens (deduplicated, sorted)
2. For each verified atom:
   - Tokenize claim + title + tags
   - Compute overlap score: |R ∩ A| / |R|
3. Sort by score descending, then atom_id ascending (tie-break)
4. Select top K_lex candidates (default K_lex = 25)

#### Stage 2: Embedding Candidate Selection

1. Build query embedding from concatenated requirement texts
2. Query IVectorIndex for top K_emb results (default K_emb = 25)
3. Extract atom IDs from search results

#### Stage 3: Merge Candidates

1. Union lexical and embedding atom IDs (std::set for deterministic ordering)
2. Retrieve atom objects from repository
3. Result: merged candidate set (sorted by atom_id)

#### Stage 4: Final Scoring

1. For each requirement, score **only** the merged candidates (not all atoms)
2. Scoring formula unchanged: |R ∩ A| / |R|
3. Tie-breaking unchanged: lexicographically smaller atom_id wins

#### Provenance Tracking

MatchReport includes RetrievalStats:
```cpp
struct RetrievalStats {
  size_t lexical_candidates;    // Top-K from lexical scoring
  size_t embedding_candidates;  // Top-K from vector search
  size_t merged_candidates;     // Union of both sets
};
```

### IEmbeddingProvider Interface

#### Purpose
Abstraction boundary for embedding generation. Enables:
- Deterministic testing (DeterministicStubEmbeddingProvider)
- Future integration with real models (OpenAI, sentence-transformers, etc.)
- Disabling embeddings (NullEmbeddingProvider)

#### Interface
```cpp
class IEmbeddingProvider {
 public:
  virtual ~IEmbeddingProvider() = default;
  virtual vector::Vector embed_text(std::string_view text) const = 0;
  virtual size_t dimension() const = 0;
};
```

#### Implementations

**NullEmbeddingProvider**
- Returns empty vectors (dimension = 0)
- Disables embedding retrieval (falls back to pure lexical)

**DeterministicStubEmbeddingProvider**
- Generates stable vectors from text statistics
- Strategy: Hash tokens to vector indices, accumulate counts, normalize to unit vector
- Guarantees: Same text → same vector (deterministic)
- Dimension: 128 (configurable)

### Matching Strategy Enum

```cpp
enum class MatchingStrategy {
  kDeterministicLexicalV01,       // v0.1 default: all atoms scored
  kHybridLexicalEmbeddingV02,     // v0.2: top-K lexical + top-K embedding
};
```

### CLI Usage

#### Default Mode (v0.1 Lexical)

```bash
./ccmcp_cli
# Uses all verified atoms for scoring (no embedding retrieval)
```

Output:
```json
{
  "strategy": "deterministic_lexical_v0.1",
  "retrieval_stats": {
    "lexical_candidates": 2,    // All verified atoms
    "embedding_candidates": 0,
    "merged_candidates": 2
  }
}
```

#### Hybrid Mode

```bash
./ccmcp_cli --matching-strategy hybrid
# Uses lexical top-K + embedding top-K
```

Output:
```json
{
  "strategy": "hybrid_lexical_embedding_v0.2",
  "retrieval_stats": {
    "lexical_candidates": 25,
    "embedding_candidates": 25,
    "merged_candidates": 30     // Union may overlap
  }
}
```

#### Vector Backend Selection

```bash
./ccmcp_cli --matching-strategy hybrid --vector-backend inmemory
# Uses InMemoryVectorIndex (default)

./ccmcp_cli --matching-strategy hybrid --vector-backend lancedb
# Error: LanceDB not yet implemented in v0.2
```

### Hybrid Configuration

```cpp
struct HybridConfig {
  size_t k_lexical{25};    // Top K from lexical pre-scoring
  size_t k_embedding{25};  // Top K from vector search
};
```

**Future:** CLI flags for K values (not implemented in v0.2)

### Determinism Guarantees

#### Functional Determinism
- Same inputs → same outputs (byte-stable)
- DeterministicStubEmbeddingProvider: same text → same vector
- InMemoryVectorIndex: cosine similarity with deterministic tie-breaks (epsilon + key)
- Merge: std::set preserves lexicographic atom ID order

#### Environmental Determinism
- No OS/locale dependencies
- No floating-point non-determinism (uses epsilon comparisons)
- No random number generation

### Testing Strategy

**Test Coverage:**
- `test_matcher_hybrid_retrieval.cpp`: Recall expansion, fallback, tie-breaking
- `test_matcher_hybrid_determinism.cpp`: Byte-stable output, stable embeddings

**Key Tests:**
1. **Hybrid expands recall**: Atom missed by lexical but found via embedding
2. **Determinism**: Two runs produce identical output
3. **Tie-breaking preserved**: atom-a < atom-z (lexicographic)
4. **NullEmbeddingProvider fallback**: Hybrid mode with empty embeddings works

### Performance Characteristics

**v0.1 Lexical (Default):**
- Candidates: All verified atoms
- Complexity: O(R × A × T) where R = requirements, A = atoms, T = avg tokens

**v0.2 Hybrid:**
- Candidates: top-K_lex + top-K_emb (merged)
- Complexity: O(R × A × T) + O(A × log A) for sorting + O(K_emb × dim) for embedding search
- Trade-off: More work for candidate selection, but potentially better recall

### Integration Points

**Services Container:**
```cpp
struct Services {
  // ... existing fields
  embedding::IEmbeddingProvider& embedding_provider;
};
```

**Matcher API:**
```cpp
Matcher::evaluate(
  const Opportunity& opportunity,
  const std::vector<ExperienceAtom>& atoms,
  const IEmbeddingProvider* embedding_provider = nullptr,  // v0.2: pass for hybrid
  const IVectorIndex* vector_index = nullptr               // v0.2: pass for hybrid
);
```

### File Layout

```
include/ccmcp/embedding/
└── embedding_provider.h         # IEmbeddingProvider interface + implementations

src/embedding/
└── deterministic_stub_embedding_provider.cpp

tests/
├── test_matcher_hybrid_retrieval.cpp
└── test_matcher_hybrid_determinism.cpp
```

### Future Enhancements (Post-v0.2)

**v0.3: Real Embedding Models**
- OpenAI embeddings (text-embedding-3-small)
- Local sentence-transformers
- Caching strategy for repeated embeddings

**v0.4: LanceDB Integration**
- Production vector index
- Persistent embeddings
- Batch indexing pipeline

**v0.5: Advanced Hybrid**
- Configurable K values via CLI
- Weighted fusion (lexical + semantic scores)
- Multi-stage retrieval (coarse + fine)

---

## Summary: v0.2 Complete Architecture

| Component          | Implementation   | Purpose                          | Status |
|--------------------|------------------|----------------------------------|--------|
| Atoms              | SQLite           | Canonical persistence            | ✅     |
| Opportunities      | SQLite           | Canonical persistence            | ✅     |
| Interactions       | SQLite           | Canonical persistence            | ✅     |
| Audit Events       | SQLite           | Append-only log                  | ✅     |
| Vector Index       | InMemoryVectorIndex | Testing/development           | ✅     |
| Embedding Provider | DeterministicStubEmbeddingProvider | Testing       | ✅     |
| Hybrid Retrieval   | Lexical + Embedding | Recall expansion             | ✅     |
| In-Memory          | Available        | Testing, development             | ✅     |

---

## v0.2 Slice 4: Redis-backed Interaction State Machine

### Overview

Redis provides atomic, idempotent coordination for Interaction state transitions. Redis is **NOT** a general-purpose datastore—it is used **only** for coordinating the Interaction state machine to ensure correctness under concurrency.

**Status:** ✅ Implemented in v0.2 Slice 4

### Architecture Decision

**Redis Scope: Interaction Coordination Only**

- **Canonical Persistence**: SQLite (atoms, opportunities, interactions, audit events)
- **Coordination Layer**: Redis (interaction state + transition index + idempotency receipts)
- **Purpose**: Atomic transitions + idempotency + conflict detection

Redis does NOT store:
- Atoms, opportunities, or match reports
- Audit events (those go to SQLite via IAuditLog)
- Any domain data beyond interaction state coordination

### Why Redis for Interaction Coordination?

**Problem**: Multiple workers attempting concurrent state transitions must not corrupt state.

**Solution**: Redis provides atomic operations (via Lua scripts) to:
1. Check idempotency receipt (has this transition already been applied?)
2. Validate current state and transition_index (detect conflicts)
3. Update state + increment transition_index
4. Record idempotency receipt

All in one atomic operation.

### IInteractionCoordinator Interface

```cpp
class IInteractionCoordinator {
 public:
  // Apply state transition atomically with idempotency
  TransitionResult apply_transition(
      const InteractionId& interaction_id,
      InteractionEvent event,
      const std::string& idempotency_key);

  // Get current state and transition index
  std::optional<StateInfo> get_state(const InteractionId& interaction_id) const;

  // Initialize new interaction
  bool create_interaction(const InteractionId& interaction_id,
                         const ContactId& contact_id,
                         const OpportunityId& opportunity_id);
};
```

**TransitionOutcome:**
- `kApplied`: Transition successfully applied
- `kAlreadyApplied`: Idempotency—same key already applied
- `kConflict`: Concurrent modification detected (reserved for future use)
- `kNotFound`: Interaction does not exist
- `kInvalidTransition`: Domain validation failed (not allowed from current state)
- `kBackendError`: Redis unavailable or error

### Redis Data Model

#### State Record

**Key**: `ccmcp:interaction:{id}:state` (Redis hash)

**Fields**:
- `state` (int): InteractionState enum value (0=kDraft, 1=kReady, 2=kSent, 3=kResponded, 4=kClosed)
- `transition_index` (int): Monotonic counter for optimistic concurrency control
- `contact_id` (string): Associated contact
- `opportunity_id` (string): Associated opportunity

**Example**:
```
HGETALL ccmcp:interaction:int-001:state
> state: 2
> transition_index: 3
> contact_id: contact-001
> opportunity_id: opp-001
```

#### Idempotency Receipt

**Key**: `ccmcp:interaction:{id}:idem:{idempotency_key}` (Redis hash)

**Fields**:
- `after_state` (int): State after transition was applied
- `transition_index` (int): Transition index when applied
- `applied_event` (int): Event that was applied

**Purpose**: Detect replays of the same idempotency key.

**Example**:
```
HGETALL ccmcp:interaction:int-001:idem:request-abc-123
> after_state: 1
> transition_index: 1
> applied_event: 0
```

**TTL**: No expiration by default (for determinism and simplicity). Future: configurable TTL.

### Atomicity via Lua Script

**Strategy**: Single Lua script performs:
1. Check if interaction exists (`EXISTS`)
2. Check idempotency receipt (`EXISTS`)
3. If receipt exists, return `kAlreadyApplied` with cached result
4. Read current state and transition_index (`HGETALL`)
5. Update state and increment transition_index (`HSET`)
6. Write idempotency receipt (`HSET`)

**Guarantees**:
- Atomic: All steps succeed or all fail
- No race conditions: Script executes atomically on Redis server
- Idempotent: Same key returns same result

**Lua Script** (simplified):
```lua
-- Check existence
if redis.call('EXISTS', state_key) == 0 then
  return {3, 0, 0, 0}  -- NotFound
end

-- Check idempotency
if redis.call('EXISTS', idem_key) == 1 then
  local after_state = redis.call('HGET', idem_key, 'after_state')
  local transition_index = redis.call('HGET', idem_key, 'transition_index')
  return {1, after_state, after_state, transition_index}  -- AlreadyApplied
end

-- Update state
local current_state = redis.call('HGET', state_key, 'state')
local current_index = redis.call('HGET', state_key, 'transition_index')
local next_index = current_index + 1

redis.call('HSET', state_key, 'state', new_state)
redis.call('HSET', state_key, 'transition_index', next_index)

-- Record idempotency receipt
redis.call('HSET', idem_key, 'after_state', new_state)
redis.call('HSET', idem_key, 'transition_index', next_index)
redis.call('HSET', idem_key, 'applied_event', event)

return {0, current_state, new_state, next_index}  -- Applied
```

### Domain Validation

**Important**: Transition validation logic remains in domain code (`Interaction::can_transition`).

**Flow**:
1. Coordinator reads current state from Redis
2. Calls `Interaction::can_transition(event)` to validate
3. If invalid, returns `kInvalidTransition` (does NOT call Lua script)
4. If valid, computes new state via `Interaction::apply(event)`
5. Passes new state to Lua script for atomic commit

**Rationale**: Keep domain logic in domain code, not in Lua scripts.

### Idempotency Semantics

**First Call** (with idempotency key `K`):
- Validates transition
- Applies transition
- Increments transition_index
- Records receipt for key `K`
- Returns `kApplied`

**Second Call** (same key `K`):
- Checks receipt for key `K`
- Receipt exists: returns `kAlreadyApplied` with cached `after_state` and `transition_index`
- **Does NOT** apply transition again
- **Does NOT** increment transition_index

**Different Keys**: Each unique idempotency key applies once.

### Conflict Detection

**Optimistic Concurrency**: `transition_index` acts as version number.

**Scenario**: Two workers race to apply different transitions:
- Worker A: Reads state (transition_index=5), validates, prepares transition
- Worker B: Reads state (transition_index=5), validates, prepares transition
- Worker A: Commits first (transition_index=6)
- Worker B: Commits second

**Current Behavior**: Worker B succeeds if transition is still valid from new state.

**Future Enhancement** (not in v0.2): Explicit optimistic lock check in Lua script.

### Implementations

#### InMemoryInteractionCoordinator

**Purpose**: Default coordinator for testing and development.

**Storage**: `std::map` with `std::mutex` for thread safety.

**Features**:
- Idempotency tracking (in-memory map)
- Deterministic (no time dependencies)
- No external dependencies
- Used by all unit tests

**Usage**:
```cpp
interaction::InMemoryInteractionCoordinator coordinator;
coordinator.create_interaction(id, contact, opportunity);
auto result = coordinator.apply_transition(id, event, "idem-key-001");
```

#### RedisInteractionCoordinator

**Purpose**: Production coordinator with Redis backend.

**Connection**: Requires Redis URI (e.g., `"tcp://127.0.0.1:6379"`).

**Features**:
- Atomic transitions via Lua script
- Idempotency receipts in Redis
- Throws `std::runtime_error` if Redis unavailable

**Usage**:
```cpp
try {
  interaction::RedisInteractionCoordinator coordinator("tcp://localhost:6379");
  coordinator.create_interaction(id, contact, opportunity);
  auto result = coordinator.apply_transition(id, event, "idem-key-001");
} catch (const std::runtime_error& e) {
  // Handle Redis connection failure
}
```

### CLI Integration

**Note**: CLI integration for Redis coordinator is **not yet implemented** in v0.2 Slice 4.

**Planned CLI Flags** (future):
```bash
# Use in-memory coordinator (default)
./ccmcp_cli

# Use Redis coordinator
./ccmcp_cli --interaction-backend redis --redis tcp://localhost:6379
```

**Current Status**: Coordinators implemented and tested, but not wired into CLI Services container.

### Testing Strategy

#### Unit Tests (Always Run)

**File**: `test_inmemory_interaction_coordinator.cpp`

**Tests**:
- Create and get state
- Valid transitions
- Invalid transitions
- Idempotency (same key returns AlreadyApplied)
- Idempotency (different keys both apply)
- Transition on non-existent interaction
- Full state machine lifecycle (kDraft → kReady → kSent → kResponded → kClosed)

**All tests pass without Redis.**

#### Integration Tests (Opt-in)

**File**: `test_redis_interaction_coordinator.cpp`

**Requires**: Redis running at `tcp://127.0.0.1:6379` (or `CCMCP_REDIS_URI` environment variable)

**Enable**:
```bash
export CCMCP_TEST_REDIS=1
./build/tests/ccmcp_tests "[redis]"
```

**Tests**:
- Idempotency (same key returns AlreadyApplied)
- Concurrent transitions (one succeeds, other detects invalid)
- Valid transition sequence
- Invalid transition rejected

**Default**: Tests are skipped if `CCMCP_TEST_REDIS` is not set.

### Error Handling

**Redis Unavailable**: Returns `TransitionOutcome::kBackendError` with error message.

**Domain Validation Failure**: Returns `TransitionOutcome::kInvalidTransition`.

**Interaction Not Found**: Returns `TransitionOutcome::kNotFound`.

**Idempotency Replay**: Returns `TransitionOutcome::kAlreadyApplied` (not an error).

### Performance Characteristics

**In-Memory Coordinator**:
- O(1) create, get_state, apply_transition
- Thread-safe via coarse-grained mutex
- No I/O latency

**Redis Coordinator**:
- O(1) create, get_state, apply_transition (single Redis round-trip each)
- Network I/O latency to Redis
- Atomic via Lua script (no multi-round-trip WATCH/MULTI/EXEC)

### Future Enhancements (Post-v0.2)

**v0.3: CLI Integration**
- Wire RedisInteractionCoordinator into Services
- Add `--interaction-backend` and `--redis` CLI flags
- Support both in-memory and Redis modes

**v0.4: TTL Configuration**
- Configurable TTL for idempotency receipts
- Default: no TTL (determinism)
- Optional: 24-hour TTL for production cleanup

**v0.5: Explicit Optimistic Locking**
- Pass expected transition_index to Lua script
- Return `kConflict` if index mismatch (stale read)
- Enable retry logic in application layer

**v0.6: Redis Sentinel/Cluster**
- Support Redis Sentinel for high availability
- Support Redis Cluster for horizontal scaling

---

## Summary: v0.2 Complete Architecture

| Component          | Implementation   | Purpose                          | Status |
|--------------------|------------------|----------------------------------|--------|
| Atoms              | SQLite           | Canonical persistence            | ✅     |
| Opportunities      | SQLite           | Canonical persistence            | ✅     |
| Interactions       | SQLite           | Canonical persistence            | ✅     |
| Interaction Coordination | Redis      | Atomic state transitions         | ✅     |
| Audit Events       | SQLite           | Append-only log                  | ✅     |
| Vector Index       | InMemoryVectorIndex | Testing/development           | ✅     |
| Embedding Provider | DeterministicStubEmbeddingProvider | Testing       | ✅     |
| Hybrid Retrieval   | Lexical + Embedding | Recall expansion             | ✅     |
| In-Memory          | Available        | Testing, development             | ✅     |

**Next Steps:**
- v0.2 Slice 5: MCP server
## v0.2 Slice 5: MCP Protocol Server (Thin Transport Layer)

### Overview

The MCP server is a **thin transport layer** that exposes the career-coordination-mcp pipeline as Model Context Protocol (MCP) tools. It implements JSON-RPC 2.0 over stdio and delegates all business logic to the `app_service` layer.

**Status:** ✅ Implemented in v0.2 Slice 5

### Architecture Decision

**Three-Layer Architecture:**

```
┌─────────────┐
│ MCP Server  │ ← Transport layer (JSON-RPC, stdio)
└──────┬──────┘
       │
┌──────▼──────┐
│ app_service │ ← Business logic (match, validate, audit, interaction)
└──────┬──────┘
       │
┌──────▼──────┐
│  Services   │ ← Data access (repos, audit, vector, coordinator)
└─────────────┘
```

**Why app_service Layer?**

Before v0.2 Slice 5, the CLI contained inline business logic. The MCP server introduction motivated extracting this logic into a reusable `app_service` layer to avoid duplication.

**Benefits:**
1. **No duplication**: CLI and MCP server share the same code paths
2. **Testability**: Business logic tested independently of transport
3. **Maintainability**: Protocol changes don't require rewriting business logic
4. **Determinism**: Same inputs → same outputs, regardless of transport

### app_service Layer

#### Responsibilities

- Run matching + validation pipeline
- Run interaction state transitions
- Emit audit events consistently
- Manage trace_id propagation

#### Public API

```cpp
// Match pipeline
MatchPipelineResponse run_match_pipeline(
    const MatchPipelineRequest& req,
    Services& services,
    IIdGenerator& id_gen,
    IClock& clock);

// Validation only
ValidationReport run_validation_pipeline(
    const MatchReport& report,
    Services& services,
    IIdGenerator& id_gen,
    IClock& clock,
    const std::string& trace_id);

// Interaction transition
InteractionTransitionResponse run_interaction_transition(
    const InteractionTransitionRequest& req,
    IInteractionCoordinator& coordinator,
    Services& services,
    IIdGenerator& id_gen,
    IClock& clock);

// Audit trace
std::vector<AuditEvent> fetch_audit_trace(
    const std::string& trace_id,
    Services& services);
```

#### Request/Response Types

**MatchPipelineRequest:**
- `opportunity` or `opportunity_id` (required)
- `atoms` or `atom_ids` or neither (defaults to all verified)
- `strategy` (default: kDeterministicLexicalV01)
- `k_lex`, `k_emb` (hybrid configuration)
- `trace_id` (optional, auto-generated if not provided)

**MatchPipelineResponse:**
- `trace_id`
- `match_report`
- `validation_report`

**InteractionTransitionRequest:**
- `interaction_id`
- `event` (kPrepare, kSend, kReceiveReply, kClose)
- `idempotency_key`
- `trace_id` (optional)

**InteractionTransitionResponse:**
- `trace_id`
- `result` (outcome, before_state, after_state, transition_index)

### MCP Server

#### Protocol

**JSON-RPC 2.0 over stdio**

- Read requests from stdin (one JSON object per line)
- Write responses to stdout (one JSON object per line)
- Write diagnostic messages to stderr

#### Supported Methods

1. **initialize**: Handshake, capability exchange
2. **tools/list**: Return available tools
3. **tools/call**: Invoke a tool

#### Supported Tools

1. **match_opportunity**: Run matching + validation pipeline
2. **validate_match_report**: Validate match report (not yet implemented)
3. **get_audit_trace**: Fetch audit events by trace_id
4. **interaction_apply_event**: Apply interaction state transition

See `docs/MCP_SERVER.md` for detailed tool schemas and examples.

### Safety and Constraints

**What the MCP Server Does NOT Do:**

- ❌ No file system access (read/write arbitrary files)
- ❌ No shell command execution
- ❌ No network access (except stdin/stdout)
- ❌ No database mutations beyond what tools explicitly expose

**Least-Privilege Design:**

The server operates on **in-memory services** by default. Future work:
- Add `--db` flag for SQLite persistence
- Add `--redis` flag for Redis coordination

### Testing Strategy

#### Unit Tests (app_service layer)

**Files:**
- `test_app_match_pipeline.cpp` (5 test cases)
- `test_app_interaction_pipeline.cpp` (5 test cases)

**Coverage:**
- Match pipeline determinism
- Validation integration
- Interaction idempotency
- Audit trace correctness
- Error handling (missing opportunity, missing atoms, invalid transitions)

**All tests pass without running MCP server.**

#### Integration Tests (MCP protocol)

**Status:** Not yet implemented (opt-in with `CCMCP_TEST_MCP=1` planned for future).

Manual testing via stdio:
```bash
echo '{"jsonrpc":"2.0","id":"1","method":"tools/list","params":{}}' | ./build/apps/mcp_server/mcp_server
```

### CLI Refactoring

**Before v0.2 Slice 5:** CLI contained inline matching logic.

**After v0.2 Slice 5:** CLI calls `app_service::run_match_pipeline()`.

**Result:**
- CLI output remains byte-identical to v0.1
- Business logic is now shared with MCP server
- No code duplication

### File Layout

```
include/ccmcp/app/
└── app_service.h            # Request/response types + public API

src/app/
└── app_service.cpp          # Business logic implementations

apps/mcp_server/
├── main.cpp                 # Server loop + tool handlers
├── mcp_protocol.h           # JSON-RPC helpers (parse, make_response, make_error)
└── mcp_protocol.cpp

tests/
├── test_app_match_pipeline.cpp
└── test_app_interaction_pipeline.cpp

docs/
└── MCP_SERVER.md            # Comprehensive MCP server documentation
```

### Future Enhancements (Post-v0.2)

**v0.3: Configuration**
- Add `--db`, `--redis`, `--vector-backend` CLI flags to MCP server
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

### Production-Ready Refactor

After initial implementation, the MCP server was refactored to production standards with:
- **Configuration system** (CLI flags for backend selection)
- **Registry pattern** (extensible method/tool dispatch)
- **ServerContext** (clean dependency bundling)
- **Backend composition** (SQLite + Redis support)

**Result:** ✅ C++ Core Guidelines compliant, production-ready server

**See:** `docs/V0_2_SLICE_5_REFACTOR.md` for detailed refactor documentation

---

## Summary: v0.2 Complete Architecture

| Component              | Implementation   | Purpose                          | Status |
|------------------------|------------------|----------------------------------|--------|
| Atoms                  | SQLite           | Canonical persistence            | ✅     |
| Opportunities          | SQLite           | Canonical persistence            | ✅     |
| Interactions           | SQLite           | Canonical persistence            | ✅     |
| Interaction Coordination | Redis          | Atomic state transitions         | ✅     |
| Audit Events           | SQLite           | Append-only log                  | ✅     |
| Vector Index           | InMemoryVectorIndex | Testing/development           | ✅     |
| Embedding Provider     | DeterministicStubEmbeddingProvider | Testing | ✅     |
| Hybrid Retrieval       | Lexical + Embedding | Recall expansion             | ✅     |
| In-Memory              | Available        | Testing, development             | ✅     |
| **app_service Layer**  | **Shared Logic** | **Business logic for CLI + MCP** | **✅** |
| **MCP Server**         | **JSON-RPC/stdio** | **Tool-based API**             | **✅** |

**Next Steps:**
- v0.3: Production configuration (persistent backends via CLI flags)
- v0.4: Advanced MCP tools (streaming, bulk operations)
- v0.5: Authentication and authorization
