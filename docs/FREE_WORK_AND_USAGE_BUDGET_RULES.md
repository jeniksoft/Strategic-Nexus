# Strategic Nexus - Free Work And Usage Budget Rules

## Core Rule

Codex may perform autonomous Free Work only when the declared usage budget is high enough and the next work item is clear, local, testable, and valuable.

Free Work must never mean burning compute just because budget exists.

Free Work must produce project value.

---

# Budget Visibility Rule

Codex cannot reliably detect the user's remaining account or tariff limit internally.

Codex must treat usage budget as externally declared user context.

Allowed budget sources:

* local Codex app-server `account/rateLimits/read` snapshot
* direct user statement in chat
* user-maintained local project note
* future explicit automation or exported account status if safely available

Rejected assumptions:

* guessing remaining limit from time alone
* assuming tariff reset time
* assuming billing state from model availability
* pretending internal account visibility exists

If no current budget state is known, Codex should assume a conservative budget mode.

---

# Rate-Limit Reader Reliability Rule

Manual owner checking of Codex reset time and remaining limit should be treated as a degraded fallback, not the normal workflow.

Preferred source:

```text
tools/dev_attention/read_codex_rate_limits.ps1 -UpdateUsageBudgetState
```

This script uses the local Codex app-server `account/rateLimits/read` endpoint.

If the reader fails, Codex should:

1. record the failure in a local diagnostic/report file or task-board report when useful
2. inspect whether the Codex executable path changed
3. inspect whether the local app-server protocol/schema changed
4. try a bounded local repair when the fix is mechanical and low-risk
5. update docs/scripts if the protocol changed in an understandable way
6. notify the owner only when the reader remains broken, the fix would be risky, or account/authentication/user action is required

Codex should not ask the owner for screenshots or manual limit readings unless all practical local reader paths are unavailable or unsafe.

Forbidden behavior:

* silently falling back to stale budget data
* pretending a guessed reset time is verified
* repeatedly creating owner tasks for a failure Codex can diagnose or repair itself
* scraping screenshots when the local app-server endpoint is available

If the reader fails during scheduled Free Work, the worker should use conservative budget mode until it repairs the reader or gets fresh trusted data.

---

# Scheduled Rate-Limit Logging Rule

Codex should refresh and log Codex rate-limit state regularly through the local app-server reader.

Default cadence:

```text
hourly
```

Reason:

* manual limit checking is annoying and unreliable as a long-term workflow
* Free Work cadence depends on current remaining budget
* burn-rate estimates require enough timestamped readings
* missing budget data can make Codex either too timid or wasteful

Scheduled logging should run:

```text
tools/dev_attention/read_codex_rate_limits.ps1 -UpdateUsageBudgetState
tools/dev_attention/tune_freework_cadence.ps1
```

Expected outputs:

* `dist/private_reports/codex_rate_limits.json`
* `.codex_local/usage_budget_state.md`
* `.codex_local/usage_budget_log.csv`
* `dist/private_reports/freework_cadence_recommendation.json`

The check is lightweight and may run even when no Free Work implementation chunk is safe.

If the local reader fails, Codex should diagnose and repair the reader path when the fix is mechanical and safe.
Only ask the owner for manual screenshots or readings after the local reader path is unavailable, unsafe, or repeatedly failing.

---

# Declared Budget Rule

When the user declares a remaining weekly limit percentage, Codex may use that value as the current working budget estimate.

Example:

```text
weekly limit remaining: 95%
```

This value is not independently verified by Codex.

It remains valid until:

* the user updates it
* the conversation context becomes stale
* the work intensity changes significantly
* Codex has reason to believe the estimate is no longer useful

If the user says "spotreba", "usage", or similar wording, Codex must clarify whether the number means consumed budget or remaining budget when the meaning is not obvious.

If the user corrects the meaning, Codex must update the local budget state and follow the corrected interpretation.

---

# Budget Mode Thresholds

## Free Work Mode

Use when declared remaining weekly limit is above 80%.

Allowed behavior:

* continue autonomously on roadmap items
* implement small and medium scoped tasks
* run local tests
* improve regression coverage
* update architecture docs
* commit and push completed work

