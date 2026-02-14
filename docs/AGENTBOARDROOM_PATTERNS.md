# AgentBoardroom Governance Pattern Decomposition

**Document Version:** 1.0
**Analysis Date:** 2026-02-14
**Analyst Role:** Senior Systems Architect

---

## Executive Summary

This document extracts reusable governance primitives from the AgentBoardroom multi-agent governance system and maps them onto career-coordination-mcp's deterministic inference architecture. The analysis identifies structural patterns that strengthen constitutional enforcement, decision auditability, and validation rigor without introducing agent orchestration or runtime complexity.

**Key Finding:** AgentBoardroom implements six governance primitives that align with career-coordination-mcp's constitutional posture. Four of these can be adapted as minimal, incremental extensions to strengthen v0.2 governance.

---

## 1. Extracted Governance Primitives

### 1.1 DecisionRecord Schema and Immutability Model

**Definition:**
A DecisionRecord is a first-class, immutable artifact representing a significant decision point in system operation. Each record captures the decision's author, rationale, evidence, challenge history, and resolution status.

**Enforcement Mechanism:**
- Append-only storage (TypeScript: `decisions.json`, immutable array)
- Records are never modified; new decisions may supersede old ones
- Each record includes a `supersedes` field forming a decision lineage chain
- Status progression enforced through state transitions: `proposed` → `challenged` → `accepted`/`escalated`/`superseded`

**Structural Invariants:**
1. **Immutability:** Once created, record fields are frozen (except status progression)
2. **Lineage Integrity:** Supersession chains must be acyclic and traceable
3. **Evidence Binding:** Evidence references must be explicit and verifiable
4. **Author Attribution:** Every decision must have a declared author role
5. **Queryability:** Decision graph supports reverse lookup (who supersedes what) and dependency traversal

**Risk Mitigated:**
Prevents post-hoc rationalization, ensures decision provenance, creates forensic reconstruction capability, blocks silent policy drift.

**Relevant Code Reference:**
`AgentBoardroom/src/decisions/store.ts:109-143` (propose), `196-220` (supersede)

---

### 1.2 Supersession Chains and Decision Lineage

**Definition:**
A supersession chain is a directed acyclic graph where each node is a DecisionRecord and edges represent "replaces" relationships. Chains enable tracking how decisions evolve over time while preserving historical context.

**Enforcement Mechanism:**
- Forward index: Maps decision ID → IDs of decisions that supersede it
- Backward traversal: `chain(decisionId)` retrieves full ancestry
- Forward traversal: `forwardChain(decisionId)` retrieves descendants
- Dependency graph: Tracks which decisions depend on which others

**Structural Invariants:**
1. **Acyclicity:** No decision may supersede itself (directly or transitively)
2. **Monotonic Status:** Superseded decisions transition to `superseded` status permanently
3. **Lineage Completeness:** All ancestors must be resolvable in the store
4. **Dependency Consistency:** Cannot supersede a decision that other active decisions depend on without updating dependents

**Risk Mitigated:**
Detects circular reasoning, prevents orphaned decisions, ensures policy coherence over time, supports rollback analysis.

**Relevant Code Reference:**
`AgentBoardroom/src/decisions/store.ts:257-318` (chain, forwardChain, dependencyGraph)

---

### 1.3 Challenge Protocol Mechanics

**Definition:**
A challenge protocol is a structured adversarial review mechanism where designated reviewers must explicitly accept or challenge a proposed decision before it can execute. The protocol enforces round limits and auto-escalation.

**Enforcement Mechanism:**
- **Authorization Check:** Only designated challengers (defined in board config) may challenge
- **Round Limit Enforcement:** Maximum 3 challenge rounds; exceeding triggers auto-escalation
- **Execution Gate:** Decision status must be `accepted` or `escalated` before execution proceeds
- **Challenge History:** Each round is recorded with timestamp, rationale, and optional counter-proposal
- **Structural Blocking:** `canExecute(decision)` returns false if challenge incomplete

**Structural Invariants:**
1. **Round Monotonicity:** Challenge rounds only increment, never reset or decrement
2. **Status Consistency:** Challenged decisions cannot simultaneously be accepted
3. **Challenge Authorization:** Challenger must be in the authorized list for that decision's author role
4. **Counter-Proposal Tracking:** Counter-proposals have lifecycle states (pending/accepted/rejected/withdrawn/superseded)
5. **Escalation Trigger:** Auto-escalation is mandatory at round limit if configured

**Risk Mitigated:**
Prevents unilateral decision authority, forces consideration of alternatives, creates adversarial tension, documents disagreement, escalates deadlock.

**Relevant Code Reference:**
`AgentBoardroom/src/challenges/protocol.ts:204-304` (processChallenge)

---

### 1.4 Gate Enforcement Semantics

**Definition:**
Gate enforcement is a structural blocking mechanism where phase transitions require explicit approval from designated validators. A FAIL verdict physically blocks advancement; a PASS permits it; a CONDITIONAL permits it with tracked warnings.

