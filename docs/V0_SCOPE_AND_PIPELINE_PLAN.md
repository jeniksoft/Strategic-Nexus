# Strategic Nexus - v0 Scope And Pipeline Plan

## Purpose

This document defines the first implementation boundary after the proof-of-concept phase.

v0 must create a small end-to-end technical skeleton.

It must not attempt full Strategic Nexus intelligence.

---

# v0 Core Goal

Build a minimal, testable pipeline:

```text
empire snapshot
-> ministry input context X
-> ministry output Y
-> clerk-reduced input brief U
-> lightweight cabinet review
-> validated strategy_dimensions payload
-> generated overlay or bridge-core validation artifact
```

The goal is architectural proof, not strategic depth.

In the revised production architecture, this v0 pipeline is an offline validator and generated-output preparation path.
The bridge-core effect batch remains useful for tests and development harnesses, but it is not a production promise of live LLM delivery into a running game.

The owner priority is to reach the first functional v0 quickly enough to test it in real Stellaris sessions.
Do not optimize v0 for final polish before this loop exists:

```text
real save/session
-> archive/analyze
-> produce validated v0 output or generated overlay
-> expose readiness/status
-> owner runs another game/session
-> compare whether intended behavior is observable
```

Once a slice is good enough for this first real-game validation loop, record follow-up improvements separately and move to the next missing v0 integration piece.
Polish, broader diagnostics, nicer UI, extra wording, and convenience automation belong after the first functional v0 unless they directly block that real-game test.

---

# v0 Non-Goals

Do not implement in v0:

* full save parsing
* full LLM memory library
* full ministerial government
* long-term relationship modeling
* personality drift
* ethics reform logic
* war theater control
* custom networking
* direct tactical control
* save editing

If a system is needed for shape, use a small stub or fixed sample input.

---

# Initial Strategy Domains

v0 uses only two strategy domains:

```text
military_posture:
  defensive
  aggressive

research_bias:
  economy
  military_industry
```

These already match the current modular strategy dimension direction.

Do not add more domains until the end-to-end pipeline is stable.

Note:
`military_industry` is the bridge-contract-safe value for military research bias.
Avoid the shorter `military` value in payloads because the current bridge validator does not accept it.

---

# First Pipeline Objects

## X - Ministry Input Context

`X` is the bounded input for one ministry.

Initial fields:

```json
{
  "schema_version": 1,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "ministry": "military",
  "turn_context": {
    "year": 2230,
    "is_at_war": false,
    "strategic_pressure": 0.35
  },
  "known_facts": [],
  "uncertainties": [],
  "current_agenda": {}
}
```

For v0 this may come from sample JSON, not real save parsing.

## Y - Ministry Output / Agenda Proposal

`Y` is the ministry proposal.

Initial fields:

```json
{
  "schema_version": 1,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "ministry": "military",
  "proposal_id": "proposal_001",
  "requested_changes": {
    "military_posture": "defensive"
  },
  "stated_reasons": [],
  "explicit_risks": [],
  "explicit_uncertainties": [],
  "confidence": 0.6
}
```

The v0 ministry may initially be deterministic or fixture-driven.

The interface must allow later LLM replacement.

## U - Clerk-Reduced Input Brief

`U` is a compact brief derived from `X`, not from hidden context.

Initial fields:

```json
{
  "schema_version": 1,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "ministry": "military",
  "source_context_id": "context_001",
  "relevant_facts": [],
  "explicit_uncertainties": [],
  "missing_information": [],
  "compression_notes": []
}
```

The v0 clerk should be stateless.

It may be deterministic first.

If LLM extraction is later used, it must obey the same schema.

---

# Cabinet Review

The v0 cabinet receives:

```text
Y + U
```

It does not receive full `X`.

Initial cabinet behavior:

* validate proposal shape
* compare requested change against current agenda
* accept safe known enum values
* preserve existing agenda when confidence is too low
* emit final bounded `strategy_dimensions`

The v0 cabinet may initially be deterministic.

The interface must allow later LLM replacement.

---

# Final v0 Payload

