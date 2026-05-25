# Strategic Nexus - Automation Candidates

## Purpose

This document lists project rules that are good candidates for recurring Codex automations.

Automations should monitor, summarize, and create task-board requests.
They should not silently make major architecture decisions.

---

# Recommended Automations

## Created App Automations

These Codex app automations have been created, not merely proposed:

* `sn-task-board-hygiene` - hourly Task Board hygiene
* `sn-safety-boundary-audit` - every 6 hours safety boundary audit
* `sn-usage-budget-check` - hourly usage budget check
* `sn-free-work-readiness` - daily Free Work readiness review
* `sn-bounded-free-work-execution` - frequent bounded Free Work execution gate
* `sn-architecture-consistency-review` - weekly architecture consistency review
* `sn-local-test-health-check` - weekly local test health check
* `sn-stellaris-modding-refresh` - weekly gated Stellaris/modding refresh check
* `sn-docs-encoding-review-gate` - weekly docs encoding/mojibake gate

Important distinction:

Documenting an automation candidate in this file does not create the app automation.
If the owner asks for an automation to be active, Codex should both update this file and create/update the real Codex app automation.

---

## 1. Architecture Consistency Review

Status:

```text
RECOMMENDED
```

Suggested cadence:

```text
weekly, Monday 09:00 Europe/Prague
```

Why:

* architecture docs are large and evolving
* roadmap items can become stale after new implementation
* contradictions waste Free Work budget

Expected output:

* no output if nothing concrete changed
* otherwise create/update a task-board note under `tools/dev_attention/`

Notes:

This automation already matches the previously requested architecture review idea.

---

## 2. Free Work Readiness Review

Status:

```text
RECOMMENDED
```

Suggested cadence:

```text
daily, 08:30 Europe/Prague
```

Why:

* Free Work should continue only when the next item is clear, bounded, and aligned with architecture
* blocked/stale roadmap items should not stop unrelated useful work
* user availability varies

Expected output:

* refresh the daily Free Work operating plan from latest budget/cadence/project data
* identify next 1-3 safe Free Work candidates
* identify blockers needing owner input
* automatically accept the daily plan when it stays inside approved `FREE_WORK_AND_USAGE_BUDGET_RULES.md`
* prefer product/runtime implementation when maintenance balance is too high and safe implementation work is available
* update task-board entries only when action is needed
* optionally contribute to one daily Free Work report entry instead of posting frequent small reports

Do not:

* start long implementation automatically from this review
* choose new architecture, gameplay philosophy, privacy/legal/safety risk, or major scope expansion
* spend heavy reasoning every day unless blockers exist

---

## 3. Usage Budget And Work Mode Check

Status:

```text
RECOMMENDED
```

Suggested cadence:

```text
hourly
```

Why:

* Codex cannot see account usage directly
* work mode depends on user-reported remaining weekly budget
* stale budget data can cause either underuse or waste
* hourly readings make burn-rate estimates useful without asking the owner to check the UI

Expected output:

* run `tools/dev_attention/read_codex_rate_limits.ps1 -UpdateUsageBudgetState` when the local Codex app-server protocol is available
* if the rate-limit reader fails, diagnose and attempt a bounded mechanical repair before asking the owner for manual data
* if repair is not safe or not obvious, create a concise Czech TB report/task with the failure, attempted diagnosis, and exact owner action needed
* read `.codex_local/usage_budget_state.md` if present
* run `tools/dev_attention/tune_freework_cadence.ps1`
* write `dist/private_reports/freework_cadence_recommendation.json`
* warn only if budget data is stale or missing during active Free Work
* suggest current work mode from `FREE_WORK_AND_USAGE_BUDGET_RULES.md`
* recommend whether `sn-bounded-free-work-execution` should change cadence
* if the recommendation differs, update the Free Work automation only when the change is clearly within approved budget rules; otherwise create a `Navrhy` item for owner review
* if the recommendation matches current cadence, do not create report spam

Do not:

* guess real account usage
* assume reset happened without user-provided data
* silently use stale budget data after reader failure
* ask the owner for screenshots before trying the local reader repair path
* parse screenshots when the app-server rate-limit endpoint is available
* increase cadence when no useful roadmap-aligned work exists
* increase cadence during gaming quiet mode for heavy implementation work

---

## 3a. Bounded Free Work Execution Gate

Status:

```text
CONDITIONAL
```

Suggested cadence:

```text
every 2 hours while budget is high
```

Why:

* the owner explicitly wants Free Work to continue when meaningful work exists
* the owner uses foreground chat time for architecture while background automation can handle implementation chunks
* recurring execution must still obey budget, roadmap, architecture, and decision boundaries
* if there is no useful safe roadmap work, the cheapest correct action is no action

Expected output:

