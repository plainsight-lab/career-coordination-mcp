# Embedding Index — Design and Implementation

**Implementation status:** v0.3 Slice 4 — fully implemented.

This document describes the embedding lifecycle pipeline for `career-coordination-mcp`:
how canonical sources produce deterministic, auditable embeddings stored with full provenance.

---

## 1. Canonical vs. Derived (Invariant)

Embeddings are **derived artifacts**. They are never canonical.

| Store | Role | Rebuildable? |
|-------|------|-------------|
| SQLite (`atoms`, `opportunities`, `resumes`) | Canonical source of truth | No |
| SQLite (`index_runs`, `index_entries`) | Embedding provenance log | Yes |
| Vector index (`InMemoryEmbeddingIndex`, `SqliteEmbeddingIndex`) | Derived similarity index | Yes |

**Rebuild rule:** deleting all `index_entries` rows and re-running `index-build` is always safe and produces an equivalent state.

---

## 2. Schema v4 + v6 — Provenance and Identity Tables

Schema v4 adds the embedding provenance tables; schema v6 adds the monotonic counter table.
All are applied via `ensure_schema_v6()` (chained: v1 → v4 → v5 → v6).

### `id_counters` (schema v6)

Persistent monotonic counter backing `next_index_run_id()`.

| Column | Type | Notes |
|--------|------|-------|
| `name` | TEXT PK | Counter name — `'index_run'` for run IDs |
| `value` | INTEGER | Current counter value; incremented atomically per allocation |

**Invariant:** `value` is 1-based and strictly monotonically increasing. It is never reset.
Each call to `next_index_run_id()` increments `value` by 1 and returns `"run-" + value`.

---

## 2a. Schema v4 — Provenance Tables

Schema v4 adds two tables to the main SQLite database (applied via `ensure_schema_v4()`):

### `index_runs`

Records a single index build run.

| Column | Type | Notes |
|--------|------|-------|
| `run_id` | TEXT PK | Unique run identifier |
| `started_at` | TEXT | ISO 8601 UTC; nullable (set when run begins) |
| `completed_at` | TEXT | ISO 8601 UTC; nullable (set when run completes) |
| `provider_id` | TEXT | Embedding provider identifier |
| `model_id` | TEXT | Model identifier (empty for deterministic stub) |
| `prompt_version` | TEXT | Prompt template version (empty for stub) |
| `status` | TEXT | `pending` \| `running` \| `completed` \| `failed` |
| `summary_json` | TEXT | `{"indexed":N,"skipped":N,"stale":N,"scope":"..."}` |

### `index_entries`

Records one embedding per (run, artifact).

| Column | Type | Notes |
|--------|------|-------|
| `run_id` | TEXT FK | References `index_runs.run_id` |
| `artifact_type` | TEXT | `atom` \| `resume` \| `opportunity` |
| `artifact_id` | TEXT | ID of the indexed artifact |
| `source_hash` | TEXT | `stable_hash64_hex` of canonical text |
| `vector_hash` | TEXT | `stable_hash64_hex` of raw float bytes |
| `indexed_at` | TEXT | ISO 8601 UTC; nullable |

Primary key: `(run_id, artifact_type, artifact_id)`.
Index on `(artifact_type, artifact_id)` for drift detection queries.

---

## 2b. Run ID Semantics

Run IDs are allocated from the `id_counters` SQLite table via `IIndexRunStore::next_index_run_id()`.

**Format:** `"run-N"` where N is a 1-based integer starting from 1.

**Invariant:** The counter is persistent across process boundaries. Two CLI invocations on
the same database always produce distinct run IDs (`run-1`, `run-2`, …). The counter is never
reset by process restart, preventing the WARN-001 class of bug where the prior completed run
record was overwritten before drift detection could query it.

**Atomicity:** The increment is wrapped in `BEGIN IMMEDIATE ... COMMIT` for multi-process safety
on file-based databases.

**Schema requirement:** `next_index_run_id()` requires the `id_counters` table from schema v6.
Callers must apply `ensure_schema_v6()` before using `SqliteIndexRunStore`.

---

## 3. Provenance Fields

- **`source_hash`**: hash of the canonical text used to generate the embedding.
  Changes when the artifact's content changes.
- **`vector_hash`**: hash of the raw float bytes of the embedding vector.
  Changes when either the source or the embedding model/prompt changes.