Constraints:

* avoid speculative rabbit holes
* avoid unbounded research loops
* stop when strategic direction becomes unclear

Suggested automation cap:

```text
one bounded chunk, normally up to 90 minutes, then commit/report or stop
```

Suggested background cadence:

```text
frequent gate, for example every 2 hours, because the gate may decide to do nothing
```

---

## Productive Work Mode

Use when declared remaining weekly limit is 60-80%.

Allowed behavior:

* continue high-value implementation
* prioritize roadmap tasks
* prefer testable commits
* avoid low-value polish

Suggested automation cap:

```text
one high-value chunk, normally up to 45 minutes
```

Suggested background cadence:

```text
moderate gate, for example every 4 hours or manual Free Work when useful
```

---

## Focused Work Mode

Use when declared remaining weekly limit is 40-60%.

Allowed behavior:

* important implementation only
* bug fixes
* validation fixes
* architecture blockers

Avoid:

* broad audits without clear output
* optional refactors
* speculative documentation expansion

Suggested automation cap:

```text
one narrow important chunk, normally up to 30 minutes
```

Suggested background cadence:

```text
low frequency gate or manual start only
```

---

## Conservation Mode

Use when declared remaining weekly limit is 20-40%.

Allowed behavior:

* critical blockers
* small exact edits
* important review
* short planning

Avoid:

* long autonomous loops
* large exploratory work
* noncritical polish

Recurring Free Work implementation should not run in this mode unless the user explicitly requests the exact task.

Suggested background cadence:

```text
readiness/report only
```

---

## Critical Reserve Mode

Use when declared remaining weekly limit is below 20%.

Allowed behavior:

* critical fixes only
* user-requested exact actions
* short explanations
* emergency debugging

Codex should preserve remaining budget for high-value decisions and blockers.

Recurring Free Work implementation must not run in this mode.

It may only produce short blocker/readiness notes when that prevents wasted future work.

Suggested background cadence:

```text
off, except exact user requests
```

---

# Free Work Eligibility Rule

Free Work is allowed only if the task is:

* aligned with the roadmap
* bounded
* locally testable or document-verifiable
* safe to commit
* not dependent on missing user decisions
* not dependent on live Stellaris interaction unless explicitly requested

Good Free Work examples:

* add regression tests
* fix bounded, locally verifiable bugs
* harden validators
* improve fail-safe behavior
* document confirmed runtime findings
* update architecture index
* clean small CI issues
* implement deterministic pipeline scaffolding

Bad Free Work examples:

* invent large new systems without review
* rewrite architecture direction
* disguise architecture changes as bug fixes
* attempt lower-level game-process integration
* generate speculative gameplay content
* expand scope without tests
* research indefinitely without concrete output

---

# No Useful Roadmap Work Rule

If no roadmap-aligned, architecture-compatible, bounded, locally testable, decision-free task is available, Codex should do nothing.

This applies especially to scheduled or recurring Free Work.

Allowed minimal output:

* update an existing task-board blocker when a concrete owner decision is needed
* write one concise readiness/report note when it prevents duplicate work

Disallowed behavior:

* searching indefinitely for something to do
* inventing speculative roadmap work
* spending compute only to keep the automation busy

Free Work should optimize for useful completed work, not visible activity.

---

# Autonomous Work Efficiency Rule

This does not override safety, usefulness, or owner-decision boundaries.

Codex should not burn compute just to avoid theoretical waste.

When a trustworthy reset timestamp and current remaining percentage are available, cadence planning should target a small reserve at the reset, not merely classify the current remaining percentage. Default target reserve:

```text
5% remaining at reset
```

This means a high remaining budget shortly before reset should increase cadence more aggressively than the simple threshold table would suggest, as long as useful safe work exists.

The reserve-target calculation should be corrected by recent usage history when enough post-reset samples exist. Historical burn rate is a correction signal, not the authority:

* if actual burn is far behind the reserve trajectory, tighten cadence by one safe step
* if actual burn is far ahead of the reserve trajectory, soften cadence by one safe step
* ignore samples across a known reset boundary
* deduplicate repeated readings with the same used percentage so they do not fake a trend

