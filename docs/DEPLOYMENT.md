# Deployment Guide — career-coordination-mcp

This document describes how to build and run `career-coordination-mcp` inside
Docker for local development.

> **Scope:** Local-only containerized deployment. This configuration is not
> designed for distributed, multi-tenant, or public-facing operation. All
> services run on a single host and assume single-user access.

---

## Prerequisites

| Tool | Minimum version |
|------|----------------|
| Docker | 24.x |
| Docker Compose | v2.x (`docker compose` plugin) |
| Available ports | 6379 (Redis, internal to bridge network only) |

---

## Quick Start

```bash
# 1. Create the host data directory for SQLite persistence
mkdir -p data

# 2. Build the container image
docker compose build

# 3. Start Redis and the MCP server
docker compose up
```

The MCP server communicates over **stdin/stdout** (JSON-RPC). Once running,
attach your MCP client to the container's stdio stream.

---

## Building the Container Image

```bash
docker compose build
```

Or build directly with Docker:

```bash
docker build -t ccmcp:latest .
```

### Build arguments

| Argument | Default | Purpose |
|----------|---------|---------|
| `VCPKG_COMMIT` | `e6ebaa9c3ca8fca90c63af62fc895c2486609580` | vcpkg baseline commit |
| `CMAKE_BUILD_PARALLEL_LEVEL` | `4` | Parallel build jobs |

Override during build:

```bash
docker build \
  --build-arg CMAKE_BUILD_PARALLEL_LEVEL=8 \
  -t ccmcp:latest .
```

### Build stages

| Stage | Base image | Purpose |
|-------|------------|---------|
| `builder` | `ubuntu:22.04` | Full toolchain: g++, CMake, Ninja, vcpkg |
| `runtime` | `debian:bookworm-slim` | Minimal: binary + libstdc++6 only |

The custom vcpkg overlay triplet (`docker/triplets/x64-linux-static-release.cmake`)
forces static linkage for all vcpkg-managed packages. The runtime image contains
only the compiled binary and the system C++ runtime — no vcpkg `.so` files.

---

## Running with Docker Compose

```bash
# Start all services (Redis + MCP server)
docker compose up

# Start in detached mode
docker compose up -d

# Stop all services
docker compose down

# Remove data volume (WARNING: destroys SQLite database)
docker compose down -v
rm -rf data/
```

### Services

| Service | Image | Purpose |
|---------|-------|---------|
| `redis` | `redis:7-alpine` | Interaction coordinator (required) |
| `app` | `ccmcp:latest` (built locally) | MCP server |

Redis starts before the app. The app waits for Redis to pass its health check
(`redis-cli ping`) before starting.

---

## Volume Setup

SQLite persistence uses a **bind mount** from `./data` on the host to `/data`
inside the container. The host directory must exist before starting:

```bash
mkdir -p data
```

After a successful run, the following files appear under `./data/`:

| Path | Contents |
|------|----------|
| `data/ccmcp.db` | Main SQLite database (atoms, opportunities, interactions, audit log, snapshots) |
| `data/vectors/` | SQLite vector index (when `VECTOR_BACKEND=sqlite`) |

The `.gitignore` excludes `data/` and `*.db` so database files are never
committed to source control.

---

## Environment Variables

All configuration is passed via environment variables. The entrypoint script
(`scripts/docker-entrypoint.sh`) validates required variables and translates
them to CLI arguments for the `mcp_server` binary.

### Required

| Variable | Description |
|----------|-------------|
| `REDIS_HOST` | Hostname or IP of the Redis service |

### Optional (with defaults)

| Variable | Default | Description |
|----------|---------|-------------|
| `REDIS_PORT` | `6379` | Redis port |
| `REDIS_DB` | `0` | Redis database index |
| `DB_PATH` | _(none — ephemeral in-memory SQLite)_ | Path to SQLite database file |
| `VECTOR_BACKEND` | `inmemory` | Vector storage: `inmemory` or `sqlite` |
| `VECTOR_DB_PATH` | _(none)_ | Required when `VECTOR_BACKEND=sqlite`; path to vector index directory |

### Startup validation

The entrypoint script fails fast (exit code 1) if:

- `REDIS_HOST` is unset or empty
- `VECTOR_BACKEND=sqlite` but `VECTOR_DB_PATH` is unset

The `mcp_server` binary additionally fails fast if:

- The constructed Redis URI is invalid
- Redis is unreachable (connection refused, timeout)

There are no silent fallbacks beyond the documented defaults above.

---

## Overriding Configuration

Edit `docker-compose.yml` or pass variables on the command line:

```bash
# Use a different Redis database index
REDIS_DB=1 docker compose up

# Run with ephemeral in-memory storage (no persistence)
docker compose run --rm \
  -e REDIS_HOST=redis \
  -e DB_PATH= \
  -e VECTOR_BACKEND=inmemory \
  app
```

---

## Connecting an MCP Client

The MCP server uses **stdio transport** (JSON-RPC over stdin/stdout). To
connect Claude Desktop or another MCP client to the Docker container, use
`docker compose run --rm --no-deps -i app` to attach the client's stdio to the
container.

Example `claude_desktop_config.json` entry:

```json
{
  "mcpServers": {
    "career-coordination": {
      "command": "docker",
      "args": [
        "compose",
        "-f", "/path/to/career-coordination-mcp/docker-compose.yml",
        "run", "--rm", "--no-deps", "-i", "app"
      ]
    }
  }
}
```

> **Note:** The `redis` service must be running separately (`docker compose up redis -d`)
> when using `docker compose run` for the app service.

---

## Local Development Notes

### Running without Docker

The native build remains the recommended path for development and testing:

```bash
./scripts/build.sh
```

Tests run only in the native build. Docker images do not include the test suite.

### Native + Docker Redis

For local development without containerizing the app itself:

```bash
# Start only Redis in Docker
docker compose up redis -d

# Run the native binary against the Docker Redis
./build-vcpkg/apps/mcp_server/mcp_server \
  --redis tcp://localhost:6379 \
  --db /tmp/ccmcp-dev.db \
  --vector-backend sqlite \
  --vector-db-path /tmp/vectors
```

### Security posture

- The container runs as non-root user `ccmcp` (UID 10001)
- Redis is only accessible within the `ccmcp-network` bridge network
- No ports are exposed to the host by default
- The binary is statically linked against vcpkg packages

---

## Determinism Guarantees in Docker

The container build preserves the project's determinism properties:

| Property | Mechanism |
|----------|-----------|
| Dependency versions | vcpkg baseline `e6ebaa9c3ca8fca90c63af62fc895c2486609580` pinned via `ARG VCPKG_COMMIT` |
| Repeatable builds | Same `VCPKG_COMMIT` + same source → identical binary |
| Runtime behavior | Identical to native: same startup validation, same hash chain, same schema migrations |
| No ambient state | All configuration is explicit (env vars → CLI args) |
