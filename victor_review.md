\# Review: ACTA Gamma



I'll go through this systematically, flagging what worries me and what I'd want answered before I'd trust this in anything beyond a demo.



\---



\## 1. The "replay is exact" claim is doing too much work



You say:



> a replay is exact when it reuses the same context, skill revision, model revision, and execution prompt



The \*\*inputs\*\* to the LLM call are identical. Full stop. The \*output\* is not. Unless you've pinned `temperature=0` \*\*and\*\* the backend is truly deterministic (most OpenAI-compatible servers are not, even at temp 0 — sampling kernels vary, quantization differs, batch scheduling can matter), you will get different observations for the same four inputs. Calling that "exact replay" is going to mislead the next person who reads this README and assumes reproducibility of results.



Worse: the \*\*model revision\*\* captures `base\_url`, `model\_identifier`, and a config blob. It does \*\*not\*\* capture the actual model weights. If someone swaps the `.gguf` file on a llama.cpp server between run A and run B, the `model\_revision\_id` is unchanged, the "replay" is technically identical in every field you store, and the outputs will differ. There is no checksum of the weights, no fingerprint of the backend state, no way to detect that the thing you're pointing at has changed underneath you.



\*\*What I'd want:\*\* Rename the claim to "reproducible \*inputs\*." Add a note that output reproducibility depends on backend determinism and weight immutability, which the system cannot verify.



\---



\## 2. Revision lifecycle: no escape hatch, no deprecation, no draft state



The lifecycle is: create parent → trigger snapshots a revision → update parent → trigger snapshots another revision. Every keystroke in a prompt template creates an immutable row. For iterative prompt engineering, you'll be creating dozens of near-identical revisions, and there is no way to mark any of them as "draft" or "broken — do not use."



You explicitly reject `promote` / `deprecate` / `active` marking. I understand the design philosophy, but consider the operational consequence: if revision 14 was a bad prompt that produced garbage, the only signal that revision 14 was bad is that executions bound to it failed. There's no first-class record saying "this revision was rejected." An auditor looking at the revision list sees 14 rows and has no indication that 14 was a mistake.



Also: what happens if you delete the parent skill? The README says revisions are immutable and can't be deleted, but it doesn't say what happens to orphaned revisions when the parent is gone. Can you delete the parent at all? If yes, do the revisions become dangling rows that can't be resolved by an execution?



\*\*What I'd want:\*\* A `note` or `status` field on the parent (not the revision) that at least lets you say "this parent is under revision." And explicit documentation of what happens to revisions when a parent is deleted.



\---



\## 3. Concurrency: the claim is safe, but the pipeline is not



The claim step — optimistic `UPDATE … WHERE status = 'pending'` — is fine. Two runners can't grab the same execution. Good.



But the full pipeline is: \*\*claim → resolve → preflight → chat call → record → complete/fail\*\*. That's a multi-step sequence. If the process dies between "claim" (status = `running`) and "complete/fail," the execution is stuck in `running`. You handle this with `sweep`, but:



\- \*\*Sweep is manual.\*\* Nothing in the system runs it automatically. If your runner crashes at 3 AM and nobody runs sweep until Thursday, three executions are stuck in `running` for a day. For a benchmark run of 500 executions, that's 500 stuck rows.

\- \*\*The stale-seconds threshold interacts with the HTTP timeout.\*\* The default `--timeout` is 300 s. If `--stale-seconds` is set to, say, 120, sweep will mark a \*live\* execution as stale while it's legitimately waiting on a slow backend response. The README doesn't call out this interaction.

\- \*\*The GUI runs the pipeline in-process on a worker thread.\*\* If the GUI crashes (segfault, Qt exception, user closes the window), the execution is in `running` and the only cleanup is the external `sweep` command. The GUI has no signal handler or cleanup-on-exit.



\*\*What I'd want:\*\* Document the minimum safe `--stale-seconds` relative to `--timeout`. Consider a periodic sweep (cron, systemd timer, or an in-process timer in the runner) rather than relying on a human to remember. Add a comment in the GUI about what happens to in-flight executions if the app is force-killed.



\---



\## 4. The in-process GUI runner duplicates the standalone runner



> `run.c`/`backend.c` are compiled into the GUI, no `acta\_runner` binary needed



You now have \*\*two compiled copies\*\* of the same pipeline logic. If there's a bug in the claim logic, the preflight check, or the curl call, you fix it in `acta\_runner`, but the GUI has its own copy. The README doesn't mention how they're kept in sync — shared source files? A library? Copy-paste? If it's copy-paste, that's a maintenance liability. If it's shared source, where is the shared source, and does the Makefile reflect that?



