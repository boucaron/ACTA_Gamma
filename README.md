# ACTA Gamma

**LLMs as actions, not agents.**

A small, stateless LLM execution engine for versioned skills, reproducible analysis, and model benchmarking.

## What is ACTA Gamma?

ACTA Gamma treats an LLM as a single controlled action in a larger deterministic flow.

```text
Context + Skill + Model
              │
              ▼
         One LLM Call
              │
              ▼
         Observation
```

The engine controls the execution. The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next.

## Core ideas

* **Stateless** — every execution is independent and one-shot.
* **Versioned skills** — prompts and output schemas are revisioned.
* **Immutable contexts** — the exact input can be retained for replay.
* **Model independent** — use llama.cpp, cloud models, or other OpenAI-compatible backends.
* **Auditable** — executions retain prompts, raw responses, results, errors, and execution events.
* **Replayable** — run the same context and skill against another model or revision.
* **Benchmarkable** — compare models and skill revisions against the same datasets.
* **Generic** — suitable for review, analysis, classification, extraction, auditing, and similar tasks.

## Architecture

```text
                 ┌──────────────┐
                 │    Context   │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │ Skill @ N    │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │  Model @ N   │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │  Execution   │
                 └──────┬───────┘
                        │
             ┌──────────┴──────────┐
             ▼                     ▼
        Observation              Audit
```

## Initial implementation

The prototype is intentionally small:

* Python
* SQLite
* PySide6
* OpenAI-compatible LLM API
* llama.cpp as the initial local backend

## Status

Early prototype / POC.

The initial goal is to validate the execution model, skill versioning, auditability, replay, and model comparison before adding more infrastructure.

## Philosophy

> **The engine decides what happens. The LLM only does the work it's asked to do.**

ACTA Gamma is deliberately not an agent framework. It provides controlled, observable LLM actions that can be composed and evaluated by software outside the model.

## License

BSD Zero Clause License (BSD-0-Clause).
