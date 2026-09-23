## Review: ACTA Gamma README

**Overall verdict:** This is one of the more honest and well-structured READMEs I've seen for a POC. It doesn't oversell, it names its non-goals explicitly, and the "Minimal end-to-end example" is genuinely executable. A competent engineer *can* follow it to a working run without asking the author. That said, there are a handful of friction points and ambiguities that would make first-time experience smoother.

---

### What works well (and is worth keeping)

- **The one-liner + the "This is not an agent framework" sentence.** Most LLM-tool READMEs get ambiguous here and the reader has to infer the scope. You front-load the boundary. Good.
- **"These are product decisions, not missing features."** That sentence does a lot of trust-building in one clause. It pre-empts the "why doesn't it retry?" and "why no streaming?" questions before they're asked.
- **The "What it does / deliberately does not" table.** Clear, scannable, and the "No prompt-injection defense — the context reaches the model verbatim" row is the kind of thing that saves someone from a security incident.
- **The numbered end-to-end example with inline comments.** The "each entity above got id 1 — first rows in a fresh database" note is exactly the kind of context a reader would miss otherwise. The `export OPENAI_API_KEY=` with the keyless-localhost explanation is well-handled.
- **Fail-closed behavior called out explicitly** (config file, model configuration blob, `db exec` keyword check). "Hard error, never a silent retarget" is the right language.
- **The "Replay caveat — input determinism, not output determinism" callout.** This is the kind of thing that, if left implicit, leads someone to build a CI pipeline on the assumption that replays are bit-identical. Stating it twice (Core ideas and Revisions sections) is appropriate.

---

### What I'd change or tighten

**1. The "get running" path is front-loaded with context and back-loaded with action.**

A new reader hits ~600 words of philosophy ("What is ACTA Gamma?", "Core ideas", "What it does") before they see *what they need installed and how to start the server.* The "Quick start" (three steps) is good, but it sits after the full Implementation table. I'd consider a very short "Prerequisites / TL;DR" block at the top — something like:

> **You need:** a C compiler (gcc/clang), SQLite, curl, cJSON, and a running `llama-server` in router mode serving at least one GGUF. Everything else is in this repo.

That's four lines and lets the reader triage before committing to the full read.

**2. The `llama-server` dependency is a hard gate but is discovered late.**

Step 3 of Quick Start mentions it, but the *requirement* that you must have llama.cpp installed, have downloaded a GGUF model, and launched the server in router mode is only fully spelled out in the "Implementation" section and the `llamacpp_server_contract.md` link. For a first-time user, "start the backend" in Quick Start is too abstract — they don't yet know what "router mode" means or what flags matter. The Quick Start step could say one more sentence:

> `llama-server --models-dir models -c 2048` serves every GGUF in `models/` at `http://127.0.0.1:8080/v1/chat/completions`. This is not a generic OpenAI endpoint; it's the llama.cpp router (see `docs/llamacpp_server_contract.md` for the full contract).

That's one sentence, but it closes the gap between "start the backend" and "here's the actual command and what it gives you."

**3. The architecture diagram's "Audit" node is under-specified.**

The diagram shows `Observation` and `Audit` as two outputs of an Execution. "Observation" is defined in the prose above. "Audit" is not — the reader has to infer it means the `execution_log` rows + the resolved prompt + the raw response. I'd either rename it to "Execution log / Audit trail" in the diagram, or add a one-line caption: `Audit = execution_log phase rows + resolved prompt + raw_response`.

**4. The example assumes a fresh database without flagging it.**

The comment "each entity above got id 1 — first rows in a fresh database — so every '1' below is the corresponding row id" covers it, but a reader who already has an `acta.db` from a previous session will copy-paste the example and get "row not found." A short parenthetical — "(this assumes a fresh `acta.db`; if you have an existing database, substitute the actual ids)" — would prevent that.

**5. The config-file section is thorough but dense, and the platform-specific paths are easy to misread.**

