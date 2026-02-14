#!/usr/bin/env bash
# Run clang-tidy static analysis on all C++ source files
# Usage: ./scripts/lint.sh [--fix]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

FIX_MODE=0
if [[ "${1:-}" == "--fix" ]]; then
  FIX_MODE=1
  echo "Running clang-tidy with automatic fixes..."
else
  echo "Running clang-tidy static analysis..."
fi

# Check if compile_commands.json exists
COMPILE_DB="$PROJECT_ROOT/build-vcpkg/compile_commands.json"
if [[ ! -f "$COMPILE_DB" ]]; then
  echo "Error: compile_commands.json not found at $COMPILE_DB"
  echo "Please build the project first with: cmake -S . -B build-vcpkg -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  exit 1
fi

# Find all C++ source files (exclude build dirs and dependencies)
FILES=$(find "$PROJECT_ROOT" \
  -path "$PROJECT_ROOT/build*" -prune -o \
  -path "$PROJECT_ROOT/cmake-build*" -prune -o \
  -path "$PROJECT_ROOT/vcpkg_installed" -prune -o \
  -path "$PROJECT_ROOT/.git" -prune -o \
  \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print)

CLANG_TIDY_ARGS="-p $PROJECT_ROOT/build-vcpkg"
if [[ $FIX_MODE -eq 1 ]]; then
  CLANG_TIDY_ARGS="$CLANG_TIDY_ARGS --fix"
fi

RESULT=0
for file in $FILES; do
  echo "Analyzing: $file"
  if ! clang-tidy $CLANG_TIDY_ARGS "$file"; then
    RESULT=1
  fi
done

if [[ $RESULT -eq 0 ]]; then
  echo "✓ Static analysis complete - no issues found"
else
  echo "✗ Static analysis found issues"
  if [[ $FIX_MODE -eq 0 ]]; then
    echo "Run './scripts/lint.sh --fix' to automatically fix some issues"
  fi
  exit 1
fi
