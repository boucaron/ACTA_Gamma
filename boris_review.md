# Review: ACTA Gamma

I've read the whole thing. Let me be direct.

## What this actually is

A C11 program that makes one HTTP call to a local llama-server and writes the result into a SQLite table. That's it. One call. No loop. No state machine beyond pending/running/completed/failed/cancelled. The README spends a lot of words explaining what it *isn't*, and that's the most honest section in the document. "LLMs as actions, not agents" is not a slogan; it's the architecture, and the architecture matches the words.

I'll say what I expect you're tired of hearing: **this is the correct level of complexity.** You do not need an agent framework to call an LLM once and record the output. You need a runner, a log, and a way to prove what was sent. You have all three. Good.

## Technology choices — no surprises, and that's the point

C11, SQLite, curl, cJSON, Qt 6 Widgets, `make`. No Python. No Node. No Docker Compose. No Kubernetes. No framework. No dependency graph beyond two small C libraries. The build is `make all` on three platforms. The backend is a single process the user launches by hand.

This is what I want to see. Every technology here has a decade or more of production scar tissue behind it. I can debug a segfault in the runner with `gdb` in an afternoon. I can read the schema in `DBDesign.md` and know exactly what's in the file. There is no abstraction layer I have to learn. No "middleware" that decides to retry on my behalf. No plugin system that someone will break in a patch release and I won't notice for six weeks.

**I have no objection to any of these choices.**

## Things I would flag

### 1. The hand-rolled JSON Schema validator is a trap, even though you documented it

You validate `type`, `required`, `properties`, and `items` recursively. You ignore `pattern`, `enum`, `format`, length constraints, `oneOf`, `anyOf`. You say this explicitly. Good.

But here's the problem: a user will write an `output_schema` with `{"enum": ["positive", "negative"]}` because that's what "schema" means to most people. Your validator will see `type: "string"` on the property, the string will be a string, and it will pass. The schema constrains nothing. The user has written a schema that they *believe* is doing something, and it isn't.

You've documented this. The README says "an `output_schema` that relies on any of those constrains nothing." But the documentation lives in the README and in `runner_contract.md`. The error message when validation fails will be `validation_failed`. The user who wrote the `enum` schema will see `completed` and think the schema was enforced. They won't.

**What I'd do:** Either (a) restrict the schema field to a subset and document it *in the error output and in the create/update help text*, so the constraint is visible at the point of use, or (b) use a real JSON Schema validator. I know (b) adds a dependency. For a project that is *about* proving what happened, a real validator is not cleverness — it's the minimum bar for the word "schema." A hand-rolled subset that silently ignores half the spec is a liability, not a simplicity win.

### 2. "No prompt-injection defense" is fine for v1, but write it down as a hard boundary, not a scope note

The context reaches the model verbatim. You say sanitizing untrusted content is the operator's job. For a local, single-user tool talking to a local llama-server, that's a defensible trade-off. I'm not going to make you add a content filter.

But the README buries this in the "deliberately does not" table and in `PointOfView.md`. If this ever becomes anything beyond a single person's local tool — a team, a CI job, a service that ingests external files — that line is the entire security model. I want it stated as a **hard precondition of the deployment context**, not a "these are product decisions" footnote. The distinction is: "we chose not to build this" vs. "this is only safe in this specific deployment shape, and if you change the deployment shape, you change the threat model."

### 3. The DB trigger for revision snapshots is fine, but it's a subtle coupling

A trigger fires on INSERT/UPDATE/soft-delete of the parent and inserts a revision row. This is clean enough for a small system, and I won't tell you to rip it out. But it means the revisioning logic lives in the schema, not in the application code. If you ever want to change how revisions work — say, coalesce rapid edits, or skip a revision on a no-op update — you have to write a schema migration and re-test every path that touches the parent table.

I'm not saying this is wrong. I'm saying: **the trigger is a design decision that should be visible in the schema documentation as a behavioral contract, not just an implementation detail.** `DBDesign.md` presumably covers this. If it does, fine. If it just says "a trigger exists," that's not enough. The trigger is load-bearing for the immutability guarantee.

