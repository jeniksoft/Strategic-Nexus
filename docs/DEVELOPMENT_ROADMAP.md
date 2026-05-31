# Strategic Nexus - Development Roadmap

## Purpose

This document defines the implementation order of Strategic Nexus.

The project must evolve incrementally.

Architecture stability is more important than immediate strategic complexity.

New systems should be added gradually using already established patterns.

The roadmap is a living document and must be continuously updated.

Before using this roadmap for autonomous Free Work, revalidate the selected item against the current canonical architecture documents.
If this roadmap conflicts with newer architecture decisions, update the roadmap or ask the project owner before implementing the stale item.
Roadmap order is the default implementation order.
Codex should finish the current active roadmap slice before starting the next one, then take the next unblocked item in order.
Codex should skip ahead only when the current item is genuinely blocked, stale, unsafe, externally unavailable, or requires owner input before meaningful progress can continue.
When Codex skips a roadmap item, it must record the blocker, what would unblock it, and the date/commit context when useful, then continue with the next unblocked item in roadmap order.
Speculative optimization, polish, or preference for a more interesting task is not a valid reason to skip roadmap order.

Use `MASTER_ARCHITECTURE_INDEX.md` as the canonical navigation layer for project architecture.

Use `V0_SCOPE_AND_PIPELINE_PLAN.md` as the current implementation boundary for the first bounded offline validation and pipeline skeleton.

Use `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md` as the revised production direction for autosave archiving, offline analysis, campaign memory, and next-session mod refresh.

Use `CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md` as the target release UX/automation direction for minimizing mandatory user interaction.

Use `MULTIPLAYER_SEASON_ORCHESTRATOR.md` as the target release direction for low-friction host-coordinated MP seasons.

Use `META_RULE_LANGUAGE_AND_COMPILER.md` for the bounded DSL and deterministic compiler that converts LLM proposals into generated mod overlays.

Use `LOCAL_LLM_INTEGRATION_CONTRACT.md` for the model-weights, local runtime, companion Model Manager, validation, reduced-mode, and mod boundary contract.

---

# Current Scope Discipline

Roadmap work should prefer completed vertical slices over broad parallel expansion.

Work should proceed sequentially inside the active stabilization slice.
Each step should be implemented, verified, documented when needed, and either marked done or explicitly blocked before Codex moves to the next step.

Default stabilization slice:

```text
verified archive -> season delta ledger -> empire brief -> validated DSL -> generated overlay -> Status Center visibility
```

Until that path is robust enough to feel like a real product spine, new strategic intelligence features should usually be recorded as `Navrhy` instead of immediately implemented.

Task Board work remains allowed when it fixes bugs, reliability, owner visibility, or repeated friction.
Otherwise, prefer product/runtime roadmap progress over additional meta-tooling.

---

# Worker-Ready Roadmap Rule

The foreground owner/Codex chat owns architecture direction, roadmap changes, product intent, gameplay philosophy, risk acceptance, and budget policy.

Background Free Work is an implementation worker.

Roadmap items are good background-worker candidates only when they can be reduced to a bounded task that is:

* aligned with current architecture
* locally testable or document-verifiable
* independent of missing owner decisions
* safe to commit as one coherent chunk
* scoped to implementation, validation, tests, or documentation of confirmed behavior

Roadmap items that require architecture direction, gameplay philosophy, multiplayer risk acceptance, save-integrity tradeoffs, or LLM authority decisions must stay foreground-owned until the owner resolves them.

When a roadmap item is too broad for background execution, Codex should split out a worker-ready subtask instead of letting the worker decide the strategic direction.

---

# Status Definitions

## NOT_STARTED

The system or milestone does not exist yet.

## IN_PROGRESS

Partial implementation exists.
Testing or architecture work is ongoing.

## IMPLEMENTED

The system exists and basic functionality is verified.

## VERIFIED

The system has been tested and validated successfully.

## DONE

A roadmap slice is done when the agreed behavior exists, relevant tests or document-verification pass, known unsafe input paths fail safely, and remaining ideas are non-blocking improvements.

When this threshold is met, Codex should mark the slice as implemented or verified, record any follow-up ideas as `Navrhy`, and move to the next roadmap item instead of continuing open-ended polish.

## BLOCKED

Work cannot continue until another dependency is solved.

---

# Development Philosophy

Strategic Nexus must be built in layers.

Do NOT attempt:

* full strategic complexity immediately
* massive doctrine sets immediately
* huge prompt systems immediately
* full empire simulation immediately

Instead:

* build minimal stable loops
* verify architecture
* expand incrementally
* preserve compatibility
* keep systems modular
* keep systems bounded

---

# Rule Of Expansion

Start with:

* small enum sets
* small strategy domains
* small payload schemas
* small memory systems

Expand later using the same architectural patterns.

The architecture must be additive and extensible.

---

# Current Development Priorities

## 0. Task Board And Owner Report Log

Status:
IMPLEMENTED

Goal:
Make the task board the reliable owner-facing coordination surface for manual decisions, progress visibility, and important work reports.

Required:

* active tasks with `progress_percent`
* automatically remove active tasks once verified as 100% complete
* additive report log for important work summaries
* daily batched automation report instead of hourly report spam
* hourly task-board hygiene automation
* separate UI modes/tabs for owner tasks and work reports
* clear separation between action tasks and report entries

Notes:
This is currently the highest priority because the project owner uses the task board instead of reviewing chat history or code diffs manually.
Task-board UI must be in Czech.
Task-board tasks and reports must also be written in Czech by default.
If Windows PowerShell cannot reliably render diacritics, use readable Czech without diacritics rather than broken text.

Current progress:
The task board can display active tasks and archived reports from separate JSON files under separate modes.
Active tasks with `progress_percent >= 100` are filtered out.
Helper scripts exist for adding reports and updating task progress.
`tools/dev_attention/sync_user_task_board_state.ps1` reconciles locally verifiable task state such as gaming quiet mode blockers when configured game processes are no longer running.
The same sync helper writes local gaming quiet start/end transitions to `.codex_local/gaming_quiet_session_log.csv`; start time comes from the process when available, and end time is the hygiene/sync observation time.
Task Board project completion and remaining-time estimates are automatically derived from roadmap priority statuses, estimated roadmap complexity, and logged implementation work when enough data exists. The helper `tools/dev_attention/analyze_roadmap_complexity.ps1` writes `dist/private_reports/roadmap_complexity_estimate.json`. The helper `tools/dev_attention/log_implementation_work.ps1` appends real implementation time and completed complexity points to `.codex_local/implementation_work_log.csv`. The helper `tools/dev_attention/set_project_progress_estimate.ps1` regenerates `dist/private_reports/project_progress_estimate.json`, and the live TB UI refreshes that estimate automatically.
Task-board sync/hygiene should also call `tools/dev_attention/sync_user_task_board_state.ps1`, which refreshes both project progress and roadmap complexity outputs. This keeps the estimate current after roadmap edits even when the visible Task Board window is not the thing that noticed the change.
Remaining work is polish and automation hygiene, not the core two-mode TB function.

---

## 1. Core Runtime Skeleton

Status:
IN_PROGRESS

Goal:
Create minimal stable end-to-end architecture for the campaign orchestrator, offline campaign analysis, and next-session generated mod overlays.

Required:

* companion app lifecycle skeleton
* campaign orchestrator flow that automates safe routine staging decisions
* optional start-with-Windows setting with first-install default disabled
* first-run explanation for why early app startup helps preserve autosave history
* tray/background lifecycle where window close keeps the app running, but explicit user exit stops it
* watchdog restart only for unexpected crashes, with backoff and crash-loop guard
* no auto-restart after explicit user exit or normal OS shutdown/restart/logoff
* local crash/support report preparation with explicit approval before sending to `support@jeniksoft.cz`
* companion Model Manager for user-selected supported local LLM models
* local LLM integration contract for model weights, runtime, companion, validator, reduced-mode, and mod separation
* no LLM model weights distributed with the Stellaris mod
* reduced-mode Status Center warning when no supported local model is installed
* autosave archive skeleton
* local save campaign directory monitor
* autosave cadence guidance for more precise history steps
* parser skeleton
* read-only Stellaris `.sav` ZIP container reader for `meta` and `gamestate`
* offline worker skeleton
* latest-session-only analysis by default
* payload schema
* Strategic Nexus DSL parser/validator/compiler skeleton
* payload validation
* campaign identity handling
* sequence handling
* fallback handling
* audit logging
* bounded enum validation
* doctrine persistence
* generated overlay staging
* generated overlay compilation from validated DSL
* generated campaign library cleanup and rebuild
* full rewrite generated overlay publishing, not append-only updates
* durable on-disk Strategic Nexus archive and memory store
* release Status Center for ordinary user status, warnings, support approval, archive status, and generated overlay status

Notes:
Do not optimize strategic intelligence yet.
Optimize architecture stability.

Current implementation boundary:
Follow `V0_SCOPE_AND_PIPELINE_PLAN.md` for bounded payload validation and deterministic pipeline shape.
Follow `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md` for the production delivery model.
Follow `CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md` for release automation and low-interaction UX.
Follow `LOCAL_LLM_INTEGRATION_CONTRACT.md` for local model installation, runtime trust boundaries, reduced mode, and validator-owned promotion into generated overlays.

The first pipeline should prove the small `verified archive -> season delta ledger -> empire brief -> validated DSL -> generated overlay` direction before expanding strategic complexity.
Generated overlays should come from validated DSL programs, not raw LLM script.
Generated overlay work must include functional behavior verification, not only parser, schema, manifest, or file hash verification.

Realtime LLM decisions entering an already-running game are no longer a production architecture goal.