The paragraph about `ACTA_Gamma.conf` location has three platform-specific paths in one sentence (`%APPDATA%\ACTA_Gamma\acta.db`, `~/.local/share/ACTA_Gamma/acta.db`, `$XDG_DATA_HOME\ACTA_Gamma\acta.db`). A reader on Linux might scan past the Windows path and miss that their path is the second one. I'd suggest a small table or bullet list:

| Platform | Default DB path | Config file path |
|---|---|---|
| Windows | `%APPDATA%\ACTA_Gamma\acta.db` | `%APPDATA%\ACTA_Gamma\ACTA_Gamma.conf` |
| Linux | `~/.local/share/ACTA_Gamma/acta.db` | `~/.local/share/ACTA_Gamma/ACTA_Gamma.conf` |
| (XDG override) | `$XDG_DATA_HOME/ACTA_Gamma/acta.db` | `$XDG_DATA_HOME/ACTA_Gamma/ACTA_Gamma.conf` |

It's the same information, but it's scannable in 2 seconds instead of requiring a full sentence parse.

**6. "Skill" vs. "prompt" terminology is consistent but the shift to `prompt_template` in JSON could trip a reader.**

The README defines "a skill is a versioned prompt template." Then the JSON shows `"prompt_template":"..."`. A reader skimming might think "is `prompt_template` the skill, or a field *of* the skill?" It's clear in context, but a one-line note in the example comment — `# prompt_template is the skill's instruction text` — would close the micro-gap.

**7. Minor: no version number or release date.**

"Early prototype / POC" is clear, but there's no `v0.1.0` or similar. If this ever moves to a tag-based release, the README will need it. Not urgent for a POC, but worth noting in the docs plan.

**8. Minor: the `db exec` "exit 4" detail in CLI ergonomics is implementation-level.**

"A `SELECT` is rejected before the DB is touched by a first-statement keyword check (exit 4)" — the "exit 4" is useful for scripting but is a low-level detail for a README section titled "CLI ergonomics." I'd move the exit code to `cli_spec.md` and keep the README at "a `SELECT` is rejected before the DB is touched."

---

### What a first-time user might ask that the README doesn't answer

| Question | Where the answer is (or isn't) |
|---|---|
| "Where do I get a GGUF model?" | Not stated. The Quick Start says "point `--models-dir` at a folder containing that one GGUF" but doesn't say *where to get one* (e.g., Hugging Face, llama.cpp's model list). A link would help. |
| "Can I use this with a hosted API (OpenAI, Groq, etc.)?" | Implied no by "the only supported backend," but a reader might not parse that from the Implementation table without reading `PointOfView.md`. The "What it does / does not" table says "llama.cpp `llama-server` router — the only supported backend" which is clear enough, but the "Implementation" section could restate it in one clause. |
| "What happens if the server goes down mid-run?" | `sweep` covers the stale-run case, but the "server dies mid-request" case (TCP timeout) isn't explicitly described. The `--timeout` flag is mentioned but the failure mode (execution → `failed` with an error) could be one sentence. |
| "Is the SQLite DB file human-readable / inspectable?" | Implied by "plain `make`" + "SQLite audit log" but not stated. A reader might want to know "can I `sqlite3 acta.db .tables`?" A one-liner would help. |

---

### Bottom line

The README answers the core question: **"Could a competent engineer understand and use this without asking the author?"** — Yes, with the caveat that the "understand" phase is long (the README is ~2,000 words of dense technical prose before the first command). The "use" phase is clear and executable.

The main risk isn't confusion — it's **attrition**. A new user who hasn't installed llama.cpp, hasn't downloaded a model, and hasn't built the C toolchain is going to hit a wall at Quick Start step 3. The README doesn't hold their hand through that, and that's probably fine for the target audience (engineers who already have llama.cpp in their toolchain). But a single line — "If you don't have llama.cpp yet, see `docs/building.md` §1 for a 5-minute install" — would reduce the first-time drop-off without adding much length.

The project's identity is clear, its boundaries are explicit, and its example is honest. That's harder to write than most POC READMEs manage. The remaining issues are ergonomics, not correctness.