# Strategic Nexus - Documentation Consistency Audit

## Purpose

This document records focused audits for contradictions between project Markdown files.

The goal is to prevent old architecture notes from confusing future work.

---

# 2026-05-23 Offline Architecture Consistency Pass

Status:

```text
APPROVED - APPLIED
```

## Trigger

The project owner asked whether Markdown files might contradict each other and confuse future Codex work.

## Audit Focus

The audit focused on conflicts between:

* old realtime daemon/bridge assumptions
* new offline companion architecture
* generated overlay staging
* multiplayer delivery expectations
* roadmap priority wording

## Findings

### Fixed

The following conflicts were corrected:

* `MULTIPLAYER_REQUIREMENTS.md` no longer describes host daemon delivery as the preferred production flow.
* `MASTER_ARCHITECTURE_INDEX.md` multiplayer architecture now points to byte-identical generated overlay preparation before launch.
* `MASTER_ARCHITECTURE_INDEX.md` v0 implementation priority no longer says to implement a minimal daemon pipeline skeleton as the next canonical step.
* `V0_SCOPE_AND_PIPELINE_PLAN.md` now records the generated overlay DSL/compiler harness progress.
* General strategy rule docs now use host/coordinator offline analysis wording instead of host daemon wording where production architecture is implied.
* `BRIDGE_CORE_TEST_PLAN.md` now frames bridge output as development harness/generated-overlay preparation intent, not future host-only bridge delivery.

### Intentionally Left As Historical

Some older PoC/result documents still mention host-only daemon delivery or bridge delivery.

Those are historical test records, not current production architecture.

They should be read through:

* `LEGACY_RUNTIME_ARCHITECTURE_INTERPRETATION_RULES.md`
* `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md`
* `TECHNICAL_RUNTIME_LIMITATIONS.md`

Historical PoC documents should not be rewritten aggressively unless their status sections become actively misleading.

## Current Canonical Interpretation

Current production direction is:

```text
companion app archives autosaves
-> offline analysis after session
-> validated DSL
-> deterministic generated overlay
-> next launch consumes ordinary mod files
```

Multiplayer direction is:

```text
host/coordinator prepares generated overlay before launch
-> participants receive byte-identical gameplay-affecting generated files
-> ordinary scripted state synchronizes during MP
```

## Remaining Watch Items

Future audits should watch for:

* roadmap items that imply live LLM delivery
* docs that describe daemon/bridge as production transport instead of development harness
* multiplayer docs that imply per-client generated strategic decisions
* status text saying generated overlay layout is missing after `GENERATED_OVERLAY_LAYOUT_CONTRACT.md`
* stale `NOT_STARTED` status after implementation skeletons are added

## Verification

After this audit, `tools/run_safety_audit.ps1` should pass before commit.

---

# 2026-05-24 Foreground Architecture / Background Worker Pass

Status:

```text
APPROVED - APPLIED
```

## Trigger

The project owner clarified the development model:

```text
foreground chat defines architecture and roadmap
background Free Work acts as an implementation worker
```

The owner does not plan to manually review code, so background automation must be tightly bounded by architecture, roadmap, validation, and budget rules.

## Audit Focus

The audit checked consistency between:

* `PROJECT_OWNER_COLLABORATION_MODEL.md`
* `FREE_WORK_AND_USAGE_BUDGET_RULES.md`
* `AUTOMATION_CANDIDATES.md`
* `DEVELOPMENT_ROADMAP.md`
* `MASTER_ARCHITECTURE_INDEX.md`

## Findings

### Already Consistent

The core docs already agreed that:

* the owner owns architecture intent, priorities, gameplay philosophy, and final tradeoff decisions
* Codex owns implementation, tests, docs, validation, commits, and warnings
* Free Work must not silently make major architecture decisions
* roadmap items must be revalidated against current architecture
* missing owner decisions should be recorded visibly instead of stopping all useful work

### Fixed

The following gaps were corrected:

* `DEVELOPMENT_ROADMAP.md` now defines what makes a roadmap item worker-ready for background Free Work.
* `FREE_WORK_AND_USAGE_BUDGET_RULES.md` now includes a background concurrency rule so scheduled Free Work does not collide with foreground chat edits or unrelated dirty files.
* `AUTOMATION_CANDIDATES.md` now requires the bounded Free Work execution gate to check unresolved dirty worktree state before modifying files, record a dirty baseline, and work only in disjoint files.
* The real `SN Bounded Free Work Execution` automation prompt was updated to follow the same worker-only, dirty-worktree, and no-overlap rules.

### Additional Check Added

The owner did not explicitly ask about concurrency, but it is a practical risk in the new model.

Because foreground architecture chat and background implementation can run near the same time, scheduled Free Work must not stage, overwrite, or commit unrelated dirty files.

Dirty state alone should not permanently stop background work.

If unrelated dirty files exist, the worker may continue only on a worker-ready task whose write set is disjoint from the dirty baseline.

This is now a required pre-flight gate.

## Current Canonical Interpretation

```text
owner/Codex foreground chat
-> architecture, roadmap, product intent, risk acceptance, budget policy
-> worker-ready roadmap subtasks
-> background Free Work implements one bounded validated chunk
-> tests/audits/commit/report/work-log
```

## Verification

This pass was verified with:

```text
tools/check_docs_encoding.ps1
tools/run_safety_audit.ps1
```

---

# 2026-06-05 Reactive Policy Pack Architecture Pass

Status:

```text
APPROVED - APPLIED
```

## Trigger

The project owner identified a product flaw in the pure session-to-session model:

```text
If the player runs one long session, Strategic Nexus may not affect gameplay until a later session.
Strategy updates are not regular or event-reactive enough.
```

The owner approved a safer near-realtime direction that remains legal and ordinary-mod based.

## Audit Focus

The audit focused on docs that could incorrectly force Strategic Nexus to stay post-session-only:

* runtime limitation docs
* offline analysis architecture
* DSL/compiler boundary
* generated overlay layout
* roadmap and sprint-mode work selection
* entry-point/branch safety rules

## Findings

### Fixed

The canonical direction now distinguishes:

```text
forbidden: live LLM decisions entering the already-running game
allowed: precompiled reactive policy branches loaded before play and selected by ordinary Stellaris script
```

Updated architecture wording now points future work toward a generated reactive policy pack.

The policy pack is still:

* generated only from validated DSL
* compiled deterministically
* published only while Stellaris is closed
* entry-point scoped
* manifest verified
* multiplayer byte-identity aware

### New Canonical Document

Added:

* `REACTIVE_POLICY_PACK_ARCHITECTURE.md`

## Current Canonical Interpretation

Current production direction is now:

```text
companion app archives autosaves
-> offline analysis after session
-> validated DSL with allowlisted event-family branches
-> deterministic generated reactive policy pack
-> next launch consumes ordinary mod files
-> active session selects among already-loaded branches through ordinary on_actions/events/triggers
```

This remains incompatible with:

* game-process address-space hooks
* loading external binary code into the running game process
* live game patching
* process command/control
* raw LLM Stellaris script
* per-client multiplayer LLM generation

## Remaining Watch Items

Future audits should watch for:

* docs that treat "not realtime" as forbidding precompiled event-reactive behavior
* docs that imply SNC may publish gameplay files while Stellaris is running
* DSL features that expose raw on_action names or raw Stellaris script to the LLM
* roadmap chunks that add broad strategy complexity before the first reactive branch is verified
