#!/usr/bin/env bash
# Format all C++ source files using clang-format
# Usage: ./scripts/format.sh [--check]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CHECK_ONLY=0
if [[ "${1:-}" == "--check" ]]; then
  CHECK_ONLY=1
  echo "Running format check (dry-run mode)..."
else
  echo "Formatting C++ source files..."
fi

# Find all C++ source files (exclude build dirs and dependencies)
FILES=$(find "$PROJECT_ROOT" \
  -path "$PROJECT_ROOT/build*" -prune -o \
  -path "$PROJECT_ROOT/cmake-build*" -prune -o \
  -path "$PROJECT_ROOT/vcpkg_installed" -prune -o \
  -path "$PROJECT_ROOT/.git" -prune -o \
  \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print)

if [[ $CHECK_ONLY -eq 1 ]]; then
  # Check mode: verify formatting without modifying files
  RESULT=0
  for file in $FILES; do
    if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
      echo "Format check failed: $file"
      RESULT=1
    fi
  done

  if [[ $RESULT -eq 0 ]]; then
    echo "✓ All files are properly formatted"
  else
    echo "✗ Format check failed. Run './scripts/format.sh' to fix."
    exit 1
  fi
else
  # Format mode: modify files in-place
  for file in $FILES; do
    echo "Formatting: $file"
    clang-format -i "$file"
  done
  echo "✓ Formatting complete"
fi