The final payload must remain compatible with bridge-core direction:

```json
{
  "schema_version": 1,
  "sequence_id": 1,
  "created_unix_ms": 0,
  "ttl_ms": 30000,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "strategy_dimensions": {
    "military_posture": "defensive",
    "research_bias": "economy"
  },
  "fallback_required": false
}
```

Invalid output must fallback safely.

---

# Persistence In v0

Persist only:

* sequence state
* last valid final payload
* minimal audit record
* validation errors
* replay input/output fixture paths

Do not persist yet:

* memory books
* strategic episodes
* relationship memory
* personality drift
* cabinet conversation history
* clerk conversation history

---

# Testing Requirements

v0 tests must cover:

* valid `X -> Y -> U -> payload`
* invalid enum fallback
* low confidence preserves current agenda
* missing optional fields degrade safely
* stale payload rejection remains intact
* bridge-core tests still pass
* generated overlay contract verification remains distinct from gameplay behavior verification

Testing should be local and deterministic where possible.
When local deterministic tests cannot prove real Stellaris behavior, the gap must be recorded as an acceptance-test requirement instead of silently counted as done.

---

# Implementation Order

1. Create schema/data structs for `X`, `Y`, and `U`.
2. Add fixture JSON files.
3. Add deterministic ministry stub.
4. Add stateless clerk reducer.
5. Add lightweight cabinet evaluator.
6. Emit final `strategy_dimensions` payload.
7. Reuse bridge-core validation where useful and keep effect batch generation as a development-harness artifact.
8. Add tests.

Current progress:

* `src/strategic_pipeline/` contains the first deterministic v0 pipeline skeleton.
* `resources/ministry_context_military_defensive.json` and `resources/ministry_context_military_aggressive.json` provide sample `X` fixtures.
* `resources/ministry_context_research_economy.json` and `resources/ministry_context_research_military_industry.json` provide sample research ministry fixtures.
* `Strategic Nexus.exe --v0-pipeline <input_context> <decision_output> <sequence_id> <created_unix_ms> <ttl_ms> [audit_output]` emits a bridge-compatible decision JSON and can optionally emit a compact audit JSON.
* V0 decision and audit outputs are written through temporary files and replacement to avoid partial active outputs.
* V0 output failure tests verify a directory path is rejected without deleting the directory.
* Audit output failures are reported but do not invalidate an otherwise valid decision output.
* `tools/run_v0_pipeline_tests.ps1` verifies fixture `X -> Y -> U -> payload -> bridge-core effect batch` and audit record emission.
* V0 pipeline tests parse generated decision and audit JSON outputs before bridge handoff checks.
* `tools/run_all_tests.ps1` runs both bridge-core and v0 pipeline regression tests.
* The v0 skeleton is deterministic and does not use an LLM yet.
* Edge-case fixtures verify low-confidence agenda preservation, missing optional fields, escaped JSON string handling, and invalid current agenda fallback.
* Read-failure tests verify missing ministry input, malformed input, and unsupported schema fail closed without decision or audit output.
* Ministry input untrusted text is currently bounded to 512 characters per string and 16 entries per string array.
* Untrusted input tests verify ministry input arrays are bounded before audit and future LLM context use.
* Cabinet enum validation uses the same bridge-core strategy dimension parsers as runtime payload ingestion.
* Both initial v0 domains, `military_posture` and `research_bias`, are covered by local pipeline tests.
* The v0 pipeline now computes a deterministic processing priority score from war state, strategic pressure, and explicit uncertainty count.
* A first deterministic processing queue can order ministry input contexts by priority while preserving stable order inside ties.
* `Strategic Nexus.exe --v0-priority-queue <output_json> <input_context>...` writes a compact queue JSON for local scheduler testing.
* Priority scoring is audit-only for single v0 pipeline runs; queue ordering exists as a local skeleton for future offline-worker scheduling.
* The audit record contains the ministry input context, processing priority score, ministry output, clerk-reduced input brief, and final payload.
* `src/generated_overlay/` contains the first bounded DSL parser, validator, and deterministic generated overlay compiler skeleton.
* `Strategic Nexus.exe --compile-generated-overlay <input.dsl> <output_dir>` stages generated event, scripted effect, and scripted trigger files from validated DSL.
* Generated overlay staging also emits `strategic_nexus_generated_manifest.json` with deterministic file hashes, byte counts, checksum relevance classification, and complete replacement snapshot metadata.
* Generated overlay tests verify both successful staging and fail-closed rejection of invalid tactical-style DSL.
* `Strategic Nexus.exe --archive-stable-saves <save_root> <archive_root> <session_id> [stability_delay_ms]` copies stable `.sav` files into a separate session archive and writes a manifest.
* `Strategic Nexus.exe --verify-autosave-archive <session_archive_dir>` verifies copied saves against the archive manifest.
* `Strategic Nexus.exe --summarize-autosave-archive <session_archive_dir> <summary_output.json>` emits bounded metadata from a verified archive session.
* `Strategic Nexus.exe --build-ministry-input-from-archive <session_archive_dir> <campaign_id> <empire_id> <ministry> <output_json>` emits a conservative v0 ministry input context from verified archive metadata.
* `Strategic Nexus.exe --discover-stellaris-save-roots <output.json>` emits likely Windows Stellaris save root candidates with existence flags.
* Save-root discovery includes local Documents, OneDrive Documents/Dokumenty, and Steam Cloud userdata cache roots.
* Distribution handling is provider-neutral: v0 must support non-Steam installs that write to the standard Paradox Documents save tree, and Steam Cloud discovery is only an additive provider-specific root. See `STELLARIS_DISTRIBUTION_AND_SAVE_ROOTS.md`.
* `Strategic Nexus.exe --scan-save-campaigns <save_root> <inventory_output.json>` emits a deterministic read-only inventory of locally present campaign save directories and loose `.sav` files.
* Campaign save inventory entries include a selected local anchor save fingerprint for future identity/memory lookup.
* Real save-folder inspection confirms that active save history is bounded: autosaves are retained only as a small window and older autosaves may be pruned or overwritten by Stellaris during the active session.
* A campaign folder is a stable save boundary for one campaign, but not a stable preselected active target. Only one campaign is played at a time, but SNC must rescan the whole `save games` tree because the user can switch campaigns or create new campaigns while `stellaris.exe` and SNC are already running.
* SNC app lifetime is not the capture-session boundary. SNC is expected to run as a tray app; autosave capture sessions are tied to observed `stellaris.exe` play activity.
* A live capture session may contain sequential autosave segments from multiple campaign folders. V0 analysis must avoid treating the capture session id as the campaign id.
* A live capture session may contain branch/reload segments inside the same campaign. V0 analysis must avoid treating the latest captured autosave as the only valid future entry point.
* Post-play v0 must evolve toward entry-point-scoped rules for every currently loadable save in the campaign, using captured autosaves as historical evidence only when compatible with that entry point.
* During active `stellaris.exe` play, V0 should capture only. After `stellaris.exe` exits, SNC verifies the capture archive, analyzes archived autosaves, and generates/stages new mod rules for the next launch.
* V0 CLI harnesses may still accept explicit `campaign_id`, `empire_id`, ministry, or `input.dsl` values for testing, but final SNC behavior must derive or confirm campaign/empire identity and construct the bounded decision/DSL path internally.
* `Strategic Nexus.exe --build-snc-dsl-draft-package <candidate_package.json> <draft.dsl> <audit.json>` builds the first validated dry-run DSL draft from candidate decisions that have parsed entry-point identity.
* The DSL draft slice remains conservative: it emits only marker/fingerprint/date-gated `doctrine_inertia high` proposals, writes an audit package, and explicitly keeps overlay compile/publish disabled.
* `Strategic Nexus.exe --stage-snc-generated-overlay <draft.dsl> <staging_dir> <status.json>` converts a validated SNC DSL draft into a clean verified generated-overlay staging snapshot and status JSON, but keeps `publish_allowed=false`.
* `Strategic Nexus.exe --publish-snc-generated-overlay <staging_status.json> <active_overlay_dir> <publish_status.json> owner-approved [stellaris_running_override] [backup_root]` is the SNC owner gate: it requires an explicit owner approval token, refuses to run while Stellaris is running, verifies the staged status, backs up any existing active generated overlay, then delegates to the manifest-bound generated overlay publisher.
* SNC status snapshots and tray now expose the owner-facing generated-overlay publish gate: staged status path, staged overlay path, active generated-overlay snapshot path, publish status path, backup root, manifest hash, rule count, owner-approval requirement, and whether publishing is currently allowed.
* The owner-facing publish target is a separate active generated-overlay snapshot (for example `dist/private_reports/snc_generated_overlay_active`), not the whole mod root. SNC must not replace `mod/strategic_nexus_poc` directly because that root also contains descriptor, localization, and manually maintained PoC files.
* SNC tray now also writes `snc_generated_overlay_staged/` and `snc_generated_overlay_staging_status.json` after a valid DSL draft; this is still a private staging gate until the owner explicitly publishes the staged overlay snapshot.
* SNC tray now updates `snc_dsl_draft_package.json` and `snc_validated_dsl_draft.dsl` after post-play candidate generation when enough parsed identity is available.
* Observed live-relevant save names include `autosave_YYYY.MM.DD.sav` and `ironman.sav`; `continue_game.json` is only a hint and can point at a save stem that is not currently present on disk.
* `tools/build_snc_tray.ps1`, `tools/run_snc_tray.ps1`, and `tools/smoke_snc_tray.ps1` provide the first native tray companion slice for owner validation before a real Stellaris session test.
* `tools/run_real_session_v0_loop.ps1` can now auto-resolve bounded effective `campaign_id` / `empire_id` values from single-campaign post-play decision-input artifacts, and it can auto-use the validated SNC DSL draft for `--run-offline-spine` when no explicit DSL path is supplied.
* `Strategic Nexus.exe --archive-live-saves <save_games_root> <archive_root> <session_id> [stability_delay_ms]` recursively captures stable `autosave*.sav` and `ironman.sav` revisions from a whole save-games root, deduplicated by source identity and content hash.
* `Strategic Nexus.exe --snc-live-autosave-monitor <save_root|auto> <archive_root> <session_id> [poll_ms] [stability_delay_ms] [max_iterations] [use_detected_stellaris_state] [stellaris_running_override] [capture_when_not_running]` provides the native SNC-owned monitor loop for preserving every observed stable autosave revision before the game rotates it away.
* `Strategic Nexus.exe --run-snc-session-capture [archive_root] [status_output.json] [session_id] [poll_ms] [stability_delay_ms] [max_iterations] [capture_when_not_running]` is the first owner-facing test mode for real Stellaris session capture, with auto root discovery and heartbeat status output.
* `tools/watch_stellaris_live_autosaves.ps1` is a manual development harness only; production SNC must not rely on hidden PowerShell startup.
* `Strategic Nexus.exe --diff-save-campaigns <previous_save_root> <current_save_root> <diff_output.json>` emits a deterministic read-only availability diff for generated campaign library maintenance.
* `Strategic Nexus.exe --plan-campaign-library <save_root> <max_campaigns> <plan_output.json>` emits a bounded include/skip plan for the active generated campaign library.
* `Strategic Nexus.exe --compile-campaign-library-overlay <input.dsl> <save_root> <max_campaigns> <output_dir>` compiles a generated overlay snapshot filtered to included local campaign keys.

Validation note:
Preserving an existing agenda under low confidence must still validate the existing agenda values.
Low confidence must not allow invalid current strategy values to pass through to bridge-core.

---

# Open Questions

These do not block initial skeleton implementation:

* exact real save parser fields
* final LLM provider
* real gameplay effect balancing
* long-term memory schema
* observer-target social intelligence
* integrated empire-state/internal pressure/reputation simulation

These must be decided before production gameplay:

* checksum-safe scripted mod packaging rules
* checksum-safe release packaging rules
* real empire identity mapping
* real campaign identity source