Current progress:
The first generated overlay contract skeleton exists under `src/generated_overlay/`.
It parses the small campaign/empire/rule DSL shape, rejects unsafe identifiers, unsupported preferences, out-of-budget values, and tactical-style domains, then emits deterministic generated event, scripted effect, and scripted trigger text for contract tests.
`Strategic Nexus.exe --compile-generated-overlay <input.dsl> <output_dir>` can stage the generated overlay file layout for local testing.
`GENERATED_OVERLAY_LAYOUT_CONTRACT.md` records the current v0 generated file layout.
The compiler now emits `strategic_nexus_generated_manifest.json` with complete replacement snapshot intent, generated gameplay file paths, checksum relevance classification, deterministic content hashes, and byte counts.
`Strategic Nexus.exe --verify-generated-overlay <output_dir>` verifies generated files against the manifest and fails closed on drift.
The generated overlay verifier also fails closed on unknown v0 generated overlay file paths or wrong checksum relevance classifications.
`Strategic Nexus.exe --archive-stable-saves <save_root> <archive_root> <session_id> [stability_delay_ms]` provides the first read-only one-shot autosave archive harness and manifest writer.
`Strategic Nexus.exe --archive-live-saves <save_games_root> <archive_root> <session_id> [stability_delay_ms]` provides the first live-session archive pass for stable `autosave*.sav` and `ironman.sav` revisions across campaign folders, preserving repeated active filenames as hash-named archive copies.
`tools/watch_stellaris_live_autosaves.ps1` wraps the live archive pass with filesystem event watching plus repeated sweeps, so a long active game session can retain the full observed autosave timeline even when Stellaris keeps only a small rolling buffer.
`tools/start_stellaris_live_autosave_watcher.ps1` and `tools/install_stellaris_live_autosave_watcher_task.ps1` provide hidden background startup capture; when Task Scheduler is unavailable, the installer uses the current user's Windows Run key.
`Strategic Nexus.exe --verify-autosave-archive <session_archive_dir>` verifies copied saves against the archive manifest before offline analysis uses them.
`Strategic Nexus.exe --summarize-autosave-archive <session_archive_dir> <summary_output.json>` emits a bounded verified archive metadata summary for future offline analysis handoff.
Stellaris `.sav` files should be read as ZIP containers with `meta` and `gamestate` entries; the first real parser should operate on verified archived copies, extract bounded structured facts, and never send raw `gamestate` text to the LLM.
`Strategic Nexus.exe --parse-stellaris-save <save.sav-or-extracted-save-dir> <summary_output.json>` now provides the first narrow read-only save headline parser for `meta`/`gamestate`, including player country resolution, empire/species/capital/home-system headlines, owned fleet headlines, active war count, explicit missing fields, and raw-LLM-input safety uncertainty.
`Strategic Nexus.exe --build-season-delta-ledger <session_archive_dir> <campaign_id> <output_json>` now augments verified-archive metadata with bounded parsed latest-save headline facts when available (`metadata_plus_save_headline`), while failing safely back to metadata-only uncertainty reporting.
`Strategic Nexus.exe --build-empire-brief-from-archive <session_archive_dir> <campaign_id> <empire_id> <output_json>` now accepts both `metadata_only` and `metadata_plus_save_headline` source quality and still emits explicit missing parser fields.
`Strategic Nexus.exe --build-ministry-input-from-archive <session_archive_dir> <campaign_id> <empire_id> <ministry> <output_json>` now uses the same season-delta ledger feed as archive/brief, so ministry input can include bounded parsed-headline facts when available, while keeping explicit uncertainty reporting.
`Strategic Nexus.exe --v0-pipeline-from-archive <session_archive_dir> <campaign_id> <empire_id> <ministry> <ministry_input_output_json> <decision_output_json> <sequence_id> <created_unix_ms> <ttl_ms> [audit_output_json]` now derives ministry input from that same ledger path before running the deterministic v0 pipeline.
`Strategic Nexus.exe --run-offline-spine <session_archive_dir> <campaign_id> <empire_id> <input.dsl> <work_dir> <overlay_output_dir> <status_output_json>` verifies one archive session, writes season delta ledger and empire brief with optional bounded parsed latest-save headline facts, compiles an explicit validated DSL into a generated overlay, verifies the overlay manifest, and emits an SNC status snapshot.
This is the first tested vertical product spine for `verified archive -> season delta ledger -> empire brief -> validated DSL -> generated overlay -> Status Center visibility`.
`tools/run_real_session_v0_loop.ps1` / `.cmd` now provide a single-command owner test loop for real saves (`archive-stable-saves -> verify-autosave-archive -> summarize-autosave-archive -> run-offline-spine`) and emit concrete output paths for the next in-game validation session.
The same real-session loop now supports optional MP package export (`-ExportMpPackage`) and asserts `mp_overlay_package_export_readiness=ready_for_mp`, so one run can produce both single-session archive evidence and host/client package handoff artifacts.
When MP export is enabled, the loop also refreshes an SNC snapshot bound to that package (`snc_status_snapshot_with_mp.json`) and asserts `ready_for_mp`, so Status Center copy text can be validated in the same run.
The same loop now also emits structured gameplay acceptance fields (`real_session_v0_loop_gameplay_acceptance_state/reason/path`) directly from the SNC snapshot, so first-session owner validation output surfaces acceptance state without extra parsing.
`tools/run_real_session_v0_loop.ps1` now also emits structured MP package handshake fields (`real_session_v0_loop_mp_package_manifest_hash`, warning count, identity-mismatch warning, verify/import commands), so release-companion/manual host flow can consume one real-session output without reparsing raw export lines.
`tools/compare_real_session_v0_outputs.ps1` / `.cmd` now provide a bounded two-session compare harness for owner real-game loops, including generated overlay manifest-hash change, archive save-count delta, gameplay acceptance state delta, MP package readiness/hash/warning deltas, and a next-step recommendation JSON output.
The compare harness now also emits `mp_identity_mismatch_warning` previous/current/changed delta fields, so host/client identity-risk drift is visible directly in one compare JSON.
The compare harness now also emits structured `mp_identity_mismatch_warning_codes` previous/current/changed deltas, so exact MP identity-risk code changes are visible between two real-session runs.
`tools/run_real_session_v0_loop.ps1` now emits a ready-to-run compare command hint line tied to the current session output directory, so the next owner session can be compared without manual command composition.
`tools/run_real_session_v0_loop.ps1` now also supports optional `-PreviousSessionDirForCompare`, which runs compare automatically after a successful loop and emits structured `real_session_v0_loop_compare_auto_*` fields (output path + recommendation) for immediate owner follow-up.
`tools/analyze_real_session_v0_trend.ps1` / `.cmd` now provide a bounded multi-session trend summary over `dist/real_session_v0_loop` (session count, unique overlay hash count, unique gameplay-state count, latest delta recommendation), so owner/release-companion flows can quickly see whether repeated v0 runs show any observable pipeline delta.
`tools/run_real_session_v0_loop.ps1` now supports optional `-EmitTrendSummary`, which emits structured `real_session_v0_loop_trend_auto_*` fields from that trend summary in the same run output.
Trend summary now also emits structured latest identity-risk warning fields (`real_session_v0_trend_identity_risk_warning*`), and real-session loop auto-trend forwarding includes `real_session_v0_loop_trend_auto_identity_risk_warning*`, so host/client mismatch risk remains visible even in trend-only follow-up flows.
Trend summary now also emits structured MP warning-count fields (`real_session_v0_trend_mp_warning_count_current`, `real_session_v0_trend_mp_warning_count_delta`), and real-session loop auto-trend forwarding includes `real_session_v0_loop_trend_auto_mp_warning_count*`, so warning-count drift is visible in one command.
Trend summary now also emits structured latest MP readiness/next-step fields (`real_session_v0_trend_mp_*_current`), and real-session loop auto-trend forwarding includes `real_session_v0_loop_trend_auto_mp_*_current`, so host/client follow-up actions remain visible in trend-only flow.
`ArchiveMinistryInputBuilder` now threads parsed-headline war-state plus `save_date` year/month/day hints into ministry turn-context defaults, adds explicit hint confidence metadata, and now derives conservative turn-context pressure hints from year + war facts while failing closed with explicit uncertainty on missing/invalid hints.
Archive-fed v0 acceptance assertions now verify hinted turn-context propagation (including pressure) through both `--build-ministry-input-from-archive` and `--v0-pipeline-from-archive`, including fail-closed invalid `save_date` coverage that keeps conservative defaults and rejects date-derived hint facts.
`Strategic Nexus.exe --publish-generated-overlay <staged_overlay_dir> <active_overlay_dir> [stellaris_running_override]` verifies a staged generated overlay, refuses to publish while Stellaris is running, copies only manifest-allowed files to a temporary snapshot, verifies that snapshot, then replaces the active generated overlay directory as a complete snapshot.
`Strategic Nexus.exe --detect-stellaris-running [output.json]` reports whether the local Stellaris process is running; `--publish-generated-overlay` now uses that detector by default when no explicit `true`/`false` override is supplied.
SNC status snapshots now also include `status_center_summary_text`, a copyable human-readable status summary for archive, save discovery, generated overlay, and MP overlay package readiness.
SNC status snapshots now include `generated_overlay_publish_gate_status`; Status Center summary shows whether generated overlay publish is currently allowed, waiting for generated overlay readiness, blocked because Stellaris is running, or blocked because process detection is unavailable.
Generated overlay verification now exposes `generated_overlay_manifest_hash`; SNC status JSON, CLI output, and Status Center summary include it as a stable generated overlay identity signal.
MP overlay package verification now exposes `package_manifest_hash`; SNC status JSON, CLI output, copyable MP package text, and Status Center summary include that hash as a stable package identity signal.
MP overlay package verification now exposes `readiness`; copyable MP package status text is emitted only after the manifest, expected files, hashes, byte counts, and unexpected-file scan all pass, and Status Center summary surfaces `mp_readiness: ready_for_mp` for verified packages.
`tools/run_end_to_end_spine_smoke_tests.ps1` is repeatable and clears only its own smoke output directories before rerunning the spine through staged overlay publish, active overlay verification, MP package export, and SNC status snapshot.
`tools/run_generated_overlay_gameplay_acceptance.ps1` now runs bounded v0 gameplay acceptance cases (A-F), records pass/fail evidence with manifest hashes into `dist/private_reports/generated_overlay_gameplay_acceptance_v0.json`, and fails closed when any acceptance case fails.
SNC status snapshots now include `gameplay_acceptance_status`; the copyable Status Center summary also surfaces gameplay acceptance state/reason and report path for owner-facing visibility.

---

## 2. Minimal Strategy Dimensions

Status:
IN_PROGRESS

Goal:
Verify that LLM decisions can safely alter empire strategic behavior.

Initial domains:

* military_posture
* research_bias

Initial enum count:
2 values per domain only.

Example:

military_posture:

* defensive
* aggressive

research_bias:

* economy
* military

Notes:
More enums and domains will be added later using the same schema.

Current progress:
Bridge core can validate modular `strategy_dimensions` payloads and reject legacy `doctrine_id`.
Bridge core one-shot pipeline can emit an allowlisted effect batch and persist replay-protection sequence state.
The scripted PoC receiver is implemented for `military_posture` and `research_bias`; real gameplay effects are not implemented yet.
The first deterministic v0 strategic pipeline skeleton can transform fixture ministry input `X` into ministry proposal `Y`, clerk brief `U`, final `strategy_dimensions` payload, and bridge-core effect batch.
V0 pipeline tests now cover military posture, research bias, low-confidence agenda preservation, missing optional fields, invalid current agenda fallback, malformed input rejection, read-failure handling, and untrusted input bounds.
The v0 pipeline can emit a compact audit record for `X`, `Y`, `U`, and the final payload.
The v0 pipeline computes an audit-visible deterministic processing priority score.
A first local processing queue skeleton can order ministry contexts by priority and emit a compact queue JSON for future offline-worker scheduling.
V0 ministry input text is bounded before audit or future LLM context use.
V0 cabinet enum validation reuses bridge-core strategy dimension parsing to avoid contract drift.
V0 decision and audit files are written through atomic temp-file replacement.
Audit output failure does not invalidate an otherwise valid v0 decision output.
Common atomic file writes reject directory targets without deleting them.
Bridge pipeline failure tests now cover effect batch write failure, malformed payload cleanup, replay cleanup, and sequence write failure cleanup.
`tools/run_all_tests.ps1` now runs the local bridge-core and v0 pipeline regression tests together.
GitHub Actions Windows CI runs the same local regression suite on push and pull request.
Generated overlay contract tests now compile and run with the v0 test suite.
The v0 suite verifies both successful generated overlay staging and fail-closed rejection of an invalid tactical-style DSL rule.
Remaining test gap:
Generated overlay tests currently prove deterministic staging and fail-closed validation, but they do not yet prove that Stellaris applies the generated scripts with the intended gameplay effect.
Before generated overlays are treated as gameplay-ready, add mod-side smoke tests, local scripted harnesses, or documented manual Stellaris acceptance tests that verify behavior in-game or against a faithful mod-script execution surface.

