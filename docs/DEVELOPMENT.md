# Development Guide

This document describes the build system, dependency governance, and clean-room build
procedure for `career-coordination-mcp`.

---

## Prerequisites

| Tool | Minimum version | Purpose |
|------|----------------|---------|
| CMake | 3.21 | Build system |
| C++20 compiler | Clang 14+, GCC 12+, or MSVC 19.30+ | Compilation |
| vcpkg | commit `e6ebaa9c3ca8fca90c63af62fc895c2486609580` or later | Dependency management |
| Python 3 | any | Required by `scripts/build.sh` baseline verification |
| clang-format | 14+ | Code formatting (optional — enforced by pre-commit hook) |
| clang-tidy | 14+ | Static analysis (optional) |

---

## Dependency Governance

### Pinned vcpkg Baseline

All third-party dependencies are resolved through [vcpkg](https://github.com/microsoft/vcpkg)
in **manifest mode**. The `vcpkg.json` file at the repository root declares all direct
dependencies. The `builtin-baseline` field pins the vcpkg registry to a specific commit:

```json
"builtin-baseline": "e6ebaa9c3ca8fca90c63af62fc895c2486609580"
```

**What the baseline guarantees:**

- Every `vcpkg install` resolves to the exact package versions that existed at that
  registry commit — regardless of when or where the build runs.
- If the local vcpkg installation is older than the pinned commit, the build fails fast
  with an actionable error message. It never silently resolves different versions.
- No separate lockfile format is used; the `builtin-baseline` in `vcpkg.json` is the
  vcpkg equivalent of a dependency lock.

**Current pinned package versions** (at baseline `e6ebaa9c3ca8fca90c63af62fc895c2486609580`):

| Package | Version |
|---------|---------|
| catch2 | 3.12.0 |
| fmt | 12.1.0 |
| nlohmann-json | 3.12.0 |
| sqlite3 | 3.51.2 |
| redis-plus-plus | 1.3.15 |
| libzip | 1.11.4 |
| pugixml | 1.15 |

Transitive dependencies (bzip2 1.0.8, hiredis 1.3.0, zlib 1.3.1) are resolved from the
same baseline.

### Updating the Baseline

Updating the baseline is a **cross-cutting change** (CLAUDE.md §6.1) and must be:

1. Intentional — state which package(s) need a new version and why.
2. Verified — run the full test suite after updating.
3. Committed atomically with the new `builtin-baseline` SHA.

To update:

```bash
# Fetch latest vcpkg registry
git -C ~/vcpkg fetch origin && git -C ~/vcpkg pull

# Get the new HEAD commit
git -C ~/vcpkg rev-parse HEAD

# Update builtin-baseline in vcpkg.json to the new SHA, then:
./scripts/build.sh --clean
```

---

## Standard Build

The recommended entry point is `scripts/build.sh`. It verifies the pinned baseline,
configures CMake, builds, and runs tests:

```bash
# First-time or standard build
./scripts/build.sh

# Build without running tests
./scripts/build.sh --skip-tests
```

### Manual Build (without the script)

If you need to configure or build manually:

```bash
# Configure with vcpkg toolchain
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake

# Build
cmake --build build-vcpkg --parallel

# Run tests
./build-vcpkg/tests/ccmcp_tests
```

The vcpkg toolchain (`cmake/toolchains/vcpkg.cmake`) resolves vcpkg from `$VCPKG_ROOT`
or `~/vcpkg`. It defaults to the `arm64-osx` triplet; override with
`-DVCPKG_TARGET_TRIPLET=x64-linux` for other platforms.

---

## Clean-Room Reproducibility Validation

The following procedure validates that a fresh checkout produces an identical dependency
graph and a passing build. This is the canonical reproducibility check.

```bash
# 1. Wipe all build artifacts (build dir contains vcpkg_installed)
./scripts/build.sh --clean

# 2. Full build and test run from scratch
# (the --clean flag deletes build-vcpkg/ before configuring)
```

Or equivalently, manual steps:

```bash
rm -rf build-vcpkg/
./scripts/build.sh
```

### What the clean build verifies

1. **Baseline present**: vcpkg.json contains a `builtin-baseline` SHA.
2. **Baseline reachable**: the local vcpkg clone contains that commit.
3. **Dependencies resolved identically**: `vcpkg install` produces the same package
   versions as the pinned baseline specifies.
4. **Build succeeds**: all targets compile without warnings.
5. **Tests pass**: `ccmcp_tests` reports 0 failures.

### Expected output (clean build)

```
=== career-coordination-mcp deterministic build ===
Project root: /path/to/career-coordination-mcp
vcpkg:        /path/to/vcpkg
Baseline:     e6ebaa9c3ca8fca90c63af62fc895c2486609580
Baseline:     verified ✓
Configuring CMake...
Building...
Running tests...
test cases:  201 |  194 passed | 7 skipped
assertions: 1306 | 1306 passed
=== All tests passed ===
=== Build complete ===
```

The 7 skipped tests are opt-in integration tests for Redis (`CCMCP_TEST_REDIS=1`) and
SQLite vector persistence (`CCMCP_TEST_LANCEDB=1`). They are skipped by default.

---

## Running Redis Locally

The MCP server requires a running Redis instance (`--redis <uri>`). For local development:

### macOS (Homebrew)

```bash
# Install
brew install redis

# Start (foreground, for quick checks)
redis-server

# Start as a background service (persists across reboots)
brew services start redis

# Verify
redis-cli ping
# Expected: PONG

# Stop background service when done
brew services stop redis
```

### Docker (cross-platform)

```bash
# Start Redis on default port 6379
docker run --rm -p 6379:6379 redis:7-alpine

# In another terminal, verify
redis-cli ping
# Expected: PONG
```

### Health Check CLI

After building, use the `redis-health` subcommand to verify connectivity:

```bash
./build-vcpkg/apps/ccmcp_cli/ccmcp_cli redis-health --redis tcp://127.0.0.1:6379
# Expected: OK: Redis reachable at 127.0.0.1:6379
# Exit code: 0 on success, 1 on failure
```

---

## Opt-In Integration Tests

```bash
# Enable Redis coordinator tests (requires a running Redis instance)
CCMCP_TEST_REDIS=1 ./build-vcpkg/tests/ccmcp_tests "[redis]"

# Enable SQLite vector persistence tests (no external service needed)
CCMCP_TEST_LANCEDB=1 ./build-vcpkg/tests/ccmcp_tests "[lancedb]"
```

---

## Code Quality

```bash
# Format all C++ source files in-place
./scripts/format.sh

# Check formatting without modifying
./scripts/format.sh --check

# Run clang-tidy static analysis
./scripts/lint.sh

# Install pre-commit hooks (runs format check on staged files)
./scripts/install-hooks.sh
```

See [LINTING.md](LINTING.md) for detailed documentation.

---

## vcpkg_installed/ Is Not Committed

`vcpkg_installed/` is excluded from version control (`.gitignore`). Dependency artifacts
are always resolved from the pinned baseline at build time. This prevents binary drift
and ensures reproducibility from a clean clone.

---

## Platform Notes

The default CMake triplet is `arm64-osx` (Apple Silicon). To build on other platforms,
pass the appropriate triplet:

```bash
# Linux x64
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux

# Windows x64
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows
```
