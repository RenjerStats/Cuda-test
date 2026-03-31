---
name: nir-report-sync
description: Keep engineering outputs aligned with the NIR materials in plan/. Use when a code change, benchmark result, experiment table, architecture note, or acceptance claim must be checked against the thesis wording, metrics, and declared scope.
---

# Nir Report Sync

## Overview

Use this skill to keep code, benchmark outputs, and research wording consistent with the academic plan.

## Workflow

1. Read the relevant sections in the explanatory note under `plan/`.
2. Read the NIR assignment note under `plan/` for the assignment framing.
3. Compare the current engineering artifact with the planned terminology, metrics, and scope.
4. Flag drift in names, acceptance claims, experiment counts, or reported metrics.
5. Update the engineering-side artifact or log a follow-up if the mismatch is intentional.

## Default Checks

- Match module names with the planned architecture.
- Match metrics and experiment counts with the stated methodology.
- Match acceptance claims with actual test and report evidence.

## References

Read `references/nir-alignment-rules.md` for the default alignment checklist.
