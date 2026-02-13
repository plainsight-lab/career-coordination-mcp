# Tools (examples)

## atoms.list

Returns all atoms or filtered by domain/tag.

## opportunities.upsert

Insert/update an opportunity + requirements.

## match.evaluate

Input: opportunity_id
Output: deterministic MatchReport:

- matched atoms
- missing requirements
- score breakdown
- recommended resume strategy

## resume.compose

Input: opportunity_id, mode (generate_new | patch | replace_section)
Output: ResumeDraft + ValidationReport + diff vs prior if applicable.

## contacts.upsert

Upsert contact identity and relationship state.

## interactions.transition

Input: interaction_id, event (send_first_touch, follow_up, close_out, etc.)
Output: updated state + audit event id.

## audit.query

Query audit log by opportunity/contact/time window.

# Request/Response Schema Guidance

- JSON-RPC style as per MCP
- All tool outputs include:
  - trace_id
  - rule_version
  - inputs references
