\# Architecture Review: ACTA Gamma



\## The short version



This is a well-scoped POC with a strong architectural spine: \*\*one LLM call, fully specified by four immutable inputs, fully audited, exactly replayable.\*\* The "LLM as action, not agent" framing isn't just a tagline — it's enforced structurally by the data model (no session state, no multi-turn, no delegation), the execution pipeline (a single claim→call→record sequence), and the explicit refusal to add lifecycle state like `promote`/`deprecate`/`active`. The system will remain coherent as long as that refusal holds, and the README is unusually disciplined in stating the non-features.



\## What I'd call clean



\*\*The revision model is the right level of complexity.\*\* Auto-snapshot on parent create/update, immutable revision rows, "current" = latest, executions reference explicit revision IDs. No version-policy engine, no deprecation workflow, no "which revision is live" question. You point at an ID. That's the minimum that gives you replay and audit without a version-management subsystem. Resisting the urge to add lifecycle states here is the most important architectural decision in the document, and it's made explicitly.



\*\*The four-input replay invariant is airtight.\*\* Context (immutable), skill revision (immutable), model revision (immutable), execution prompt (stored on the execution record). The final user message is a deterministic concatenation. No hidden state, no server-side memory, no "the model remembers this from the last call." If you have those four, you can reproduce the observation. That's a strong property, and it falls out of the data model rather than being bolted on.



\*\*The component separation is honest.\*\* `acta\_db` is persistence. `acta\_db\_cli` is a thin CLI over that persistence. `acta\_runner` is the HTTP-and-state-machine layer that talks to OpenAI-compatible endpoints and drives the execution lifecycle. `acta\_gamma` is the GUI. Each has a clear responsibility, and the dependency graph is a simple tree: GUI → runner → DB; CLI → DB; runner → DB. No cycles, no shared "service layer" that will become a kitchen sink.



\*\*The concurrency model is right-sized.\*\* SQLite in WAL mode, optimistic claim via `UPDATE … WHERE status = 'pending'` with an affected-rows check, sequential use as the normal pattern. No distributed coordination, no message queue, no actor model. For a single-node engine calling HTTP backends, this is exactly the right amount of machinery. The `sweep` mechanism for dead-runner cleanup is a practical, well-specified defensive addition that doesn't complicate the happy path.



\*\*The non-features are stated with reasoning.\*\* No streaming, no automatic retries, no agent orchestration. Each is marked as deliberate, not deferred-accidentally. "Transient backend failures are rare in the current single-node deployment, and a manual reset is simpler to reason about and avoids retry storms" — that's a defensible engineering judgment, not an excuse.



\## What I'd watch as the system grows



\*\*Runner code shared by source inclusion, not by a library.\*\* The README says `run.c`/`backend.c` are "compiled into the GUI" rather than linked from a shared `libacta\_runner`. At POC stage this is fine and arguably simpler than maintaining a fourth artifact. But the moment the runner pipeline gains complexity — streaming, retries, multiple backend protocols, output-schema validation with richer error handling — you'll have the same C source in two build trees (the `acta\_runner` Makefile and the `acta\_gamma` qmake project), and a change to one that isn't mirrored in the other will produce silent divergence. The fix when you get there is small: extract a `libacta\_runner.a` (or `.so`) that both the standalone binary and the GUI link against, and delete the source-level inclusion from the GUI. I'd do this before adding streaming or retries, because those are the changes that will touch `run.c` and `backend.c` most.



\*\*The model configuration JSON blob is a pragmatic placeholder.\*\* A model record stores `backend`, `base\_url`, `model\_identifier`, and a JSON blob with a handful of known keys (`api\_key`, `temperature`, `max\_tokens`, `top\_k`, `supports\_response\_format`). Unknown keys are warned and ignored; a malformed blob is treated as empty. This works for the current single-endpoint, single-auth-key model. If you ever need per-model endpoint pools, load-balancing, multiple auth schemes, or backend-specific parameters that aren't just temperature/max\_tokens, the blob will need a schema or a typed struct. I wouldn't redesign it now, but I'd note the seam: the moment you add a second parameter that's backend-specific (say, a llama.cpp-specific `n\_ctx` override), the "read known keys, warn on unknown" pattern starts to feel like a workaround rather than a design.



\*\*The execution-log rows are the audit trail, and they'll grow.\*\* Every phase (claim, resolve, preflight, chat call, record, complete/fail) gets a log row. For a single execution, that's maybe 6–10 rows. For a benchmark run of 500 contexts × 10 model/skill pairs, that's 30,000+ log rows. SQLite handles this fine at single-node scale, but the "inspect the audit trail" workflow in the README is `actagamma\_db log list` — which will need pagination or filtering long before you hit a problem, and the CLI/GUI will need to make that usable. Not an architecture problem, just a usability seam that will appear early.



\*\*The "one action" boundary is the load-bearing wall.\*\* The entire architecture is coherent \*because\* an execution is one LLM call. The moment someone wants "extract entities, then classify each entity, then aggregate," the current model says: do that in external code, compose multiple executions. That's the right answer, and the README says it. But it means the system's value proposition is \*controlled single calls with full auditability\*, not \*workflow automation\*. If the team's use case drifts toward multi-step pipelines, the architecture will start to feel constraining, and the temptation to add a "pipeline" entity or a "session" state will be strong. I'd hold that line. If multi-step is needed, compose at the CLI/script level, not inside the engine.



\## What I would \*not\* change



\- I would not add an abstraction layer over the OpenAI-compatible endpoint (a `BackendInterface` with `LlamaBackend`, `OpenAIBackend`, `AzureBackend` implementations). One OpenAI-compatible HTTP call is the model. If someone needs a non-OpenAI protocol, that's a new backend type, not a refactor of the existing one. Adding a polymorphic backend abstraction now is architecture astronautics for a POC that has one protocol.



\- I would not separate the "execution prompt" from the skill's prompt template into a more formal template engine. `execution.prompt + "\\n\\n" + context.content` is a three-string concatenation. It's explicit, auditable, and the exact composition is visible in the audit trail. A template engine would add power and opacity in the same stroke.



\- I would not add a `promote`/`deprecate` workflow. The "latest revision is current, pick the ID you want" model is simpler, stateless, and sufficient. Adding lifecycle states would introduce exactly the kind of hidden context that makes replay harder to reason about.



\- I would not extract the DB layer into a separate service or use a different database. SQLite with WAL is the right tool for a single-node, file-backed persistence layer. Moving to Postgres would add operational cost without solving a problem the system actually has.



\## Bottom line



The architecture is coherent and will stay coherent at its intended scale: single-node, file-backed, one LLM call per execution, fully audited, exactly replayable. The boundaries are clear, the state is minimal, and the non-features are deliberate. The main thing to do before adding complexity (streaming, retries, richer model config) is to turn the runner's shared C source into a shared library so the GUI and standalone runner can't silently diverge. Everything else is a matter of adding features within the existing boundaries, not restructuring.



The philosophy line is doing real architectural work: \*"The engine decides what happens. The LLM only does the work it's asked to do."\* That sentence is the invariant. As long as every addition can be expressed as "the engine decides one more thing about one more single call," the system will grow cleanly. The moment you can't say that — the moment a call needs to spawn a sub-call, or a result needs to be fed back as a new input without an external orchestrator — you've crossed the boundary the architecture was built to respect, and the fix is not a bigger engine but an external composition layer.

