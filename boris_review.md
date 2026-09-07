\# Review: ACTA Gamma



\## Overall impression



This is a boring project, and that's a compliment. Plain C, SQLite, `make`, curl, cJSON. No framework, no dependency tree, no service mesh, no "AI orchestration layer." The philosophy section — \*the engine decides, the LLM does the work\* — is exactly right, and the fact that it's called out explicitly rather than buried in a "design decisions" appendix means someone actually thought about what they're not doing. That's rare.



The stateless, one-shot execution model is the right shape for a POC that will grow into a benchmarking/audit tool. You're not building an agent framework, and you should be proud of that in a year of "autonomous agents" hype.



That said, I have a few things I'd push back on or tighten up before this leaves prototype status.



\---



\## The GUI is premature and duplicates the runner



You have four components: `acta\_db`, `acta\_db\_cli`, `acta\_runner`, `acta\_gamma`. The first three form a clean, linear dependency chain. The GUI is where things get murkier.



You've compiled `run.c` and `backend.c` — the entire runner pipeline — \*into the Qt app\*. That means the claim → resolve → preflight → chat call → record → complete/fail logic now exists in two codebases: the standalone `acta\_runner` binary and the GUI process. They share the same SQLite database, which is fine, but you now have to keep two copies of the pipeline in sync, each with its own DB connection, its own cancellation path, its own status-polling loop.



For a POC, that's a lot of surface area. Qt 6 (Core + Widgets) is a heavy dependency for what is, functionally, a table browser with a "Run" button. If I'm honest: the CLI already covers everything the GUI does, plus `acta\_runner` handles the actual execution. The GUI is a convenience layer for non-CLI users, but at the POC stage, maintaining two execution paths is a liability, not an asset.



\*\*What I'd do:\*\* ship the C trio (db, cli, runner) as the v0.1 deliverable. Make the GUI a v0.2 milestone. When you do build it, don't recompile the runner into it — make the GUI a thin client that shells out to `acta\_runner` or talks to it over a simple IPC boundary. One pipeline, one codebase, one set of tests.



If you do keep the in-process run, then at minimum extract the pipeline into a shared C library (`libacta\_pipeline`) so both the standalone runner and the GUI link against the \*same\* object files. Right now the README implies the code is compiled into each target, which means it's duplicated, not shared.



\---



\## The JSON config blob is a soft edge



> The runner reads the keys `api\_key`, `temperature`, `max\_tokens`, `top\_k`, and `supports\_response\_format`; unknown keys are warned about and ignored, and a malformed blob is warned about and treated as empty.



"Warned about and ignored" is the wrong default for a field that controls model behavior. If someone typo's `temperatue` instead of `temperature`, you silently fall back to the backend default. That's a different model behavior, and you won't notice until the output is wrong and you're three benchmarks deep.



For a tool whose entire selling point is \*reproducible, auditable execution\*, a config typo that changes the effective model parameters without a hard error is a hole. I'd make unknown keys an error, or at least make the warning go to stderr with the offending key name \*and\* refuse the run. "Treated as empty" for a malformed blob is the same problem — you're running with a default config the user never intended.



You don't need a full JSON schema validator. You need a whitelist of known keys and a strict "unknown key → error" check. That's twenty lines of C.



\---



\## Revision snapshots via DB trigger: fine, but document the blast radius



Auto-snapshotting a new revision row on every `CREATE` or `UPDATE` of the parent is operationally convenient — you don't have to remember to call `revision create`. I like that. But it means that a stray `UPDATE` on a skill (say, a typo fix in the prompt template) silently creates a new immutable revision, and any execution that was pointing at the old revision now has a "previous version" that's no longer the latest.



That's fine — it's the whole point of immutability. But the README should be explicit about the operational consequence: \*\*editing a skill creates a new revision; it does not modify the old one; executions keep pointing at the revision they were bound to.\*\* A future engineer who runs `skill update` expecting the in-place edit to "just work" for existing executions will be confused. One sentence in the README would save that confusion.



Also: what's the cap? If someone script-updates a skill in a loop, you get unbounded revision rows. I don't care for the POC, but in a year when someone is running 500 benchmark iterations and each one touches the skill, that's a table growing without bound. A `revision\_count` sanity check or a "latest N revisions" retention policy isn't urgent, but it should be in the roadmap.



\---



\## The `sweep` mechanism is good. The `--stale-seconds` default should be documented.



The stale-runner cleanup is the right pattern. An optimistic `UPDATE … WHERE status = 'pending'` for claiming is standard SQLite idiom and correct. The sweep logic (compare last activity timestamp against a threshold, skip rows that leave `running` between query and fail) is careful and handles the race. Good.