Codex should increase Free Work cadence only when all of these are true:

* the user-declared remaining budget is high
* the estimated reset is approaching
* there is roadmap-aligned, architecture-compatible, bounded, decision-free work available
* gaming quiet mode is not blocking heavy work
* the repository state allows a safe disjoint implementation chunk

If no useful Free Work task exists, the correct budget-optimized behavior is to do nothing.

Planning caveat:

```text
Codex cannot directly read billing truth from memory.
Use user-declared readings, local Codex app-server rate-limit snapshots, and local work logs as approximate planning data.
```

---

# Free Work Cadence Tuning Rule

Free Work cadence should be re-evaluated periodically from local planning data.

The local cadence tuner is:

```text
tools/dev_attention/tune_freework_cadence.ps1
```

The local rate-limit reader is:

```text
tools/dev_attention/read_codex_rate_limits.ps1
```

Inputs:

* `.codex_local/usage_budget_state.md`
* `.codex_local/usage_budget_log.csv`
* `dist/private_reports/codex_rate_limits.json` when refreshed
* `dist/private_reports/project_progress_estimate.json`
* current `sn-bounded-free-work-execution` automation cadence

Output:

```text
dist/private_reports/freework_cadence_recommendation.json
```

The tuner recommends:

* Free Work RRULE
* interval hours
* chunk mode
* whether the Free Work automation should be updated
* why the recommendation was chosen

Default cadence policy:

```text
target reserve when reset is known: about 5% remaining at reset
>80% remaining budget: every 2 hours
60-80% remaining budget: every 4 hours
40-60% remaining budget: every 6 hours
<40% remaining budget: every 12 hours, critical-only
no useful roadmap work: every 24 hours or manual-only
high spendable budget near reset: hourly bounded chunks when needed to approach the reserve target
```

The cadence tuner may recommend a more frequent gate near reset, but it must not recommend overlapping implementation chunks.

One Free Work run still means at most one bounded useful chunk.

---

# Daily Operating Plan Rule

Once per day, Codex should create or refresh the Free Work operating plan for that day.

The daily plan should combine:

* latest local rate-limit snapshot
* usage budget log trend
* Free Work cadence recommendation
* maintenance balance
* project progress estimate
* roadmap readiness
* current blockers
* gaming quiet mode constraints

If the resulting plan stays inside already-approved rules, Codex may accept it automatically for that day.

Approved automatic changes include:

* choosing the current work mode from the budget thresholds
* preferring implementation over maintenance when maintenance share is too high
* selecting the next safe roadmap-aligned Free Work slice
* following the cadence recommendation when it matches approved budget rules
* lowering cadence or doing nothing when no useful safe work exists

Automatic acceptance must not choose:

* new architecture direction
* new gameplay philosophy
* privacy/legal/safety risk acceptance
* irreversible repository or account actions
* major scope expansion
* heavy work during gaming quiet mode

The daily plan should avoid report spam.
Create a Task Board report only when the plan materially changes, a blocker needs owner attention, or a useful summary prevents confusion.

---

# User Decision Boundary Rule

Codex should stop and ask the user when work requires:

* architecture direction choice
* gameplay philosophy choice
* risk acceptance
* native runtime interaction
* manual Stellaris testing
* dependency installation
* account authentication
* ambiguous design tradeoff

Codex should not spend Free Work budget deciding major project philosophy alone.

---

# Work Chunk Rule

Even in Free Work Mode, Codex should work in completed chunks.

Each chunk should preferably end with:

* tests run
* docs updated if relevant
* commit created
* push completed if appropriate
* CI checked when available

This prevents long invisible work sessions from becoming hard to review.

## Definition Of Done And Anti-Polish Rule

Free Work must not keep improving the same slice forever.

A worker-ready slice should be marked implemented or verified and left when:

* the agreed user-visible or runtime behavior exists
* relevant targeted tests pass, or the work is document-verifiable when code tests do not apply
* malformed, missing, or unsafe inputs fail safely instead of crashing or producing unsafe output
* the safety audit passes when the slice touches architecture, automation, generated overlay, workflow, or other safety-relevant areas
* remaining ideas are optional polish, broader design questions, or future scope rather than blockers

After that point, Codex should:

* record optional improvements as `Navrhy`
* update the roadmap or report with the verified state
* move to the next safe roadmap item

Codex may continue on the same slice only when a real bug, failed test, unsafe input path, owner-facing usability break, security/privacy/safety issue, or explicit owner request makes the slice not actually done.

---

# Background Concurrency Rule

Scheduled or background Free Work must protect foreground architecture work.

Before modifying files, background Free Work should check whether the repository has uncommitted changes.

Dirty worktree state is not automatically a global stop condition.

If dirty files are unrelated to the selected worker-ready task, background Free Work may continue only in a disjoint file set.

Before doing so, it must record the dirty baseline paths and ensure its planned write set does not overlap them.

If the selected task would need to edit a dirty file that was not created by the same background chunk, the background worker should not implement that task.

Allowed behavior in that case:

* choose another worker-ready task with a disjoint write set
* record a concise blocker/readiness note only if useful
* resume on a later run after the foreground work is committed or otherwise resolved

Background Free Work must not:

* overwrite foreground chat edits
* stage unrelated dirty files
* commit unrelated user/Codex changes
* start a second implementation chunk while a previous background chunk is still unresolved

If all useful roadmap work overlaps dirty foreground files, the worker should stop and create or update one visible task-board blocker instead of silently failing.

If future automation supports isolated worktrees reliably, background implementation may use an isolated worktree or branch, but it must still reconcile through reviewed commits and must not hide conflicts.

---

# Gaming Quiet Mode

When a known game is running, Free Work should enter gaming quiet mode.

Detection signals:

```text
any process listed in .codex_local/gaming_quiet_processes.txt exists
stellaris.exe process exists
```

The local process list is private machine configuration and should not be committed.

It should contain one process executable name per line.

Default required entry:

```text
stellaris.exe
```

Codex may add other known game executables to the local list when the owner identifies them.

Codex should not treat launchers such as Steam, Epic Games Launcher, Discord, or GPU utilities as games by default, because that would keep Free Work quiet too often.

If process detection is uncertain, prefer not running heavy work and create/update a concise Task Board note only when this uncertainty blocks useful work.

In gaming quiet mode, background Free Work may:

* read small local files
* update or consolidate Task Board reports/tasks
* record blockers
* do lightweight documentation checks
* run very small non-compiling commands only when they are unlikely to affect gameplay

In gaming quiet mode, background Free Work must not:

* compile C++ code
* run `cl.exe`, MSBuild, or full test suites
* run `tools/run_all_tests.ps1`
* run `tools/run_v0_pipeline_tests.ps1`
* run expensive search/audit loops
* spawn long-running workers
* perform disk-heavy package/export/build work

If a task requires compilation, full tests, package building, or other heavy local work while a known game is running, the worker should skip or defer the task and create/update a concise Task Board note only if useful.

The goal is to avoid adding CPU, disk, antivirus, and thermal load during gameplay.

Games should lag because of their own ambitions, not because Codex decided that now is a beautiful moment to compile C++.

---

# Uncommitted Worker Output Visibility Rule

Background Free Work must leave an owner-visible trace when it creates or modifies project files but cannot commit them.

This applies when:

* git commit fails
* repository permissions or locks prevent commit
* verification is incomplete
* a coherent chunk is useful but must remain uncommitted
* a dirty disjoint work set is intentionally left for a later foreground or background run

Required trace:

* create or update one Czech task-board Report or Suggestion
* list the affected file paths at a high level
* explain what was produced
* explain why it was not committed
* state the next safe action
* record the same event in `.codex_local/codex_work_log.csv` when possible

Use `Reporty` when the uncommitted output is an actual completed or partially completed work result that the owner should know exists.

Use `Navrhy` when the output mainly implies a better future workflow, architecture path, or implementation order.

Use `Ukoly pro me` only when the owner must make a decision, run a manual test, or perform an action before work can continue.

