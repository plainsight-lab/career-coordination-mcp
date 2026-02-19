#!/usr/bin/env bash
# Deterministic build entrypoint for career-coordination-mcp.
#
# Verifies that the local vcpkg registry contains the pinned baseline from
# vcpkg.json, then configures CMake, builds the project, and runs the test suite.
#
# Usage:
#   ./scripts/build.sh               # configure, build, test
#   ./scripts/build.sh --skip-tests  # configure and build only
#   ./scripts/build.sh --clean       # wipe build dir first, then build and test
#   ./scripts/build.sh --clean --skip-tests
#
# Requirements:
#   - CMake 3.21+
#   - C++20 compatible compiler (Clang 14+, GCC 12+, MSVC 19.30+)
#   - vcpkg installed at $VCPKG_ROOT or ~/vcpkg
#   - Network access if dependencies are not already cached
#
# Dependency Governance:
#   Package versions are pinned via the builtin-baseline field in vcpkg.json.
#   This script verifies the baseline commit exists in the local vcpkg registry
#   before configuring the build. If the baseline is absent, the build fails
#   fast with actionable instructions — it never silently resolves wrong versions.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Parse flags ────────────────────────────────────────────────────────────────
SKIP_TESTS=0
CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --skip-tests) SKIP_TESTS=1 ;;
    --clean)      CLEAN=1 ;;
    *)
      echo "Unknown option: $arg"
      echo "Usage: $0 [--clean] [--skip-tests]"
      exit 1
      ;;
  esac
done

echo "=== career-coordination-mcp deterministic build ==="
echo "Project root: $PROJECT_ROOT"

# ── Locate vcpkg ───────────────────────────────────────────────────────────────
if [[ -n "${VCPKG_ROOT:-}" && -d "$VCPKG_ROOT" ]]; then
  VCPKG_DIR="$VCPKG_ROOT"
elif [[ -d "$HOME/vcpkg" ]]; then
  VCPKG_DIR="$HOME/vcpkg"
else
  echo ""
  echo "ERROR: vcpkg not found."
  echo "  Set VCPKG_ROOT to your vcpkg directory, or install vcpkg at ~/vcpkg:"
  echo ""
  echo "    git clone https://github.com/microsoft/vcpkg.git ~/vcpkg"
  echo "    ~/vcpkg/bootstrap-vcpkg.sh"
  echo ""
  exit 1
fi
echo "vcpkg:        $VCPKG_DIR"

# ── Verify pinned baseline ─────────────────────────────────────────────────────
# Extract the 40-char SHA from vcpkg.json's builtin-baseline field.
BASELINE=$(python3 -c "
import json, sys
with open('$PROJECT_ROOT/vcpkg.json') as f:
    d = json.load(f)
b = d.get('builtin-baseline', '')
if len(b) != 40:
    print('ERROR: builtin-baseline missing or malformed in vcpkg.json', file=sys.stderr)
    sys.exit(1)
print(b)
")

echo "Baseline:     $BASELINE"

if ! git -C "$VCPKG_DIR" cat-file -e "${BASELINE}^{commit}" 2>/dev/null; then
  echo ""
  echo "ERROR: Pinned baseline $BASELINE not found in local vcpkg registry."
  echo ""
  echo "Your vcpkg installation is older than the pinned baseline."
  echo "Update your vcpkg clone to include this commit:"
  echo ""
  echo "  git -C $VCPKG_DIR fetch origin"
  echo "  git -C $VCPKG_DIR checkout $BASELINE"
  echo "  $VCPKG_DIR/bootstrap-vcpkg.sh"
  echo ""
  echo "Or set VCPKG_ROOT to a vcpkg installation at commit $BASELINE or later."
  exit 1
fi
echo "Baseline:     verified ✓"

# ── Clean (optional) ───────────────────────────────────────────────────────────
BUILD_DIR="$PROJECT_ROOT/build-vcpkg"
if [[ $CLEAN -eq 1 ]]; then
  echo "Cleaning build directory: $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

# ── Configure ─────────────────────────────────────────────────────────────────
echo "Configuring CMake..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/toolchains/vcpkg.cmake"

# ── Build ─────────────────────────────────────────────────────────────────────
echo "Building..."
cmake --build "$BUILD_DIR" --parallel

# ── Test ──────────────────────────────────────────────────────────────────────
if [[ $SKIP_TESTS -eq 0 ]]; then
  echo "Running tests..."
  "$BUILD_DIR/tests/ccmcp_tests"
  echo ""
  echo "=== All tests passed ==="
fi

echo "=== Build complete ==="