### 4. The `db exec` first-keyword blocklist

You say the SQL "must be a developer-written literal, never composed from runtime input" and that a `SELECT` is rejected by "a first-keyword blocklist, not a full parse." I get it. It's a developer escape hatch, not a user-facing API.

But "first-keyword blocklist, not a full parse" is the kind of thing that looks fine in code review and then bites you when someone puts `;` in the middle and the second statement is a `DROP`. You've scoped it to "developer-written," which is the right scoping. Just make sure the code path is genuinely unreachable from any input the CLI user can type. If it's only callable via `acta_cli db exec --sql "..."` and the `--sql` value is taken verbatim, then yes, the developer is the threat, and the threat is also the operator, so a blocklist is fine. But I want to see that the function signature and the call sites don't have a path where a user-supplied string reaches that `sqlite3_exec`.

### 5. The README is long, and that's correct

I want to address the obvious "this is a 4,000-word README for a POC" reaction. For a system whose entire value proposition is *auditability and reproducibility*, the documentation is not overhead. If the README is thin, the next engineer who picks this up in nine months has to reverse-engineer the revision model, the execution state machine, the soft-delete semantics, and the runner pipeline from the C source. That's not maintenance; that's archaeology.

The "deliberately does not" table is the most important section in the README. It saves you from three years of "can you add X" questions. Keep it. Update it every time you say no.

### 6. The GUI is correctly optional

Qt 6 Widgets, Core + Widgets. Not QML. Not Electron. Not a webview. The GUI is not part of `make all`. Every operation has a CLI equivalent. The in-app Run button compiles the same pipeline source, not a second copy.

This is the right call. I will say: the GUI is where your maintenance cost doubles, because Qt versions, platform quirks, and display issues are a long tail. But you've kept it optional and you've made the CLI the complete surface. If the GUI rots, the system still works. That's the conservative choice, and you made it.

### 7. Manual retry, no streaming. Agree with both.

Automatic retries are where subtle state bugs live. A retry that fires twice, or a retry that fires after the operator has reset the row, is a race you have to reason about. Manual retry is one extra keypress and zero race conditions.

Streaming adds a whole category of partial-state handling: what do you record if the stream drops at token 40 of 200? Do you write a partial `raw_response`? What's the state machine row? You avoid all of it by making an execution a single call with a timeout. Boring. Correct.

## What I would not change

- The SQLite single-file DB. No Postgres. No Redis. No "we'll move to Postgres in v2." For a local, single-user, audit-focused tool, a file you can `cp` is the right storage. The `db backup --to` command and the WAL discussion in `DBDesign.md` show you've thought about durability without reaching for infrastructure.

- The llama-server-only backend. You don't need a backend abstraction layer. You talk to one HTTP server. The model configuration blob is a flat JSON with four keys and strict type checking. If you later need a second backend, you add a code path, you don't refactor an interface.

- The stateless, one-call execution model. This is the whole point. Don't let the "replay" word make someone think you want a session manager or a conversation store. You don't. You replay by re-creating the execution with the same revision ids and context. The inputs are deterministic. The output isn't, and you say so. That honesty is worth more than a false guarantee.

- The `max_chars` preflight as a char count, not a token count. You can't do a token count without the tokenizer, and you're model-agnostic. A char count against a conservative ceiling is a good-enough guard that fails closed. It's not elegant, but it's correct and simple.

## Bottom line

This is a boring system that does one thing and documents what it won't do. The technology is C, SQLite, and a local HTTP server. The scope is explicitly bounded. The documentation is thorough enough that I could onboard a junior engineer in a week without reading the source. The "deliberately does not" table is worth more than most feature lists I've seen in "AI platform" READMEs.

The two things I'd actually push on: the JSON Schema subset should either be a real validator or be more aggressively constrained at the input boundary, and the prompt-injection stance needs to be framed as a deployment precondition, not a scope note. Everything else: I'm not going to touch it. It's the right system at the right complexity level, and the next person maintaining it in three years will thank you for the boring choices.

Build it. Ship the POC. Don't add features. Make the audit trail airtight. That's the whole job.