The worker must not leave meaningful uncommitted output hidden only in `git status`, shell logs, or chat history.

Technical retry rule:

If the only blocker is routine automation plumbing, such as a transient git lock, delayed commit, test process still finishing, temporary filesystem race, stale generated helper state, or an issue that Codex can likely fix in a later run, the worker should not create an owner task immediately.

Instead it should:

1. record the failure in `.codex_local/automation_run_log.csv`, `.codex_local/codex_work_log.csv`, or a concise report when useful
2. leave enough file/context detail for the next run to retry safely
3. retry on a later Free Work or hygiene run when the expected transient condition may have cleared
4. escalate to `Ukoly pro me` only after repeated failed retries, when owner action is actually required, or when the required action is outside Codex capability or permission

Examples that should stay internal/retry-first:

* commit failed because another git process probably held `index.lock`
* verification is still running and should be checked again later
* a worker produced a coherent code chunk that Codex can commit once the repository is writable

Examples that may become `Ukoly pro me`:

* the owner must choose architecture or gameplay behavior
* a manual Stellaris/MP test is required
* Windows account permissions must be changed by the owner
* an external app or account login blocks progress and Codex cannot safely resolve it

---

# Persistent Free Work Rule

When the user explicitly grants Free Work and says to continue until interrupted, Codex should continue working through successive completed chunks without waiting for a new prompt after each chunk.

This permission remains bounded by all other Free Work rules.

Free Work should follow the project-owner collaboration model in `PROJECT_OWNER_COLLABORATION_MODEL.md`.

The project owner is expected to define architecture, priorities, and manual test responses.

Codex is expected to implement, validate, document, and commit coherent chunks without requiring the owner to manually code or manually review every diff.

Foreground architecture rule:

```text
Architecture, roadmap direction, product intent, and gameplay philosophy are handled in the foreground chat with the owner.
Background Free Work is an implementation worker that follows the current architecture and roadmap.
```

Background Free Work may:

* implement roadmap tasks
* add tests
* fix bugs
* update docs that describe implemented or confirmed behavior
* record blockers and owner-facing reports

Background Free Work must not:

* silently choose major architecture direction
* treat lack of owner code review as permission to expand scope
* spend budget on speculative product philosophy
* consume the reserve needed for foreground architecture work
* expand meta-tooling when a non-blocked product/runtime roadmap task is available
* spend significant time on maintenance unless it removes repeated friction, fixes automation reliability, protects owner attention, or unblocks implementation
* sneak new architecture philosophy into implementation without a foreground decision or `Navrhy` proposal

Maintenance balance rule:

Codex should track whether recent work is mostly product/runtime implementation or mostly maintenance.

Planning target:

```text
maintenance around 25% of recent logged work
implementation and product/runtime progress around 75%
```

This is an efficiency target, not a hard law.
Early data is sparse, so Codex should use it as a trend signal for several days before over-optimizing.
The target exists to simplify or defer maintenance that is too broad, repetitive, or low-value.

Maintenance is justified when it:

* removes repeated owner friction
* fixes automation reliability
* protects privacy, safety, or repository integrity
* unblocks implementation
* prevents repeated wasted compute

Maintenance should be deferred or simplified when it:

* only polishes coordination tools
* adds process without a concrete pain
* creates more owner-facing items than it removes
* consumes Free Work time while safe roadmap implementation is available

The local helper `tools/dev_attention/measure_maintenance_balance.ps1` writes `dist/private_reports/maintenance_balance.json` from `.codex_local/codex_work_log.csv`.
Task-board sync may refresh that cached measurement.
If maintenance exceeds a practical share of recent logged work, Free Work should prefer the next safe product/runtime roadmap slice and turn further maintenance ideas into `Navrhy` instead of implementing them immediately.

When `dist/private_reports/maintenance_balance.json` reports:

```text
recommendation = pause-nonessential-maintenance
```

Free Work should actively choose the next safe SNC/product/runtime implementation slice when one is available.
Nonessential Task Board polish, workflow tuning, report reshaping, and automation hygiene should be recorded as `Navrhy` or deferred unless they are blocking, safety-critical, privacy-critical, or preventing repeated wasted work.

