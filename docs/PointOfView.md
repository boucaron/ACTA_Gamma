# ACTA Gamma — Point of View

**LLMs as actions, not agents.**

## The idea

ACTA Gamma starts from a simple premise:

> **An LLM should be treated as an action inside a controlled software flow, not as the controller of the flow.**

Instead of building an autonomous agent that maintains context, decides what to do next, delegates tasks, and accumulates state, ACTA Gamma keeps the surrounding system deterministic.

The engine decides:

* what context is provided;
* which skill is executed;
* which model performs the action;
* how the result is validated;
* what is persisted;
* and what happens next.

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

The engine does not need to understand the domain.

## Models are replaceable

The model is another independent dimension of an execution.

```text
Context × Skill Revision × Model
```

The initial implementation can use llama.cpp through its OpenAI-compatible API, but the engine should not depend on llama.cpp itself.

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

Once executions are recorded, regression testing becomes a natural extension.

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

This makes it possible to measure changes in:

* detection;
* false positives;
* severity;
* consistency;
* latency;
* token usage;
* cost;
* and other application-specific metrics.

## Composition belongs outside the LLM

ACTA Gamma intentionally keeps orchestration outside the model.

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

those are separate executions controlled by the application or a higher-level workflow.

The individual skills do not delegate to one another.

This keeps each operation small, understandable, testable, and independently replaceable.

Workflow composition is built **on top of** this primitive, not inside it: a
higher-level program can chain the actions — security → correctness → test
analysis → final report — using each execution's result as the next context, while
the engine itself stays a single, replayable, auditable building block.

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

The engine remains the same:

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

> **Deterministic orchestration, probabilistic actions.**

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
