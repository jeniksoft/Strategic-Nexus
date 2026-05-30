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
`tools/run_real_session_v0_loop.ps1` now also emits structured MP package handshake fields (`real_session_v0_loop_mp_package_manifest_hash`, warning count, identity-mismatch warning, verify/import commands), so release-companion/manual host flow can consume one real-session output without reparsing raw export lines.
`tools/compare_real_session_v0_outputs.ps1` / `.cmd` now provide a bounded two-session compare harness for owner real-game loops, including generated overlay manifest-hash change, archive save-count delta, gameplay acceptance state delta, MP package readiness/hash/warning deltas, and a next-step recommendation JSON output.
The compare harness now also emits `mp_identity_mismatch_warning` previous/current/changed delta fields, so host/client identity-risk drift is visible directly in one compare JSON.
`tools/run_real_session_v0_loop.ps1` now emits a ready-to-run compare command hint line tied to the current session output directory, so the next owner session can be compared without manual command composition.
`tools/run_real_session_v0_loop.ps1` now also supports optional `-PreviousSessionDirForCompare`, which runs compare automatically after a successful loop and emits structured `real_session_v0_loop_compare_auto_*` fields (output path + recommendation) for immediate owner follow-up.
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
