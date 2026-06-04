# Strategic Nexus - v0 Sprint Mode

## Purpose

This document defines the temporary acceleration mode for reaching the first useful v0 real-game validation loop as quickly as safely possible.

It does not replace the roadmap or architecture.
It narrows how Codex and Free Work choose, finish, verify, and commit implementation chunks while the owner priority is speed to a functional v0.

Canonical inputs:

* [MASTER_ARCHITECTURE_INDEX.md](MASTER_ARCHITECTURE_INDEX.md)
* [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md)
* [V0_SCOPE_AND_PIPELINE_PLAN.md](V0_SCOPE_AND_PIPELINE_PLAN.md)
* [PROJECT_OWNER_COLLABORATION_MODEL.md](PROJECT_OWNER_COLLABORATION_MODEL.md)
* `tools/dev_attention/v0_sprint_chunk_queue.json`

---

# Sprint Goal

The sprint goal is:

```text
first owner-testable Strategic Nexus v0 loop on real Stellaris sessions
```

The desired loop is:

```text
SNC preserves session saves
-> offline spine builds bounded evidence
-> generated overlay/package is staged or reported
-> Status Center tells the owner what is ready or unsafe
-> owner can run another real game session
-> Codex can compare whether the intended v0 effect is observable
```

Work that directly advances this loop is preferred over polish, wider feature design, broad refactors, nicer wording, extra UI decoration, or speculative intelligence expansion.

---

# Non-Negotiables

Sprint Mode is not permission to bypass safety.

Every chunk must still obey:

* project safety scope
* OpenAI and legal boundaries
* dirty worktree discipline
* no overwrite of unrelated user/Codex changes
* no unsafe save editing
* no raw LLM authority over generated mod output
* no hidden owner-facing persistence pattern that triggers avoidable security alarms
* no unbounded background work without logs

If a requested or queued chunk conflicts with those boundaries, Codex must mark it blocked or replace it with a safer bounded design.

---

# Chunk Definition

A sprint chunk is good enough when it is:

* aligned with the current roadmap and architecture
* small enough to implement and verify in one coherent commit
* testable locally or document-verifiable
* independent of new owner decisions
* useful for the first real-session v0 loop
* safe to stop after without leaving ambiguous dirty state

If a chunk becomes broader than this, split it.

---

# Selection Rule

Default selection order:

1. Read `tools/dev_attention/v0_sprint_chunk_queue.json`.
2. Pick the first `ready` chunk whose dependencies are done or already satisfied by current code.
3. If the first chunk is already implemented, verify it and record the evidence instead of reimplementing it.
4. If it is blocked, record the blocker and take the next unblocked chunk.
5. If the queue is stale, update the queue from the current `DEVELOPMENT_ROADMAP.md` next worker-ready slice before implementing.

The foreground chat may override the queue.

Background Free Work should not invent a new strategic direction when queued worker-ready chunks exist.

`no_safe_task` is allowed only when all of these are true:

* the sprint queue has no available chunk
* `DEVELOPMENT_ROADMAP.md` has no unblocked `IN_PROGRESS` or `NOT_STARTED` implementation/documentation/verification work that can be made into a bounded chunk
* no useful verification, regression, fail-safe hardening, documentation correction, or roadmap queue refresh can be done without owner input

If the queue is empty or fully verified while the roadmap still contains unblocked work, the worker must not stop.
It must either:

* append the next bounded roadmap-derived chunk to `tools/dev_attention/v0_sprint_chunk_queue.json`, commit that queue refresh, and then claim it, or
* implement a directly obvious bounded roadmap chunk and update the queue/roadmap afterward.

Owner readiness reports are not a stop condition.
After reporting that an owner test is possible, continue with the next unblocked roadmap implementation unless all remaining useful work genuinely depends on that owner test.

---

# Claim And Overlap Rule

Before editing, a worker should create a run id and check:

* current `git status --short`
* current `git rev-parse HEAD`
* recent `.codex_local/automation_run_log.csv`
* whether another visible run is already implementing the same chunk

If another run is using the same write set, the worker must switch to read-only verification or choose a disjoint queued chunk.

Runtime claim/ledger state may live in `.codex_local/` because it is machine-local coordination state.
The source-controlled queue describes intended work; local ledgers describe live claims and evidence.

Use `tools/dev_attention/claim_v0_sprint_chunk.ps1` when a worker needs a concrete claim:

```powershell
tools/dev_attention/claim_v0_sprint_chunk.ps1 -AutomationId sn-bounded-free-work-execution-2 -RunId <run-id>
tools/dev_attention/claim_v0_sprint_chunk.ps1 -Mode complete -ChunkId <chunk-id> -RunId <run-id> -Evidence <test-or-summary> -Commit <sha>
tools/dev_attention/claim_v0_sprint_chunk.ps1 -Mode block -ChunkId <chunk-id> -RunId <run-id> -Evidence <blocker>
```

The helper writes only local runtime claim state under `.codex_local/`; it does not mutate the source-controlled queue.

---

# Finish Rule

For each implemented chunk:

1. Run the smallest meaningful targeted validation.
2. Run broader v0 pipeline tests when the chunk touches shared contracts, command surfaces, generated overlay output, Status Center output, save/archive behavior, or release-companion evidence.
3. Ensure malformed, missing, stale, or mismatched inputs fail safely where the chunk accepts external or generated data.
4. Commit the chunk before starting the next implementation chunk.
5. Push when safe and useful for continuity.
6. Update roadmap/current progress only when the fact is durable enough to help future workers.

Do not keep polishing a chunk after the agreed behavior exists and validation passes.
Record follow-up polish as `Navrhy` or queue a later chunk only if it affects v0 usability.

---

# Two-Lane Execution

Sprint Mode allows two safe lanes:

* `implementation`: build the next missing v0 piece.
* `verification`: prove an already-built piece works, add regression coverage, or clarify readiness evidence.

Both lanes use the same architecture and queue.
They must avoid overlapping write sets.

If compute budget is abundant, run more verified chunks.
Do not spend surplus budget on open-ended redesign when queueable v0 implementation remains.

---

# Owner Test Readiness

Codex should explicitly tell the owner when the project reaches an owner-testable point, for example:

```text
You can now run a real Stellaris session test for save capture / generated overlay staging / MP package handoff.
```

The readiness notice must include:

* what exactly is ready to test
* what the owner should do
* what evidence Codex will inspect afterward
* any known limitations

Until such a notice exists, Free Work should keep moving through the v0 queue instead of waiting for the owner.
After such a notice exists, Free Work should still continue with unrelated or follow-on roadmap chunks unless the next meaningful implementation depends on the owner test result.
