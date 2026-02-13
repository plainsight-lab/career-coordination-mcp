# Status

Design spec (v0.2 target).
Applies to both:
- Career Coordination MCP (opportunity → experience matching)
- Context-Forge (long-lived corpus semantic retrieval)

Embeddings are a derived semantic layer. Canonical truth remains in files / atoms. The vector index is rebuildable at any time.

---

## 1. Purpose

Provide a unified semantic matching strategy that supports:
1. Opportunity → Experience matching
2. Query → long-lived writing corpus retrieval
3. Reuse across both via shared primitives:

- Experience Atoms
- Document Chunks

Embeddings are one signal in a hybrid retrieval pipeline; they never serve as sole authority.

---

## 2. Core Principles

- Hybrid retrieval: lexical candidates (deterministic) + semantic candidates (vector) → merged set.
- Rebuildable: embeddings and vector DB may be deleted and regenerated from canonical sources.
- Structure-aware chunking: chunking follows document headings and paragraph boundaries.
- Explainability: each result returns evidence pointers + component scores.
- No hidden network calls in scoring: embedding generation may call a provider; scoring is deterministic.

---

## 3. Units of Embedding

Only two unit types are embedded.

### 3.1 Experience Atoms

Small, high-signal capability facts. Used for job matching and grounded resume composition.

Each atom embedding input is a normalized text payload:
- title
- claim
- optional: condensed tags

Example embedding text (normalized):

```text
[ATOM]
Title: Enterprise Architecture — Governance & invariants
Claim: Designed and led governance-first architecture for adversarial systems; explicit authority boundaries; deterministic validation pipelines.
Tags: architecture, governance, security, ai
```

### 3.2 Document Chunks

Structure-aware chunks of markdown documents used for corpus retrieval.

Each chunk embedding input is the chunk text + minimal context:
- heading path
- file identifier (not full path if privacy desired)

Example embedding text:

```text
[DOC_CHUNK]
Path: writing/book/ch03.md
Heading: Systems Thesis > AI reshapes adversarial landscape
Content: <chunk text...>
```

## 4. Chunking Algorithm

### 4.1 Chunking Rules

Chunking is structure-first:
1. Split by markdown headings (H1/H2/H3…)
2. Within each heading section:
- group paragraphs into chunks targeting ~300–800 “token-ish words”
- prefer paragraph boundaries
- overlap by 1 paragraph only if section is long and coherence suffers

### 4.2 Chunk Metadata

Each chunk stores:
- chunk_id (UUID)
- artifact_id (file id)
- relative_path
- heading_path (e.g., H1 > H2 > H3)
- start_line, end_line
- content_hash (hash of normalized chunk text)
- created_at, updated_at

### 4.3 Re-embed Trigger

A chunk is re-embedded if content_hash changes.

---

## 5. Storage Strategy

Use two stores:
1. SQLite (canonical metadata + lexical search)
2. LanceDB (vectors + metadata pointers)

The semantic layer remains rebuildable from:
- filesystem documents
- atom store

### 5.1 SQLite Schema (conceptual)

artifacts
- artifact_id TEXT PRIMARY KEY
- type TEXT  (file, atom, opportunity, etc.)
- relative_path TEXT NULL
- title TEXT NULL
- created_at TEXT
- updated_at TEXT

chunks
- chunk_id TEXT PRIMARY KEY
- artifact_id TEXT
- relative_path TEXT
- heading_path TEXT
- start_line INTEGER
- end_line INTEGER
- content_hash TEXT
- chunk_text TEXT
- updated_at TEXT

atoms
- atom_id TEXT PRIMARY KEY
- domain TEXT
- title TEXT
- claim TEXT
- tags_json TEXT
- content_hash TEXT
- verified INTEGER
- updated_at TEXT

fts_chunks (SQLite FTS5 virtual table)
- index over chunk_text, heading_path, relative_path

fts_atoms (SQLite FTS5 virtual table)
- index over title, claim, domain, tags

### 5.2 LanceDB Tables

emb_atoms
- vector: embedding
- metadata:
  - atom_id
  - domain
  - tags
  - content_hash
  - updated_at

emb_chunks
- vector: embedding
- metadata:
  - chunk_id
  - artifact_id
  - relative_path
  - heading_path
  - start_line, end_line
  - content_hash
  - updated_at

---

## 6. Embedding Provider Interface

Embeddings must be provider-swappable.

### 6.1 Interface

IEmbeddingProvider
- name() -> string
- dimension() -> int
- embed(texts: list<string>) -> list<vector<float>>