---

## 3. Empire-Scoped Context Pipeline

Status:
NOT_STARTED

Goal:
Ensure each empire receives isolated knowledge context.

Required:

* known empire filtering
* intel filtering
* sensor filtering
* anti-omniscience validation
* empire-specific snapshots
* multiple empire-perspective analyses over the same archived save sequence
* one personality profile per detected campaign empire

Notes:
No global galaxy AI brain allowed.
The same save history may produce different memories and personality drift for different empires.

---

# Strategic Intelligence Phasing Guard

Items 3A and 3B are approved architecture, but they must not be implemented as full strategic simulation before parser and evidence contracts exist.

Allowed early work:

* schemas
* fixtures
* validation contracts
* parser field availability maps
* fail-closed tests
* evidence/confidence plumbing
* generated overlay no-op or conservative test harnesses
* functional acceptance test plans for intended gameplay behavior

Not allowed early:

* pretending unavailable save fields are known
* broad LLM prompts over raw saves
* target-specific gameplay rules without identity and evidence validation
* internal-pressure gameplay rules without resource/capability/ethics evidence
* full domestic politics simulation
* hidden AI bonuses or player penalties

Worker rule:
when a 3A or 3B task lacks required save evidence, implement the contract and missing-field reporting first, then stop before gameplay-affecting output.
When a 3A or 3B task generates gameplay-affecting overlay behavior, include a functional acceptance test or mark the output as not gameplay-verified.

---

## 3A. Subjective Other-Empire Profiles And Targeted Rules

Status:
NOT_STARTED

Goal:
Let each empire build its own predictive picture of important other empires and use that memory for bounded next-session strategic rules.

Required:

* observer-target profile schema keyed by campaign, observer empire, and target empire
* relationship delta schema that separates general trust from predicted future behavior
* parser field availability map for observer, target, diplomacy, war, subject, federation, border, and intel evidence
* per-observer evidence extraction from latest-session autosaves
* LLM interpretation contract for subjective target profiles
* validation rules for confidence, source evidence, target identity, and allowed rule domains
* bounded target-specific DSL rule candidates
* generated overlay support for campaign-marker guarded observer-target rules
* tests where the same event produces different memories for different observer empires

Notes:
Profiles are subjective and predictive, not objective truth about the target empire.
This is the main path toward galactic social intelligence: empires remember who helped, who betrayed, who exploits weakness, and who can be trusted only in specific contexts.
If profile confidence is low, store a concise memory summary and skip gameplay-affecting generated rules.

---

## 3B. Internal Pressure And Strategic Reputation

Status:
NOT_STARTED

Goal:
Make empires feel like real political entities with internal constraints and reputational consequences, while keeping AI difficulty fair.

Required:

* campaign-empire internal pressure schema
* integrated empire-state contract that combines identity, ethics, population ethics distribution when known, government, civics, resources, income trajectory, capability, memory, relationships, internal pressure, reputation, and doctrine inertia
* parser field availability map for each integrated-state input
* bounded pressure dimensions such as security anxiety, economic pressure, public trauma, elite agenda, prestige need, diplomatic flexibility, and war exhaustion memory
* strategic reputation schema that separates private self-image from observer/audience perception
* plausible reputation spread rules based on visibility, diplomacy, war participation, federation/subject links, espionage, or other validated information paths
* doctrine inertia and reform-cost fields
* cross-domain consistency checks before generated overlay rules are accepted
* validation rules that clamp all pressure, reputation, and reform deltas
* tests proving the same autosave evidence can produce different internal-pressure and reputation interpretations for different empires
* tests proving personality-driven recommendations are downgraded or rejected when they contradict resources, capability, ethics pressure, or internal state
* generated overlay support for coarse strategic nudges only, never hidden AI bonuses or player penalties

Notes:
This layer is intended to make the game harder by making empires more coherent, historical, and socially predictive.
It must not become a detailed domestic politics simulator or a cheating AI advantage.
If evidence is weak, store memory summaries and skip gameplay-affecting generated rules.
Subsystem schemas are implementation boundaries; the empire itself is one integrated state.

---

## 4. Save Timeline Memory Bootstrap

Status:
NOT_STARTED

Goal:
Use Stellaris timeline/history as portable memory anchor.

Required:

* timeline parsing
* historical event extraction
* bootstrap memory reconstruction
* campaign continuity after daemon loss

Notes:
Save portability is mandatory.

---

## 5. Multiplayer Host-Authoritative Validation

Status:
IN_PROGRESS

Goal:
Verify multiplayer synchronization architecture.

Required:

* host-only campaign archive and analysis
* identical generated gameplay-affecting overlay package for host and clients
* generated overlay export/package workflow
* byte-identical host/client generated file verification
* desync resistance
* fallback handling

Notes:
Clients must never process LLM payloads independently.

Current progress:
Scripted synchronized-state behavior is verified in MP.
Host-only daemon decisions are still outside production gameplay. Future MP work must stay within normal scripted state synchronization.
Offline generated overlays must be packaged identically for MP participants when they affect checksum-relevant files.
The generated overlay manifest is now the v0 byte-identity verification contract for generated gameplay-affecting files.

Planned requirement:
The companion app must provide a multiplayer generated overlay distribution flow before MP generated overlays are considered usable.

---

## 6. Safe Mod Integration

Status:
IN_PROGRESS

Goal:
Keep Strategic Nexus integrated through supported mod scripting, generated overlay files, local archives, and bounded JSON payloads.

Allowed targets:

* scripted mod receiver behavior
* local archived autosave analysis
* generated campaign overlay files staged between sessions
* generated campaign library containing multiple marker-guarded local campaign rulesets
* local daemon request/response files as development harnesses
* bridge-core validation and allowlisted artifacts
* manual development harnesses for private testing

Forbidden:

* lower-level modification of the running game process
* proprietary executable analysis
* arbitrary command execution
* save editing
* combat patching
* direct AI replacement

Notes:
Lower-level game integration is out of scope for this repository.

Current progress:
The project now treats bridge-core output as a validation artifact and keeps gameplay delivery in the scripted mod layer or explicit development harnesses.
Production integration should prefer next-session generated mod overlay updates over live daemon delivery.
The companion app should maintain the active generated campaign library from locally present Stellaris save campaigns.
`Strategic Nexus.exe --scan-save-campaigns <save_root> <inventory_output.json>` provides the first read-only local save campaign inventory harness.

---

## 6A. Generated Campaign Library Maintenance

Status:
IN_PROGRESS

Goal:
Keep the active generated mod overlay synchronized with locally available Stellaris save campaigns.

