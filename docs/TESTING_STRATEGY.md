# Deterministic Test Categories

## Unit Tests

- Atom verification logic
- Requirement parsing/mapping
- Scoring feature calculations
- Constitutional rule validators
- State transition table enforcement

## Property Tests (where feasible)

- “No output claim without atom support”
- “No invalid state transitions”
- “Dedupe is idempotent”

## Golden Tests

- Given a fixed Opportunity + Atom set → MatchReport must be identical
- Given a fixed selection → Resume must render deterministically (except LLM text which must be mocked)

## LLM Boundary Tests

- LLM output is treated as untrusted input
- Validators must reject hallucinated claims
- “Draft” must only use provided atom text + controlled templates

# Falsifiability Conditions

The system is wrong if:

- it produces an ungrounded claim and does not flag it
- it allows duplicate outreach without explicit override
- it cannot explain score contributions for a match
