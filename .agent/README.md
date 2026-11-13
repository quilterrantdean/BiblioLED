# Agent workspace

This directory contains project context intended to help coding agents and future contributors continue work without relying on the original conversation.

The repository-root `AGENTS.md` is the agent instruction entry point. It directs agents to these notes.

## Files

- `plan.md` — project phases, current status, and next work.
- `decisions.md` — user-approved product, technical, and workflow decisions.
- `architecture.md` — proposed component boundaries and portable directory layout.
- `codebase-review.md` — factual review of the existing implementation and known issues.

## Working rules

- Treat user-approved decisions as requirements unless the user changes them.
- Distinguish confirmed decisions from provisional recommendations and unresolved questions.
- Update these notes when implementation changes the architecture or resolves an open question.
- Keep notes concise, factual, and useful to another agent.
- Do not store credentials, tokens, private user data, database contents, or other secrets here.
- All development changes belong on the `development` branch. Never commit directly to `main` or `master`.

These files are project documentation, not application runtime data.