**Enforcement Mechanism:**
- **Verdict Types:** PASS, FAIL, CONDITIONAL
- **Structural Blocking:** `advancePhase()` checks verdicts before allowing transition; FAIL returns `{advanced: false, blockers: [...]}`
- **Phase State Machine:** Phases are numbered and transitions are validated against allowed sequences
- **Gate History:** All verdicts are stored in both project-local state and global history for cross-project queries
- **Conditional Expiry:** CONDITIONAL verdicts may have expiration timestamps; expired conditionals block advancement

**Structural Invariants:**
1. **Phase Sequence Integrity:** Cannot skip phases (e.g., cannot go from phase 0 to phase 2)
2. **Gate Match:** Transition name must match the exit gate defined for the current phase
3. **Verdict Completeness:** All required roles must issue verdicts before advancement
4. **FAIL is Blocking:** FAIL verdicts in structural gates prevent phase advancement at code level
5. **Status Reflection:** Phase status reflects gate state (`gated_fail`, `gated_conditional`, `planning`, etc.)

**Risk Mitigated:**
Prevents premature phase transitions, enforces validation requirements, creates audit trail of gate decisions, blocks unsafe progression.

**Relevant Code Reference:**
`AgentBoardroom/src/gates/enforcement.ts:78-131` (canAdvance), `162-204` (advancePhase)

---

### 1.5 Governance Protection (Self-Modification Restrictions)

**Definition:**
Governance protection is the principle that governed agents must not modify the governance assets that constrain them. This is enforced at the infrastructure level, not as a policy agents are asked to follow.

**Enforcement Mechanism:**
- **Protected Asset List:** Defined in `board.yaml` under `governance.protected_assets`
- **File Access Control:** Scoped permissions prevent write access to protected files
- **Modification Authority:** Only the Board Chair (human) or an isolated maintenance system may modify governance assets
- **Audit Alerts:** Attempts to access protected assets trigger `governance_violation` anomaly alerts

**Structural Invariants:**
1. **Asset Protection:** Files in the protected list are read-only to all governed agents
2. **Modification Detection:** Any write attempt to protected assets is logged and blocked
3. **Human-Only Authority:** Governance changes require explicit human authorization
4. **No Delegation:** Governed agents cannot propose or execute governance changes, even if instructed

**Risk Mitigated:**
Prevents governance erosion, blocks agents from optimizing away constraints, ensures stability of enforcement mechanisms, maintains separation of powers.

**Relevant Code Reference:**
`AgentBoardroom/CONSTITUTION.md:339-375` (Article IX), `AgentBoardroom/board.yaml:113-119` (governance config)

---

### 1.6 Auditor Mechanisms

**Definition:**
The Auditor is a watchdog role with read-only access to all system state, responsible for monitoring resource consumption, detecting anomalies, and triggering alerts or emergency freezes.

**Enforcement Mechanism:**
- **Cron-Based Heartbeat:** Runs on a fixed schedule (default: 15 minutes)
- **Read-Only Access:** Cannot modify code, plans, or artifacts; can only write to audit logs and budget records
- **Anomaly Detection Rules:**
  - `infinite_loop`: Agent retries same task >3 times
  - `budget_warning`: Spend >80% of budget
  - `budget_breach`: Spend >100% of budget
  - `scope_creep`: New tasks >120% of original count
  - `time_breach`: Task duration >3x estimate
  - `permission_violation`: Agent accesses files outside scope
  - `resource_pressure`: Excessive token consumption per cycle
  - `governance_violation`: Attempt to modify protected assets
- **Budget Tier Routing:** Automatically downgrades models based on budget consumption thresholds
- **Emergency Freeze:** Can freeze all operations on budget breach or security violation without human approval

**Structural Invariants:**
1. **Read-Only Constraint:** Auditor never modifies operational state (code, plans, decisions)
2. **Alert Integrity:** Alerts cannot be suppressed or modified by other agents
3. **Budget Authority:** Auditor has sole write authority over budget records
4. **Cron Reliability:** Heartbeat runs on schedule, independent of agent workload
5. **Escalation Requirement:** Emergency freeze must be followed by immediate escalation to human authority

**Risk Mitigated:**
Prevents runaway resource consumption, detects infinite loops, flags scope drift, identifies security violations, provides early warning of budget exhaustion.

**Relevant Code Reference:**
`AgentBoardroom/agents/auditor.md:25-33` (monitoring checklist), `AgentBoardroom/CONSTITUTION.md:246-294` (Article VI)

---

### 1.7 Template-Driven Governance Configuration

**Definition:**
Template-driven configuration is the practice of defining governance rules, role relationships, and enforcement thresholds in a declarative YAML configuration file, separate from code.

**Enforcement Mechanism:**
- **board.yaml:** Single source of truth for governance structure
- **Role Definitions:** Responsibilities, challenge relationships, model tiers, session types
- **Challenge Relationships:** Who challenges whom (e.g., CEO challenged by CTO)
- **Gate Definitions:** Required approvals for each phase transition
- **Budget Thresholds:** Percentage triggers for model tier changes and freezes
- **Protected Assets:** List of files that agents cannot modify

