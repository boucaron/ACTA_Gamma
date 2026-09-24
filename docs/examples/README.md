# ACTA Gamma — worked examples

A small set of hands-on tutorials built around the database
[`acta.db`](acta.db) bundled in this folder (a snapshot of
`acta_gui/src/release/acta.db`, containing the five `*_summarize` persona
skills and the `JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS` model). Treat it as a
snapshot — copy it before doing anything, never work on it directly.

All examples use only `acta_cli` and, where marked, `acta_runner` — the CLI
is the complete surface, and every command here has a GUI equivalent.

## The tutorials, in suggested order

1. **[playground.md](playground.md)** — poke the database with `acta_cli`
   (list skills, models, contexts, executions, logs; soft-delete and
   restore). **No backend, no API key needed.** Start here.
2. **[create-and-revise.md](create-and-revise.md)** — create and update a
   skill, and watch the immutable revision snapshots appear. **No backend.**
3. **[replay.md](replay.md)** — replay a finished execution by reusing its
   exact binding, and see what "input determinism, not output
   determinism" means. **Needs a running backend.**
4. **[multi-persona-review.md](multi-persona-review.md)** — the flagship
   example: five versioned skills over one immutable context, one model,
   full id discovery, run, export, and audit trail — with the complete
   annotated script embedded. **Needs a running backend.**

## Before you run the "needs a backend" examples

- Build the binaries: `make all` from the repo root.
- Serve a model with a llama.cpp `llama-server` router — see the
  [Quick start](../../README.md#quick-start) in the main README. (You can
  bring your own model; each example says where to adapt it.)
- Export `OPENAI_API_KEY` (the key you launched the router with) and
  `ACTA_DB` (your working DB copy).

## One convention shared by all examples

```sh
mkdir acta_runner/MYTEST
sqlite3 docs/examples/acta.db ".backup 'acta_runner/MYTEST/acta.db'"
export ACTA_DB=$PWD/acta_runner/MYTEST/acta.db
```

Every tutorial starts from this working copy.