- **`provider_id` / `model_id` / `prompt_version`**: the three dimensions of a provider scope.
  A change in any of these fields triggers re-indexing even if the source text is unchanged.

---

## 4. Canonical Text Extraction

Each artifact type has a deterministic canonical text function:

| Artifact type | Canonical text |
|--------------|----------------|
| `atom` | `title + " " + claim + " " + tags.join(" ")` |
| `resume` | `resume_md` (full markdown content) |
| `opportunity` | `role_title + " " + company + " " + requirements[].text.join(" ")` |

---

## 5. Drift Detection

Before embedding an artifact, the pipeline checks for a prior `source_hash` via
`IIndexRunStore::get_last_source_hash()`. The query scopes to:
- the artifact's `(artifact_id, artifact_type)`
- the run's `(provider_id, model_id, prompt_version)` combination
- only runs with `status = 'completed'`
- ordered by `completed_at DESC`, limit 1

**Result:**
- Hash **matches**: artifact is skipped (`skipped_count++`).
- Hash **differs**: artifact is re-indexed (`stale_count++`, `indexed_count++`).
- **No prior run**: artifact is indexed as new (`indexed_count++`).

---

## 6. NullEmbeddingProvider Behaviour

`NullEmbeddingProvider::embed_text()` returns an empty vector (dimension = 0).

The pipeline checks `embedding.empty()` before upsert. If empty:
- no vector is written to the index
- no `IndexEntry` is written to `index_entries`
- `indexed_count` is **not** incremented
- `skipped_count` is **not** incremented

This is not an error — it is an explicit opt-out of indexing. The run still completes
with `status = completed`.

---

## 7. Rebuild Strategy

A full rebuild consists of:

1. Delete (or truncate) `index_entries` rows, or simply re-run `index-build` — the
   pipeline does full upsert (INSERT OR REPLACE), so duplicates overwrite.
2. Run `ccmcp_cli index-build --scope all`.

No schema migration is needed for content changes. Schema changes require `ensure_schema_v4()`.

---

## 8. Vector Index Key Naming

| Artifact type | Vector key |
|--------------|-----------|
| `atom` | `{artifact_id}` |
| `resume` | `resume:{artifact_id}` |
| `opportunity` | `opp:{artifact_id}` |

---

## 9. Audit Events

Every index build run emits three event types into `IAuditLog`, using the `run_id` as `trace_id`:

| Event type | Payload fields |
|-----------|---------------|
| `IndexRunStarted` | `run_id`, `scope`, `provider_id` |
| `IndexedArtifact` | `artifact_type`, `artifact_id`, `source_hash`, `stale` |
| `IndexRunCompleted` | `run_id`, `indexed`, `skipped`, `stale` |

---

## 10. CLI Usage

```bash
# Build/rebuild with in-memory vector index (useful for testing)
ccmcp_cli index-build --db career.db --vector-backend inmemory --scope all

# Build with SQLite vector backend (persisted)
ccmcp_cli index-build --db career.db --vector-backend sqlite --vector-db-path ./vectors --scope atoms

# Scope options: atoms | resumes | opportunities | all
ccmcp_cli index-build --db career.db --scope resumes
```

Default values if flags are omitted:
- `--db`: `data/ccmcp.db`
- `--vector-backend`: `inmemory`
- `--scope`: `all`

---

## 11. Interfaces

| Interface | Location | Purpose |
|-----------|----------|---------|
| `IIndexRunStore` | `include/ccmcp/indexing/index_run_store.h` | Persist/retrieve runs and entries |
| `SqliteIndexRunStore` | `include/ccmcp/storage/sqlite/sqlite_index_run_store.h` | SQLite implementation |
| `run_index_build()` | `include/ccmcp/indexing/index_build_pipeline.h` | Pipeline entry point |

---

## 12. Design Constraints

- **Matcher scoring formula**: unchanged.
- **`IEmbeddingProvider` interface**: unchanged.
- **`IEmbeddingIndex` interface**: unchanged.
- **`Services` struct**: unchanged.
- **Schemas v1–v5**: unchanged. Schema v6 is additive (adds `id_counters` table only).
- **All existing CLI commands**: unchanged behavior; `index-build` now applies `ensure_schema_v6()`.
- **`run_index_build()` signature**: unchanged — `id_gen` is still required for audit event IDs.
- **`get_last_source_hash()` semantics**: unchanged — queries completed runs ordered by `completed_at DESC`.
