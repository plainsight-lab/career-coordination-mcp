# Entities

## ExperienceAtom

Immutable, verifiable capability fact.

Fields:

- atom_id (stable UUID)
- domain (e.g., “enterprise_architecture”, “security”, “ai_governance”)
- title
- claim (short statement)
- evidence (links or references)
- tags (skills, tech, outcomes)
- confidence (human-set)
- created_at, version

Invariants:

- Atoms are append-only (new versions create new record, old remains)
- verify() must be deterministic (or human-attested)

## Opportunity

Structured representation of a role.

Fields:

- opportunity_id
- company
- role_title
- location / remote
- source (LinkedIn, company site, recruiter)
- requirements: list of Requirement
- seniority, comp_band (optional)
- notes, status (new/active/closed)

Invariants:

- Requirements must be classifiable into deterministic categories (skill, domain, constraint)
- Source and timestamps must be stored (for audit & dedupe)

## Contact

A person associated with an opportunity.

Fields:

- contact_id
- name
- org
- channels (email, LinkedIn URL, phone)
- relationship_state (unknown/warm/cold/recruiter/hiring_mgr)
- identity_keys (normalized identifiers)

Invariants:

- Identity resolution prevents duplicates (same LinkedIn URL == same contact)

## Interaction

Audit-tracked state machine node.

Fields:

- interaction_id
- contact_id
- opportunity_id
- state (see state machine)
- messages (references to drafts/sent)
- timestamps

Invariants:

- State transitions must be valid per transition table
- Each “send” action must record an audit event (even if “not sent”)

## Resume

A composed artifact.

Fields:

- resume_id
- opportunity_id
- selected_atoms (ordered list)
- sections (summary, experience bullets, skills)
- rendered_output (text)
- validation_report

Invariants:

- Rendered prose may only reference selected atoms
- Every bullet must be traceable to ≥1 atom

# Relationships

- Opportunity ↔ Requirements
- Opportunity ↔ Contact(s)
- Opportunity ↔ Resume variants
- Contact ↔ Interactions (many)
- Resume ↔ selected ExperienceAtoms (many)
