-- Career Coordination MCP Schema v1
-- Canonical persistence for atoms, opportunities, interactions, and audit events

PRAGMA foreign_keys = ON;

-- Schema version tracking
CREATE TABLE IF NOT EXISTS schema_version (
  version INTEGER PRIMARY KEY,
  applied_at TEXT NOT NULL
);

-- Experience Atoms
CREATE TABLE IF NOT EXISTS atoms (
  atom_id TEXT PRIMARY KEY,
  domain TEXT NOT NULL,
  title TEXT NOT NULL,
  claim TEXT NOT NULL,
  tags_json TEXT NOT NULL,  -- JSON array, deterministic sort
  verified INTEGER NOT NULL CHECK(verified IN (0, 1)),
  evidence_refs_json TEXT NOT NULL  -- JSON array
);

CREATE INDEX IF NOT EXISTS idx_atoms_verified ON atoms(verified);

-- Opportunities
CREATE TABLE IF NOT EXISTS opportunities (
  opportunity_id TEXT PRIMARY KEY,
  company TEXT NOT NULL,
  role_title TEXT NOT NULL,
  source TEXT NOT NULL
);

-- Requirements (preserves original order via idx)
CREATE TABLE IF NOT EXISTS requirements (
  opportunity_id TEXT NOT NULL,
  idx INTEGER NOT NULL,
  text TEXT NOT NULL,
  tags_json TEXT NOT NULL,  -- JSON array, deterministic sort
  required INTEGER NOT NULL CHECK(required IN (0, 1)),
  PRIMARY KEY(opportunity_id, idx),
  FOREIGN KEY(opportunity_id) REFERENCES opportunities(opportunity_id)
    ON DELETE CASCADE
);

-- Interactions
CREATE TABLE IF NOT EXISTS interactions (
  interaction_id TEXT PRIMARY KEY,
  contact_id TEXT NOT NULL,
  opportunity_id TEXT NOT NULL,
  state INTEGER NOT NULL,
  FOREIGN KEY(opportunity_id) REFERENCES opportunities(opportunity_id)
);

-- Audit Events (append-only log)
CREATE TABLE IF NOT EXISTS audit_events (
  event_id TEXT PRIMARY KEY,
  trace_id TEXT NOT NULL,
  event_type TEXT NOT NULL,
  payload TEXT NOT NULL,
  created_at TEXT NOT NULL,
  entity_ids_json TEXT NOT NULL,  -- JSON array
  idx INTEGER NOT NULL  -- Monotonic append index per trace for deterministic ordering
);

CREATE INDEX IF NOT EXISTS idx_audit_events_trace ON audit_events(trace_id, idx);

-- Insert initial schema version
INSERT OR IGNORE INTO schema_version (version, applied_at)
VALUES (1, datetime('now'));
