# Status

Design spec (v0.1–v0.2).
Applies to both:
- Context-Forge (files → chunks → semantic index)
- career-coordination-mcp (atoms/opportunities → index → retrieval)

This document defines how canonical sources are indexed into derived stores (SQLite FTS + optional LanceDB) with deterministic rebuild rules.

---

## 1. Purpose

Provide a local-first, rebuildable indexing pipeline that:
- treats filesystem artifacts and ExperienceAtoms as canonical sources of truth,
- generates derived indices (FTS, metadata tables, vector embeddings),
- supports incremental updates via file watchers,
- is deterministic and auditable,
- and avoids “knowledge gravity” where derived stores become canonical.

---

## 2. Canonical vs Derived

### 2.1 Canonical Sources

Canonical truth lives in:
- markdown files (book/essays/specs)
- ExperienceAtoms (stored as canonical records or files)
- explicitly versioned configs/rules (constitutions)

### 2.2 Derived Stores (Rebuildable)

Derived stores may be deleted and rebuilt at any time:
- SQLite metadata tables
- SQLite FTS indices
- LanceDB vector tables
- semantic graphs, summaries, caches

Invariant: derived stores are never the only record of meaning.

---

## 3. Indexing Targets

### 3.1 Text Artifacts (Files)

Index:
- file metadata (path, timestamps)
- heading structure
- chunks (text spans)
- FTS for chunk search
- optional embeddings for chunks

### 3.2 ExperienceAtoms

Index:
- atom metadata (domain, tags, verified flag)
- atom text (title, claim)
- FTS for atoms
- optional embeddings for atoms

### 3.3 Opportunities (Career Coordination)

Index:
- structured requirements
- extracted tags/skills
- canonical posting metadata (source + timestamp)
- optional query embeddings (cache)

---

## 4. Directory and File Rules (Context-Forge)

### 4.1 Included Files

Default include:
- **/*.md
- **/*.txt
- (optional) **/*.rst

### 4.2 Excluded Paths

Always exclude:
- .git/**
- .context-forge/** (derived artifacts, staging, caches)
- node_modules/**, bin/**, obj/**, build/**
- .DS_Store, temp files, editor swap files

### 4.3 File Size Guardrails

- If a text file exceeds a size threshold (configurable), index headings + partial chunks only.
- Always log that truncation occurred.

---

## 5. Identity and Stability Strategy

### 5.1 Artifact Identity

Artifacts are identified by:
- artifact_id = stable UUID derived from (repo_id, relative_path) or stored once and persisted.

Prefer deterministic IDs if you want portability across machines:
- artifact_id = UUIDv5(namespace, relative_path)

### 5.2 Chunk Identity (Stability)

Chunks must be stable across minor edits.

Use:
- chunk_id = UUIDv5(namespace, artifact_id + heading_path + ordinal_index)

Where ordinal_index is the chunk’s index within that heading section.

If chunk boundaries change significantly (e.g., heading changes), chunk IDs may shift; this is acceptable as long as:
- old chunks are removed
- new chunks are created deterministically
- changes are logged

### 5.3 Content Hash

Each chunk and atom stores:
- content_hash = SHA256(normalized_text)

Normalization includes:
- normalized line endings
- trimmed trailing whitespace
- canonical whitespace collapsing (optional; decide once)

---

## 6. File Watcher Behavior

### 6.1 Debounce / Batch Windows

To avoid thrash during editing:
- Debounce per file: 250–750 ms
- Batch window: 1–3 seconds
- Coalesce multiple events into one index pass

### 6.2 Event Types

Handle:
- create
- modify
- delete
- rename/move

Rename handling:
- treat as delete(old_path) + create(new_path) unless you have a robust rename detector

### 6.3 Index Queue

Watcher emits events → queue → indexer consumes in order.

Indexer must be single-threaded per workspace (for determinism), but may parallelize:
- embedding generation batches
- FTS rebuild operations (carefully)

---

## 7. Indexing Pipeline (Incremental)

Phase 0 — Discovery
- Walk workspace roots
- Identify canonical artifacts
- Record current snapshot: (path, mtime, size, hash) (hash optional at discovery)

Phase 1 — Parse Structure (Files)

For each changed file:
- parse headings (markdown AST or lightweight heading parser)
- compute chunk spans (see chunking rules from EMBEDDINGS.md)
- store chunk metadata + chunk text into SQLite

Phase 2 — Update FTS
- Upsert chunk text rows into FTS virtual tables
- Remove rows for deleted chunks/files

Phase 3 — Embedding Update (Optional)

If embeddings enabled:
- For each chunk/atom where content_hash changed:
  - request embedding via provider
  - upsert vector row into LanceDB with same content_hash

Phase 4 — Cleanup
- Remove orphaned chunks
- Remove orphaned embeddings (rows that no longer exist in SQLite)
- Emit index summary stats

---

## 8. Full Rebuild Procedure

A full rebuild occurs when:
- schema version changes
- embedding dimension/model changes
- the index is corrupted
- user explicitly triggers rebuild

Procedure:
1. Drop derived tables / clear LanceDB
2. Re-scan canonical sources
3. Recreate schema
4. Re-index all files/atoms
5. Rebuild FTS
6. Re-embed everything (if enabled)

Full rebuild must be:
- deterministic
- logged with trace_id
- produce summary counts

---

## 9. Schema Versioning

Maintain:
- INDEX_SCHEMA_VERSION (integer or semver)

Store in SQLite:
- meta table: key, value

On startup:
- if schema mismatch:
  - either migrate (if supported)
  - or require full rebuild

Recommendation: early phase → prefer full rebuild over complex migrations.

---

## 10. Performance Targets (Practical)

v0.1 (FTS-only)
- Index 1,000 markdown files in < 10 seconds on a modern machine
- Incremental update for a single edited file < 250 ms (excluding debounce)

v0.2 (Embeddings enabled)
- Embedding updates batched:
  - batch size 16–64 chunks per request (provider dependent)
- UI remains responsive:
  - embeddings run in background worker thread
  - results are applied atomically

---

## 11. Atomicity and Consistency

Index updates should be applied atomically:
- Use SQLite transactions per batch update.
- Embedding updates should be committed only after successful vector upsert.

Consistency rule:
- SQLite metadata is source-of-truth for derived index integrity.
- LanceDB rows must correspond to existing SQLite rows with matching content_hash.

---

## 12. Error Handling and Recovery

Indexer failures must not corrupt canonical sources.

On error:
- log a structured error event
- mark index state as “stale”
- allow user-triggered rebuild
- continue serving last-known-good index if possible

Track error types:
- parse errors (bad markdown)
- FTS update errors
- embedding provider errors
- LanceDB upsert errors

---

## 13. Observability / Logging

For each indexing run emit:
- trace_id
- start/end time
- number of files scanned
- number of files changed
- chunks added/updated/removed
- FTS rows updated
- embeddings queued/completed/failed
- schema version
- embedding provider + dimension (if enabled)

Use structured logs (JSON) so you can feed into CVE/audit tooling later.

---

## 14. Integration Notes (Career Coordination MCP)

Atoms and opportunities can be treated as “artifacts” too.

Indexing for career coordination:
- Atom changesDV: whenever atom changes → update SQLite FTS + embedding row
- Opportunity ingestion: create structured Opportunity record + derived query text for retrieval
- Matching uses EMBEDDINGS.md pipeline with Job Matching preset weights

---

## 15. Non-Goals

- Real-time indexing at keystroke speed
- Distributed indexing
- Cross-machine index sync
- Treating derived stores as canonical truth
