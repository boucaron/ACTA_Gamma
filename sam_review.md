\# Review: ACTA Gamma



\## The short version



The core idea is sound and the constraints are well-chosen: one deterministic LLM call, immutable inputs, versioned revisions, full audit trail. That's a real problem (reproducibility in LLM workflows) and the solution shape is correct. I'm not going to argue with the architecture.



What I \*will\* push back on is the \*\*build-out relative to the stage\*\*. This is labelled "early prototype / POC," but it already has four compiled components, three test suites, an e2e suite, a Qt 6 GUI with in-process runner, cancel handling, live status polling, a stale-execution sweeper, and a top-level wrapper Makefile. That's not a POC. That's a product with a README.



\---



\## What's actually justified



\- \*\*Immutable revisions via DB trigger.\*\* For a C/SQLite codebase, letting the trigger handle "on parent update, insert a new revision row" is \*simpler\* than coordinating that in application code. You avoid a race window. Good call. The "latest is current, no promote/deprecate" rule is also right — it removes a whole state machine you don't need.



\- \*\*Replay exactness via four immutable inputs\*\* (context, skill revision, model revision, execution prompt). Clean. The `execution.prompt + "\\n\\n" + context.content` assembly is explicit and auditable. No magic.



\- \*\*Optimistic claim\*\* (`UPDATE … WHERE status = 'pending'`, check affected rows). Minimal, correct, no locking overhead. Right size for the problem.



\- \*\*WAL journal mode.\*\* Correct default for a SQLite app where multiple processes might read while one writes.



\- \*\*No automatic retries.\*\* The README is explicit about \*why\*: single-node, transient failures are rare, manual reset is simpler to reason about, avoids retry storms. That's a good "not yet" decision. Don't add it until you hit it.



\- \*\*Honest "Not yet implemented" section.\*\* Streaming, retries, benchmarking infrastructure — none of these are built, and the README says so. Good.



\---



\## Where I'd push back



\### 1. Four components at POC stage



You have:



| Component | Role |

|---|---|

| `acta\_db/` | C persistence library |

| `acta\_db\_cli/` | CLI over the library |

| `acta\_runner/` | Standalone runner |

| `acta\_gamma/` | Qt 6 GUI with in-process runner |



Plus three unit-test suites and an e2e suite. For a POC whose job is "does the deterministic-call pattern actually work end-to-end?" that's a lot of surface. A \*\*single C program\*\* with embedded SQLite + curl that does create → run → inspect in one binary would answer the POC question in a fraction of the code. The library/CLI/runner/GUI split is a \*production\* concern. You can refactor toward it later. Building all four now means four build targets, four Makefiles, four dependency-resolution paths (cJSON, curl, Qt, SQLite), and four places a bug can live — all before you've confirmed the core loop is right.



\*\*Do you actually need the GUI right now?\*\* The README says the GUI runs the runner "in-process" with "live status polling" and a "Cancel" toggle. That's a non-trivial amount of Qt code (worker thread, shared DB connection, cooperative cancellation, button-state management) for a POC. A CLI loop is sufficient to validate the pipeline.



\### 2. "Benchmarkable" is in the README but not in the code



The core-ideas list says:



> \*\*Benchmarkable\*\* — compare models and skill revisions against the same datasets.



But there is no dataset entity, no batch execution, no comparison/report output, no way to say "run skill@3 against model@5 over these 40 contexts and produce a score table." The building blocks (versioned skills, model records, executions) are there, but the \*benchmark workflow\* isn't. Calling it a feature in the README at POC stage is aspirational. Either cut the bullet or mark it "planned" so a reader doesn't think it exists.



\### 3. "Generic" is a tautology



> \*\*Generic\*\* — suitable for review, analysis, classification, extraction, auditing, and similar tasks.



That's what you get if you have a prompt template + a context + a model and you make one API call. It's not a differentiator; it's a description of the interface. Every single-call LLM wrapper is "generic." Remove the bullet or replace it with something specific: "the skill schema supports a JSON output contract with optional validation" — that's the actual generic capability.



\### 4. The execution\_log phase rows — necessary?



