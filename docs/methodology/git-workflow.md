# Local Git Workflow

## Repository Rules

- Work in a local git repository even without a remote.
- Keep `main` as the integration branch.
- Use one branch per work item: `task/<short-name>`.

## Stage-Aligned Commits

- `spec:` approved feature packet
- `test:` test contract or test skeleton
- `impl:` implementation work
- `review:` fixes after Claude findings

## Review Surface

- Use `git diff main...task/<short-name>` for branch review.
- Use `git diff --staged` for staged review.
- Reference the reviewed diff in `03-review-report.md`.

## Milestones

Create annotated local tags for meaningful checkpoints, for example:

- `mvp-scaffold`
- `kernel-test-v1`
- `autotune-v1`

## Decision Notes

When a process or architecture decision affects multiple tasks, create a note in
`docs/decisions/` and include the related commit hash in the note.
