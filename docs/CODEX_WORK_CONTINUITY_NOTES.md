# Strategic Nexus - Codex Work Continuity Notes

## Purpose

This document is a lightweight continuity ledger for Codex work.

It exists to avoid wasting compute on rediscovering maintenance context that has already been checked, fixed, or intentionally deferred.

Use it as a quick pre-flight note before repeating broad repository maintenance, documentation consistency audits, Free Work reorientation, or context recovery work.

This file is not a replacement for canonical architecture documents.

Canonical decisions still belong in the relevant subsystem docs, the roadmap, and the master index.

---

# Operating Rule

Before repeating broad maintenance work, Codex should check:

1. `MASTER_ARCHITECTURE_INDEX.md`
2. `DOCUMENTATION_CONSISTENCY_AUDIT.md`
3. this file
4. the relevant subsystem docs

If this file says a maintenance pass was already completed, Codex should either:

* continue from the recorded remaining watch items
* perform a narrower follow-up
* explain why a repeat pass is necessary

Do not use this file to hide blockers from the roadmap.

If a task is blocked, stale, or needs user attention, record that in the appropriate roadmap, audit, or `tools/dev_attention/` note.

---

# Context Compaction Checkpoint Note

Chat context and internal assistant memory are not reliable project storage.

When the conversation is likely to be compacted, summarized, reset, blocked, deleted, or handed to a future assistant, Codex should preserve project-critical state in Markdown before relying on continued chat memory.

If compaction already happened, Codex should immediately compare the recovered summary against repository docs and write any missing durable state back into Markdown before continuing substantial work.

The platform may not expose a real pre-compaction hook.

This document therefore records the project-level event rule, not a hidden system hook.

If a real automation hook becomes available later, it should call the same checkpoint behavior.

---

# Recent Maintenance Checkpoints

## 2026-05-23 - Offline Architecture Consistency

Status:

```text
DONE
```

Recorded in:

* `DOCUMENTATION_CONSISTENCY_AUDIT.md`
* `LEGACY_RUNTIME_ARCHITECTURE_INTERPRETATION_RULES.md`
* `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md`
* `GENERATED_OVERLAY_LAYOUT_CONTRACT.md`
* `META_RULE_LANGUAGE_AND_COMPILER.md`

Result:

* production direction is offline companion analysis plus generated overlay for next launch
* realtime LLM decisions entering a running game are no longer a production target
* generated overlay files are full fresh snapshots, not append-only updates
* active generated mod library contains only campaigns that can currently be loaded from local saves
* durable Strategic Nexus archive and memory remain on disk outside the active generated mod overlay

Do not redo the broad offline-vs-realtime reconciliation unless new docs or code introduce contradictions.

Follow-up should focus on remaining concrete watch items.

---

# Current High-Value Resume Items

These are not commands to execute blindly.

Revalidate each item against current architecture before starting Free Work.

## Generated Overlay Manifest And Packaging

Status:

```text
PARTIAL_DONE
```

Why it matters:

* multiplayer requires byte-identical gameplay-affecting generated files
* the companion app needs a clear export/package verification contract
* generated overlay publishing should be auditable and reproducible

Completed:

* generated overlay compiler emits `strategic_nexus_generated_manifest.json`
* manifest records complete replacement snapshot intent
* manifest records generated gameplay file paths
* manifest records checksum relevance classification
* manifest records deterministic `fnv1a64` content hashes and byte counts
* v0 tests parse and validate the manifest
* `--verify-generated-overlay <overlay_dir>` verifies generated files against the manifest
* verifier tests cover accepted overlays and file drift rejection

Remaining:

* final export archive format
* companion app package/export UI
* host/client package comparison workflow
* game/mod version metadata

Likely docs to update:

* `GENERATED_OVERLAY_LAYOUT_CONTRACT.md`
* `MULTIPLAYER_REQUIREMENTS.md`
* `DEVELOPMENT_ROADMAP.md`

## Checksum-Sensitive File Classification

Status:

```text
PARTIAL_DONE
```

Why it matters:

* the project must know which generated files affect multiplayer checksum
* packaging and validation cannot be trustworthy without file classification

Completed:

* v0 generated files under `events/`, `common/scripted_effects/`, and `common/scripted_triggers/` are classified as `gameplay_affecting`
* generated manifest is classified as diagnostic/package verification support
* `--verify-generated-overlay <overlay_dir>` now fails closed if a manifest entry uses an unknown generated overlay path or the wrong checksum relevance classification for the known v0 gameplay files

Remaining:

* broader mod folder checksum-sensitive classification outside the generated overlay package
* verification against actual Stellaris checksum behavior when needed

Likely docs to update:

* `GENERATED_OVERLAY_LAYOUT_CONTRACT.md`
* `MULTIPLAYER_REQUIREMENTS.md`
* `TECHNICAL_RUNTIME_LIMITATIONS.md`