* at the start of every gate invocation, run `tools/dev_attention/log_automation_run.ps1 -AutomationId sn-bounded-free-work-execution -Trigger unknown -Result started`; use `manual_ui` only when the owner explicitly says this invocation was started from UI, otherwise use `unknown` because Codex may not be able to distinguish scheduled from manual app execution
* before ending the gate, append a second run-log row with the same run id and a final result: `implemented`, `blocked`, `quiet`, `no_safe_task`, or `failed`
* check unresolved dirty worktree state before modifying files
* record dirty baseline paths and avoid overlapping them
* check whether any process from `.codex_local/gaming_quiet_processes.txt` is running, and always check `stellaris.exe`; if yes, enter gaming quiet mode
* read `.codex_local/usage_budget_state.md`
* read `dist/private_reports/codex_rate_limits.json` if refreshed by the usage-budget check
* read `.codex_local/usage_budget_log.csv`
* read `.codex_local/codex_work_log.csv`
* optionally read `dist/private_reports/freework_cadence_recommendation.json`
* read `FREE_WORK_AND_USAGE_BUDGET_RULES.md`
* estimate approximate burn rate from recent user-reported readings and known work windows when enough data exists
* revalidate the chosen roadmap item against current architecture docs
* if remaining budget is above 80%, allow frequent one-chunk execution
* if remaining budget is 60-80%, keep execution focused and shorter
* if remaining budget is 40-60%, implement only important narrow chunks
* if remaining budget is below 40%, do not implement unless the owner explicitly requested the exact task
* if gaming quiet mode is active, do not compile, run full tests, run v0 pipeline tests, or perform heavy disk/package/build work
* if foreground or unrelated dirty work overlaps the selected task write set, choose another task or stop
* if no roadmap-aligned, architecture-compatible, bounded, locally testable, decision-free task exists, do nothing
* otherwise choose a chunk size from remaining budget, burn-rate estimate, and task value
* implement at most one bounded chunk, run targeted verification, commit coherent changes, and create a Czech task-board report only for important owner-facing results
* if useful files are created or modified but cannot be committed, prefer retry-first handling: log the exact files, reason, and retry condition; create/update one Czech task-board Report/Suggestion only when the owner should know important uncommitted work exists; create `Ukoly pro me` only when owner action is genuinely required
* write any owner-facing automation final message and inbox item title/summary in Czech; technical paths, commands, symbols, and commit identifiers may remain unchanged
* do not create owner tasks for routine technical automation problems that Codex can retry or resolve later, such as transient git locks, delayed commits, tests still finishing, or local helper state

Do not:

* perform continuous unattended implementation
* invent speculative tasks to stay busy
* override `BLOCKED_USER` items
* collide with foreground chat work or unrelated dirty files
* run heavy build/test work while Stellaris is running
* silently fail forever because unrelated dirty files exist
* leave meaningful uncommitted worker output visible only through `git status`
* make the owner administer routine automation plumbing
* create report spam when nothing changed

Important:

This is not the same as a continuous runner.
It is a gated one-chunk worker with permission to stop silently when work would be wasteful.

Observability note:

Even when the worker does no implementation, the run log should show that the gate fired.
This distinguishes a healthy automation that chose `blocked`, `quiet`, or `no_safe_task` from a scheduler that did not run at all.

Tariff planning note:

The owner reports using a Pro plan and manually supplies remaining limit readings.
Codex should not assume exact account internals, but high remaining Pro budget justifies a more frequent background gate than daily execution.

---

## 4. Task Board Hygiene Review

Status:

```text
RECOMMENDED
```

Suggested cadence:

```text
hourly
```

Why:

* blocked owner decisions should stay visible
* old task-board notes can become stale after commits
* the owner may not read chat history
* the owner may forget that a task was already handled

Expected output:

* list open TB items
* update progress percentages for known tasks
* remove active task-board items that are verified as 100% solved
* run `tools/dev_attention/sync_user_task_board_state.ps1` so process-state blockers such as gaming quiet mode are cleared when the relevant game process is no longer running
* keep a local gaming quiet session log in `.codex_local/gaming_quiet_session_log.csv` with detected game start/end transitions so stale TB state can be explained later
* mark ambiguous stale items for review, not deletion
* keep items concise and actionable
* create at most one daily task-board hygiene summary report, not one report every hour

Do not:

* remove owner-facing tasks automatically unless they are clearly resolved by a committed change
* remove ambiguous owner-facing tasks from process state alone unless the task explicitly represents that process state
* create hourly report spam

---

## 5. Local Test Health Check

Status:

```text
CONDITIONAL
```

Suggested cadence:

```text
weekly, Wednesday 10:00 Europe/Prague
```

Why:

* full local tests are getting slower
* regression coverage is important
* running them too often burns compute and time

Expected output:

* run lightweight checks first
* run full `tools/run_all_tests.ps1` only when recent code changes or scheduled health review justify it
* create TB item if tests fail