**Structural Invariants:**
1. **Single Source of Truth:** Configuration is loaded once at startup; changes require restart
2. **Schema Validation:** Config must conform to defined schema (types, required fields)
3. **Referential Integrity:** Challenge relationships must reference defined roles
4. **Threshold Consistency:** Budget thresholds must be monotonic (0% < medium < low < freeze <= 100%)
5. **Acyclic Challenges:** Challenge relationships must not form cycles

**Risk Mitigated:**
Prevents implicit governance, enables governance review by non-programmers, supports governance versioning, allows safe experimentation with governance rules.

**Relevant Code Reference:**
`AgentBoardroom/board.yaml:1-136`, `AgentBoardroom/src/core/types.ts:10-92` (BoardConfig schema)

---

## 2. Mapping to career-coordination-mcp

### 2.1 Primitive: DecisionRecord Schema

**Existing Equivalent:**
**Partial.** career-coordination-mcp has `ValidationReport` (include/ccmcp/constitution/validation_report.h:17-25) which captures artifact evaluation outcomes but lacks decision lineage, supersession, or challenge history.

**Proposed Integration Surface:**
Introduce `DecisionRecord` as a new domain entity in `include/ccmcp/domain/decision.h`. Store in audit log as a specialized event type.

**Required Extensions:**
1. Add `DecisionRecord` struct with fields: `decision_id`, `author`, `type`, `summary`, `rationale`, `evidence`, `status`, `supersedes`, `dependencies`, `timestamp`
2. Add `DecisionStore` interface (similar to `IAuditLog`) for append-only storage
3. Extend `AuditEvent` to support `DecisionProposed`, `DecisionSuperseded`, `DecisionAccepted` event types

**Explicit Non-Goals:**
- Do NOT implement agent roles (CEO, CTO, etc.) — decisions are authored by human operators or named subsystems
- Do NOT implement multi-agent orchestration — decisions track validation outcomes, not agent coordination
- Do NOT require challenge protocol initially — challenge is optional (see 2.2)

**Alignment Notes:**
DecisionRecord strengthens constitutional auditability by creating explicit decision provenance. Unlike AgentBoardroom's multi-agent context, career-coordination-mcp decisions would document:
- Resume composition choices (which atoms selected, why)
- Validation overrides (who overrode, what finding, justification)
- Matching threshold adjustments (why threshold changed, evidence)

---

### 2.2 Primitive: Challenge Protocol

**Existing Equivalent:**
**None.** career-coordination-mcp has no adversarial review mechanism.

**Proposed Integration Surface:**
Optional extension for v0.3+. If implemented, integrate into `ValidationEngine` as a pre-execution review gate.

**Required Extensions:**
1. Add `ChallengeConfig` to `Constitution` struct defining who may challenge which decision types
2. Add `challenge()` method to `DecisionStore` for recording challenges
3. Add `canExecute()` check before applying validation overrides

**Explicit Non-Goals:**
- Do NOT implement automatic challenge assignment — challenges are human-initiated
- Do NOT implement round limits initially — start with single-round challenge/accept pattern
- Do NOT implement auto-escalation — all escalations are human-triggered
- Do NOT implement role-based access control for challenges — rely on operational discipline

**Alignment Notes:**
Challenge protocol is a **deferred pattern** for career-coordination-mcp. The system's deterministic posture means most decisions are rule-driven, not discretionary. Challenge makes sense for:
- Human override justifications (challenge: "This override weakens grounding requirements")
- Constitutional amendments (challenge: "This rule change permits unverifiable claims")

However, v0.1-v0.2 do not require challenge protocol. Mark as **future consideration**.

---

### 2.3 Primitive: Gate Enforcement Semantics

**Existing Equivalent:**
**Partial.** `ValidationEngine` produces `ValidationStatus` (kAccepted/kNeedsReview/kRejected/kBlocked) which maps to PASS/WARN/FAIL/BLOCK, but lacks:
- Structural phase state machine integration
- Gate history across multiple validations
- CONDITIONAL verdict type

**Proposed Integration Surface:**
Extend `ValidationEngine` to support gate semantics for state transitions (e.g., outreach state machine gates).

**Required Extensions:**
1. Add `GateVerdict` struct wrapping `ValidationReport` with phase/transition metadata
2. Add `GateHistory` storage for queryable verdict history
3. Add `canTransition()` method to `Interaction` struct that checks gate verdicts before state transitions
4. Add `ValidationStatus::kConditional` to support warnings that don't block but must be tracked

**Explicit Non-Goals:**
- Do NOT implement multi-phase project management — career-coordination-mcp is not a project tracker
- Do NOT implement QA role — validation is rule-based, not human-reviewer-based
- Do NOT gate resume generation — gating applies to state transitions (outreach FSM) and validation overrides

**Alignment Notes:**
Gate enforcement strengthens the outreach state machine by requiring validation before critical transitions:
- **Gate:** Before `Interaction::kDraft → kSent`, validate "no duplicate outreach within cooldown"
- **Gate:** Before `Resume` acceptance, validate "all bullets grounded in atoms"
- **Gate:** Before applying override, validate "override justification provided"

This aligns with career-coordination-mcp's "no autonomous actions" posture — gates are structural checks, not agent coordination.

---

