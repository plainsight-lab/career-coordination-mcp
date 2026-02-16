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
