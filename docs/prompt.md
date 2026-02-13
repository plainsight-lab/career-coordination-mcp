You are a senior C++20 engineer and repo scaffolding assistant. Starting from the CURRENT working directory which contains ONLY a /docs folder with documentation, scaffold a new project named `career-coordination-mcp` focused ONLY on deterministic, LLM-optional job search coordination.

GOAL
Create the directory structure, build system, and stub domain entities (headers + minimal implementations) needed to begin work. Do NOT implement full behavior yet—only stubs, types, and clean boundaries.

CONSTRAINTS
- C++20
- Modern CMake
- vcpkg for dependencies
- Coroutines will be used later for MCP handling, but do not implement MCP server yet.
- Focus only on job search coordination (atoms, opportunities, contacts, interactions, resume composition, validation engine).
- Add strong “deterministic + auditable” posture in docs and code comments.
- Output should be a repo ready to build with placeholder tests.

STRUCTURE TO CREATE
Create this folder layout under the current directory:


├── README.md
├── LICENSE                 # Apache 2.0 full text
├── NOTICE
├── .editorconfig
├── .gitignore
├── CMakeLists.txt
├── vcpkg.json
├── cmake/
│   └── toolchains/         # placeholder
├── include/
│   └── ccmcp/
│       ├── core/
│       │   ├── types.h
│       │   ├── result.h
│       │   ├── ids.h
│       │   ├── time.h
│       │   └── hashing.h
│       ├── domain/
│       │   ├── experience_atom.h
│       │   ├── opportunity.h
│       │   ├── requirement.h
│       │   ├── contact.h
│       │   ├── interaction.h
│       │   ├── resume.h
│       │   └── match_report.h
│       ├── constitution/
│       │   ├── constitution.h
│       │   ├── rule.h
│       │   ├── finding.h
│       │   ├── validation_report.h
│       │   └── validation_engine.h
│       ├── storage/
│       │   ├── audit_event.h
│       │   ├── audit_log.h
│       │   └── repositories.h
│       └── matching/
│           ├── scorer.h
│           ├── matcher.h
│           └── presets.h
├── src/
│   ├── core/
│   │   └── hashing.cpp
│   ├── domain/
│   │   ├── experience_atom.cpp
│   │   ├── opportunity.cpp
│   │   ├── contact.cpp
│   │   ├── interaction.cpp
│   │   ├── resume.cpp
│   │   └── match_report.cpp
│   ├── constitution/
│   │   └── validation_engine.cpp
│   ├── storage/
│   │   └── audit_log.cpp
│   └── matching/
│       └── matcher.cpp
├── apps/
│   └── ccmcp_cli/
│       ├── CMakeLists.txt
│       └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_ids.cpp
│   ├── test_validation_engine.cpp
│   └── test_matching_stub.cpp

**KEEP the docs directory. Do not move or otherwise modify it**


DEPENDENCIES (vcpkg.json)
Add vcpkg manifest with these baseline dependencies:
- nlohmann-json
- fmt
- Catch2 (for tests)
(Do NOT add Redis or LanceDB yet. We’ll add later.)

BUILD SYSTEM (CMake)
- Top-level CMakeLists defines:
  - library target `ccmcp` from src/**
  - include directories include/
  - C++20 standard
  - warnings on (MSVC/GCC/Clang)
- `apps/ccmcp_cli` builds an executable linking to `ccmcp`
- `tests/` builds tests via Catch2 and registers with CTest

CODE STUB REQUIREMENTS
Implement minimal, compilable stubs for the domain and engine pieces:

1) IDs:
- Strong types for AtomId, OpportunityId, ContactId, InteractionId, ResumeId, TraceId using std::string or UUID strings.
- Provide simple `new_*_id()` functions (deterministic UUID not required yet; can be placeholder random-like with timestamp + counter, but keep it isolated so it can be replaced later).

2) Result type:
- `Result<T, E>` minimal variant-based.
- Define a few error enums: GovernanceError, StorageError, ParseError.

3) Domain entities:
Create header + .cpp for:
- ExperienceAtom (immutable-ish fields; verify() stub returns bool)
- Requirement
- Opportunity (contains vector<Requirement>)
- Contact (identity keys; relationship_state enum)
- Interaction (state enum + transition validation stub)
- Resume (selected atom ids, rendered_output string, validation report)
- MatchReport (score breakdown placeholders)

4) Validation Engine (CVE):
- Define Rule, Finding (severity enum), Constitution, ValidationReport
- ValidationEngine::validate(ArtifactEnvelope, ValidationContext) returns ValidationReport
- Include a single example rule: “No ungrounded claims” that currently always passes (stub), but the structure must exist.

5) Audit Log:
- Append-only audit log interface with `append(event)` and `query(...)` stubs.
- Store events in-memory for now (vector), but keep interface so it can be swapped to SQLite later.

6) Matcher:
- `Matcher::evaluate(Opportunity, vector<ExperienceAtom>) -> MatchReport`
- Return a stub report with deterministic scoring placeholder (e.g., score = 0.0) but structure present.

7) CLI:
Create a minimal CLI that:
- Prints version banner “career-coordination-mcp v0.1”
- Loads a tiny hardcoded Opportunity and 2 ExperienceAtoms
- Runs matcher and prints JSON of MatchReport using nlohmann-json
(Use placeholder serialization; minimal fields only.)


GIT
- Initialize git repo in career-coordination-mcp
- Create initial commit “Initial scaffold”

OUTPUT
After generating:
- Print the tree of career-coordination-mcp
- Print exact build commands using CMake preset or standard out-of-source build:
  - cmake -S . -B build
  - cmake --build build
  - ctest --test-dir build
- Print how to run CLI.

Now execute these steps in the filesystem.