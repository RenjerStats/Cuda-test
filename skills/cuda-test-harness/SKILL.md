---
name: cuda-test-harness
description: Design isolated CUDA kernel tests, fixtures, and validation policy for this repository. Use when adding or revising unit or integration tests for kernels, host reference checks, fixture generation, edge cases, or numerical tolerance rules.
---

# Cuda Test Harness

## Overview

Design tests that validate kernel behavior in isolation, not only through a larger application flow.

## Workflow

1. Read the active feature packet and test contract first.
2. Identify the kernel inputs, outputs, launch configuration inputs, and failure modes.
3. Define a deterministic host-side reference for the expected result.
4. Choose fixture sizes that cover small, boundary, and representative workloads.
5. Define the comparison policy: exact equality, tolerance-based comparison, or structured validation.
6. Capture invalid configuration and invalid input cases when the interface allows them.
7. Keep test setup reusable through shared fixtures under `tests/fixtures/`.

## Default Expectations

- Validate correctness before benchmark or autotune work.
- Keep tests reproducible with fixed seeds and fixed fixture generation.
- Prefer fixtures that are cheap to inspect on the host.

## References

Read `references/test-rules.md` for the required scenario set and fixture guidelines.
