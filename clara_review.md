\# README Review — ACTA Gamma



\## Overall impression



The conceptual spine is strong. "LLMs as actions, not agents" is a clear, memorable position, and the two ASCII diagrams nail the mental model in under thirty seconds. The end-to-end example is the best part of the doc: it's concrete, sequential, and a competent engineer could follow it top to bottom. The "no promote/deprecate/active" call-out is exactly the kind of preemptive clarification that saves a confused reader from building a mental model that doesn't match the code.



That said, a first-time user who has never touched this project will hit a handful of real friction points before they get a working `acta\_runner run 1`. Most of them are gaps between "what the system does" and "how I bootstrap it."



\---



\## Specific issues, roughly in order of impact



\### 1. No "where does the database live?" / "how do I bootstrap it?"



The README talks about "SQLite persistence," "a DB trigger inserts a new revision row," "the SQLite connection uses WAL journal mode," and a top-level `make all` — but never says \*\*where the `.db` file is created, what its default name is, or whether there's an `init` / `migrate` step.\*\* The example starts at `actagamma\_db model create` as if the database is already there and already schema'd.



A new engineer will run that command, get a "no such table" or a file-not-found error, and have to grep the source to find the answer. One sentence in the building section ("the CLI creates `actagamma.db` in the current working directory on first use" or "run `actagamma\_db init` to bootstrap the schema") would close this gap.



\### 2. The example presupposes a running model server



Step 1 says "register the model" pointing at `http://127.0.0.1:8080`, but nothing in the README tells you \*\*how to get that server up.\*\* "e.g. `llama-server` on `127.0.0.1:8080`" is a hint, not a setup. For someone who has never run llama.cpp, the first real task is actually:



```sh

llama-server -m qwen3-8b.gguf -c 4096  # or whatever the actual command is

```



Even a one-line "prerequisite: you need a reachable OpenAI-compatible chat endpoint; see llama.cpp's server docs for details" would set expectations. Right now the doc jumps from "here's the architecture" to "here's a command that talks to a server you must already have running."



\### 3. `execution.prompt` is "optional" but the example always shows one



The "How a run is assembled" section correctly explains the optional path, but the minimal example (step 4) always includes `"prompt":"What is the sentiment of the context?"`. A reader skimming the example will infer it's required. Either:



\- add a one-line note in the example: `# (prompt is optional; omit it to send just the context)`, or

\- show a second, shorter `exec create` without the prompt, or

\- rename the field in the example to make the optionality visible.



Small thing, but it shapes the first mental model.



\### 4. "Skill" is a slightly overloaded term in the current LLM ecosystem



In the agent/tool-use world, "skill" is being used for prompt templates, function schemas, MCP tools, Claude's own "skill" concept, etc. ACTA Gamma's "skill" = "a versioned prompt template + optional output schema." The README does define it, but the word collision means a reader from the agent-framework side may expect function-calling, tool use, or multi-step reasoning. A one-sentence disambiguation early on ("A \*skill\* here is a versioned prompt template with an optional output schema — it is not a tool, function, or agent capability") would prevent that misread.



\### 5. "Context" is doing a lot of generic work



In LLM parlance, "context" usually means the entire prompt window, conversation history, or the surrounding input to a model call. Here it means \*\*a named, immutable input-data snapshot\*\* (a document, a code file, a log excerpt). That's a perfectly good domain term, but it will collide with the generic usage. Consider:



\- Introducing the term once with a parenthetical: \*"A \*\*context\*\* (an immutable snapshot of input data — a document, code snippet, log, etc.)…"\*

\- Or using "input" / "input snapshot" in the body and keeping "context" only for the entity name.



Right now the first time a reader sees "context" in the diagram they'll read it as "the prompt context" rather than "the input payload."



\### 6. The "Current status" paragraph is a wall of text



The "Done" paragraph is roughly 250 words of run-on sentences covering:



\- what's implemented

\- how the in-app Run button works (worker thread, in-process runner, cancel behavior)

\- how `sweep` works (stale detection, threshold, transition, skip logic)

\- the `--stale-seconds` constraint

\- the timeout default

\- the retry mechanism



This is technically complete but very hard to scan. A reader who only wants "what's not done yet" has to wade through all of it. Suggest:



```markdown

\### Done

\- Entity model, persistence, versioning, folders

\- Execution lifecycle + execution log

\- Replayable immutable contexts

\- Standalone runner (claim → … → complete/fail)

\- In-app Run button (worker thread, cancel support)

\- `sweep` for stale-running cleanup

\- Manual rerun of failed executions



\### Not yet implemented

\- Streaming responses

\- Automatic retries (deliberately deferred; see note below)



\### Design note: no automatic retries

Transient backend failures are rare in the current single-node deployment;

a manual `failed → pending` reset is simpler to reason about and avoids

retry storms.

```



