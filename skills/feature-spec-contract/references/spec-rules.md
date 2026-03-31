# Feature Spec Rules

## Packet Rules

- Use the template from `workitems/templates/00-feature-packet.md`.
- Keep the packet decision-complete for the implementer.
- Name the task folder with a short, stable slug.
- Link the packet to the relevant sections in `plan/`.

## What To Lock Before Coding

- Objective and non-goals
- Affected modules and boundaries
- Public API impact
- Acceptance criteria
- Risks and assumptions

## Test Contract Rules

- Use `workitems/templates/01-test-contract.md`.
- Include correctness, edge, boundary, and negative scenarios.
- Define the numerical tolerance strategy for floating-point work.
- Define the host-side reference check whenever kernel output is non-trivial.
- Define performance evidence separately from correctness evidence.

## Review Gate

- Send the packet and test contract to the reviewer before implementation.
- Reopen the packet if scope or interfaces change during implementation.
