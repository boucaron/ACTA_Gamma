# Runner — implementation plan

Concrete, file-level plan for the queued runner work. Scope and status per
[`runner_active_action.md`](runner_active_action.md); specs and decisions per
[`runner_analysis.md`](runner_analysis.md). Order: R5.

---

## R5 — JSON validation (analysis first, then UI)

Goal (H2 / UR #15): validate `output_schema` with clear "invalid JSON"
feedback.

### Analysis (decision, then implement)

- `skill.output_schema`: JSON Schema → **validate**, must be a JSON
  *object*.
- `model.configuration`: **do not validate in the GUI** (owner decision,
  2026-07-10): model tuning is handled in the llama.cpp router/server
  configuration for the time being — no client-side tuning. The runner
  still parses and validates the field at run time when non-empty.
- `context.content`: may be plain text → **do not validate as JSON**.

### Implementation

- `QJsonDocument::fromJson` on the field text; on parse error show
  `QJsonParseError` message with line/column — inline warning label, plus a
  "Validate JSON" affordance.
- Exact widget (verified):
  - `SkillDialog::ui->outputSchemaTextEdit` (`skillDialog.cpp`, save path at
    the `s.output_schema = ...` site).
- Block save on invalid JSON in the SkillDialog (creation + edit paths),
  matching the existing error-handling style (P4). Empty fields stay
  allowed (the field is optional today — `dupString` gets `nullptr` on
  empty text).
- `tr()`-wrap all new strings; add to `translations/acta_gui.ts`.

---

## Sequencing and documentation hygiene

1. **R5** → #15 closed in `ui_review.md` / `ui_active_action.md`.
2. **R6** — closed (owner decision, 2026-07-10: JSON highlighting /
   line numbers are pointless at the time, keep it simple).
   **R7** (housekeeping: `.gitignore` for build outputs) whenever
   convenient.
