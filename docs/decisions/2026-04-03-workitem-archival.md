# Decision: Archive Accepted Work Items

- Date: 2026-04-03
- Related commit: `7460bc1` (`impl: archive completed workitems`)

## Context

After Phase 1-7 completion, `workitems/active/` contained only accepted packets. That blurred the distinction between in-flight work and historical task records and made agent instructions ambiguous after archival.

## Decision

- Use `workitems/active/` only for tasks that are currently moving through `spec -> tests -> implementation -> review`.
- After review acceptance and the final follow-up commit, move the packet to `workitems/archive/<task-name>/`.
- Treat archived packets as the canonical task history for completed work.

## Consequences

- Agent and workflow documentation must mention both `active` and `archive`.
- Archived packets should keep self-references consistent with their archived location.
- New tasks still start in `workitems/active/`.
