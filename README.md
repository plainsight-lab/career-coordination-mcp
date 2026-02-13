# career-coordination-mcp

Deterministic, auditable job-search coordination scaffolding in C++20.

## Scope (v0.1 scaffold)

This repository provides clean boundaries and stubs for:
- Experience atoms
- Opportunities and requirements
- Contacts and interactions
- Resume composition artifacts
- Constitutional validation engine
- Deterministic matching and append-only audit logging

LLM use is optional and out of authority scope. Canonical decisions are intended to remain deterministic and explainable.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Run CLI

```bash
./build/apps/ccmcp_cli/ccmcp_cli
```
