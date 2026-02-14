# Code Linting and Formatting

This document describes the linting and formatting tools used in career-coordination-mcp to enforce code quality and consistency.

---

## Tools

### clang-format

**Purpose:** Automatic code formatting for consistent style.

**Configuration:** `.clang-format` (based on Google Style Guide with project adjustments)

**Key Rules:**
- 2-space indentation
- 100-character line limit
- Left-aligned pointers and references (`int* ptr` not `int *ptr`)
- Sorted includes with project headers first
- Attached braces (K&R style)

### clang-tidy

**Purpose:** Static analysis to catch bugs and enforce C++ Core Guidelines.

**Configuration:** `.clang-tidy`

**Check Categories:**
- `bugprone-*` — Detect common programming errors
- `cppcoreguidelines-*` — Enforce C++ Core Guidelines
- `modernize-*` — Suggest modern C++20 idioms
- `performance-*` — Detect performance issues
- `readability-*` — Improve code readability
- `concurrency-*` — Detect concurrency bugs

**Naming Conventions Enforced:**
- Namespaces: `lower_case` (e.g., `ccmcp::domain`)
- Classes/Structs/Enums: `CamelCase` (e.g., `ValidationEngine`)
- Functions/Variables: `lower_case` (e.g., `validate()`, `artifact_id`)
- Member variables: `lower_case_` with trailing underscore (e.g., `constitution_`)
- Constants/Enum values: `kCamelCase` with k prefix (e.g., `kAccepted`)

---

## Usage

### Prerequisites

Install clang-format and clang-tidy (version 14 or later):

```bash
# macOS
brew install llvm

# Ubuntu/Debian
sudo apt install clang-format clang-tidy

# Verify installation
clang-format --version
clang-tidy --version
```

### Format Code

**Automatic formatting (modifies files in-place):**
```bash
./scripts/format.sh
```

**Check formatting without modifying files:**
```bash
./scripts/format.sh --check
```

**Format a single file:**
```bash
clang-format -i include/ccmcp/domain/resume.h
```

### Run Static Analysis

**Run clang-tidy (requires prior build):**
```bash
# First, ensure compile_commands.json exists
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run static analysis
./scripts/lint.sh
```

**Run with automatic fixes (use with caution):**
```bash
./scripts/lint.sh --fix
```

**Analyze a single file:**
```bash
clang-tidy -p build-vcpkg src/domain/resume.cpp
```

---

## IDE Integration

### Visual Studio Code

Install extensions:
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
- [clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)

Configure in `.vscode/settings.json`:
```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build-vcpkg",
    "--clang-tidy"
  ],
  "editor.formatOnSave": true,
  "[cpp]": {
    "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"
  }
}
```

### CLion

CLion has built-in clang-format and clang-tidy support:
1. Preferences → Editor → Code Style → C/C++ → Set from... → Predefined Style → .clang-format file
2. Preferences → Editor → Inspections → C/C++ → Clangd → Enable Clang-Tidy
3. Set compilation database path to `build-vcpkg/compile_commands.json`

---

## CI Integration

### GitHub Actions

Add to `.github/workflows/lint.yml`:

```yaml
name: Lint

on: [push, pull_request]

jobs:
  format-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-format
        run: sudo apt install clang-format
      - name: Check formatting
        run: ./scripts/format.sh --check

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt install clang-tidy cmake ninja-build
      - name: Configure
        run: |
          cmake -S . -B build \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -G Ninja
      - name: Run clang-tidy
        run: ./scripts/lint.sh
```

---

## Pre-Commit Hooks

The project includes a pre-commit hook that automatically formats C++ files before each commit.

### Installation

The hook is already installed in this repository. For new clones or to reinstall:

```bash
./scripts/install-hooks.sh
```

### How It Works

When you run `git commit`, the hook will:
1. Find all staged C++ files (`.cpp`, `.h`, `.hpp`)
2. Run `clang-format -i` on each file
3. Re-add the formatted files to the staging area
4. Proceed with the commit

**Note:** If `clang-format` is not installed, the hook will warn but allow the commit to proceed.

### Bypassing the Hook

To skip formatting for a specific commit (not recommended):

```bash
git commit --no-verify
```

### Testing the Hook

Create a test file with poor formatting:

```bash
# Create a poorly formatted file
cat > /tmp/test.cpp << 'EOF'
#include <string>
int main(){int x=5;return 0;}
EOF

# Stage it
git add /tmp/test.cpp

# Commit (hook will auto-format)
git commit -m "Test formatting hook"

# Check the result
cat /tmp/test.cpp  # Should be nicely formatted now
```

---

## Suppressing Warnings

### In Code (Use Sparingly)

Suppress specific warnings with NOLINT comments:

```cpp
// Suppress specific check
int* ptr = nullptr;  // NOLINT(readability-identifier-naming)

// Suppress all checks for a line
void legacy_function() {  // NOLINT
  // ...
}
```

### In Configuration

Disable checks globally by modifying `.clang-tidy`:

```yaml
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters  # Disable this check
```

---

## Rationale

### Why Linting?

Aligns with **C++ Core Guidelines P.12**: "Use supporting tools as appropriate"

Benefits:
- **Consistency:** All code follows the same style (human and AI authored)
- **Correctness:** Catch bugs early via static analysis
- **Maintainability:** Easier to read and review code
- **Determinism:** Formatting is not subjective; machines enforce it
- **Auditability:** Style violations are explicit, not implicit

### Why These Rules?

- **2-space indent:** Matches existing `.editorconfig` and Google Style Guide
- **100-char line limit:** Balance between readability and screen real estate
- **Snake_case for functions/variables:** Consistent with existing codebase
- **CamelCase for types:** Standard C++ convention
- **Trailing underscore for members:** Prevents name shadowing (C++ Core Guidelines C.131)
- **k prefix for constants:** Distinguishes constants from variables at a glance

---

## Troubleshooting

### "clang-format not found"

Ensure LLVM is installed and in PATH:
```bash
which clang-format
export PATH="/usr/local/opt/llvm/bin:$PATH"  # macOS Homebrew
```

### "compile_commands.json not found"

Run CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`:
```bash
cmake -S . -B build-vcpkg \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/vcpkg.cmake \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### "clang-tidy reports false positives"

Suppress specific checks with NOLINT comments or disable them in `.clang-tidy`. If a check is consistently unhelpful, propose disabling it globally via code review.

---

## References

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [clang-format documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [clang-tidy documentation](https://clang.llvm.org/extra/clang-tidy/)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