But `--stale-seconds` has no default in the README. You say it "must be a positive integer (0 is rejected)." What \*is\* the default? Is it required? If it's required, the CLI help should say so. If there's a default (say 300, matching the HTTP timeout), say it. As written, a new user doesn't know whether `acta\_runner sweep` without the flag is an error or uses some magic number.



\---



\## Minor things



\- \*\*macOS cJSON from source.\*\* "Build it from source and pass `CJSON\_DIR` / `CJSON\_LIB`" is a fine workaround, but it means your macOS dev environment has a hand-rolled build path that nobody else has to deal with. If you care about macOS at all, consider a one-line `brew install` of a prebuilt, or just drop macOS support until it's actually needed. Don't half-support three platforms at the POC stage.



\- \*\*`EXEEXT` and `CC ?= cc`.\*\* The Makefile portability notes (MinGW vs. POSIX) are fine. But the fact that you need to document "EXEEXT is empty under POSIX make" in the README suggests the Makefile has a Windows-specific extension assumption baked in. That's not a bug, but it's a wart that will trip someone up on the first Linux build. A two-line comment in the Makefile would be cheaper than a paragraph in the README.



\- \*\*The "replay is exact only with all four inputs" sentence.\*\* You state this clearly, but it's the single most important guarantee in the project and it's buried in a paragraph. Make it a callout, a bolded line, or a one-sentence box. If the replay guarantee is the product, it should be the most visible thing in the README, not a clause in the revisions section.



\- \*\*`acta\_gamma` as the GUI directory name.\*\* The product is "ACTA Gamma." The C components are `acta\_db`, `acta\_db\_cli`, `acta\_runner`. The GUI is `acta\_gamma`. That naming implies the GUI \*is\* ACTA Gamma and the other components are subsystems. In reality, `acta\_db` is the foundation and the GUI is one consumer. I'd name the GUI `acta\_gui` or `acta\_gamma\_gui` to make the hierarchy clear.



\---



\## What I'd actually change, in priority order



1\. \*\*Don't ship the GUI yet.\*\* Ship `acta\_db`, `acta\_db\_cli`, `acta\_runner`. That's the product. The GUI is a nice-to-have that doubles your execution-path maintenance.



2\. \*\*Make unknown JSON config keys a hard error\*\*, not a warning. One line per unknown key to stderr, non-zero exit, run doesn't start. This protects the reproducibility guarantee.



3\. \*\*Extract the runner pipeline into a shared library\*\* if you do keep the in-process GUI run. No duplicated code paths.



4\. \*\*Document the `--stale-seconds` default\*\* (or make it required) in the README and the CLI `--help` output.



5\. \*\*Add one sentence\*\* explicitly stating that `skill update` does not mutate existing executions — it creates a new revision; old executions keep their binding.



6\. \*\*Name the GUI directory `acta\_gui`\*\* or similar, so the component hierarchy is clear from the directory structure.



\---



\## What I would \*not\* change



\- The stateless, one-shot execution model. Correct.

\- Immutable contexts and revisions. Correct.

\- SQLite with WAL. Correct. Boring. Good.

\- `make` as the build system. Correct. You don't need CMake, Bazel, or Meson for four C targets and one Qt app.

\- OpenAI-compatible endpoints as the only backend interface. Correct. It's a superset that covers llama.cpp, cloud APIs, and anything else that speaks that protocol. You don't need a "backend adapter" abstraction layer for three backends.

\- The "no promote / deprecate / active" lifecycle. Correct. "Latest revision is the current one, point at the ID you want" is simpler than a state machine with lifecycle transitions.

\- Manual retry via `failed → pending` reset instead of automatic retries. Correct for the POC. Automatic retry with backoff, circuit breakers, and retry-storm protection is a whole subsystem you don't need yet.

\- BSD-0-Clause. Correct. No copyleft, no patent trap, no "you must share improvements" clause. It's a tool, not a religion.



\---



\## Bottom line



This is a well-scoped, conservative, deterministic tool that does one thing: run a single LLM call against a versioned prompt and an immutable context, record the result, and let you replay or benchmark it. The technology choices are right, the architecture is simple, and the "not an agent" positioning is sound.



The main risk I see is scope creep through the GUI. Four components, two of which contain execution logic, one of which is a 200-MB Qt dependency — that's where a "small stateless engine" starts becoming a "platform" before anyone needs a platform.



Build the three C targets. Get the CLI-to-runner loop working end-to-end. Run your first benchmark. \*Then\* decide whether a GUI earns its maintenance cost. In a year, if you still need one, it'll be a thin table viewer over a stable API, not a second copy of the execution pipeline.



It's a good project. Keep it boring.