Required:

* scan local Stellaris save campaign directories
* detect newly added, restored, renamed, or removed local campaign save folders
* include generated rules only for campaigns that are currently playable locally, unless the user pins a campaign
* remove generated rules for campaigns whose local saves are absent
* retain Strategic Nexus archive and memory outside the active generated mod overlay
* store Strategic Nexus archive and memory as durable on-disk state, not volatile runtime state
* regenerate known campaign rules when a known save folder reappears
* keep every campaign ruleset marker-guarded
* enforce size limits for the active generated campaign library
* publish generated overlay as a complete replacement snapshot, not incremental appended rules

Notes:
Removing a ruleset from the active generated mod overlay is not the same as deleting campaign memory.
The active mod library should reflect campaigns the local Stellaris installation can actually load.
Generated events/rules should stay bounded because each update replaces the previous generated set.

Current progress:
A read-only campaign save scanner can inventory campaign directories and loose `.sav` files into deterministic JSON.
Inventory entries include a local anchor save fingerprint: selected `.sav` name, byte count, hash algorithm, and read-only content hash.
`--discover-stellaris-save-roots <output.json>` reports likely Windows Stellaris save roots from user Documents and OneDrive Documents/Dokumenty candidates without creating or modifying directories.
The local harness can compare two read-only save roots with `--diff-save-campaigns <previous_save_root> <current_save_root> <diff_output.json>` and report added, removed, changed, and unchanged entries.
The local harness can plan a bounded active generated campaign library with `--plan-campaign-library <save_root> <max_campaigns> <plan_output.json>`.
The planner includes only locally present campaigns with anchor fingerprints and skips overflow entries with auditable reasons.
`--compile-campaign-library-overlay <input.dsl> <save_root> <max_campaigns> <output_dir>` compiles only DSL rules whose campaign id matches an included local campaign key and writes the active library plan beside the generated overlay.
It does not parse save contents or assign final campaign identity yet.
Renames without a stable save-content fingerprint may still appear as remove+add.

---

## 7. Modular Strategy Dimension Expansion

Status:
NOT_STARTED

Goal:
Expand strategy dimensions gradually.

Future domains may include:

* border_strategy
* fleet_philosophy
* diplomatic_behavior
* crisis_policy
* espionage_policy
* economic_focus
* internal_stability_policy

Notes:
Do not expand aggressively before v0 architecture stabilizes.

---

## 8. Espionage And Intel Layer

Status:
NOT_STARTED

Goal:
Use espionage/intel as legitimate information acquisition path.

Required:

* intel-aware reasoning
* uncertainty handling
* influence cost awareness
* espionage priorities
* counterintelligence posture

Notes:
Espionage must never become cheating.

---

## 9. Personality And Civilization Identity

Status:
IN_PROGRESS

Goal:
Make civilizations strategically distinct.

Required:

* campaign-empire scoped personality profiles
* observer-target predictive profiles for important other empires
* unknown campaign personality bootstrap from archived saves
* personality traits
* doctrine inertia
* risk tolerance
* trust/distrust
* trauma memory
* internal pressure
* strategic reputation interpretation
* strategic culture

Notes:
Different civilizations should react differently to the same situation.
The same civilization template in a different campaign is a different personality instance.
LLM conversation may support offline interpretation, but v0 should persist only validated personality state, source save/date, concise update summary, and confidence.
Structured deltas may be added later for audit and rollback.

---

## 10. Strategic Memory System

Status:
NOT_STARTED

Goal:
Create long-term strategic continuity.

Required:

* memory summaries
* doctrine success/failure memory
* trauma persistence
* trust persistence
* observer-target predictive profile persistence
* internal pressure persistence
* strategic reputation persistence
* doctrine reform-cost memory
* memory decay
* historical interpretation
* incremental session-delta memory updates

Notes:
Civilizations should feel historically aware.
Normal analysis should combine previous durable memory with autosaves from the latest play session, not reprocess the entire campaign history every time.

---

## 11. Capability-Constrained Strategic Reasoning

Status:
NOT_STARTED

Goal:
Ensure strategies are realistic for the civilization.

Required:

* economy-aware reasoning
* infrastructure-aware reasoning
* technology-aware reasoning
* influence-aware reasoning
* logistics-aware reasoning
* transition-cost reasoning

Notes:
Theoretical optimality is not enough.
Strategies must be realistically executable.

---

## 12. Schema Evolution And Compatibility

Status:
IN_PROGRESS

Goal:
Allow safe long-term evolution of the architecture.

Required:

* schema versioning
* backward compatibility
* unknown field handling
* partial compatibility
* migration rules
* enum expansion safety

Notes:
The project must survive years of iteration.

Current progress:
Schema versioning rules exist.
Bridge core now requires `schema_version`.
Compatibility and migration layers are not fully implemented yet.

---

## 13. Advanced Strategic Behavior

Status:
NOT_STARTED

Goal:
Allow more sophisticated strategic interpretation.

Possible future areas:

* strategic deception
* proxy warfare
* alliance manipulation
* strategic bluffing
* ideological behavior
* long-term hegemon balancing
* adaptive federation strategy

Notes:
Only after core architecture is stable.

---

## 14. Advanced Runtime Integration

Status:
BLOCKED

Goal:
Create stable automated runtime bridge if a future legal and safe integration path exists.

Dependencies:

* validated architecture
* stable payload schema
* stable strategy dimensions system
* verified multiplayer behavior
* verified fallback handling

Notes:
Must remain bounded and safe.
This is no longer required for the main project direction.
The main direction is offline archived-save analysis and next-session mod refresh.

---

## 15. Multiplayer Generated Overlay Distribution

Status:
IN_PROGRESS

Goal:
Create a safe workflow for distributing generated campaign overlays to multiplayer participants.

Required:

* export generated campaign overlay package
* verify gameplay-affecting files are byte-identical for host and clients
* include manifest with campaign id, overlay version, game version, mod version, and file hashes
* distinguish checksum-sensitive generated files from local-only diagnostics
* provide clear fallback path if a client has the wrong overlay
* avoid per-client divergent gameplay-affecting generated files
* support players joining later without predeclaring them in Strategic Nexus
* support normal Steam friends / Stellaris lobby communication instead of IP-address workflows
* provide Status Center copyable package/invite status text
* export/import MP handoff packages so host role can change between seasons
* support the previous host being absent from the next season entirely
* treat client-provided manual saves as recovery anchors when host autosave archive or handoff is missing
* persist small campaign/empire/human-control markers into saves through normal mod flags/variables
* avoid binding empire personality to a specific human user or host machine

Notes:
This is mandatory before generated overlays are used for multiplayer campaigns.
Until this exists, multiplayer generated overlays remain a private host-side planning artifact or must be manually packaged and verified.