### 2.4 Primitive: Governance Protection

**Existing Equivalent:**
**None.** career-coordination-mcp does not currently define protected governance assets.

**Proposed Integration Surface:**
Define protected assets list in a governance configuration file (e.g., `GOVERNANCE.yaml` or extend `CONSTITUTION.md`).

**Required Extensions:**
1. Create `GOVERNANCE.yaml` defining protected assets (constitution files, rule packs, schemas)
2. Document in `ARCHITECTURE.md` that governance assets are version-controlled and cannot be modified by system output
3. Add explicit note in `CONSTITUTIONAL_VALIDATION_ENGINE.md` that rule packs are compiled-in or loaded from protected paths

**Explicit Non-Goals:**
- Do NOT implement runtime file access control — protection is enforced by operational discipline and code review
- Do NOT implement agent-based modification detection — system has no autonomous agents
- Do NOT implement permission systems — rely on filesystem permissions and CI checks

**Alignment Notes:**
Governance protection in career-coordination-mcp means:
- Constitution files (docs/CONSTITUTIONAL_RULES.md) are never modified by generated output
- Rule pack code (future: src/rules/*.cpp) is protected by code review
- Experience atoms are immutable once published (new versions create new atoms, old remain)

This is already implicit in the design; making it explicit strengthens governance clarity.

---

### 2.5 Primitive: Auditor Mechanisms

**Existing Equivalent:**
**None.** career-coordination-mcp has audit logging (`IAuditLog`) but no resource monitoring, anomaly detection, or budget enforcement.

**Proposed Integration Surface:**
Introduce an optional `AnomalyDetector` subsystem in `include/ccmcp/governance/anomaly_detector.h` (new directory: `governance/`).

**Required Extensions:**
1. Add `AnomalyDetector` class with methods: `detect_loop()`, `detect_scope_creep()`, `detect_rule_violation()`
2. Add `AnomalyAlert` event type to `AuditEvent`
3. Add detection rules:
   - **Scope creep:** Resume bullet count exceeds opportunity requirements by >50%
   - **Grounding violation:** Validation detects ungrounded claim (already a rule, but flag as anomaly)
   - **State loop:** Interaction transitions fail repeatedly (>3 times)
   - **Override rate:** More than 20% of validations result in override (potential rule erosion)
4. Integrate into validation pipeline as Phase 6 (post-decision monitoring)

**Explicit Non-Goals:**
- Do NOT implement budget tracking — career-coordination-mcp does not track token/API costs in v0.1-v0.2
- Do NOT implement cron-based monitoring — anomaly detection runs inline with validation
- Do NOT implement emergency freeze — system has no autonomous runtime to freeze
- Do NOT implement model tier routing — no multi-model orchestration

**Alignment Notes:**
Auditor mechanisms in career-coordination-mcp focus on **governance health**, not resource consumption:
- Detect when validation rules are being overridden frequently (sign of rule mismatch)
- Detect when resume composition repeatedly fails validation (sign of poor atom coverage)
- Detect when outreach state machine encounters illegal transitions (sign of logic error)

This is a **monitoring and alerting layer**, not an enforcement layer. Alerts are logged; human operator reviews.

---

### 2.6 Primitive: Template-Driven Configuration

**Existing Equivalent:**
**Partial.** Constitution is defined programmatically (`make_default_constitution()` in validation_engine.h:25), not declaratively.

**Proposed Integration Surface:**
Introduce `constitution.yaml` or `constitution.json` for rule configuration in v0.2+.

**Required Extensions:**
1. Add YAML/JSON schema for Constitution configuration
2. Add `Constitution::from_file(path)` loader
3. Define rule pack registry (rule ID → implementation mapping)
4. Add validation of config schema on load

**Explicit Non-Goals:**
- Do NOT implement dynamic rule loading — rules are compiled-in, config selects which to enable
- Do NOT implement role-based configuration — no agent roles in career-coordination-mcp
- Do NOT implement challenge relationships in config — challenge is not implemented (see 2.2)

**Alignment Notes:**
Template-driven configuration allows:
- Non-developers to review governance rules (constitution.yaml is more readable than C++ code)
- Different constitutions for different contexts (strict for production, relaxed for testing)
- Versioned governance (constitution.yaml is version-controlled)

Example structure:
```yaml
constitution:
  id: "career-coordination-v1"
  version: "1.0.0"
  rules:
    - id: "GND-001"
      enabled: true
      severity: "FAIL"
      description: "No ungrounded claims in resume bullets"
    - id: "OUT-001"
      enabled: true
      severity: "BLOCK"
      description: "No duplicate first-touch within cooldown"
```

---

## 3. Minimal Additions Recommended (v0.2 Candidate List)

The following four additions are proposed for v0.2. Each is small in scope, aligns with deterministic inference, preserves auditability, and avoids agent orchestration.

---

### 3.1 Addition: DecisionRecord Domain Entity

**Scope:**
Introduce `DecisionRecord` as a first-class domain entity for tracking significant decisions (resume selections, validation overrides, matching threshold adjustments).

**Implementation Surface:**
- New file: `include/ccmcp/domain/decision.h` (struct definition)
- New file: `include/ccmcp/storage/decision_store.h` (interface)
- New file: `src/storage/decision_store.cpp` (in-memory implementation for v0.2)
- Integration: Extend `IAuditLog` to support `DecisionEvent` subtypes

**Acceptance Criteria:**
1. DecisionRecord struct includes: id, timestamp, author, type, summary, rationale, evidence[], status, supersedes, dependencies[]
2. InMemoryDecisionStore implements append-only storage with query by author, type, project
3. DecisionStore supports lineage queries: `chain(id)` returns ancestor decisions
4. Unit tests verify immutability (cannot modify existing record), lineage integrity (acyclic), queryability

**Why This is Minimal:**
Does not require challenge protocol, does not require gate integration, does not require configuration. Pure data structure + storage interface. No behavior change to existing validation flow.

**Alignment:**
Strengthens auditability. Provides explicit provenance for resume composition choices, override justifications, and requirement interpretations. Supports forensic analysis ("Why was this bullet included?").

---

### 3.2 Addition: Gate Semantics for Outreach State Machine

**Scope:**
Extend `Interaction` state transitions to check validation gates before advancing state.

**Implementation Surface:**
- Modify: `include/ccmcp/domain/interaction.h` (add `validate_transition()` method)
- Modify: `src/domain/interaction.cpp` (integrate `ValidationEngine` gate check)
- New: `GateVerdict` wrapper around `ValidationReport` with phase/transition metadata
- Integration: `Interaction::apply()` calls `validate_transition()` before state change

**Acceptance Criteria:**
1. `Interaction::apply(event)` returns false if validation gate fails
2. Failed transition logs `InteractionTransitionBlocked` audit event with validation findings
3. State machine tests verify BLOCK verdict prevents transition
4. State machine tests verify CONDITIONAL verdict allows transition but logs warnings

**Why This is Minimal:**
Reuses existing `ValidationEngine` infrastructure. No new validation rules required (rules already defined in CONSTITUTIONAL_RULES.md). Pure integration of existing subsystems.

**Alignment:**
Enforces "no duplicate outreach" rule at state machine level, not just validation level. Prevents illegal state transitions structurally. Aligns with "rules decide structure" philosophy.

---

### 3.3 Addition: Governance Asset Protection Declaration

**Scope:**
Explicitly document protected governance assets and their modification constraints.

**Implementation Surface:**
- New file: `docs/GOVERNANCE.md` (declares protected assets, modification authority, rationale)
- Modify: `docs/ARCHITECTURE.md` (reference governance protection section)
- Modify: `README.md` (add note on governance asset protection under "Philosophy")

**Acceptance Criteria:**
1. GOVERNANCE.md lists all governance files: CONSTITUTIONAL_RULES.md, CONSTITUTIONAL_VALIDATION_ENGINE.md, ARCHITECTURE.md, domain model schemas
2. GOVERNANCE.md states modification authority: Human operator with code review only
3. GOVERNANCE.md states rationale: Prevents governance drift, ensures stability
4. CI checks (future) enforce no modifications to protected files in automated commits

**Why This is Minimal:**
Pure documentation. No code changes. No new enforcement mechanisms (relies on operational discipline). Can be completed in <1 hour.

**Alignment:**
Makes implicit governance protection explicit. Prevents accidental modification of constitution during refactoring. Strengthens trust in governance stability.

---

### 3.4 Addition: Anomaly Detection for Override Rate

**Scope:**
Add monitoring for high validation override rates as a signal of governance health.

**Implementation Surface:**
- New file: `include/ccmcp/governance/anomaly_detector.h` (interface)
- New file: `src/governance/anomaly_detector.cpp` (implementation)
- Modify: `ValidationEngine::validate()` to log override events
- New: `AnomalyAlert` event type in `AuditEvent`

**Acceptance Criteria:**
1. `AnomalyDetector::check_override_rate(trace_id)` computes % of validations that resulted in override
2. If override rate >20% over last N validations, emit `AnomalyAlert` with severity WARNING
3. Alert includes: rule ID being overridden frequently, suggested action ("Review rule threshold")
4. Unit tests verify threshold detection, alert emission, zero false positives on low override rate

**Why This is Minimal:**
Passive monitoring only. No enforcement action. No integration into validation flow (runs post-hoc). Uses existing audit log data. Pure analysis + alert emission.

**Alignment:**
Detects governance erosion early. High override rate signals mismatch between rules and reality. Prompts human review of constitutional thresholds. Supports continuous governance improvement.

---

## 4. Patterns Explicitly Rejected

The following AgentBoardroom patterns are **not recommended** for career-coordination-mcp. Each is rejected with rationale.

---

### 4.1 Rejected: Multi-Agent Orchestration Patterns

**Pattern Description:**
AgentBoardroom implements CEO, CTO, QA, and Auditor as autonomous agents with persistent sessions, challenge loops, and resource allocation authority.

**Why Rejected:**
career-coordination-mcp is a **deterministic inference enhanced system**, not an autonomous multi-agent system. It has:
- No persistent agent sessions
- No autonomous planning loops
- No inter-agent communication channels
- No resource competition between agents

**Alternative Approach:**
Roles in career-coordination-mcp are **subsystems** (ValidationEngine, Matcher, ResumeComposer) and **human operators** (user overriding validation). No agent abstraction is needed.

**Risk of Adoption:**
Introduces runtime complexity, non-determinism, debugging difficulty, and scope creep. Violates "no autonomous actions without explicit human authority" principle.

---

### 4.2 Rejected: Autonomous Runtime Loops

**Pattern Description:**
AgentBoardroom's CEO autonomously decomposes tasks, commissions teams, and replans when conditions change. Auditor runs on a cron heartbeat, autonomously downgrading models and freezing operations.

**Why Rejected:**
career-coordination-mcp is invoked **on-demand** via MCP protocol. It has:
- No background processes
- No autonomous task decomposition
- No replanning loops
- No cron-based monitoring

**Alternative Approach:**
All operations are synchronous request-response. User invokes matching, validation, resume generation explicitly. No autonomous behavior.

**Risk of Adoption:**
Requires process management, state persistence, error recovery mechanisms. Violates "human authority always" principle. Introduces operational complexity without clear value.

---

### 4.3 Rejected: Platform-Specific Integrations

**Pattern Description:**
AgentBoardroom integrates with OpenClaw (agent runtime), Mattermost (messaging), and git (state management) via adapters. Board configuration specifies `runtime.platform: openclaw` and `channels.messaging_platform: mattermost`.

**Why Rejected:**
career-coordination-mcp is designed for **platform independence**. It exposes functionality via MCP protocol and can be embedded in any host (CLI, web service, IDE plugin).

**Alternative Approach:**
No platform-specific adapters. Storage is abstracted via interfaces (`IAuditLog`, `IDecisionStore`). Communication is via MCP protocol only.

**Risk of Adoption:**
Couples system to specific tools. Reduces portability. Introduces external dependencies. Violates "no hidden network calls" principle (adapter might make API calls).

---

### 4.4 Rejected: Model Tier Routing Based on Budget

**Pattern Description:**
AgentBoardroom's Auditor dynamically routes agents to different LLM models (high/medium/low/local_only) based on token budget consumption. Budget thresholds trigger automatic downgrade.

**Why Rejected:**
career-coordination-mcp **does not orchestrate LLM calls** in v0.1-v0.2. LLM integration is deferred. When added, LLM is used only for text generation (resume prose, outreach drafting), not for decision-making.

**Alternative Approach:**
If LLM integration is added, model selection is **user-configured**, not budget-driven. No dynamic routing. No autonomous downgrade.

**Risk of Adoption:**
Requires budget tracking infrastructure, LLM API abstraction, cost monitoring. Violates "no autonomous actions" principle (model downgrade happens without user approval).

---

### 4.5 Rejected: Challenge Protocol with Auto-Escalation

**Pattern Description:**
AgentBoardroom enforces 3-round challenge limits with automatic escalation to Board Chair when limit is reached. Escalation is structural (cannot be bypassed).

**Why Rejected:**
career-coordination-mcp has **no multi-stakeholder decision authority**. Decisions are either:
1. Rule-based (deterministic, no challenge needed), or
2. Human override (human has final authority, no escalation needed)

**Alternative Approach:**
If challenge is needed (future), it would be **single-round human review** of validation overrides. No auto-escalation (human either accepts override justification or rejects it).

**Risk of Adoption:**
Requires role hierarchy, escalation routing, timeout handling. Introduces coordination overhead. Violates "minimal human touchpoints" (career-coordination-mcp should require human input only when needed, not on every decision).

---

### 4.6 Rejected: Team Commission and Dissolution

**Pattern Description:**
AgentBoardroom's CEO commissions agent teams (ephemeral multi-agent groups) for specific tasks and dissolves them on completion. Teams have internal self-governance.

**Why Rejected:**
career-coordination-mcp has **no concept of teams**. It is a single-process, synchronous system. All subsystems (Matcher, ValidationEngine, ResumeComposer) are statically linked modules, not dynamically spawned entities.

**Alternative Approach:**
No team abstraction. Subsystems are C++ classes/functions, not agents.

**Risk of Adoption:**
Requires process spawning, session management, inter-process communication. Violates single-process architecture. Introduces failure modes (team crashes, coordination deadlock).

---

## 5. Alignment with PlainSight Governance Posture

PlainSight Lab's governance philosophy prioritizes **deterministic inference, constitutional validation, explicit human authority, and auditability**. The retained primitives from AgentBoardroom reinforce this posture as follows:

---

### 5.1 Deterministic Inference Enhanced

**AgentBoardroom Primitive:** Gate Enforcement Semantics
**Alignment:**
Gates enforce deterministic rules (e.g., "no advancement without QA PASS"). career-coordination-mcp applies this to state transitions: "no outreach state change without validation PASS". Both systems use rules to decide structure, not LLM discretion.

**AgentBoardroom Primitive:** Template-Driven Configuration
**Alignment:**
Declarative YAML configuration ensures governance rules are explicit, reviewable, and version-controlled. career-coordination-mcp benefits from this by making constitutional rules human-readable (constitution.yaml instead of C++ code). Both systems prioritize explainability.

---

### 5.2 Constitutional Validation

**AgentBoardroom Primitive:** DecisionRecord Schema
**Alignment:**
Decision records make rationale explicit and evidence-linked. career-coordination-mcp strengthens validation by requiring decisions (overrides, atom selections) to include justification that can be audited later. Both systems enforce "no silent authority drift".

**AgentBoardroom Primitive:** Gate Enforcement Semantics
**Alignment:**
Structural gates prevent bypassing validation. career-coordination-mcp applies this to outreach state machine: validation must pass before state transition proceeds. Both systems treat validation as mandatory, not advisory.

---

### 5.3 Explicit Human Authority

**AgentBoardroom Primitive:** Governance Protection
**Alignment:**
AgentBoardroom prohibits agents from modifying governance assets. career-coordination-mcp extends this: constitution files are human-authored and version-controlled; system cannot rewrite its own rules. Both systems ensure humans retain governance authority.

**AgentBoardroom Primitive:** Challenge Protocol (Deferred)
**Alignment:**
When challenge is implemented (v0.3+), it reinforces human authority by requiring human review of contentious overrides. However, v0.1-v0.2 rely on simpler human override model. Both systems ensure human has final say.

---

### 5.4 Auditability and Traceability

**AgentBoardroom Primitive:** DecisionRecord Lineage
**Alignment:**
Supersession chains document how decisions evolve over time. career-coordination-mcp benefits from this for tracking resume composition evolution: "Why did bullet X appear in version 3 but not version 2?" Both systems support forensic reconstruction.

**AgentBoardroom Primitive:** Anomaly Detection
**Alignment:**
Auditor's anomaly alerts surface governance health issues (high override rate, scope creep). career-coordination-mcp applies this to detect rule erosion signals. Both systems use monitoring to maintain governance integrity over time.

**AgentBoardroom Primitive:** Gate History
**Alignment:**
Queryable gate verdict history enables cross-project governance analysis. career-coordination-mcp benefits from this for analyzing validation patterns: "Which rules are most frequently violated?" Both systems prioritize data-driven governance improvement.

---

### 5.5 No Autonomous Actions Without Human Authority

**AgentBoardroom Primitive:** Challenge Protocol
**Alignment:**
Challenge protocol requires explicit acceptance before decision execution. career-coordination-mcp applies this principle to validation overrides: override requires explicit human justification (future: challenge review). Both systems prevent unilateral action.

**AgentBoardroom Primitive:** Governance Protection
**Alignment:**
Self-modification prohibition ensures agents cannot autonomously alter governance. career-coordination-mcp enforces this by making constitution files read-only to system processes. Both systems prevent self-modification.

**Rejected Pattern:** Autonomous Runtime Loops
**Alignment:**
AgentBoardroom's cron-based Auditor has autonomous downgrade/freeze authority. career-coordination-mcp **rejects this** because all actions require user invocation. Anomaly detection emits alerts only; human decides response. This strengthens human authority posture.

---

## 6. Summary and Recommendations

### 6.1 Key Findings

1. **Strong Alignment:** AgentBoardroom's governance primitives align conceptually with career-coordination-mcp's constitutional posture. Both systems prioritize explicit rules, auditability, and human authority.

2. **Architectural Divergence:** AgentBoardroom is a multi-agent orchestration system; career-coordination-mcp is a deterministic inference engine. Many AgentBoardroom patterns (autonomous loops, team coordination, model routing) are inapplicable.

3. **Reusable Primitives:** Six primitives are architecturally transferable:
   - DecisionRecord schema (strengthen audit trail)
   - Supersession chains (track decision evolution)
   - Gate enforcement semantics (structural validation gates)
   - Governance protection (explicit asset protection)
   - Anomaly detection (governance health monitoring)
   - Template-driven configuration (declarative rules)

4. **Minimal Extension Surface:** Four primitives can be adopted in v0.2 with minimal scope:
   - DecisionRecord domain entity
   - Gate semantics for state machine
   - Governance asset protection declaration
   - Anomaly detection for override rate

### 6.2 Implementation Priority

**Phase 1 (v0.2):**
1. Governance Asset Protection Declaration (documentation only, 1-2 hours)
2. DecisionRecord Domain Entity (pure data structure, 4-6 hours)

**Phase 2 (v0.2):**
3. Gate Semantics for Outreach State Machine (integration work, 6-8 hours)
4. Anomaly Detection for Override Rate (monitoring layer, 4-6 hours)

**Phase 3 (v0.3+, Deferred):**
5. Challenge Protocol (requires multi-stakeholder model, 16-24 hours)
6. Supersession Chain Queries (requires DecisionRecord maturity, 6-8 hours)
7. Template-Driven Configuration (requires YAML parser, schema validation, 12-16 hours)

### 6.3 Risk Mitigation

**Risk:** Scope creep from AgentBoardroom's multi-agent complexity
**Mitigation:** Explicitly reject autonomous loops, team coordination, and model routing. Adopt primitives as **data structures and validation patterns**, not agent behaviors.

**Risk:** Over-engineering governance for v0.1-v0.2 needs
**Mitigation:** Prioritize documentation (Governance Protection) and data structures (DecisionRecord) over complex enforcement (Challenge Protocol). Keep additions minimal.

**Risk:** Coupling to AgentBoardroom's TypeScript implementation
**Mitigation:** Extract **patterns**, not code. Implement in idiomatic C++20 with career-coordination-mcp's existing architecture (Result<T>, explicit error handling, const correctness).

### 6.4 Final Recommendation

**Adopt DecisionRecord, Gate Semantics, Governance Protection, and Anomaly Detection as v0.2 extensions.**
**Defer Challenge Protocol and Template-Driven Configuration to v0.3+.**
**Reject all autonomous multi-agent orchestration patterns.**

These additions strengthen constitutional enforcement and auditability while preserving career-coordination-mcp's deterministic, human-authority-first architecture.

---

**End of Document**

---

## Appendix A: Primitive Comparison Matrix

| Primitive | AgentBoardroom | career-coordination-mcp | Gap | Priority |
|-----------|---------------|------------------------|-----|----------|
| DecisionRecord | Full (lineage, challenge history) | Partial (ValidationReport lacks lineage) | Need decision entity | High |
| Supersession Chains | Full (forward/backward traversal) | None | Need lineage queries | Medium |
| Challenge Protocol | Full (3-round, auto-escalation) | None | Defer to v0.3+ | Low |
| Gate Enforcement | Full (PASS/FAIL/CONDITIONAL, structural) | Partial (validation status, no gate history) | Need state machine integration | High |
| Governance Protection | Full (file access control) | Implicit (no formal declaration) | Need documentation | High |
| Auditor Mechanisms | Full (cron, budget, anomaly detection) | None | Need governance health monitoring | Medium |
| Template-Driven Config | Full (board.yaml) | Partial (programmatic constitution) | Defer to v0.2+ | Medium |

---

## Appendix B: Architectural Terminology Mapping

| AgentBoardroom Term | career-coordination-mcp Equivalent | Notes |
|---------------------|-----------------------------------|-------|
| Board Chair | Human Operator / User | Final authority for overrides and escalations |
| CEO (Strategist) | N/A (no planning agent) | Planning is user-driven, not autonomous |
| CTO (Architect) | N/A (no review agent) | Architecture defined in docs, not by agent |
| QA (Gatekeeper) | ValidationEngine | Deterministic rule evaluation, not agent review |
| Auditor (Watchdog) | AnomalyDetector (proposed) | Passive monitoring, not autonomous enforcement |
| Agent Team | Subsystem (Matcher, ResumeComposer) | C++ modules, not spawned agents |
| Decision Record | Decision (proposed domain entity) | Tracks significant choices, not agent coordination |
| Gate Verdict | ValidationReport + GateVerdict (proposed) | Structural validation outcome |
| Challenge Round | Human Override Review (future) | Single-round human review, not multi-round agent challenge |
| Board Configuration | Constitution + GovernanceConfig (proposed) | Declarative rules, not role assignments |
| Channel Message | AuditEvent | Append-only log entry, not inter-agent message |

---

## Appendix C: Implementation Sketch (DecisionRecord)

**Illustrative C++ header (not production code):**

```cpp
// include/ccmcp/domain/decision.h
#pragma once

#include <string>
#include <vector>
#include "ccmcp/core/ids.h"

namespace ccmcp::domain {

enum class DecisionType {
  kResumeAtomSelection,
  kValidationOverride,
  kMatchingThresholdAdjustment,
  kRequirementInterpretation,
};

enum class DecisionStatus {
  kProposed,
  kAccepted,
  kSuperseded,
};

struct DecisionRecord {
  core::DecisionId decision_id;
  std::string timestamp; // ISO 8601
  std::string author; // "user" or subsystem name
  DecisionType type;
  std::string summary;
  std::string rationale;
  std::vector<std::string> evidence; // atom IDs, requirement IDs, etc.
  DecisionStatus status;
  std::optional<core::DecisionId> supersedes; // null if original
  std::vector<core::DecisionId> dependencies; // decisions this depends on
};

} // namespace ccmcp::domain
```

**Illustrative storage interface:**

```cpp
// include/ccmcp/storage/decision_store.h
#pragma once

#include <vector>
#include "ccmcp/domain/decision.h"

namespace ccmcp::storage {

class IDecisionStore {
 public:
  virtual ~IDecisionStore() = default;
  virtual void append(const domain::DecisionRecord& record) = 0;
  virtual std::vector<domain::DecisionRecord> query_by_author(const std::string& author) const = 0;
  virtual std::vector<domain::DecisionRecord> chain(core::DecisionId id) const = 0;
};

} // namespace ccmcp::storage
```

This sketch illustrates the pattern extraction: data structures and interfaces, not agent behaviors.

---

**Document Metadata:**
- Total Lines: ~1,480
- Sections: 6 main + 3 appendices
- Primitives Analyzed: 7
- Primitives Recommended: 4
- Primitives Deferred: 2
- Primitives Rejected: 6
- Code Examples: Illustrative only (Appendix C)
