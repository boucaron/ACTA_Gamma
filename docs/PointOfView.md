# ACTA Gamma — Point of View

**LLMs as actions, not agents.**

## The idea

ACTA Gamma starts from a simple premise:

> **An LLM should be treated as an action inside a controlled software flow, not as the controller of the flow.**

Instead of building an autonomous agent that maintains context, decides what to do next, delegates tasks, and accumulates state, ACTA Gamma keeps the surrounding system deterministic.

The surrounding program — and the person running it — decides:

* what context is provided;
* which skill is executed;
* which model performs the action;
* and what happens next.

The runner handles the rest deterministically:

* how the prompt is assembled and the call is issued;
* how the result is validated;
* what is persisted.

The LLM performs one task and returns an observation.

```text
Context
   +
Skill Revision
   +
Model
   │
   ▼
One-shot LLM action
   │
   ▼
Observation
```

Two needs, in order:

1. **Run one skill against one context once, and read the result.** That is the entire workflow — four creates, a run, a read.
2. **Reproduce that run later, or compare it against other skill revisions and models.** That is what the versioning, immutable contexts, and audit trail are for.

The POC serves both, but need 1 is complete on its own: with one revision of each entity (created automatically), the one-shot run depends on nothing else in the system.

## Why this matters

LLMs are changing extremely quickly. Prompts change, models change, inference engines change, and model behavior can change without the surrounding application changing.

Treating the LLM as an isolated action creates a useful boundary.

A skill can be revised without changing the model.

A model can be replaced without changing the skill.

A context can be replayed against both.

An execution can be recorded and inspected afterwards.

This turns LLM experimentation from something implicit and conversational into something that can be treated more like a normal software operation.

## Skills

A skill is a small, focused instruction describing one analysis operation.

For example:

```text
Identify authorization problems in the supplied implementation.

Only report concrete security issues.
Do not report style, architecture, or performance concerns.
Return observations using the defined output schema.
```

A skill is versioned.

```text
security@1
security@2
security@3
```

Existing revisions are never silently modified.

This makes changes to prompts observable and testable.

The same context can therefore be evaluated with:

```text
security@2 + model A
security@2 + model B
security@3 + model A
security@3 + model B
```

## Context is data, not memory

ACTA Gamma does not maintain conversational context.

An execution receives a complete context and operates on it once.

The context can be stored so that the exact same input can be replayed later.

This is important for experimentation:

> **If the input is reproducible, the execution can be reproduced as an experiment.**

The context does not need to be related to software development. It could be:

* source code;
* requirements;
* documents;
* logs;
* specifications;
* incident reports;
* test results;
* arbitrary structured data.

The runner does not need to understand the domain.

## Models are replaceable

The model is another independent dimension of an execution.

```text
Context × Skill Revision × Model
```

The initial implementation can use llama.cpp through its OpenAI-compatible API, but the runner should not depend on llama.cpp itself.

A local model can be replaced by another local model or a cloud model without changing the skill system.

This makes model upgrades much less disruptive.

Instead of asking whether a new model "seems better", existing contexts and skills can be replayed against it.

## Auditability

Every execution can record:

* context;
* skill revision;
* model configuration;
* resolved prompt;
* raw LLM response;
* validated result;
* errors;
* execution events;
* timing and other metadata.

The raw response is retained even when parsing or validation fails.

This means the system records not only successful analyses, but also failures and unexpected model behavior.

An execution becomes a concrete historical artifact rather than an ephemeral chat interaction.

## Regression testing and benchmarking

This is not our business; it is an example of what **can be built on top of**
the primitive once executions are recorded.

A dataset can contain known contexts and expected observations.

A new skill revision or model can then be run against the same dataset:

```text
                 Regression Dataset
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
       Model A        Model B        Model C
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                     Compare
```

A large cloud model can also be used as a reference or evaluator, without making it part of the production execution path.

Such a setup makes it possible to measure changes in:

* detection;
* false positives;
* severity;
* consistency;
* latency;
* token usage;
* cost;
* and other application-specific metrics.

## Composition belongs outside the LLM

ACTA Gamma intentionally keeps orchestration outside the model, and workflows
are not our business. ACTA Gamma is just a building block: one replayable,
auditable action. The following is an example of how a higher-level program
could **use that building block** to do what the runner does not do.

If an application wants to perform:

```text
security
   ↓
correctness
   ↓
test analysis
   ↓
final report
```

those are separate executions controlled by an application built on top of ACTA Gamma, not by ACTA Gamma itself.

The individual skills do not delegate to one another.

This keeps each operation small, understandable, testable, and independently replaceable.

Workflow composition is not implemented here: a
higher-level program can chain the building blocks — security → correctness →
test analysis → final report — using each execution's result as the next
context, while ACTA Gamma itself stays a single, replayable, auditable action.

## The broader idea

ACTA Gamma is not fundamentally a code-review tool.

Code review is simply one useful application.

The same primitive can support:

```text
Review
Analysis
Classification
Extraction
Auditing
Comparison
Verification
Evaluation
```

The primitive remains the same:

```text
Context + Skill + Model
          ↓
       One action
          ↓
      Observation
```

That is the core abstraction.

## Design principle

The central philosophy can be summarized as:

> **Deterministic flow, probabilistic actions.**

The software remains responsible for the process.

The LLM provides intelligence at precisely defined points in that process.

That separation is what makes the system controllable, auditable, replayable, and adaptable as models evolve.

## What ACTA Gamma is not

ACTA Gamma is deliberately not:

* an autonomous agent;
* a conversational memory system;
* a prompt-management SaaS;
* a model provider;
* a workflow engine;
* a replacement for application logic.

It is a small execution layer around a simple primitive:

> **Run this skill, against this context, using this model, once.**

Everything else can be built around that primitive.