Do not:

* run full tests daily without a reason

---

## 6. Safety Boundary Audit

Status:

```text
RECOMMENDED
```

Suggested cadence:

```text
every 6 hours during active Free Work, otherwise daily at 11:00 Europe/Prague
```

Why:

* safety scope is foundational
* new docs/code can accidentally introduce forbidden direction
* this is cheaper than late cleanup
* the owner has previously experienced a blocked conversation and wants earlier warning signals

Expected output:

* run or review `tools/run_safety_audit.ps1`
* create a TB item or direct finding if something fails
* include safety status in the daily TB report when Free Work is active

Limit:

OpenAI does not publish a reliable project-specific probability that a conversation will be blocked.
The automation can reduce risk by catching unsafe direction early, but it cannot guarantee that a platform moderation event will never happen.

Do not:

* weaken the audit to make it pass

---

## 7. Documentation Encoding / Mojibake Review

Status:

```text
BLOCKED_USER
```

Suggested cadence after decision:

```text
monthly, first Friday 14:00 Europe/Prague
```

Why:

* canonical docs should stay free of visible mojibake
* bad encoding can make rules harder to read and easier to misinterpret

Explanation:

Mojibake means text encoding corruption, for example when a UTF-8 dash or quote is displayed as broken multi-character noise.
It is mostly a documentation readability problem, not a runtime bug.

Current policy:

Docs were normalized repo-wide to UTF-8 with ASCII-safe punctuation.
New docs should stay UTF-8, LF, and preferably ASCII unless there is a clear reason to use non-ASCII text.

Expected output:

* if approved, monitor docs for newly introduced mojibake
* run `tools/check_docs_encoding.ps1` as the local gate
* create TB item when it appears

Do not:

* mass-normalize docs again without owner approval

---

## 8. Stellaris Version And Modding Knowledge Refresh

Status:

```text
CONDITIONAL
```

Suggested cadence:

```text
monthly minimum, second Monday 10:00 Europe/Prague
weekly during known upcoming Stellaris patch/DLC windows
one extra check within 48 hours after a known patch/DLC release
```

Why:

* Stellaris mechanics and modding hooks change over time
* project rules say not to rely on stale remembered identifiers

Expected output:

* check whether a new Stellaris version or relevant modding documentation change requires review
* create TB item if a concrete compatibility review is needed
* generate a task-board report only when there is a relevant version/modding signal

Do not:

* rewrite mod assumptions without source-backed evidence

---

# Not Recommended As Recurring Automations

## Continuous Full Free Work Runner

Reason:

Persistent Free Work should be driven by active context, commits, and current budget.
A blind recurring automation could spend compute when budget or direction is stale.

Better:

Use daily Free Work readiness review plus the bounded Free Work execution gate.
The execution gate may run only one validated chunk and must do nothing when no suitable task exists.

## Automatic Architecture Rewrites

Reason:

Architecture changes often encode gameplay philosophy.
Codex should propose them, not silently adopt them.

Better:

Create task-board proposals for owner approval.

## Daily Full Regression Suite

Reason:

The full suite is already slow enough that daily runs are poor budget hygiene.

Better:

Run full tests after implementation chunks and weekly health checks.

---

# Task Board Reports

Recurring automations should use the task board as both:

* an action board for owner decisions and manual tests
* a durable project-owner report board for important work summaries
* a suggestion board for improvement proposals and architecture/workflow ideas that are not immediate blockers

These must be separated in the task-board UI as owner-visible modes/tabs:

* `Ukoly pro me`
* `Reporty`
* `Navrhy`

Report rules:

* task-board tasks and reports should be written in Czech by default
* if a Windows tool cannot reliably render Czech diacritics, use readable Czech without diacritics rather than mojibake
* reports should be additive and archived
* reports should be causal: what changed, why it matters, and what risk or decision follows
* reports should avoid low-level implementation noise
* routine automation reports should be batched into one daily summary when possible
* high-importance findings may create immediate reports
* completed active tasks should be removed from the active task list when progress reaches 100%, while any important completion summary can remain as a report

Suggestion rules:

* use `Navrhy` for ideas that may make the project better but do not require immediate owner action
* do not put suggestions into `Ukoly pro me` unless an owner decision is actually needed now
* do not put suggestions into `Reporty` unless they are part of a completed work summary
* automations may add concise suggestions when they find a better architecture path, missing definition, or smoother workflow

---

# Suggested Priority

Create in this order:

1. Architecture Consistency Review
2. Task Board Hygiene Review
3. Usage Budget And Work Mode Check
4. Safety Boundary Audit
5. Free Work Readiness Review
6. Local Test Health Check
7. Documentation Encoding Review after owner decision
8. Stellaris Version Refresh when external source workflow is ready