This keeps the same information but lets a reader find what they need in two seconds.



\### 7. The concurrency sentence is a little ambiguous



> "parallel runners are safe for claiming, but benchmarking workflows should still not share one in-flight execution."



What does "share one in-flight execution" mean? The claim step guarantees two processes won't grab the same row, so what's the concern? I think the intended meaning is: \*two runners won't double-process one execution, but a benchmark harness that fires 50 executions and expects deterministic per-execution ordering should not have two runners interleaving on the same batch.\* That's a reasonable caveat, but the sentence as written is vague. A concrete example or a short "what we mean by 'share'" would help.



\### 8. macOS cJSON instructions are a friction point



> "cJSON is not packaged by Homebrew — build it from source and pass `CJSON\_DIR=...` / `CJSON\_LIB=...` to the CLI and runner Makefiles."



"Build it from source" is a lot to ask for in a README. A competent Mac user can do it, but they have to:



1\. clone the cJSON repo

2\. figure out the Makefile or CMake target

3\. know where to point `CJSON\_DIR` and `CJSON\_LIB`



A three-line block:



```sh

git clone https://github.com/DaveGaud/cJSON.git

cd cJSON \&\& make

\# then: make CJSON\_DIR=/path/to/cJSON CJSON\_LIB=/path/to/cJSON/lib/cjson.a

```



would turn an "oh, that's annoying" into "oh, that's fine."



\### 9. Qt 6 build: `qmake6` availability on Linux



The Linux section says `sudo apt install qt6-base-dev`. On Debian/Ubuntu that does put `qmake6` in PATH, but on Fedora it's `qmake6` from `qt6-base-devel`, and on Arch it's just `qmake` (Qt 6's qmake is still named `qmake`). The README currently only covers Debian/Ubuntu, which is fine, but a one-line "on other distros, ensure `qmake6` (or `qmake` for Qt 6) is in PATH" would prevent a silent "command not found."



\### 10. The philosophy quote is the most important sentence in the doc, and it's at the bottom



> \*\*The engine decides what happens. The LLM only does the work it's asked to do.\*\*



That's the thesis. It's buried under "Current status" and "License." Moving it to the top (right after the tagline or as a subtitle) would reinforce the positioning before the reader dives into architecture. The "What is ACTA Gamma?" section already says it in prose form, but the short declarative line lands harder as a headline.



\---



\## Smaller / nits



\- \*\*`actagamma\_db` vs `acta\_runner` naming.\*\* The DB CLI has "gamma" in the name; the runner doesn't. Not wrong, but a new user typing commands will see two different naming patterns and wonder if they're from different projects. Consistent naming (`actagamma\_db`, `actagamma\_runner`, `actagamma\_gui`) would be less confusing.



\- \*\*The example uses `exec get 1` and `log list` without showing what the output looks like.\*\* Even a two-line snippet of what `exec get` returns (status, raw response, validated output, etc.) would make the audit story concrete. Right now the reader is told "the audit trail shows exactly what was asked" but never sees the shape.



\- \*\*`--json` as the only input format.\*\* The example only shows `--json '...'`. If there's also a key=value or file-based input, it's not mentioned. If `--json` is truly the only way, that's fine, but a reader might wonder "can I pass a YAML file or a prompt from stdin?"



\- \*\*The output schema is mentioned ("optional output schema") but never shown in the example.\*\* The skill JSON has `prompt\_template` but no `output\_schema` field. If it's a thing, the example should include one or explicitly say "omitted here for brevity."



\- \*\*"per-parent sequence 1, 2, 3, …"\*\* — "per-parent" is a bit opaque. "Auto-incremented within each skill/model (starting at 1)" is clearer.



\---



\## What I'd change if I owned this README



1\. Add a 3-line "Quick start / Prerequisites" box at the top: where the DB file lives, what server you need running, and the one `make all` command.

2\. Split "Current status" into bullet lists (done / not done / design note).

3\. Add a 2-line sample output for `exec get`.

4\. Move the philosophy quote up.

5\. Disambiguate "skill" and "context" in their first appearance.

6\. Show `execution.prompt` as truly optional in the example.

7\. Flesh out the macOS cJSON steps.



None of these are structural rewrites — the doc is well-organized and the core message is clear. The gaps are mostly "what do I type first?" and "what shape is the output?" — the kind of things that make a first run take 20 minutes of grepping instead of 5 minutes of following.

