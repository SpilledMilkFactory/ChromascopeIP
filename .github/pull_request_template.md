## Linked issue and milestone

<!-- Replace the placeholder with `Closes #123` when the PR completes that issue. -->
- Issue: <!-- #123; or explain why no issue exists -->
- Milestone: <!-- M1 — Repository Governance & CI Foundation -->

## What I changed

<!-- Explain the change, design decisions, and boundaries. Include relevant paths. -->

## How I verified it

<!-- Provide exact commands, environment, and outcomes. Mark only checks actually performed. -->

- [ ] I ran `cmake` configuration and built the affected targets successfully.
- [ ] I ran `ctest --test-dir build --output-on-failure` and recorded the result.
- [ ] I performed relevant manual GUI, CLI, media, or hardware checks.
- [ ] I verified the change on affected platforms (or listed outstanding platforms).
- [ ] This PR changes only documentation/templates; no native rebuild was needed.

**Evidence (commands, test totals, screenshots, logs, or CI run links):**

<!-- Do not claim a pass until a command was actually run. -->

## Quality and release considerations

- [ ] I added or updated regression tests where behavior changed, or explained why not.
- [ ] I updated user-facing documentation and verification notes where applicable.
- [ ] I checked input handling, privacy/security risks, licenses, and new dependencies as applicable.
- [ ] I reviewed `git diff --check` and ensured build artifacts, secrets, and private media are not included.

**Not applicable or deferred items:**

<!-- Explain unchecked items, including incomplete platform or hardware verification. -->

## Review and merge

<!-- I keep this in the Project's In Review column until required checks/review pass. -->
- [ ] I confirmed required GitHub Actions checks passed (once configured).
- [ ] I confirmed the issue's acceptance criteria are met.