Current progress:
`MULTIPLAYER_SEASON_ORCHESTRATOR.md` defines the host-coordinated package model, low-friction join assumptions, host rotation handoff packages, client manual save recovery fallback, checksum-gated hotjoin assumptions, and save-persisted MP markers.
The local harness now has a minimal MP generated-overlay package exporter/verifier.
It writes a package manifest with campaign id, overlay version, game version, Strategic Nexus mod version, checksum-sensitive generated gameplay files, a local-only diagnostic manifest entry, and degraded handoff status when the previous host is unavailable.
The verifier fails closed on file drift and unexpected package files.
The CLI verifier output now also emits explicit `mp_overlay_package_warning` codes (missing expected files, hash mismatch, byte-count mismatch, unexpected files, identity/version mismatch) to support release-companion import/verify UX and Status Center wiring.
`Strategic Nexus.exe --verify-mp-overlay-package <package_dir> [expected_campaign_id] [expected_overlay_version] [expected_game_version] [expected_mod_version] [expected_manifest_hash]` now emits explicit mismatch warnings (`package_campaign_id_mismatch`, `package_overlay_version_mismatch`, `package_game_version_mismatch`, `package_mod_version_mismatch`, `package_manifest_hash_mismatch`) when a package differs from expected host/session identity.
`Strategic Nexus.exe --import-mp-overlay-package <package_dir> <target_overlay_dir> [expected_campaign_id] [expected_overlay_version] [expected_game_version] [expected_mod_version] [expected_manifest_hash]` now keeps fail-closed import verification and also emits import-side mismatch warnings (`mp_overlay_package_import_warning=*`) plus package identity metadata in CLI output, so release-companion flows can surface host/client mismatch risk directly at import time.
MP package copyable status text now includes explicit host/client readiness lines (`host_readiness`, `host_next_step`, `client_readiness_gate`, `client_next_step`) and handoff continuity hinting for host-rotation fallback.
Strategic Nexus Companion snapshot/status-center tests now assert these host/client readiness lines are present in `mp_overlay_package_status_text` and in copyable Status Center summary output.
SNC MP package snapshot now carries structured `warning_codes` (plus CLI `snc_mp_overlay_package_warning_count`/`snc_mp_overlay_package_warning_code` output), so release-companion and Status Center flows can surface non-ready package reasons without parsing free-form text.
SNC snapshot/CLI now also carries explicit MP verify/import command fields, and MP package copyable status text now includes ready-to-run `verify_command`/`import_command` lines tied to the package directory.
MP verify/import CLI now also emits explicit command fields parsed from package status text (`mp_overlay_package_verify_command`, `mp_overlay_package_import_command`, strict variants, and import-side command variants), so release-companion wiring can consume stable command keys without reparsing free-form status blobs.
MP package verifier failure paths now also emit explicit copyable `warning_code` status text (`mp_overlay_package_unexpected_files`, `mp_overlay_package_files_mismatch_manifest`, and malformed/missing manifest variants), so SNC and CLI surfaces can show concrete recovery guidance even when package verification is not ready.
MP package verifier failure status text now also includes strict verify/import command variants when package identity fields are valid, and SNC missing-package status text now includes verify/import commands for direct host/client recovery flow.
MP verify/import CLI now also emits structured `mp_overlay_package_warning_code=*` and `mp_overlay_package_import_warning_code=*` fields alongside legacy warning lines, so release-companion parsers can consume stable warning keys directly.
SNC snapshot normalization now parses `warning_code:` lines from MP package status text into structured `warning_codes`, so Status Center/release-companion surfaces can rely on stable mismatch codes (for example `mp_overlay_package_files_mismatch_manifest`) instead of generic failure reasons.
SNC CLI fallback now emits all unique `snc_mp_overlay_package_warning_code=*` values found in MP package `status_text` (not only the first), preserving structured warning visibility even when the snapshot warning-code array is empty.
SNC CLI now also emits legacy-compatible `snc_mp_overlay_package_warning=*` lines alongside `snc_mp_overlay_package_warning_code=*`, so older release-companion parsers can consume MP warning keys without migration risk.
Status Center summary text now always includes explicit `mp_warning_count` (including `0`) plus structured `mp_warning_code` lines, so owner-facing SNC/CLI surfaces can show MP warning state without reparsing free-form status text.
SNC fallback MP package warning codes are now normalized to stable `snake_case` values (for example `mp_overlay_package_directory_missing`) even when the underlying failure reason text is human-readable, so release-companion parsers can rely on consistent code keys.
MP package verifier failure status text now emits the same normalized missing/manifest warning-code variants directly (`mp_overlay_package_directory_missing`, `mp_overlay_package_manifest_missing`, `mp_overlay_package_manifest_files_missing`), reducing CLI/SNC parser divergence for not-ready package flows.
SNC snapshot JSON and `--snc-status-snapshot` CLI output now expose structured `host_readiness` and `client_readiness_gate` fields for MP package state, so release-companion UX can consume host/client readiness without parsing free-form status text.
`--verify-mp-overlay-package` and `--import-mp-overlay-package` CLI outputs now also expose structured host/client readiness fields (`*_host_readiness`, `*_client_readiness_gate`) parsed from package status text, so release-companion import/verify UX can read readiness gates from stable CLI keys.
SNC snapshot JSON, Status Center summary text, and `--snc-status-snapshot` CLI output now expose explicit identity-mismatch signal fields (`identity_mismatch_warning`, `identity_mismatch_warning_codes`) so release-companion flows can surface campaign/version/hash mismatch risk without reparsing generic failure reasons.
`--verify-mp-overlay-package` and `--import-mp-overlay-package` CLI outputs now also emit explicit identity-mismatch signal fields (`*_identity_mismatch_warning`, `*_identity_mismatch_warning_code`) so release-companion import/verify UX can surface mismatch risk from stable keys instead of inferring it from generic warning lists.
`--verify-mp-overlay-package` and `--import-mp-overlay-package` CLI outputs now also emit structured host/client next-step fields (`*_host_next_step`, `*_client_next_step`) parsed from package status text; SNC snapshot JSON/CLI now includes `host_next_step` and `client_next_step` so release-companion UX can consume actionable MP guidance without parsing free-form status blobs.
`--export-mp-overlay-package` CLI output now also emits export-side structured package verification fields (`mp_overlay_package_export_readiness`, `*_manifest_hash`, `*_warning_code`, verify/import command variants, host/client readiness, and next-step keys) so release-companion staging can consume package state immediately after export without running a separate parse path.
`tools/compare_real_session_v0_outputs.ps1` now emits explicit structured identity-risk signals (`real_session_v0_compare_identity_risk_warning`, reason, warning_code lines plus JSON `identity_risk_warning`) so owner/release-companion flows can flag package identity/version/hash mismatch risk between real sessions without deriving it from generic deltas.
`tools/run_real_session_v0_loop.ps1` now forwards these compare identity-risk fields in auto-compare output (`real_session_v0_loop_compare_auto_identity_risk_warning*`), keeping the first real-game validation loop one-command and warning-visible.
`tools/compare_real_session_v0_outputs.ps1` now also emits structured current MP readiness/next-step compare signals (`real_session_v0_compare_mp_*_current`) and auto-compare forwards them (`real_session_v0_loop_compare_auto_mp_*_current`), so host/client follow-up actions remain visible after each compared real-session run.
`tools/compare_real_session_v0_outputs.ps1` now also emits structured current MP verify/import command signals (`real_session_v0_compare_mp_*_command_current`), and trend + loop auto forwarding surface them (`real_session_v0_trend_mp_*_command_current`, `real_session_v0_loop_trend_auto_mp_*_command_current`, `real_session_v0_loop_compare_auto_mp_*_command_current`) so release-companion can consume replayable MP command hints from one output stream.
`tools/compare_real_session_v0_outputs.ps1` now also emits structured `previous` and `changed` MP readiness/next-step/verify-import command signals (`real_session_v0_compare_mp_*_{previous|changed}`), and trend + loop auto forwarding now expose matching `*_previous`/`*_changed` fields for one-command session-to-session drift inspection without opening compare JSON.
`tools/compare_real_session_v0_outputs.ps1` now also emits structured MP warning-count compare signals (`real_session_v0_compare_mp_warning_count_current`, `real_session_v0_compare_mp_warning_count_delta`) and auto-compare forwards them (`real_session_v0_loop_compare_auto_mp_warning_count_*`), so warning drift between sessions is visible without opening compare JSON manually.
`tools/compare_real_session_v0_outputs.ps1` now also emits structured current MP warning-code lines (`real_session_v0_compare_mp_warning_code_current=*`), and these are forwarded by trend + loop auto outputs (`real_session_v0_trend_mp_warning_code_current=*`, `real_session_v0_loop_trend_auto_mp_warning_code_current=*`, `real_session_v0_loop_compare_auto_mp_warning_code_current=*`) for one-command release-companion/status parsing.
Compare/trend/loop now also emit structured MP warning-code drift signals (`*_mp_warning_code_previous=*`, `*_mp_warning_code_current=*`, `*_mp_warning_codes_changed=*`) so host/client package warning evolution is visible between sessions without opening compare/trend JSON files.
`tools/compare_real_session_v0_outputs.ps1` now also emits replayable compare command output (`real_session_v0_compare_command_hint`), and real-session loop auto-compare forwards it as `real_session_v0_loop_compare_auto_command_hint` so follow-up evidence replay does not require manual command reconstruction.
Compare/trend/loop now also emit structured MP package output-directory drift signals (`*_mp_package_output_dir_{previous|current|changed}`) so release-companion and owner flows can pick the right current package path without manual session-folder inspection.
Compare/loop auto outputs now also emit structured MP snapshot presence drift fields (`*_mp_status_snapshot_present_{previous|current|changed}`), so host-rotation runs can distinguish "previous host/session had no MP snapshot" from ordinary package-content drift.
Status Center summary now also emits explicit `mp_identity_mismatch_alert` text when package identity mismatch is detected, so owner/release-companion flows get a direct human-readable warning to run strict verify/import before MP join.
`tools/run_real_session_v0_loop.ps1` now also emits `real_session_v0_loop_next_session_compare_baseline_dir` and `real_session_v0_loop_next_session_command_hint`, so the next real Stellaris validation run can reuse the same inputs and auto-compare against the just-finished session in one copyable command.
`tools/analyze_real_session_v0_trend.ps1` now also emits `real_session_v0_trend_next_session_command_hint` (and stores `next_session_command_hint` in trend JSON), so trend-only follow-up can still hand off one copyable command for the next real-session run with compare baseline prefilled.
`tools/run_real_session_v0_loop.ps1` now also forwards trend next-session command output as `real_session_v0_loop_trend_auto_next_session_command_hint`, so one-command loop/trend output can be copied directly for the next real-session run.
`tools/run_real_session_v0_loop.ps1` now also forwards trend compare replay command output as `real_session_v0_loop_trend_auto_latest_compare_command_hint`, so latest compare evidence can be replayed from the same loop/trend output surface.
`tools/run_real_session_v0_loop.ps1` now also writes a machine-readable run evidence artifact (`real_session_v0_loop_evidence_json`), bundling session/archive/status paths, gameplay acceptance, command hints, MP export state, and optional auto compare/trend summary fields for release-companion one-file consumption.
The same evidence artifact now also carries full structured auto compare/trend MP drift and command-surface fields (readiness, next-step, verify/import commands, warning-code sets, identity-mismatch signals), so release-companion can consume one JSON file instead of reparsing CLI line streams.
The evidence artifact now also includes explicit metadata (`evidence_schema_version`, `generated_at_utc`) so release-companion parsing can be versioned and audit-friendly.
`tools/run_real_session_v0_loop.ps1` now also emits stable `real_session_v0_loop_run_id` in CLI output and stores the same `run_id` in evidence JSON, so release-companion can pair one owner-visible run output with exactly one evidence artifact without path/timestamp heuristics.
`tools/run_v0_pipeline_tests.ps1` now also regressions-checks that `real_session_v0_loop_evidence_json` preserves core `command_hints` plus auto-compare/trend command-hint fields, so one-file release-companion consumption fails fast on contract drift.
`tools/run_v0_pipeline_tests.ps1` now also asserts `real_session_v0_loop_run_id` parity between CLI output and `real_session_v0_loop_evidence_json`, so run pairing fails closed if either surface drifts.
`tools/run_v0_pipeline_tests.ps1` now also fail-closed asserts evidence metadata contract (`evidence_schema_version=1` and parseable `generated_at_utc`) inside `real_session_v0_loop_evidence_json`, so release-companion schema/version drift is caught during local regression.
`tools/run_v0_pipeline_tests.ps1` now also executes `tools/run_generated_overlay_gameplay_acceptance.cmd` and fail-closed validates `dist/private_reports/generated_overlay_gameplay_acceptance_v0.json` (`acceptance_state=verified_for_v0_domains`, expected case coverage), so functional generated-overlay v0 acceptance (cases A-F) is part of the primary pipeline gate.
`tools/run_real_session_v0_loop.ps1` now also surfaces archive facts directly (`real_session_v0_loop_archive_copied_save_count`, `real_session_v0_loop_archive_last_archived_path`) and mirrors them in evidence JSON `archive`, so real-session validation can confirm captured-save evidence from one output surface without opening `archive_summary.json` manually.
`tools/run_real_session_v0_loop.ps1` now preserves warning-code arrays in evidence JSON as real string arrays (no `System.Object[]` placeholder serialization), so release-companion parsing of identity/warning drift fields is stable.
`tools/run_real_session_v0_loop.ps1` now also stores `status_snapshot_with_mp_path` and `status_snapshot_with_mp_readiness` inside evidence JSON `mp_export`, so release-companion one-file parsing can consume MP snapshot routing/readiness without scraping CLI lines.
`tools/run_real_session_v0_loop.ps1` now always emits `real_session_v0_loop_status_snapshot_with_mp_path` and `real_session_v0_loop_status_snapshot_with_mp_readiness` (with `not_exported` when MP export is off), and evidence JSON mirrors that stable contract so release-companion parsers no longer need export-mode branching.
`tools/run_real_session_v0_loop.ps1` now also emits structured Status Center state/reason fields (`real_session_v0_loop_status_center_state`, `real_session_v0_loop_status_center_reason`) and stores them in evidence JSON `status_center`, so release-companion/owner v0 loop decisions can consume status readiness without parsing full status-center summary text blobs.
`tools/run_real_session_v0_loop.ps1` now also emits `real_session_v0_loop_status_center_summary_text` and stores `status_center.summary_text` in evidence JSON, so host/client readiness copy text is available from the same one-run output contract.
`tools/run_real_session_v0_loop.ps1` now also emits explicit export-side mismatch warning booleans (`real_session_v0_loop_mp_package_{game_version,mod_version,manifest_hash}_mismatch_warning`) and stores the same fields in evidence JSON `mp_export`, so release-companion can consume owner-critical package identity mismatch risk from one output surface without reparsing warning-code lists.
`tools/run_real_session_v0_loop.ps1` now also emits explicit export-side `campaign_id` and `overlay_version` mismatch warning booleans (`real_session_v0_loop_mp_package_{campaign_id,overlay_version}_mismatch_warning`) and stores the same fields in evidence JSON `mp_export`, so package-identity mismatch causes are visible in one run output without decoding warning-code arrays.
`tools/run_real_session_v0_loop.ps1` now also emits a stable aggregated MP mismatch warning contract (`real_session_v0_loop_mp_package_mismatch_warning_state/reason/code`) and mirrors it in evidence JSON `mp_export.mismatch_warning_*`, including `not_exported` defaults when MP export is skipped, so release-companion/owner flow can read one explicit package-risk signal without recomputing from many booleans.
`tools/run_real_session_v0_loop.ps1` now also produces a deterministic MP package ZIP handoff artifact (`mp_overlay_package.zip`) with structured CLI/evidence fields (`*_mp_package_zip_{state,reason,path,sha256,bytes}`), so host/share staging for the next real MP session can consume one copyable package output without manual repacking.
Compare/trend/loop outputs now also expose structured MP ZIP `path` and `bytes` drift fields (`*_mp_package_zip_{path,bytes}_{previous,current,changed}`), and loop evidence JSON mirrors them, so host handoff can confirm not only hash drift but also artifact path/size drift between sessions from one output surface.
Compare/trend/loop auto outputs now also expose structured campaign/overlay mismatch drift fields (`*_mp_campaign_id_mismatch_warning_{previous,current,changed}`, `*_mp_overlay_version_mismatch_warning_{previous,current,changed}`), and loop evidence JSON mirrors those fields for release-companion one-file parsing.
`tools/run_real_session_v0_loop.ps1` now also emits a deterministic aggregated next-action contract (`real_session_v0_loop_next_action*`) and stores it in evidence JSON `next_action`, prioritizing MP mismatch and identity-risk warnings before normal next-session compare guidance so owner/release-companion follow-up is actionable without manual field interpretation.
`tools/run_real_session_v0_loop.ps1` now fail-closed requires compare/trend command-hint fields in auto outputs, so replay/next-session command guidance cannot silently disappear from real-session v0 loop evidence.
Trend recommendation now fail-safe prioritizes `review_identity_risk_warning` over observable-delta recommendations when both conditions are present, keeping MP identity/version/hash mismatch risk as the top follow-up signal in trend flow.
`tools/run_v0_pipeline_tests.ps1` now includes `real_session_trend_identity_risk_priority` regression coverage that forces both observable delta and identity risk, then asserts trend recommendation remains `review_identity_risk_warning`.
`tools/compare_real_session_v0_outputs.ps1` now also emits previous+current MP manifest-hash compare fields (`real_session_v0_compare_mp_manifest_hash_previous/current/changed`), and trend/loop auto forwarding now surfaces the same pair (`real_session_v0_trend_mp_manifest_hash_previous/current/changed`, `real_session_v0_loop_trend_auto_mp_manifest_hash_previous/current/changed`, `real_session_v0_loop_compare_auto_mp_manifest_hash_previous/current/changed`) for direct hash-drift inspection from one run output.
Compare/trend/loop outputs now also expose explicit observable-effect signal fields (`real_session_v0_compare_observable_effect_signal/reason`, `real_session_v0_trend_observable_effect_signal/reason`, and forwarded `real_session_v0_loop_*_observable_effect_*`) so owner/release-companion flow can quickly decide whether the latest session pair shows any measurable v0 pipeline delta worth in-game follow-up.
`tools/compare_real_session_v0_outputs.ps1` now also emits structured verified-archive save-count compare fields (`real_session_v0_compare_verified_archive_save_count_{previous,current,delta,changed}`), and `tools/run_real_session_v0_loop.ps1` forwards them into auto-compare CLI/evidence output (`real_session_v0_loop_compare_auto_verified_archive_save_count_*`) so observable-effect interpretation has direct save-count context in one run artifact.
Compare/trend/loop outputs now also expose explicit gameplay-acceptance drift fields (`*_gameplay_acceptance_state_{previous,current,changed}`), so first real-session evidence can show whether acceptance state itself changed without opening compare/trend JSON.
Compare/trend/loop outputs now also expose explicit gameplay-acceptance reason drift fields (`*_gameplay_acceptance_reason_{previous,current,changed}`), so owner/release-companion flow can detect why acceptance changed (or stayed stable) without reopening status snapshots manually.
Compare/loop outputs now also expose explicit Status Center readiness drift fields (`*_status_center_state_{previous,current,changed}`, `*_status_center_reason_{previous,current,changed}`), and real-session loop auto-compare plus evidence JSON forward the same fields for one-command session-to-session readiness comparison.
Trend outputs now also expose the same Status Center state/reason drift fields, and real-session loop auto-trend plus evidence JSON forward them for one-command multi-session readiness drift checks.
Compare/trend/loop outputs now also expose structured Status Center summary-text drift fields (`*_status_center_summary_text_{previous,current,changed}`), and real-session loop auto compare/trend plus evidence JSON forward them for one-command owner/release-companion visibility of summary-text changes between sessions.
Compare recommendation is now fail-safe for MP identity risk: when `identity_risk_warning` is active, compare emits `real_session_v0_compare_recommendation=review_identity_risk_warning` even if no other pipeline delta changed.
Real-session loop auto compare/trend forwarding now fails closed on unsupported recommendation values, so release-companion flow cannot silently accept unknown compare/trend recommendation semantics.
Compare/trend/loop outputs now also expose structured MP identity-mismatch drift fields (`*_mp_identity_mismatch_warning_{previous,current,changed}`, `*_mp_identity_mismatch_warning_code_{previous,current}`, `*_mp_identity_mismatch_warning_codes_changed`), so release-companion/status flows can read mismatch evolution directly from one command output without inferring from generic identity-risk signals.
Compare/trend/loop outputs now also expose structured aggregated next-action drift fields (`*_next_action_{current,previous,changed}`, `*_next_action_reason_{current,previous,changed}`, `*_next_action_command_hint_source_{current,previous,changed}`), and loop evidence JSON mirrors them in auto compare/trend blocks so release-companion can detect guidance drift between sessions from one output artifact.
Compare/trend/loop outputs now also expose structured next-action command-hint drift fields (`*_next_action_command_hint_{current,previous,changed}`), and loop evidence JSON mirrors them in auto compare/trend blocks so release-companion can replay exact follow-up commands without inferring them from source tags alone.
Live autosave watcher tooling now also has `.cmd` wrappers for `watch`, `start`, and scheduled-task install flows, keeping execution-policy-safe startup parity with other owner-facing v0 scripts.

Next worker-ready slice:

Extend the MP package harness toward release-companion staging, import/verify UX, and Status Center visibility.
The core export/verify manifest slice already exists, so do not repeat it unless a regression or missing acceptance check is found.

The next slice should include:

* release-companion-visible package state
* clear import/verify command surface
* Status Center copy text for host/client readiness
* explicit owner-facing warning when package identity, game version, mod version, or generated file hashes do not match

This slice must not require knowing all future participants before the season.
It must assume the previous host may be absent from the next season entirely.

---

# Updating This Document

This document must evolve continuously.

Rules:

* update status fields regularly
* add new milestones only when justified
* avoid roadmap explosion
* preserve development order clarity
* preserve architecture-first philosophy

---

# Core Development Principle

Strategic Nexus should evolve like a civilization:

* slowly
* modularly
* persistently
* safely
* adaptively

Do not rush complexity before the architecture is stable.