## Companion App Lifecycle Skeleton

Status:

```text
RESUME_CANDIDATE
```

Why it matters:

* autosave archiving depends on the app running before Stellaris starts
* first-install start-with-Windows default should remain disabled unless the user explicitly approves a different product choice
* the app should explain why optional startup improves archive completeness

Likely implementation areas:

* companion app process lifecycle
* tray behavior
* read-only autosave archive monitor
* local save campaign directory monitor

Current progress:

* `--archive-stable-saves <save_root> <archive_root> <session_id> [stability_delay_ms]` performs a one-shot read-only archive pass for stable `.sav` files
* `--archive-live-saves <save_games_root> <archive_root> <session_id> [stability_delay_ms]` recursively captures stable live `autosave*.sav` and `ironman.sav` revisions, deduplicated by source identity and content hash
* `--snc-live-autosave-monitor <save_root|auto> <archive_root> <session_id> [poll_ms] [stability_delay_ms] [max_iterations] [use_detected_stellaris_state] [stellaris_running_override] [capture_when_not_running]` is the native SNC-owned live monitor entry point. Production SNC should run this logic internally instead of relying on a PowerShell background helper.
* `tools/watch_stellaris_live_autosaves.ps1` remains a manual development harness only.
* Do not use PowerShell-based hidden startup or HKCU Run persistence for live autosave capture; Avast flags that pattern as suspicious. Startup capture belongs in the native companion app with explicit owner-controlled start-with-Windows configuration.
* archived sessions receive `manifest.json` with source path, archived path, byte count, content hash, and copy/skip status
* `--verify-autosave-archive <session_archive_dir>` verifies copied saves against manifest byte counts and content hashes
* `--summarize-autosave-archive <session_archive_dir> <summary_output.json>` emits a bounded metadata summary from a verified archive
* `--build-ministry-input-from-archive <session_archive_dir> <campaign_id> <empire_id> <ministry> <output_json>` emits a conservative v0 ministry input context from verified archive metadata

Remaining:

* continuous companion-app watch loop
* retry/backoff policy for locked or changing autosaves
* session lifecycle detection from Stellaris process state
* tray/status UI for archive health

## Generated Campaign Library Save Inventory

Status:

```text
PARTIAL_DONE
```

Why it matters:

* active generated mod library should contain only campaigns that can currently be loaded locally
* companion app needs a read-only save inventory before it can clean and rebuild generated rulesets

Completed:

* `--scan-save-campaigns <save_root> <inventory_output.json>` inventories campaign directories and loose `.sav` files
* inventory entries include selected anchor save name, byte count, hash algorithm, and read-only content hash
* `--discover-stellaris-save-roots <output.json>` reports likely Windows save roots from user Documents and OneDrive Documents/Dokumenty candidates
* `--diff-save-campaigns <previous_save_root> <current_save_root> <diff_output.json>` reports added, removed, changed, and unchanged local save entries
* `--plan-campaign-library <save_root> <max_campaigns> <plan_output.json>` emits an auditable include/skip plan for the bounded active generated campaign library
* `--compile-campaign-library-overlay <input.dsl> <save_root> <max_campaigns> <output_dir>` compiles only DSL rules matching included local campaign keys and writes the library plan beside the generated overlay
* scanner is read-only and does not parse or edit save contents
* tests cover directory campaigns, loose saves, ignored non-save files, inventory diff behavior, active campaign library planning, filtered campaign library overlay compilation, and CLI JSON output

Remaining:

* user-facing save root selection and override UI
* campaign identity fingerprinting
* robust rename detection from save-content identity rather than path/key changes
* known campaign memory lookup
* production companion app cleanup/rebuild integration
* user-pinned campaign exceptions
* companion app watch loop integration

## Campaign Marker Handshake

Status:

```text
RESUME_CANDIDATE
```

Why it matters:

* the mod cannot reliably know the human campaign name without a generated marker
* campaign-specific rules must fail closed when marker identity does not match
* the companion app needs to stage the correct marker before launch

Likely docs to update:

* `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md`
* `CAMPAIGN_IDENTITY_RULES.md`
* `GENERATED_OVERLAY_LAYOUT_CONTRACT.md`

---

# Do Not Reopen Without New Evidence

The following points are already settled unless new evidence appears:

* no live LLM control path into a running Stellaris session
* no lower-level game integration outside ordinary mod files, save files, launcher-visible configuration, and companion-app file handling
* no full replacement of vanilla AI
* no independent per-client LLM decision generation for multiplayer
* no append-only generated overlay growth
* no raw LLM Stellaris script output
* no unvalidated raw conversation as campaign memory

---

# Verification Habit

After editing durable architecture or safety-related docs, run:

```powershell
powershell -NoProfile -ExecutionPolicy RemoteSigned -File tools\run_safety_audit.ps1
```

Run broader tests only when the change touches code, test contracts, generated overlay behavior, or executable pipeline behavior.