Also: the GUI opens "its own DB connection" for the worker thread while also doing "live status polling of the shared database" on the UI thread. That's at least two connections, possibly three (UI polling, worker execution, and whatever the main window uses). SQLite in WAL mode handles concurrent reads and one writer, but the checkpoint and lock-timeout behavior under sustained polling + a long-running chat call is not something I'd assume is stress-tested.



\*\*What I'd want:\*\* Clarify whether `run.c`/`backend.c` are literally the same files compiled into both targets, or copies. If copies, extract them into a shared object. If shared, say so. And document the connection topology: how many open handles does the GUI have at steady state, and what happens if the writer is holding the DB during a poll.



\---



\## 5. Security: the API key is in a plain-text JSON blob in a plain SQLite file



The model config blob stores `api\_key`. The DB file is a single SQLite file. There is no mention of:



\- Encryption of the key (at rest or in transit to the DB)

\- File-permission guidance (the DB file could be world-readable depending on umask)

\- A warning that backing up or version-controlling the DB file leaks all API keys

\- TLS enforcement on the `base\_url` (if someone points at an `http://` endpoint, is that silently accepted?)

\- Any authentication on the CLI itself (anyone with shell access can read every prompt, context, and execution log)



For a POC this is acceptable. For anything that touches real production API keys or real business data in contexts, it's a problem. At minimum, I'd want a one-line warning in the README: "API keys are stored in plaintext in the SQLite file; protect the file."



\---



\## 6. "Preflight" is undefined



The pipeline says "claim → resolve → \*\*preflight\*\* → chat call → …" but the README never says what preflight does. Is it:



\- A TCP/TLS connectivity check to `base\_url`?

\- A `GET /v1/models` to verify the model exists?

\- A check that the JSON config blob is parseable?

\- Nothing, and it's just a naming artifact?



If it's a connectivity check, what's the timeout on the check itself? If it's a model-existence check, does it cache the result or hit the endpoint every time? If a cloud API rate-limits you, does the preflight check consume a rate-limit token?



\*\*What I'd want:\*\* A sentence in the README defining what preflight checks and what failure mode it produces.



\---



\## 7. Output schema validation: what's the failure mode?



> optional output-schema validation



"Optional" means it can be turned off. But what happens when it's on and the LLM returns something that doesn't match? Is the execution:



\- \*\*Completed\*\* with a `validation\_error` field on the result?

\- \*\*Failed\*\* (status → `failed`)?

\- \*\*Completed\*\* with the raw response and a flag?



The README says "complete/fail" as the terminal states but doesn't say which one validation failure maps to. If it's a "fail," then a model that consistently produces slightly-malformed JSON will generate a failure every time, and the operator has to manually reset each one. If it's a "complete," the audit trail has a result that can't be consumed by downstream tooling that expects valid JSON.



Also: what schema format? JSON Schema? A hand-rolled validator? If it's JSON Schema, which subset? The README doesn't say.



\---



\## 8. The "benchmarkable" claim has no supporting infrastructure



You list "benchmarkable" as a core idea and say "compare models and skill revisions against the same datasets." But:



\- There is no concept of a "dataset" or "test set" in the entity model. A context is a single input. To benchmark, you'd create N contexts, run N executions per model/skill, and… what? There's no comparison view, no scoring function, no statistical summary.

\- "Benchmarking workflows should still not share one in-flight execution" — this warning is unexplained. If I create 500 separate executions, 100 per model, and run them sequentially, what breaks? The warning implies something, but the README doesn't say what.



The word "benchmarkable" in the core ideas list is a promise the implementation doesn't yet back. I'd pull it from the feature list until there's actual tooling.



\---



\## 9. The sweep race is handled, but the description is confusing



> any row that leaves `running` between the query and the fail is skipped rather than overwriting a live outcome



This is a SELECT-then-UPDATE pattern. In SQLite with WAL, if you do:



```sql

SELECT id FROM executions WHERE status = 'running' AND last\_activity < cutoff;

UPDATE executions SET status = 'failed', ... WHERE id = ? AND status = 'running';

```



The `AND status = 'running'` on the UPDATE is the guard. If the runner completed between the SELECT and the UPDATE, the UPDATE affects 0 rows and you skip. That's correct. But the README describes it as "between the query and the fail," which sounds like a two-statement race that could be interleaved in surprising ways under WAL. It's fine in practice, but the documentation should be precise: "The UPDATE is guarded by a status check in the WHERE clause; if the row has already transitioned, the UPDATE is a no-op and the sweep skips it."