Suggested response by ratio:

* below 25% maintenance: normal support range
* 25-30% maintenance: watch the trend
* above 30% maintenance: prefer product/runtime implementation when safe
* above 40% maintenance: pause nonessential maintenance unless it is blocking, safety-critical, privacy-critical, or prevents repeated wasted work

After several days of data, Codex should review which recurring checks are too broad, too frequent, or poorly scoped and propose simplifications instead of blindly keeping them.

Codex should keep going while:

* the next work item is clear
* the work is aligned with the roadmap after checking the roadmap against current architecture docs
* the work is locally testable or document-verifiable
* the current declared usage budget still supports the effort
* a reasonable reserve remains for user decisions, blockers, and review

Codex must pause and ask the user when:

* strategic direction becomes unclear
* a major architecture, gameplay philosophy, multiplayer, save integrity, privacy, or LLM authority decision is required
* implementation would cross into risky or irreversible territory
* the next useful task is speculative or low value
* the declared usage budget becomes stale or low enough to require conservation

Persistent Free Work means:

```text
continue valuable bounded chunks until interrupted or blocked
```

not:

```text
unbounded compute spending, speculative research loops, or silent risky architecture decisions
```

---

# Scope Freeze Rule

When the owner approves a scope freeze or Codex identifies that a slice needs stabilization, Free Work should keep the selected slice stable until it is implemented, verified, or explicitly reopened.

During a freeze, allowed work is:

* implementation of the frozen slice
* tests and validation for the frozen slice
* documentation that records implemented or already-approved behavior
* bug fixes
* critical contradiction fixes
* owner-visible `Navrhy` for later improvements

During a freeze, avoid:

* adding new feature branches to the slice
* expanding product philosophy
* adding speculative architecture
* doing non-blocking Task Board polish
* converting every small idea into immediate implementation

If a new idea appears during a freeze, record it in `Navrhy` unless it is a bug, blocker, safety issue, or explicit owner-approved change to the frozen scope.

Free Work should treat the current most valuable vertical slice as the default freeze candidate:

```text
verified archive -> season delta ledger -> empire brief -> validated DSL -> generated overlay -> Status Center visibility
```

---

# Roadmap Revalidation And Order Rule

Before starting each new Free Work task, Codex should treat the roadmap as the default ordered implementation sequence, not as an unordered menu.

Codex must briefly revalidate the next roadmap item in order against current canonical architecture:

1. `MASTER_ARCHITECTURE_INDEX.md`
2. `ARCHITECTURE.md`
3. `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md`
4. relevant subsystem rule documents
5. recent committed implementation state

If the roadmap item still matches the architecture and is not blocked, Codex should continue with that item.

If the roadmap is stale or conflicts with the current architecture, Codex should first:

* update the roadmap when the correction is obvious and low-risk
* mark or note the stale/conflicting item as blocked or skipped when it cannot be corrected immediately
* record what would unblock it and the current date/commit context when useful
* continue with the next unblocked roadmap item in order
* ask the user only when no useful safe task remains or when the conflict must be resolved before meaningful progress can continue

Codex should choose the next task by:

* roadmap priority
* roadmap order within the active vertical slice
* user availability
* local testability
* implementation value
* safety and rollback simplicity
* remaining declared usage budget

The roadmap should guide Free Work in order, but it must not override architectural truth.
Codex should not skip ahead because a later item is easier, more interesting, or cosmetically nicer.

---

# Free Work Status Tracking Rule

Free Work should not stop just because one roadmap item is stale, blocked, or needs user input.

When the next roadmap item cannot be worked safely, Codex should record its state and move to the next unblocked roadmap item in order when possible.

Use these states in docs, task-board notes, or implementation notes as appropriate:

* `ACTIVE` - currently being worked
* `DONE` - completed and verified enough for the current phase
* `SKIPPED_STALE` - roadmap item conflicts with current architecture
* `SKIPPED_LOW_VALUE` - item is technically possible but not worth current budget; use sparingly and not to bypass the active stabilization slice
* `BLOCKED_USER` - needs project-owner input
* `BLOCKED_EXTERNAL` - needs external/manual action, unavailable tool, or unavailable environment
* `RESUME_CANDIDATE` - should be reconsidered after architecture, budget, or user input changes

