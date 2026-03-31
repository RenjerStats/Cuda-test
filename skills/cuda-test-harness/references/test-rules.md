# CUDA Test Rules

## Required Scenario Set

- Happy path on representative data
- Smallest meaningful input
- Boundary size or shape
- At least one invalid or unsupported input scenario when relevant
- At least one scenario that checks launch configuration handling when relevant

## Validation Rules

- Prefer a host-side reference implementation.
- State the floating-point tolerance explicitly.
- Check CUDA status after kernel launch and synchronization.
- Keep input generation deterministic.

## Fixture Rules

- Place reusable fixture builders in `tests/fixtures/`.
- Prefer fixtures that can be reused across correctness and benchmark work.
- Keep the smallest fixture easy to reason about by inspection.
