# Domain Model (v0.1)

This document describes the canonical schemas and invariants for domain entities in career-coordination-mcp.

**Version:** v0.1 (LOCKED)
**Last Updated:** 2026-02-14

---

## Deterministic Normalization (v0.1)

All text normalization in this system is deterministic and locale-independent to ensure byte-stable output across platforms.

**Normalization Rules (LOCKED for v0.1):**

- **ASCII Lowercasing:** Uppercase A-Z converted to lowercase a-z via explicit character arithmetic (no `std::tolower` or locale functions)
- **Tokenization:** Non-alphanumeric characters replaced with space delimiters; text split into tokens
- **Token Filtering:** Tokens shorter than 2 characters are dropped
- **Tag Normalization:** Tags are tokenized, lowercased, deduplicated, and sorted lexicographically
- **Trimming:** Leading and trailing whitespace (space, tab, newline, carriage return) removed
- **No Locale Dependence:** All operations are byte-deterministic across OS, compiler, and locale settings

**Implementation:** See `include/ccmcp/core/normalization.h`

---

## Entities

### ExperienceAtom

Immutable, verifiable capability fact representing a single skill, achievement, or experience claim.

**Fields (v0.1 Schema):**

- `atom_id` (AtomId): Non-empty unique identifier
- `domain` (string): Normalized lowercase ASCII domain category (e.g., "enterprise architecture", "security", "ai governance")
- `title` (string): Free-form role or capability title, trimmed
- `claim` (string): Non-empty capability statement, trimmed
- `tags` (vector<string>): Normalized tags (lowercase, sorted, deduplicated, min length 2)
- `verified` (bool): Attestation flag indicating whether this atom has been verified
- `evidence_refs` (vector<string>): List of evidence pointers (URLs, document references), trimmed, empty refs removed

**Invariants:**

- `atom_id` must be non-empty
- `claim` must be non-empty
- `domain` must be normalized (lowercase ASCII, trimmed)
- `tags` must be normalized (lowercase, sorted, deduplicated)
- `evidence_refs` entries must be trimmed (empty entries removed during normalization)
- Atoms are append-only (new versions create new record; old remains immutable)

**Normalization:** `normalize_atom()` produces a normalized copy enforcing all invariants.

**Validation:** `validate()` checks invariants and returns `Result<bool, string>`.

---

### Requirement

A single job requirement with optional categorization and tags.

**Fields (v0.1 Schema):**

- `text` (string): Non-empty requirement description, trimmed
- `tags` (vector<string>): Normalized tags (lowercase, sorted, deduplicated)
- `required` (bool): Whether this requirement is mandatory (true) or nice-to-have (false)

**Invariants:**

- `text` must be non-empty
- `tags` must be normalized if present (lowercase, sorted, deduplicated)

**Normalization:** `normalize_requirement()` produces a normalized copy.

**Validation:** `validate()` checks invariants and returns `Result<bool, string>`.

---

### Opportunity

Structured representation of a job posting.

**Fields (v0.1 Schema):**

- `opportunity_id` (OpportunityId): Non-empty unique identifier
- `company` (string): Non-empty company name, trimmed
- `role_title` (string): Non-empty role title, trimmed
- `requirements` (vector<Requirement>): List of requirements (order preserved from input)
- `source` (string): Optional source reference (e.g., "LinkedIn", "Company Site"), trimmed

**Invariants:**

- `opportunity_id` must be non-empty
- `company` must be non-empty
- `role_title` must be non-empty
- `requirements` list preserves input order (not auto-sorted)
- All `requirements` must pass validation

**Normalization:** `normalize_opportunity()` produces a normalized copy, normalizing all nested requirements while preserving order.

**Validation:** `validate()` checks invariants and recursively validates all requirements.

---

### Contact

A person associated with an opportunity (schema not finalized in v0.1).

**Fields (deferred to future version):**

- `contact_id`
- `name`
- `org`
- `channels` (email, LinkedIn URL, phone)
- `relationship_state` (unknown/warm/cold/recruiter/hiring_mgr)
- `identity_keys` (normalized identifiers)

**Invariants (deferred):**

- Identity resolution prevents duplicates (same LinkedIn URL == same contact)

---

### Interaction

Audit-tracked state machine node for outreach tracking.

**Fields (implemented in v0.1):**

- `interaction_id` (InteractionId)
- `contact_id` (ContactId)
- `opportunity_id` (OpportunityId)
- `state` (InteractionState): Current state in the FSM

**States:**
- `kDraft`: Initial state
- `kReady`: Prepared for sending
- `kSent`: Sent to contact
- `kResponded`: Contact replied
- `kClosed`: Interaction closed

**Events:**
- `kPrepare`: Draft → Ready
- `kSend`: Ready → Sent
- `kReceiveReply`: Sent → Responded
- `kClose`: Any → Closed

**Invariants:**

- State transitions must be valid per transition table
- `can_transition(event)` checks validity before applying
- `apply(event)` performs transition if valid, returns false otherwise

---

### Resume

A composed artifact (schema not fully implemented in v0.1).

**Fields (deferred):**

- `resume_id`
- `opportunity_id`
- `selected_atoms` (ordered list)
- `sections` (summary, experience bullets, skills)
- `rendered_output` (text)
- `validation_report`

**Invariants (deferred):**

- Rendered prose may only reference selected atoms
- Every bullet must be traceable to ≥1 atom

---

## Relationships

- Opportunity ↔ Requirements (one-to-many, embedded)
- Opportunity ↔ Contact(s) (many-to-many, future)
- Opportunity ↔ Resume variants (one-to-many, future)
- Contact ↔ Interactions (one-to-many, implemented)
- Resume ↔ selected ExperienceAtoms (many-to-many, future)

---

## Schema Versioning

**v0.1 Status:**
- ✅ ExperienceAtom: Locked and implemented
- ✅ Requirement: Locked and implemented
- ✅ Opportunity: Locked and implemented
- ✅ Interaction: Locked and implemented (state machine only)
- 🔴 Contact: Schema deferred
- 🔴 Resume: Schema deferred

Future versions may add optional fields but must not break v0.1 invariants without explicit migration path.

---

## Deterministic Guarantees

All normalization functions guarantee:

1. **Byte Stability:** Same input produces identical byte-for-byte output across runs, OS, and compilers
2. **Idempotence:** `normalize(normalize(x)) == normalize(x)`
3. **No Locale Dependence:** Output does not vary based on system locale settings
4. **Sorted Output:** Tags are always lexicographically sorted for stable serialization

These guarantees are verified by the test suite in `tests/test_domain_schema.cpp`.
