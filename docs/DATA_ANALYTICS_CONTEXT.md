# Strategic Nexus - Data Analytics Context

## Purpose

This document is the reusable starting context for future data analysis work in Strategic Nexus.

Use it before opening generated reports, runtime logs, autosave-derived outputs, or Task Board data.

The goal is to avoid rediscovering:

* where useful data lives
* which files are durable source context versus generated output
* which data is private or volatile
* which fields are safe to summarize
* what a future analysis should verify before drawing conclusions

---

# Privacy Boundary

Default rule:

```text
local analysis only
```

Do not upload raw saves, raw campaign data, personal paths, player names, custom empire names, multiplayer identities, local account details, or full private logs unless the owner explicitly asks for that specific export.

For owner-facing summaries, prefer:

* aggregate counts
* anonymized labels
* file category names instead of full private paths
* trends and status fields instead of raw payloads
* compact evidence references

When a generated artifact may reveal local or campaign-specific data, treat it as private.

Canonical privacy rules:

* [PRIVACY_AND_LEARNING_RULES.md](PRIVACY_AND_LEARNING_RULES.md)

---

# Source Classes

## Durable Architecture Context

Primary location:

```text
docs/
```

Use for:

* architecture decisions
* roadmap state
* safety constraints
* data contract interpretation
* historical reasoning that should survive chat compaction

Trust level:

```text
high for intended architecture, not proof of runtime behavior
```

Important examples:

* [MASTER_ARCHITECTURE_INDEX.md](MASTER_ARCHITECTURE_INDEX.md)
* [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md)
* [OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md](OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md)
* [SAVE_FORMAT.md](SAVE_FORMAT.md)
* [SAVE_ENTRY_POINT_AND_BRANCH_RULES.md](SAVE_ENTRY_POINT_AND_BRANCH_RULES.md)
* [LOCAL_LLM_INTEGRATION_CONTRACT.md](LOCAL_LLM_INTEGRATION_CONTRACT.md)
* [FREE_WORK_AND_USAGE_BUDGET_RULES.md](FREE_WORK_AND_USAGE_BUDGET_RULES.md)

## Test Fixtures And Public-Like Examples

Primary locations:

```text
resources/
exchange/
```

Use for:

* schema examples
* deterministic parser/validator behavior
* regression-friendly synthetic inputs
* public-safe examples when possible

Trust level:

```text
high for contract examples, low for real campaign distribution
```

Observed committed data snapshot on 2026-06-03:

* `resources/` contains JSON fixtures for decision payloads and ministry contexts.
* `exchange/` contains an example request JSON.

## Local Codex Operational Context

Primary location:

```text
.codex_local/
```

Use for:

* automation run signals
* local usage budget state
* Codex work logs
* local-only workflow rules
* private handoff notes

Trust level:

```text
high for local operational state, private by default
```

Important files:

* `.codex_local/automation_run_log.csv`
* `.codex_local/codex_work_log.csv`
* `.codex_local/implementation_work_log.csv`
* `.codex_local/usage_budget_state.md`
* `.codex_local/usage_budget_log.csv`
* `.codex_local/private_rules.md`

Do not treat these as public documentation.

## Generated Private Reports

Primary location:

```text
dist/private_reports/
```

Use for:

* Task Board backing data
* owner-facing reports
* suggestions
* project progress estimates
* usage budget recommendations
* real-session trend summaries
* SNC tray/live autosave status

Trust level:

```text
medium-high for current local state, volatile and generated
```

Important examples:

* `user_reports.json`
* `user_suggestions.json`
* `user_tasks.json`
* `project_progress_estimate.json`
* `roadmap_complexity_estimate.json`
* `freework_cadence_recommendation.json`
* `real_session_v0_trend.json`
* `snc_live_autosave_monitor_status.json`
* `snc_tray_status.json`

This directory is generated and ignored by git. Do not assume it exists on a fresh clone.

## Real Session And Autosave-Derived Outputs

Primary locations:

```text
dist/real_session_v0_loop/
dist/test_real_session_loop_archive/
dist/snc_live_autosave_archive/
dist/autosave_*_archive/
```

Use for:

* real-session loop evidence
* archive manifests
* entry point analysis
* season delta ledgers
* empire briefs
* generated overlay staging and package outputs

Trust level:

```text
high for local run evidence, private and volatile
```

These outputs may be derived from saves and can reveal campaign-specific information.

They are suitable for local analysis and owner-approved summaries.

## Build And Test Artifacts

Primary locations:

```text
dist/*_test*/
dist/*fixture*/
*.obj
x64/
```

Use for:

* verifying that test runs produced expected files
* troubleshooting recent test behavior

Trust level:

```text
medium for local verification, not durable project memory
```

Do not use build artifacts as canonical architecture sources.

---

# Suggested Analysis Workflow

1. Start with `docs/MASTER_ARCHITECTURE_INDEX.md` and this file.
2. Identify the analysis goal and source class.
3. Prefer committed fixtures for schema questions.
4. Prefer `.codex_local/automation_run_log.csv` for automation liveness.
5. Prefer `dist/private_reports/*.json` for current Task Board and owner-facing state.
6. Prefer real-session archive manifests for session evidence.
7. Never infer user-facing conclusions from one generated file without checking its provenance or a neighboring manifest/log.
8. Summarize private/generated data by trend, count, or status unless exact raw content is necessary and owner-approved.

---

# Common Reusable Questions

## Automation Reliability

Useful inputs:

* `.codex_local/automation_run_log.csv`
* `%USERPROFILE%\.codex\automations\*/automation.toml`
* `dist/private_reports/user_reports.json`
* `docs/AUTOMATION_CANDIDATES.md`
* `docs/FREE_WORK_AND_USAGE_BUDGET_RULES.md`

Typical checks:

* Is the automation visible in the app registry?
* Does an `automation.toml` exist?
* Did it create a later run signal?
* Did it update Task Board state or only local logs?
* Is an older report stale after migration?

## Usage Budget And Free Work Cadence

Useful inputs:

* `.codex_local/usage_budget_state.md`
* `.codex_local/usage_budget_log.csv`
* `dist/private_reports/freework_cadence_recommendation.json`
* `tools/dev_attention/tune_freework_cadence.ps1`

Typical checks:

* Is the usage reading fresh?
* What reset window is being used?
* What cadence target did the tuner compute?
* Which active Free Work automation ID/path is the tuner targeting?

## Task Board State

Useful inputs:

* `dist/private_reports/user_tasks.json`
* `dist/private_reports/user_reports.json`
* `dist/private_reports/user_suggestions.json`
* `dist/private_reports/project_progress_estimate.json`
* `dist/private_reports/roadmap_complexity_estimate.json`

Typical checks:

* Are counts consistent with the native Task Board UI?
* Are stale reports still visible?
* Are suggestion statuses owner-friendly and project-neutral?
* Are report timestamps displayed in compact Czech owner-facing format where applicable?

## Real Session Evidence

Useful inputs:

* `dist/real_session_v0_loop/*/`
* `dist/test_real_session_loop_archive/*/manifest.json`
* `dist/private_reports/real_session_v0_trend.json`
* `dist/private_reports/real_session_v0_trend_latest_compare.json`

Typical checks:

* Which session generated the evidence?
* Which save entry point or branch was analyzed?
* Did generated overlay staging pass validation?
* Did package/import paths produce expected manifest state?
* Is any campaign-specific detail safe to summarize?

---

# Data Quality Rules

Before presenting a conclusion:

* check file freshness
* check whether the file is committed, local-only, generated, or ignored
* check whether a generated report may be stale after automation migration
* check whether a raw path or campaign identifier should be anonymized
* check whether the data source is a fixture, test output, or real-session output
* check whether a neighboring manifest/log confirms the same event

If evidence conflicts, prefer:

1. app registry or live runtime state for automation visibility
2. latest local run log for automation liveness
3. generated report only for owner-facing reporting state
4. architecture docs for intended design

---

# Data Analytics Artifact Guidance

When rendering a dashboard/report through Data Analytics tooling:

* keep snapshots bounded
* use aggregate rows, not raw private payload dumps
* include source labels and freshness
* mark generated/ignored directories clearly
* avoid exposing local private paths unless needed for owner debugging
* do not archive or commit visual snapshots without explicit owner approval

Good default artifact surfaces:

* automation liveness dashboard
* Task Board status summary
* usage budget and Free Work cadence report
* real-session evidence overview
* roadmap progress trend