### 6.2 Provider Policy

- Embedding generation may be local or remote (API).
- All requests must be logged with:
  - provider name
  - model identifier (if applicable)
  - batch size
  - latency
- The embedding dimension is treated as part of the index identity.

---

## 7. Retrieval Pipeline

Single pipeline, two presets.

### 7.1 Inputs

A query can be:
- job posting / requirements text
- user natural language prompt
- structured query synthesized from metadata

### 7.2 Candidate Generation

Step A — Lexical Candidate Set (Deterministic)
Use SQLite FTS:
- query atoms and chunks
- return top N_lex candidates (default 100)

Step B — Semantic Candidate Set
1. Embed query
2. Vector search:
- top N_sem_atoms atoms (default 50)
- top N_sem_chunks chunks (default 50)

Step C — Union + Dedupe
Candidate set = lexical ∪ semantic, dedupe by atom_id or chunk_id.

### 7.3 Scoring Blend

For each candidate, compute:
- S_lex = normalized FTS score (0..1)
- S_sem = cosine similarity (0..1)
- S_bonus = deterministic bonuses (0..1)

Final score:

```text
S_final = w_lex * S_lex + w_sem * S_sem + w_bonus * S_bonus
```

### 7.4 Bonuses (Explainable)

Bonuses are deterministic and derived from metadata:
- domain_match_bonus: if candidate domain matches query domain classification
- tag_overlap_bonus: overlap between query-extracted tags and candidate tags
- heading_path_bonus: if query references a known heading path / area
- pinned_bonus: user-pinned canonical atoms/chunks

### 7.5 Output Requirements

Each result must return:
- id (atom_id or chunk_id)
- type (atom or chunk)
- evidence pointer:
  - atoms: atom_id
  - chunks: relative_path + start_line/end_line + heading_path
- score breakdown:
  - S_lex, S_sem, S_bonus, S_final
- retrieval provenance:
  - whether it came from lexical, semantic, or both

---

## 8. Presets

Same pipeline; only weights differ.

### 8.1 Job Matching Preset

- w_lex = 0.55
- w_sem = 0.35
- w_bonus = 0.10

Rationale:
- Keywords and explicit requirements matter.
- Semantic helps with paraphrase matching.

### 8.2 Writing / Corpus Preset

- w_lex = 0.35
- w_sem = 0.55
- w_bonus = 0.10

Rationale:
- Concept adjacency and paraphrase retrieval are primary.

Weights are configuration-driven.

---

## 9. Optional Reranking (v0.3+)

After scoring, optionally rerank top K candidates (default 25) using:
- a local cross-encoder
- or a strict “rank-only” LLM call

Constraints:
- reranker cannot introduce new candidates
- reranker output must be logged:
  - input candidate ids
  - rerank scores
  - selected ordering

Fallback:
- if reranker fails, use S_final ordering.

---

## 10. Index Build & Refresh

### 10.1 Build Phases

1. Scan canonical sources:
- atoms store
- markdown corpus
2. Chunk markdown
3. Update SQLite metadata + FTS
4. Generate embeddings for changed atoms/chunks only
5. Upsert LanceDB rows

### 10.2 Change Detection

Use content_hash comparisons:
- if unchanged, do nothing
- if changed, regenerate embedding and update row

### 10.3 Full Rebuild

A full rebuild:
- deletes LanceDB tables
- re-chunks documents
- regenerates embeddings from canonical sources

---

## 11. Governance & Validation Integration

Embeddings never authorize claims.

When embeddings are used to select content for generation (e.g., resumes, outreach, writing edits):
- selected atoms/chunks become explicit references included in the generated artifact envelope
- the Constitutional Validation Engine enforces:
  - grounding (no unreferenced factual claims)
  - provenance (citations to atom ids or chunk spans)
  - constraint compliance

If an output includes claims not supported by referenced atoms/chunks, it is rejected.

---

## 12. Logging & Audit Fields

For each query:
- trace_id
- query text hash
- retrieval preset name
- provider name + embedding dimension
- lexical results (ids + scores)
- semantic results (ids + scores)
- final merged ranking (with breakdown)

This supports reproducibility and forensic analysis.

---

## 13. Implementation Notes (C++ / vcpkg)

- Use SQLite with FTS5 enabled.
- Use LanceDB as the vector store when enabled.
- Keep embedding provider behind a clean interface; allow “disabled” mode (lexical-only).

---

## 14. Non-Goals

- “Vector-only” retrieval
- Unbounded agentic exploration loops
- Treating similarity as truth
- Storing canonical truth in vector DB
