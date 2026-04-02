# AI Workflow

## Purpose

This document exists to reduce repeated repository rediscovery when using Codex or other AI tools.

## Recommended Session Start

For a new AI session, point the tool at these files first:

1. `AGENTS.md`
2. `README.md`
3. `docs/PROJECT_CONTEXT.md`
4. `docs/ROADMAP.md`

Only after that should it inspect code relevant to the requested task.

## Recommended Prompt

```text
Read AGENTS.md, README.md, and docs/PROJECT_CONTEXT.md first. Use them as the default repository context. Do not rescan the full repo unless the task requires deeper code inspection. Distinguish clearly between current implementation and planned features, and update docs when the implementation meaningfully changes.
```

## When A Full Rescan Is Actually Warranted

- A task touches code outside the documented current architecture
- The docs are visibly stale relative to the code
- New modules, libraries, or hardware integrations have been added
- The task requires exact line-level understanding of implementation details

## Documentation Maintenance Rule

If the project changes in a way that alters architecture, hardware assumptions, or major goals, update:

- `AGENTS.md`
- `docs/PROJECT_CONTEXT.md`
- `docs/ROADMAP.md`

Those three files form the core AI handoff set.
