# Constitutional Validation Engine Design

## 1. Purpose

The Constitutional Validation Engine (CVE) is a deterministic rule evaluation subsystem that:
- validates candidate artifacts against an explicit constitution of constraints,
- produces an inspectable validation report,
- emits audit events suitable for forensic reconstruction,
- and supports plugin-defined rule sets without allowing rule ambiguity.

It exists to prevent authority drift: no artifact becomes “accepted” unless it passes constitutional validation (or is explicitly overridden with logged human authority).

---

## 2. Core Concepts

### 2.1 Artifact

An artifact is any generated or proposed output that must be validated before acceptance.

Examples:
- Resume draft
- Outreach message draft
- Opportunity match recommendation
- Context-Forge patch/hunk
- Simulation result summary
- Governance policy draft

Artifacts are immutable inputs to validation.

### 2.2 Constitution

A constitution is a versioned collection of rules and thresholds defining:
- what is allowed,
- what is prohibited,
- what requires human override,
- and what must be logged.

A constitution is explicit, versioned, and referenced by every validation report.

### 2.3 Rule

A rule is a deterministic function that:
- evaluates an artifact + context,
- emits findings (pass/warn/fail),
- may include structured evidence pointers,
- and never mutates state.

### 2.4 Findings

A finding is the atomic result of evaluation.

Severity levels:
- PASS (informational; optional)
- WARN (acceptable but surfaced)
- FAIL (reject unless override)
- BLOCK (hard stop; override prohibited unless constitution explicitly allows)

### 2.5 Validation Report

The CVE returns a report that is:
- machine readable,
- stable across runs for the same inputs,
- and fully attributable.

---

## 3. Inputs and Outputs

### 3.1 Inputs

ArtifactEnvelope
- artifact_id (UUID)
- artifact_type (enum/string)
- content (string or structured JSON)
- format (markdown/json/plain/etc.)
- created_at
- producer (e.g., deterministic, llm:<model>, human)
- source_refs (list of references—atoms, docs, ids)
- proposed_actions (optional; e.g., “write file”, “send outreach”)

ValidationContext
- constitution_id
- constitution_version
- domain_context (Opportunity, Contact, etc.)
- ground_truth_refs (atoms, canonical docs, schema definitions)
- policy_overrides_allowed (derived from constitution)
- runtime_facts (e.g., cooldown windows, current time)
- trace_id

### 3.2 Output

ValidationReport
- report_id
- trace_id
- artifact_id
- constitution_id, constitution_version
- summary:
  - status: ACCEPTED | REJECTED | NEEDS_REVIEW | BLOCKED
  - counts by severity
- findings[] (ordered deterministically)
- evidence_index[] (normalized references)
- rule_execution[] (timings, rule ids, versions)
- override_policy:
  - is_override_possible
  - required_authority (e.g., human_admin)
  - override_reason_required (bool)

---

## 4. Determinism Requirements

A CVE run must be deterministic given identical:
- artifact content
- constitution version
- ground truth references
- runtime facts

To enforce determinism:
- rules must be pure functions
- no network calls inside rules
- no time-dependent logic unless time is provided in runtime_facts
- findings must be sorted by (severity, rule_id, stable_key)

---

## 5. Rule Model

### 5.1 Rule Metadata

Each rule has:
- rule_id (stable string)
- version (semver)
- description
- applies_to (artifact types)
- severity_on_violation (WARN/FAIL/BLOCK)
- override_allowed (bool; constitution-level can further restrict)
- tags (e.g., grounding, outreach, privacy)

### 5.2 Rule Interface (conceptual)

A rule takes:
- ArtifactEnvelope
- ValidationContext

And returns:
- RuleResult { findings[], evidence_refs[] }

---

## 6. Engine Phases (Pipeline)

Phase 0 — Normalize
- normalize artifact content (e.g., line endings)
- compute artifact_hash
- normalize references into a canonical evidence index

Phase 1 — Schema Validation (cheap)
- validate artifact conforms to expected structure for its type
- reject immediately on malformed data (BLOCK)

Phase 2 — Grounding Validation (core)
- verify artifact claims are grounded in allowed sources
- enforce “no new facts” for LLM-produced text unless explicitly allowed

Phase 3 — Domain Constraints
- apply domain rules (resume composition rules, outreach dedupe, etc.)

Phase 4 — Safety / Governance Constraints
- enforce “no autonomous execution”, “no duplicate outreach”, “no PII leakage”, etc.

Phase 5 — Summarize + Decision

Status selection:
- Any BLOCK → BLOCKED
- Any FAIL → REJECTED (or NEEDS_REVIEW if constitution permits soft-fail review)
- Any WARN only → NEEDS_REVIEW or ACCEPTED depending on constitution
- None → ACCEPTED

---

## 7. Override Model (Human Authority)

Overrides are a governed action, not a loophole.

If override is permitted:
- requires explicit human authority role
- requires override_reason
- creates an audit event linking report → override decision
- produces an “Override Validation Report” with:
  - who overrode
  - why
  - what findings were overridden
  - what residual risks remain

Overrides are forbidden when:
- constitution marks rule as non-overridable (BLOCK)
- or artifact proposes irreversible action (unless explicitly allowed)

---

## 8. Audit Logging (Forensic Reconstruction)

CVE must emit append-only events:
- ValidationStarted
- RuleEvaluated (optional granular)
- ValidationCompleted
- OverrideApplied (if any)

Each event includes:
- trace_id
- artifact_id
- constitution_version
- hashes of inputs
- deterministic ordering index

This allows reconstructing:
- what was evaluated
- under what constitution
- with what outcome
- and what authority intervened

---

## 9. Plugin-Friendly Architecture

Rules are loaded from rule packs.

A rule pack is:
- a compiled module (v0.1) or dynamically loadable plugin
- declares a set of rules + metadata
- versioned and signed (later)

Engine selects packs by constitution.

v0.1 Recommendation
- compile-in rule packs (simpler)
- treat them as “plugins” by interface, not by dynamic loader


---

## 10. Example Rule Packs (Career Coordination)

Pack: GroundingRules
- GND-001: No ungrounded claims in resume bullets (FAIL)
- GND-002: No new employers/roles not in atoms (BLOCK)
- GND-003: All bullets must reference ≥1 atom id (FAIL)

Pack: OutreachRules
- OUT-001: No duplicate first-touch within cooldown (FAIL)
- OUT-002: No outreach if DNC flag set (BLOCK)

Pack: CompositionRules
- CMP-001: Resume sections must follow ordering policy (WARN)
- CMP-002: Selected atoms must cover top-N requirements (WARN/FAIL)

---

## 11. Minimal C++ Types (Implementation Guidance)

How it should feel in C++20:
- ArtifactEnvelope as POD-ish struct + JSON serialization
- ValidationContext contains resolved facts (no hidden lookups)
- IRule interface returning RuleResult
- ValidationEngine::validate(envelope, context) -> ValidationReport

Coroutines are optional here; rules should be cheap. The MCP layer can be async; validation itself should be deterministic and fast.

---

## 12. Why This Solves “Deterministic Inference Enhanced” Goal

- deterministic steps decide what is allowed and what is selected
- LLM drafts how it is expressed
- CVE enforces:
  - grounding
  - constraints
  - state machine legitimacy
  - auditability

**Get the benefits of model-assisted generation without ever delegating authority.**
