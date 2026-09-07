# Runner — implementation plan

Concrete, file-level plan for the queued runner work. Scope and status per
[`runner_active_action.md`](runner_active_action.md); specs and decisions per
[`runner_analysis.md`](runner_analysis.md). Order: R5.

---

## R5 — JSON validation (analysis first, then UI)

Goal (H2 / UR #15): validate `output_schema` and model `configuration`
with clear "invalid JSON" feedback.

### Analysis (decision, then implement)

- `model.configuration`: JSON object (keys `api_key`, `temperature`,
  `max_tokens`, `top_k`, `supports_response_format` per phase 2) →
  **validate**, must be a JSON *object*.
- `skill.output_schema`: JSON Schema → **validate**, must be a JSON
  *object*.
- `context.content`: may be plain text → **do not validate as JSON**.

### Implementation

- `QJsonDocument::fromJson` on the field text; on parse error show
  `QJsonParseError` message with line/column — inline warning label, plus a
  "Validate JSON" affordance.
- Exact widgets (verified):
  - `ModelDialog::ui->configurationTextEdit` (`modelDialog.cpp`, save path
    at the `m.configuration = dupString(...)` sites, both create and edit);
  - `SkillDialog::ui->outputSchemaTextEdit` (`skillDialog.cpp`, save path at
    the `s.output_schema = ...` site).
- Block save on invalid JSON in both dialogs (creation + edit paths),
  matching the existing error-handling style (P4). Empty fields stay
  allowed (both fields are optional today — `dupString` gets `nullptr`
  on empty text).
- `tr()`-wrap all new strings; add to `translations/acta_gui.ts`.

---

## Sequencing and documentation hygiene

1. **R5** → #15 closed in `ui_review.md` / `ui_active_action.md`.
2. **R6** (JSON highlighting / line numbers, UR #26) and **R7**
   (housekeeping: fate of untracked `docs/llamacpp_server_README.md`,
   `.gitignore` for build outputs) whenever convenient.