The runner records a log row at each phase: claim, resolve, preflight, chat call, record, complete/fail. Six writes per execution, plus the execution row itself, plus the context and revision reads. For a POC where you're running one thing at a time and watching terminal output, is the per-phase DB log row earning its keep? It's useful for the audit trail in production, but at POC stage it's writing complexity into the happy path. I'd defer it until you need to debug a multi-process run or build the audit UI. (The GUI's "review executions" feature is where it becomes valuable — but see point 1: do you need the GUI yet?)



\### 5. The philosophy section does more work than the code



> \*\*The engine decides what happens. The LLM only does the work it's asked to do.\*\*



This is true, but it also describes \*every\* non-agent LLM call. Calling the OpenAI API with a fixed prompt is always "the engine decides." The README spends significant space distinguishing from "agents" — "The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next" — when the technical content is "we make one HTTP POST and store the result." The positioning is fine as a tagline, but the repeated emphasis ("stateless," "immutable," "no agent," "not an agent framework") is doing marketing work that the code doesn't require. A reader who looks at the architecture diagram already sees there's no orchestration loop. You can cut the philosophy to one sentence and let the diagram speak.



\### 6. C is a heavy choice for this problem



I'll grant you: the payoff is a small static binary, no runtime, easy deployment to constrained environments, and no dependency on a language runtime. Those are real benefits.



But for a POC whose entire data flow is "read four rows, assemble a string, POST to an HTTP endpoint, store the response," C means you're hand-writing:

\- SQLite wrapper (connection, prepared statements, error handling, WAL setup)

\- HTTP via curl (easy, but still manual memory management for response buffers)

\- JSON parsing via cJSON (string extraction, error paths)

\- Process lifecycle for the runner (claim, signal handling for the in-GUI case)

\- Test harnesses in C



A single Python file would do the same thing in \~300 lines with `requests`, `sqlite3`, and `json`. You'd be \*in production\* on the core loop in an afternoon and then decide whether the C implementation is worth it. I'm not saying "use Python forever." I'm saying: at POC stage, the language choice is adding cost without yet having proven the design. If the design holds, the C rewrite is straightforward. If it doesn't, you saved a lot of C debugging.



(If the C choice is driven by a specific deployment constraint — e.g., "must run on an embedded Linux box with no runtime" — then it's justified and I retract this point. The README doesn't mention that constraint, though.)



\### 7. The `sweep` command



Cleaning up stale `running` rows after a dead process is a real production concern. But the README says "sequential use is the normal pattern." If one process is the norm, the stale-row case is "you crashed the runner mid-call." A single `actagamma\_db exec reset --stale` in the CLI would handle that without a dedicated `sweep` subcommand, a `--stale-seconds` parameter, a positive-integer validation, a fallback chain of timestamps (`latest of newest execution\_log.created\_at and started\_at, falling back to created\_at`), and a skip-if-changed guard. For a POC, the simpler tool is enough. The full sweep logic is correct but is solving a problem you don't have yet.



\---



\## What I'd do if I were shipping this



1\. \*\*Cut to one binary for the POC.\*\* A single `actagamma` C (or Python) program: `init`, `skill create`, `model create`, `context create`, `exec create`, `run`, `get`, `log`. No library, no separate CLI, no GUI, no standalone runner. Get the core loop working end-to-end.



2\. \*\*Defer the GUI\*\* until the CLI pipeline is stable and you have users who need visual review. The "Run button with cancel and live polling" is a real feature, but it's a \*product\* feature, not a \*POC\* feature.



3\. \*\*Defer `sweep`\*\* until you actually run multiple processes or hit a crash-mid-run. A manual reset covers the POC case.



4\. \*\*Drop or mark "benchmarkable" and "generic" as planned.\*\* They're not built. A reader looking for benchmarking infrastructure will be confused.



5\. \*\*Keep the philosophy to one line.\*\* "One deterministic LLM call with versioned, immutable inputs. No agent, no state, no orchestration." Done. The architecture diagram already communicates the rest.



6\. \*\*Keep everything else.\*\* The revision model, the replay contract, the optimistic claim, the WAL choice, the "no automatic retries" decision, the honest "not yet" list — those are the actual content, and they're right.



\---



\## Bottom line



The design is good and the constraints are well-reasoned. The main issue is that the \*implementation scope\* has outrun the \*validation stage\*. You've built four components, three test suites, a GUI with threading and cancellation, and a stale-recovery utility — for a project whose README says "early prototype / POC." The core value (reproducible, auditable, single LLM calls with versioned inputs) could be validated with a fraction of that surface. Get the loop working in one binary, prove the replay is exact, \*then\* split into the library/CLI/runner/GUI architecture you've already designed. The design is there; the POC just doesn't need all of it yet.

