#!/usr/bin/env bash
# Install git pre-commit hooks for the project
# This script copies hooks from scripts/hooks/ to .git/hooks/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

echo "Installing git hooks..."

# Create pre-commit hook
cat > "$HOOKS_DIR/pre-commit" << 'EOF'
#!/usr/bin/env bash
# Pre-commit hook: Auto-format staged C++ files with clang-format
# This ensures all commits follow the project's formatting standards

set -euo pipefail

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
  echo "⚠️  Warning: clang-format not found. Skipping format check."
  echo "   Install with: brew install llvm (macOS) or apt install clang-format (Linux)"
  exit 0
fi

# Get list of staged C++ files
STAGED_CPP_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|h|hpp)$' || true)

if [[ -z "$STAGED_CPP_FILES" ]]; then
  # No C++ files staged, nothing to format
  exit 0
fi

echo "🔧 Formatting staged C++ files..."

# Format each staged file
FORMATTED_COUNT=0
for file in $STAGED_CPP_FILES; do
  # Check if file exists (might be deleted in working tree)
  if [[ -f "$file" ]]; then
    echo "  ├─ $file"

    # Format the file in-place
    clang-format -i "$file"

    # Re-add the formatted file to staging area
    git add "$file"

    FORMATTED_COUNT=$((FORMATTED_COUNT + 1))
  fi
done

if [[ $FORMATTED_COUNT -gt 0 ]]; then
  echo "✓ Formatted $FORMATTED_COUNT file(s) and re-staged them"
else
  echo "✓ No files needed formatting"
fi

exit 0
EOF

chmod +x "$HOOKS_DIR/pre-commit"

echo "✓ Pre-commit hook installed at $HOOKS_DIR/pre-commit"
echo ""
echo "The hook will automatically format C++ files before each commit."
echo "To bypass the hook (not recommended): git commit --no-verify"
