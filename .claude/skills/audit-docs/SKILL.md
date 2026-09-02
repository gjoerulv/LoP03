# Audit Documentation

Perform a source-of-truth audit against the exact repository state requested by the user.

## Before auditing

1. Read `README.md` and `CLAUDE.md`.
2. Verify the requested branch/commit with Git.
3. Record pre-existing working-tree changes.
4. Read all canonical documents listed by `README.md`.
5. Read a scope or roadmap only when the user or `README.md` explicitly designates it as active.
6. Treat an empty scope or roadmap as no approved plan, not as permission to invent one.
7. Read archived material only when design archaeology is required.

## Audit for

- stale implementation or milestone claims;
- conflicting authority or duplicated source-of-truth text;
- contradictory numeric limits, enum values, terminology, defaults, durations, or lifecycle rules;
- ambiguity about long-term design versus current executable capability;
- schema statements that disagree with validators, tests, or runtime behavior;
- unsupported content described as playable;
- implemented behavior still described as deferred;
- broken links, obsolete paths, or stale agent instructions;
- missing persistence, migration, determinism, validation, presentation, or AI implications;
- historical records being treated as current contracts.

Cross-check source, tests, validation, and shipped content when documentation alone cannot establish the truth.

## Output

Classify findings as:

- blocking contradiction;
- stale but non-blocking;
- ambiguity requiring a decision;
- deliberate long-term capability gap;
- historical-only issue.

For every material finding, name the exact files and explain the consequence. Do not silently choose one conflicting statement. Recommend the smallest coherent correction that restores a single source of truth.