For skipped or blocked items, Codex should record:

* task name
* reason
* source document or roadmap section
* what would unblock it
* date or commit context when useful

If user interaction is required, Codex should add or update a visible task-board entry under `tools/dev_attention/` when the request is important enough that chat history may be missed.

When the situation changes, Codex should update or clear the stale/blocked record instead of leaving tasks skipped forever.

Free Work should prefer:

```text
record blocker -> continue next unblocked roadmap item in order
```

over:

```text
stop all progress because one roadmap item needs interaction
```

---

# Draft Free Work Rule Rule

During persistent Free Work, Codex may discover workflow rules that appear necessary for autonomy to remain safe and efficient.

If such a rule is not already approved, Codex may add it to project docs only as:

```text
Status: DRAFT - NOT APPROVED
```

Codex must inform the user and wait for approval before using that draft rule to guide work.

Draft rules should be revisited, approved, revised, or removed.
They must not become hidden operating policy by accident.

---

# Budget Efficiency Rule

Codex should use the smallest adequate reasoning effort for the task.

Examples:

* low effort for simple docs, git, instructions, and small deterministic edits
* medium effort for normal implementation and tests
* high effort for architecture, security, synchronization, and complex debugging

The goal is not to minimize capability.

The goal is to match compute intensity to task value.

---

# Burn Rate Estimation Rule

Codex should use the local usage budget log as a planning signal, not only as a static percentage.

When enough readings exist, Codex should estimate approximate burn rate from:

* user-declared remaining budget readings
* timestamp differences between readings
* local Codex work windows and commits
* rough work type: docs, implementation, tests, architecture, or debugging

The estimate is approximate because Codex cannot directly read account usage.

Codex should keep local work-window records in:

```text
.codex_local/codex_work_log.csv
```

Each meaningful work session should record:

* start time
* end time
* timezone
* mode: interactive, freework, automation, review, or debug
* short summary
* commit SHA when available
* notes for resets, blockers, or unusual work intensity

Scheduled Free Work also keeps a lightweight run log:

```text
.codex_local/automation_run_log.csv
```

Each Free Work gate invocation should append a `started` row and a final result row with the same run id.
Allowed trigger values are:

* `scheduled`
* `manual_ui`
* `unknown`

Codex should use `manual_ui` only when the owner explicitly identifies the invocation as manually started.
Otherwise, if the app does not expose the trigger source to the automation, use `unknown`.

Allowed result values are:

* `implemented`
* `blocked`
* `quiet`
* `no_safe_task`
* `failed`

This log exists to answer a practical question quickly:

```text
Did the automation fire, and if yes, why did it not produce implementation work?
```

Use the estimate to choose:

* whether Free Work should run at all
* the maximum size of the next work chunk
* reasoning effort for the next task
* whether to preserve extra reserve before the next reset

If the log is sparse or ambiguous, Codex should use conservative chunk sizing and ask for a fresh user-reported budget reading when that would materially improve planning.

Useful derived metrics:

```text
budget points spent per hour of active Free Work
budget points spent per implementation chunk
budget points spent per docs/architecture chunk
remaining safe hours before reset
```

Do not pretend these estimates are exact billing data.

---

# Context Preservation Rule

Free Work should produce project memory where useful.

When work discovers durable project knowledge, Codex should record it in docs rather than relying on chat memory.

This is especially important for:

* Stellaris version behavior
* architecture decisions
* test results
* CI constraints
* failed approaches

---

# Design Goal

Strategic Nexus should use available Codex capacity like an engineering budget:

```text
high budget -> autonomous valuable progress
medium budget -> focused implementation
low budget -> preserve capacity for blockers
```

not:

```text
high budget -> uncontrolled compute spending
low budget -> accidental exhaustion before critical work
```

Free Work exists to accelerate development while preserving judgment, reviewability, and architectural discipline.
