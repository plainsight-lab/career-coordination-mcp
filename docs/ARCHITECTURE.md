# Purpose

career-coordination-mcp is a deterministic career coordination engine exposed through the Model Context Protocol (MCP). It performs structured opportunity matching, resume composition, and outreach state tracking with an auditable trail.

The system may optionally use LLMs for text generation, but all decisions are constrained and validated by deterministic rules. If LLM output cannot be validated, it is rejected.

# Design Principles

- Deterministic first: All selection, matching, and eligibility decisions are rule-based and testable.
- LLM as instrument: LLMs draft prose; deterministic validators enforce constraints.
- Immutable facts: “Experience atoms” are immutable capability statements with verifiers.
- Auditability: Every recommendation can be explained as: inputs → rules → outputs.
- No double outreach: Outreach is governed by a state machine and contact identity resolution.
- Error correction is explicit: Errors become tracked objects with correction paths.

# High-Level Components

## Experience Atom Store

Canonical, versioned “atoms” describing verified capabilities.

## Opportunity Ingestion

Converts job postings into structured requirements and metadata.

## Opportunity Matching Engine

Scores opportunities against atoms using deterministic features + rules.

## Resume Composition Engine

Assembles a resume variant using a constitution of constraints.

## Outreach State Machine

Manages contact interaction lifecycle, prevents duplicate outreach.

## Interaction Audit Log

Append-only record of decisions, messages, and state transitions.

## Error Detection/Correction

Identifies rule violations, drift, duplicates, and inconsistent states.

# Deterministic + LLM-enhanced Flow

1. Ingest opportunity → parse into structured Opportunity.
2. Match opportunity against ExperienceAtoms deterministically → produce MatchReport.
3. Compose resume deterministically (selection + ordering), then optionally:
   - ask LLM to draft bullets/summary from selected atoms only
4. Validate output against constitutional rules.
5. Store artifact + audit entry.
6. Optionally generate outreach sequence and track interactions via state machine.

# Storage Approach

- Structured state: Redis (fast state machine + queues) + local persistent store (choose one):
  - LanceDB for embeddings & retrieval (optional)
  - SQLite/Postgres for canonical entities (recommended for audit durability)
- Audit log: append-only JSONL or SQLite table with immutable events

# Non-Goals

- Autonomous emailing / messaging
- Auto-apply to jobs
- Unbounded web scraping
- “Agentic” loops without deterministic gates