Also: `--stale-seconds` rejects 0. Why? If I want to sweep \*immediately\* (e.g., after a crash, before the next run), 0 seems valid. "Positive integer" is an arbitrary constraint. If there's a reason (e.g., to avoid sweeping rows that are \*just\* starting), say so.



\---



\## 10. Missing: schema migration, backup, and operational concerns



\- \*\*No migration path.\*\* The DB schema is implicit in the C code. When you add a column, how do existing databases upgrade? There's no `migrate` command, no version column, no migration script.

\- \*\*No backup procedure.\*\* Single SQLite file. What's the recommended backup? Copy the file? `VACUUM` first? What about WAL files? If you copy the main file without the WAL, you get an inconsistent snapshot.

\- \*\*No retention policy for `execution\_log`.\*\* Every phase of every execution creates a log row. After a year of benchmarking, that's potentially millions of rows. No mention of archiving or pruning.

\- \*\*No logging to stdout/file.\*\* All observability is in the DB. If you're debugging a runner crash, you have to query the DB. There's no mention of `--verbose` or a log level.

\- \*\*No rate limiting.\*\* If you point at a cloud API and fire 500 executions in a benchmark, you'll hit rate limits. There's no backoff, no queue depth limit, no mention.



\---



\## 11. The CLI: input handling



`actagamma\_db model create --json '{"name":"llama-local",...}'`



\- What if the JSON is malformed? Is the error message useful? Does it point to the offending token?

\- What if the JSON is valid but contains fields that aren't recognized (e.g., `base\_urll`)? The model config blob warns about unknown keys, but does the top-level entity create?

\- Is there any size limit on `--json`? If someone pastes a 10 MB prompt template, what happens?

\- On Windows/MinGW, is the `--json` argument subject to shell escaping issues with quotes and braces?



These are small things, but for a CLI that's the primary interface for a POC, they matter.



\---



\## 12. What I'd actually change before calling this "done"



| Issue | Severity | Suggested fix |

|---|---|---|

| "Replay is exact" is misleading | High | Reword to "inputs are identical"; add caveat about LLM nondeterminism |

| No model-weight fingerprint | Medium | Add optional hash/checksum field to model config; document the limitation |

| No deprecation/draft status for revisions | Medium | Add a `note` or `status` field on the parent; document orphaned-revision behavior |

| Sweep is manual-only | High | Add a `--auto-sweep` mode to the runner, or a cron-friendly one-shot mode with a clear default |

| In-process GUI runner duplicates standalone runner | Medium | Extract shared pipeline into a library; document the build relationship |

| API key in plaintext | Medium (acceptable for POC) | Add a one-line warning; consider `--env ACTA\_API\_KEY` for at least the cloud case |

| Preflight undefined | Low | One sentence in README |

| Schema validation failure mode ambiguous | Medium | Document: validation failure → `failed` with a specific error string, or `completed` with a flag. Pick one. |

| "Benchmarkable" has no tooling | Medium | Remove from core ideas or add a minimal comparison command |

| No schema migration story | Medium | Add a `PRAGMA user\_version` and a migration script, or at least document "delete and recreate" |

| No DB backup procedure | Low | One paragraph: `VACUUM; cp acta.db acta.db.bak` or use SQLite's online backup API |

| `--stale-seconds` rejects 0 | Low | Allow 0 or document why not |

| No rate limiting / backoff | Low (POC) | Note as a known limitation |



\---



\## 13. What I'd actually praise



The core architecture is clean. "One LLM call, stateless, immutable inputs, explicit revision IDs" is a good design for the problem it's solving. The separation of the runner from the DB layer is right. The WAL + optimistic claim is the minimum-correct concurrency model for SQLite. The rejection of agent-style orchestration is a sound philosophical choice — most "agent frameworks" are solving problems that don't exist at this scale.



The audit trail (prompts, raw responses, results, errors, execution log phases) is the right shape for reproducibility and debugging. The immutable context is the single most important feature for the stated use case.



The Makefile story is straightforward. Building a C project with `make` and system libraries is the boring, correct thing to do.



\---



\## 14. Bottom line



This is a competent POC with a clear design philosophy and a reasonable core. The architecture decisions are defensible. Where it falls short is in the operational story: what happens when things go wrong, who cleans up, how you know the model hasn't changed underneath you, how you back up the DB, how you migrate the schema, and where the benchmarking actually lives.



For a prototype to validate the "LLM as a controlled action" concept, it works. For something you'd point at a team of five using real API keys and real data, I'd want at least the high-severity items addressed before I'd say "ship it."

