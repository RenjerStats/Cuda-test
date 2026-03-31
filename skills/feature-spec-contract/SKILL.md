---
name: feature-spec-contract
description: Create implementation-ready feature packets and test contracts for this CUDA testing library. Use when a new feature, public API change, module boundary change, experimental method, or significant refactor must be specified before coding or review.
---

# Feature Spec Contract

## Overview

Turn a raw request into the two required stage-gate documents for this repository:
`00-feature-packet.md` and `01-test-contract.md`.

Read `AGENTS.md`, then read only the relevant sections of `plan/` before drafting the packet.

## Workflow

1. Create a work item folder under `workitems/active/<task-name>/`.
2. Draft `00-feature-packet.md` from `workitems/templates/00-feature-packet.md`.
3. Fix the objective, scope, non-goals, affected modules, interface notes, and acceptance criteria.
4. Draft `01-test-contract.md` from `workitems/templates/01-test-contract.md`.
5. Define correctness scenarios before any implementation begins.
6. Define performance scenarios only after correctness checks are clear.
7. Hand the packet and contract to the reviewer before coding.

## Required Outputs

- One approved feature packet.
- One approved test contract.
- Explicit acceptance criteria that can later be checked from code, tests, and reports.

## References

Read `references/spec-rules.md` when you need the exact packet fields, review gate, or default assumptions.
