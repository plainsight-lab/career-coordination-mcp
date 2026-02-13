# Rule Classes

## R1: Source-of-Truth Rules

- The only permissible factual claims in output are those grounded in ExperienceAtoms.
- If a claim is not grounded, it must be removed or converted to a question for human review.

## R2: Composition Rules

- Resume content must be composed from atoms selected by deterministic matching.
- Ordering must follow deterministic policy (e.g., highest relevance → highest impact).

## R3: Opportunity Constraints

- If opportunity requires X and no atom verifies X, matching must report the gap explicitly.
- No “assumed experience” is allowed.

## R4: Outreach Governance

- No duplicate first-touch to same contact per channel within cooldown window.
- No outreach if contact relationship state is “do_not_contact”.

## R5: Audit Requirements

- Every generated artifact must have:
  - input references
  - rule set version
  - deterministic match report
  - validation report
- Every state transition must be logged as an event.

# Error Conditions

- UngroundedClaimError
- InvalidTransitionError
- DuplicateContactError
- RequirementParseError
- ValidationFailureError

# Correction Paths

- Human attests an atom (creates new verified atom version)
- Fix parser mapping rules
- Merge duplicate contact identities
- Override transition only via explicit “admin/human” action (logged)
