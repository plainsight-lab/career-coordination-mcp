#!/usr/bin/env bash
# docker-entrypoint.sh — startup wrapper for career-coordination-mcp in Docker.
#
# Reads environment variables, validates required fields, constructs CLI
# arguments, and exec-replaces itself with the mcp_server binary.
#
# Required environment variables:
#   REDIS_HOST          Hostname or IP of the Redis service
#
# Optional environment variables (with documented defaults):
#   REDIS_PORT          Redis port (default: 6379)
#   REDIS_DB            Redis database index (default: 0)
#   DB_PATH             Path to SQLite database file (default: ephemeral in-memory)
#   VECTOR_BACKEND      Vector storage backend: inmemory | sqlite (default: inmemory)
#   VECTOR_DB_PATH      Directory for SQLite vector index (required if VECTOR_BACKEND=sqlite)

set -euo pipefail

# ── Fail fast: required variables ─────────────────────────────────────────────

if [[ -z "${REDIS_HOST:-}" ]]; then
  echo "Error: REDIS_HOST is required." >&2
  echo "       Set REDIS_HOST to the Redis server hostname or IP address." >&2
  echo "       Example: REDIS_HOST=redis" >&2
  exit 1
fi

# ── Build Redis URI from components ───────────────────────────────────────────

REDIS_PORT="${REDIS_PORT:-6379}"
REDIS_DB="${REDIS_DB:-0}"
REDIS_URI="redis://${REDIS_HOST}:${REDIS_PORT}/${REDIS_DB}"

# ── Construct argument list ────────────────────────────────────────────────────

declare -a ARGS
ARGS+=(--redis "${REDIS_URI}")

if [[ -n "${DB_PATH:-}" ]]; then
  ARGS+=(--db "${DB_PATH}")
fi

VECTOR_BACKEND="${VECTOR_BACKEND:-inmemory}"
ARGS+=(--vector-backend "${VECTOR_BACKEND}")

if [[ "${VECTOR_BACKEND}" == "sqlite" ]]; then
  if [[ -z "${VECTOR_DB_PATH:-}" ]]; then
    echo "Error: VECTOR_DB_PATH is required when VECTOR_BACKEND=sqlite." >&2
    echo "       Set VECTOR_DB_PATH to a writable directory path." >&2
    exit 1
  fi
  ARGS+=(--vector-db-path "${VECTOR_DB_PATH}")
fi

# ── Exec the server (replaces this shell with the binary process) ──────────────

exec /usr/local/bin/mcp_server "${ARGS[@]}"
