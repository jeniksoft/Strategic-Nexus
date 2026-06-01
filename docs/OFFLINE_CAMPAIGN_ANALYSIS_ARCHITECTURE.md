# Strategic Nexus - Offline Campaign Analysis Architecture

## Purpose

This document defines the revised architecture direction for Strategic Nexus after the realtime delivery assumption was rejected.

The mod remains an overlay on top of vanilla Stellaris AI.

Strategic Nexus improves future sessions by analyzing local campaign history after play, then preparing a bounded mod update for the next launch.

It must not depend on live LLM decisions entering an already-running game.

---

# Core Direction

Strategic Nexus is a session-to-session strategic intelligence layer.

The safe loop is:

```text
player starts Stellaris with Strategic Nexus enabled
-> companion app detects the play session
-> companion app archives autosaves using read-only access
-> player exits Stellaris
-> local analysis reads archived save snapshots
-> campaign-scoped memory and personality state are updated
-> a staged Strategic Nexus mod update is prepared
-> next launch uses the updated campaign-specific strategic overlay
```

The runtime game remains controlled by vanilla Stellaris AI and normal mod scripts.

The LLM must not generate Stellaris script directly.
It may propose bounded Strategic Nexus DSL rules.
Those rules must pass validation and deterministic compilation before they can appear in the generated overlay.

Detailed rules:

* [META_RULE_LANGUAGE_AND_COMPILER.md](META_RULE_LANGUAGE_AND_COMPILER.md)
* [CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md](CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md)

The release companion app should be understood as a campaign orchestrator:

* automate safe routine staging decisions
* keep mandatory user interaction rare
* use a release Status Center for status and recoverable problems
* keep the development Task Board out of release builds

---

# Session Delta Analysis Rule

Offline analysis should normally process only autosaves archived during the most recent play session.

Older campaign history should already be represented by durable Strategic Nexus memory summaries, personality state, relationship state, and audit records.

Correct model:

```text
previous durable memory
+ autosaves from latest play session
-> updated durable memory
-> regenerated active overlay snapshot
```

Incorrect model:

```text
all autosaves from the entire campaign
-> full reanalysis every time
```

This keeps analysis time bounded as campaigns grow older.
Longer galaxies naturally have more play sessions, but each session should be analyzed incrementally.

Full campaign reanalysis may exist later as an explicit maintenance/rebuild tool, not as the normal post-session path.

The preferred production analysis shape is:

```text
verified archived autosaves from latest session
-> deterministic season delta ledger
-> empire-scoped briefs
-> LLM interpretation
-> validated structured deltas
-> durable memory update
-> generated overlay snapshot
```

The LLM should interpret bounded briefs, not raw full saves, in the normal path.

---

# Stellaris Save Parser Direction

Strategic Nexus should treat Stellaris `.sav` files as read-only ZIP containers.

Expected entries:

```text
meta
gamestate
```

The parser should run only on verified archived copies.
Active Stellaris saves remain game-owned source material and must not be edited, renamed, deleted, or overwritten.

The first production parser should:

* open `.sav` read-only as a ZIP archive
* read `meta` before `gamestate` for version, date, campaign name, DLC, and cheap identity hints
* read `gamestate` through narrow extractors for known fields only
* emit structured Strategic Nexus facts, uncertainties, and missing-field notes
* refuse corrupt ZIPs, missing entries, invalid text, unsupported versions, and oversized field values
* never pass full raw `gamestate` text to the LLM

Parser output should feed the existing chain:

```text
verified archive
-> save parser facts
-> season delta ledger
-> empire brief
-> bounded LLM interpretation
-> validated DSL
-> generated overlay
```

This means the parser improves the existing metadata-only archive pipeline.
It does not replace archive verification, empire-scoped filtering, LLM context limits, DSL validation, manifest verification, or generated overlay publishing gates.

---

# Companion App Responsibilities

The companion app may:

* detect that Stellaris is running
* detect whether the Strategic Nexus mod appears enabled through safe local signals
* minimize or close to the system tray
* optionally start with Windows if the user enables that setting
* watch known save directories at low frequency
* scan local save campaign directories into a read-only inventory
* copy autosaves into a Strategic Nexus archive directory
* wait until files are stable before copying
* analyze archived saves after the game exits
* maintain campaign-scoped memory outside the save file as durable on-disk state
* prepare a staged mod update for the next session
* automatically publish a validated staged overlay when confidence is high and Stellaris is closed
* help the user choose, download, configure, verify, and switch supported local LLM models
* show a quiet Status Center warning when user action is needed

The companion app must not:

* attach to the game
* inspect game memory
* patch the game
* issue commands to the game
* block the game while inference runs
* rename, delete, overwrite, or edit active autosaves
* modify the active mod package while Stellaris is running
* bundle LLM model weights with the Stellaris mod
* bypass model license acceptance, gated access, authentication, or user consent
* run LLM interpretation when no supported model is installed and selected

Process detection is allowed only as a lifecycle signal.
It must not become game-state inspection.

The development Task Board under `tools/dev_attention/` is not a release feature.
Release UX should use a productized Status Center, not the Codex/project-owner task board.

## Local LLM Model Setup

LLM model weights are an external local dependency, not part of the Strategic Nexus Stellaris mod package.

The user chooses which supported model to use.
The companion app may recommend a model according to hardware capability and license compatibility, then automate the download and configuration after the user confirms.

The supported-model list must be curated.
Each model entry must include license and use-policy metadata sufficient for the companion app to decide whether the model is usable for local non-commercial mod-assistant analysis.

The companion app should persist local model state:

```text
selected_model_id
model_version
runtime
source_url
license_url
use_policy_url
local_path
content_hash
installed_at
last_verified_at
hardware_fit
status
```

Model status examples:

```text
no_model_installed
model_ready
model_missing
model_incompatible_with_hardware
model_license_requires_user_action
model_license_not_supported
model_runtime_failed
model_changed_revalidation_needed
```

Changing models during use is allowed.
After a model change, cached interpretation outputs and generated overlays that depended on the previous model should be treated as stale until revalidated or regenerated.

If no supported model is installed:

* autosave archive still works
* deterministic archive, manifest, package, and overlay verification still work
* existing verified generated overlays can remain active
* new offline LLM interpretation is disabled
* Status Center must explain that Strategic Nexus is running in reduced mode

Strategic Nexus should not imply that a model is included with the mod.
It should explain the dependency plainly and offer one-click setup for supported models where license terms allow it.

Canonical details for the model weights, runtime, companion, validation, reduced-mode, and mod boundary are in [LOCAL_LLM_INTEGRATION_CONTRACT.md](LOCAL_LLM_INTEGRATION_CONTRACT.md).

## Model, Runtime, Companion, And Mod Boundary

Strategic Nexus must keep these components separate:

```text
LLM model weights
local model runtime
Strategic Nexus Companion
Strategic Nexus Stellaris mod
```

The model weights are external local dependencies chosen by the user.
They are not part of the mod and are not distributed as Strategic Nexus mod content.

The local model runtime is a replaceable execution layer.
It may be an installed local runtime, a user-provided local server, or another supported local inference backend.
The runtime is responsible for loading the selected model and returning candidate output.
The runtime is not trusted to enforce Strategic Nexus safety rules.

The companion app is the policy and validation boundary.
It owns:

* supported-model catalog
* license/use-policy metadata
* hardware recommendation
* user consent and model selection state
* runtime configuration
* offline inference calls
* prompt and context assembly from bounded summaries
* parsing and validating model output
* invalidating cached analysis when the selected model changes
* generating or refusing generated overlay candidates
* Status Center model readiness and reduced-mode reporting

The Stellaris mod is intentionally smaller and dumber.
It owns:

* stable handwritten policy kernel
* generated overlay files already accepted by companion validators
* ordinary scripted effects/triggers/events
* fallback behavior when generated input is absent or rejected

The mod must not:

* contain model weights
* call model runtimes
* accept raw model output
* parse prompts
* trust arbitrary generated text

## LLM Output Trust Boundary

All model output is untrusted until validated.

This applies even when:

* the model was recommended by Strategic Nexus
* the model is local and private
* the runtime is supported
* the user selected the model intentionally
* previous outputs from the same model were valid

Allowed promotion path:

```text
model output
-> parse as bounded candidate structure
-> reject unsupported fields
-> validate allowlisted strategy dimensions
-> compile through Strategic Nexus DSL
-> verify generated overlay manifest
-> publish only when Stellaris is closed
```

Rejected model output must not be repaired by guessing gameplay intent.
The companion may ask the model for a new bounded candidate, fall back to deterministic defaults, or keep the previous verified overlay.

No raw prompt, raw completion, model chain-of-thought, or free-form model text may enter the generated gameplay files.
Persisted memory may store only validated structured facts, concise rationale summaries, confidence, source references, and audit metadata.

## Startup And User Explanation

Strategic Nexus must assume the computer may be shut down or restarted between play sessions.

The companion app must not rely on volatile runtime continuity across sessions.

The app should offer an optional "start with Windows" setting.
This setting must be user-controlled.

Default first-install state:

```text
start_with_windows = false
```

The first-run setup should strongly recommend enabling it, but should not silently enable it.

During installation or first-run setup, the app should explain why starting with Windows is useful:

* if the app is not running before Stellaris starts, it may miss autosaves
* missed autosaves reduce historical continuity
* the game remains playable, but Strategic Nexus may have a less complete campaign history
* more frequent autosaves provide a finer history step for offline analysis

The explanation should be practical and non-alarming.
The app should not imply that the game is unsafe without it.

Recommended first-run UX:

```text
[ ] Start Strategic Nexus with Windows (recommended)
```

The label may mark the option as recommended, but the unchecked state preserves explicit user consent.
If the user enables it, the app should also show where to disable it later.

The app may run for the whole Windows user session.
Closing the visible window should normally keep the background companion alive.

The app must provide a clear explicit exit action.
After explicit user exit, the app must not auto-restart until the user starts it again or until a future Windows startup where start-with-Windows is enabled.

Windows auto-login may be treated only as an owner-controlled local machine convenience, not as a default recommendation.
If used, it is a security tradeoff and must be explicit.

Normal OS shutdown, restart, and logoff are not crashes.
The companion should stop cleanly and should not fight the system.

Unexpected crashes are different.
If the user did not explicitly exit and the OS is not shutting down, a watchdog may restart the app with bounded backoff and crash-loop protection.

Repeated crash behavior must protect the user from annoyance:

* do not restart forever
* surface a Status Center warning after repeated failures
* prepare a support report locally
* require user approval before sending anything to `support@jeniksoft.cz`
* avoid raw saves and unnecessary personal data in reports by default

The user's practical expectation is that, if they enabled the companion and did not explicitly exit it, Strategic Nexus will be running before Stellaris starts.
The lifecycle design should protect that expectation without behaving like unwanted software.

---

# Autosave Archive Rule

Autosaves are owned by Stellaris.

Strategic Nexus must treat active autosaves as read-only source material.

The archive worker must:

* copy, never move
* never write into the active save slot
* never delete active saves
* wait for file size and modification time to become stable before copying
* tolerate copy failures without retry storms
* write archived copies into a separate Strategic Nexus directory
* preserve enough metadata to reconstruct save order

Recommended archive identity:

```text
archive/
    campaigns/
        <campaign_id>/
            saves/
                2230-04-01_autosave_001.sav
                2230-07-01_autosave_002.sav
            manifest.json
            analysis/
                campaign_memory.json
                empire_memory.json
                staged_mod_update.json
```

If an autosave is locked, incomplete, or changing, the app should skip it and try later.
Missing one autosave must not threaten the campaign.

The Strategic Nexus archive and memory store is persistent disk state managed by the companion app.
It must not exist only as volatile runtime state.

The current local harness for the first read-only archive pass is:

```text
Strategic Nexus.exe --archive-stable-saves <save_root> <archive_root> <session_id> [stability_delay_ms]
```

It samples `.sav` files twice with a short delay, copies only files whose size and modification time remain stable, and writes `manifest.json` beside the archived session.
It is a one-shot archive skeleton for testing the safety contract, not the final continuous companion-app monitor.

Observed save-folder rule:

* Stellaris save roots can exist in local Documents, synced Documents/Dokumenty, and Steam Cloud local cache locations such as `Steam/userdata/<steam_user_id>/281990/remote/save games`.
* Steam may be installed outside default program directories. SNC should discover the Steam Cloud userdata root from the Windows registry Steam install path first, then keep `ProgramFiles*` probing as fallback.
* The game's cloud-save option can make the Steam Cloud local cache the active save root for a campaign. With cloud saving disabled, the campaign may write to the ordinary local save-game root instead.
* SNC should treat both locations as normal active roots and monitor them read-only; neither should be treated as a durable long-history archive.
* Campaign directories contain normal date-named saves, `ironman.sav`, or autosaves such as `autosave_YYYY.MM.DD.sav`.
* Campaign directory names are expected to be stable for a campaign. Only one campaign is actively played at a time, but the active campaign can change while `stellaris.exe` and SNC are still running.
* Campaign directories are dynamic. They may be created by a new game, restored by sync, or copied manually while SNC is already running.
* Autosaves are a bounded retention window. The game may keep only a small number of recent autosaves and prune or overwrite older ones during the active session according to its autosave cadence.
* `continue_game.json` can identify the last intended continuation target, but it is not authoritative because it may reference a save stem that is no longer present locally.
* The final companion monitor must watch all discovered save roots while `stellaris.exe` is running and copy every observed stable `autosave*.sav` and `ironman.sav` revision into the SNC archive immediately after it settles.
* SNC must not rely on knowing the active campaign folder. Each live pass scans the whole `save games` tree so newly created or manually added campaign folders are included.
* SNC itself is a long-running tray app. Its process lifetime is not a capture-session boundary; capture sessions are derived from observed Stellaris play activity because autosaves are produced only by `stellaris.exe`.
* A live capture session is a Stellaris play-activity collection window, not a campaign identity boundary. It may include sequential play segments from more than one campaign folder, so later analysis must group archived entries by source campaign folder before creating campaign ledgers, empire briefs, or generated overlay updates.
* While `stellaris.exe` is running, the companion capture responsibility is read-only archive preservation and status reporting. Analysis and generated overlay refresh happen after the game exits.
* After `stellaris.exe` exits, SNC should verify the completed capture archive, partition entries by source campaign folder, analyze those autosaves, and generate/stage new mod rules for the next launch.
* SNC owns the post-play identity and decision-input flow. In normal product use, the owner must not have to provide `campaign_id`, `empire_id`, ministry input, or DSL by hand.
* Development CLI commands may still accept explicit identity or `input.dsl` arguments as harness controls, but those arguments are not the final companion workflow.
* The archive is the long history. The active save folder is only the game's short rolling buffer.

Post-play SNC ownership:

```text
verified capture archive
-> partition by source campaign folder
-> resolve campaign identity from marker, save metadata, hashes, path, and durable memory
-> resolve player/empire identity from parsed save facts and durable memory
-> build bounded decision input
-> produce or select bounded Strategic Nexus DSL candidate
-> validate DSL and compile generated overlay
-> verify staged overlay
-> publish only while Stellaris is closed
```

If identity confidence is low, SNC should create a new campaign profile or ask for confirmation. It should not silently merge histories or require the owner to type technical identifiers into the normal flow.

The current live-session capture harness is:

```text
Strategic Nexus.exe --archive-live-saves <save_games_root> <archive_root> <session_id> [stability_delay_ms]
Strategic Nexus.exe --snc-live-autosave-monitor <save_root|auto> <archive_root> <session_id> [poll_ms] [stability_delay_ms] [max_iterations] [use_detected_stellaris_state] [stellaris_running_override] [capture_when_not_running]
Strategic Nexus.exe --run-snc-session-capture [archive_root] [status_output.json] [session_id] [poll_ms] [stability_delay_ms] [max_iterations] [capture_when_not_running]
```

The one-shot command recursively scans campaign directories, archives stable `autosave*.sav` and `ironman.sav` revisions, and stores changed revisions under hash-based filenames so repeated active autosave names do not overwrite the SNC history.
The native SNC monitor repeats that archive pass inside the companion runtime. It is the production direction for live capture because the user should only need to run SNC; hidden PowerShell startup helpers and HKCU Run persistence are not acceptable project architecture.
The session capture command is the first foreground/testable SNC runtime slice: it auto-discovers save roots, uses default archive/status paths, and writes heartbeat JSON so the owner can verify that the monitor is alive during a real Stellaris session.
The first tray slice is `dist/private_tools/StrategicNexusCompanionTray.exe`, built by `tools/build_snc_tray.ps1` and started by `tools/run_snc_tray.ps1`. It is single-instance, waits in the tray, opens a capture window when `stellaris.exe` is detected, and writes both tray and live-capture heartbeat JSON.

The current local harness for verifying an archived session is:

```text
Strategic Nexus.exe --verify-autosave-archive <session_archive_dir>
```

It verifies copied saves against the archive manifest by existence, byte count, and content hash.
Offline analysis should consume only archive sessions that pass verification.

The current local harness for producing a bounded archived-session summary is:

```text
Strategic Nexus.exe --summarize-autosave-archive <session_archive_dir> <summary_output.json>
```

It first verifies the archive, then emits metadata such as copied save count, total byte count, and first/last save hash anchors.
It does not parse save contents and must not be treated as the final strategic save summary.

The current local harness for the first narrow save-content parser is:

```text
Strategic Nexus.exe --parse-stellaris-save <save.sav-or-extracted-save-dir> <summary_output.json>
```

It extracts a bounded player-empire headline summary from `meta` and `gamestate`: save version/date/name, player country id, empire identity, founder species, capital planet, home system, owned fleet headlines, active war count, missing fields, and uncertainties.
This parser is intentionally narrow.
The next architecture-aligned implementation step is to feed this structured parser summary into the archive ledger, empire brief, ministry input, and offline spine instead of leaving those stages metadata-only.

The current local harness for producing the first season delta ledger skeleton is:

```text
Strategic Nexus.exe --build-season-delta-ledger <session_archive_dir> <campaign_id> <output_json>
```

It verifies the archived session through the archive summarizer, then writes a metadata-only ledger with archive facts, first/last hash anchors, and explicit parser uncertainties.
This is the first contract step for the `verified archive -> season delta ledger -> empire brief` path.
It does not extract wars, borders, alliances, economy, fleets, empire state, or personality evidence yet.
Now that the narrow save parser exists, this ledger should be enriched from parser summaries before broader strategic memory or LLM interpretation is added.
The explicit `<campaign_id>` parameter is a development harness input. Production SNC should derive or confirm it during post-play archive analysis.

The current local harness for producing the first empire brief skeleton is:

```text
Strategic Nexus.exe --build-empire-brief-from-archive <session_archive_dir> <campaign_id> <empire_id> <output_json>
```

It builds from the verified archive and metadata-only ledger, then emits facts, uncertainties, missing information, and compression notes for one campaign empire.
It intentionally warns that personality or strategy must not be inferred from the metadata-only brief alone.
This is a contract skeleton for future empire-scoped save parsing and LLM interpretation, not a final strategic analysis.
The explicit `<empire_id>` parameter is a development harness input. Production SNC should derive or confirm it from parsed save identity plus durable campaign memory.

The current local harness for bridging verified archive metadata into the v0 ministry pipeline is:

```text
Strategic Nexus.exe --build-ministry-input-from-archive <session_archive_dir> <campaign_id> <empire_id> <ministry> <output_json>
```

It emits a conservative ministry input context with archive facts and explicit uncertainties.
It must be replaced or enriched by real save parsing before it can support meaningful strategic interpretation.
Production SNC should call this kind of builder internally after resolving campaign and empire identity, not require the owner to supply those values manually.

The current local harness for proving the metadata-only archive input can pass through the v0 decision pipeline is:

```text
Strategic Nexus.exe --v0-pipeline-from-archive <session_archive_dir> <campaign_id> <empire_id> <ministry> <ministry_input_output_json> <decision_output_json> <sequence_id> <created_unix_ms> <ttl_ms> [audit_output_json]
```

It verifies and summarizes the archived session, writes the conservative ministry input context, then runs the deterministic v0 pipeline against that generated input.
This is an end-to-end contract harness for the current `verified archive -> ministry input -> v0 payload` path.
It does not add save parsing, LLM interpretation, or strategic memory updates.
Production SNC should own the generated ministry input and DSL candidate selection/generation before validation. Manual `input.dsl` files are developer fixtures unless the owner explicitly chooses an advanced review/edit mode.

---

# Campaign Identity

Campaign identity must be derived without editing saves.

Possible identity inputs:

* save metadata
* galaxy seed when available
* game start date
* player empire identity
* known empire roster fingerprints
* save path and first archived save hash as local fallback

The exact implementation may evolve, but the architectural rule is fixed:

```text
campaign memory, empire personality, and generated mod state must never bleed across campaigns.
```

If identity confidence is low, Strategic Nexus must create a new campaign profile or ask for user confirmation instead of merging histories aggressively.
Campaign identity resolution is an SNC responsibility. The final owner workflow should not depend on manual `campaign_id` entry.

## Mod-Side Campaign Recognition Finding

Current verification result:

* the companion app can identify campaigns externally from archived save paths, save `meta`, save `gamestate`, hashes, and manifests
* the scripted mod can persist and later detect flags and numeric variables inside normal synchronized game state
* no verified Stellaris script trigger/effect currently exposes the active save filename, archive path, campaign id, or galaxy seed directly to ordinary mod script

Therefore, the mod alone should not be expected to discover a rich campaign identity from the engine.

The safe design is a marker handshake:

```text
base mod initializes a small persistent campaign marker in normal saved game state
-> companion app reads the marker from archived saves
-> offline analysis binds memory to that marker plus external save fingerprint
-> generated overlay activates only when the loaded save still contains the expected marker
```

The marker should be:

* created only through ordinary scripted state
* persisted in the save by Stellaris
* small and bounded
* non-secret
* collision-resistant enough for local campaign separation
* treated as one signal, not the only source of identity

Recommended implementation direction:

* store one or more numeric marker variables on the player or authority country
* optionally set a static Strategic Nexus initialization flag
* have generated overlays check the expected marker before applying campaign-specific behavior
* if marker checks fail, use fallback behavior and let the companion app repair or restage the overlay after play

Campaign-specific generated rules must be guarded by the campaign marker.

The generated overlay may contain rules for a specific campaign, but those rules must be inert unless the loaded save exposes the expected marker values through normal scripted checks.

Required behavior:

```text
marker matches expected campaign
-> campaign-specific rules may activate

marker missing or mismatched
-> ignore campaign-specific rules
-> use base mod fallback
```

This is the main in-mod protection against applying the wrong campaign overlay.

Do not rely on save filename alone.
Do not rely on empire name alone.
Do not silently apply generated campaign behavior when marker confidence is low.

---

# Empire-Scoped Interpretation

The same archived saves may be analyzed multiple times from different empire perspectives.

This is intentional.

Example:

```text
same war history
-> Empire A interprets it as justified liberation
-> Empire B interprets it as betrayal
-> Empire C interprets it as proof that federations are unstable
```

Each empire receives:

* its own memory stream
* its own personality drift
* its own strategic lessons
* its own relationship interpretations
* its own doctrine inertia

There is no single global objective LLM brain.

Every detected empire in every campaign must have its own personality profile.

When the companion app sees an autosave from an unknown campaign, it must bootstrap personality profiles for all detectable empires before generating campaign-specific overlay rules.

Personality identity is:

```text
campaign_id + empire_id
```

LLM conversation may be used offline to interpret personality changes, but the first implementation should persist only validated personality state, source save/date, concise rationale summaries, and confidence.
Structured deltas may be added later for audit and rollback.
Raw conversation and chain-of-thought must not become campaign memory.

Detailed rules:

* [CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md](CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md)

---

# Subjective Other-Empire Profiles

Offline analysis must produce more than generic personality drift.

For each observer empire, Strategic Nexus should maintain subjective profiles of strategically relevant other empires.

Profile identity is:

```text
campaign_id + observer_empire_id + target_empire_id
```

These profiles represent what the observer empire believes about the target empire.
They are not global truth.

Examples of profile dimensions:

* agreement reliability
* betrayal risk
* opportunism
* border pressure
* ally support reliability
* coalition behavior
* expansion style
* threat pattern
* revenge likelihood
* diplomatic predictability

The same target empire may have different profiles from different observers.

Example:

```text
Empire A profile of Empire B:
-> "B betrayed us and attacks weakness."

Empire C profile of Empire B:
-> "B is pragmatic, predictable, and useful against stronger rivals."
```

Each offline analysis pass may produce:

* general observer personality deltas
* observer-target relationship deltas
* observer-target predictive profile deltas
* bounded target-specific Strategic Nexus DSL rule candidates

Target-specific rules are allowed only when they are:

* campaign-marker guarded
* observer-empire scoped
* target-empire scoped
* derived from validated save facts and durable memory
* clamped by runtime and generated-overlay budgets
* safe to omit when confidence is low

The LLM may interpret the meaning of events from the observer's personality context, but persisted output must remain validated structured deltas, concise rationale summaries, source references, and confidence.
Raw conversation and chain-of-thought must not become relationship memory.

This is the intended model for galactic social intelligence:
each empire builds a practical theory of other empires, then the next-session generated overlay nudges vanilla AI through coarse strategic rules.

---

# Internal Pressure, Reputation, And Doctrine Inertia

Strategic Nexus should make the galaxy more believable by making empires act like political entities, not optimized stat machines.

The goal is higher difficulty through better continuity and strategic realism, not through hidden bonuses for AI empires or penalties for the player.

Offline analysis should therefore maintain three additional campaign-empire state layers.

These layers are not independent brains.
They are projections of one integrated campaign-empire state.

Empire state should combine:

* personality and ethics
* current ethics distribution when recoverable from saves
* government and authority
* civics and species traits
* resources, income, deficits, and production trajectory
* territory, borders, threats, and military capability
* internal pressure
* relationship memory and reputation
* doctrine inertia and reform cost

Example:

```text
authoritarian militarist empire
+ high mineral income
+ low food stability
+ strong militarist population share
+ recent humiliation
+ high prestige need
-> more likely to seek a limited revenge war if capability allows it
-> less likely to accept a humiliating compromise
```

Implementation may keep separate schemas for validation and budgeting, but analysis must reconcile them before producing generated overlay rules.
A rule candidate that makes sense for personality but contradicts resources, ethics pressure, capability, or current internal state should be downgraded, revised, or rejected.

## Internal Pressure

Internal pressure represents what the empire needs, fears, and cannot easily ignore.

Examples:

* security anxiety
* economic pressure
* public trauma
* elite agenda
* prestige need
* diplomatic flexibility
* war exhaustion memory
* internal stability pressure

Internal pressure explains why an empire may avoid the theoretically best move.

Example:

```text
high security anxiety + invasion trauma
-> prefer defensive pacts
-> prefer chokepoint fortification
-> reduce risky expansion
```

## Strategic Reputation

Strategic reputation represents how an empire is perceived by other empires through observed or plausibly known behavior.

It must not become telepathy.

Reputation should be:

* observer-scoped or audience-scoped
* evidence-linked
* confidence-scored
* affected by intel and diplomatic contact
* allowed to spread only through plausible channels

Examples:

* unreliable dealmaker
* loyal federation defender
* opportunistic attacker
* predictable rival
* crisis bulwark
* serial betrayer

Strategic reputation may influence multiple observers, but each observer still has its own subjective profile and confidence.

## Doctrine Inertia And Reform Cost

Empires should not fully change strategic style every session.

Doctrine changes should have inertia and reform cost.

Examples:

* a fortress civilization does not instantly become a raiding empire
* a traumatized empire may overinvest in security
* a victorious fleet doctrine remains attractive until evidence weakens it
* a failed doctrine should not be retried without strong new justification

The generated overlay should express these layers only as coarse, bounded strategic nudges.
They must remain optional when confidence is low or when runtime budget would be exceeded.

---

# Mod Update Model

Strategic Nexus must not update the active mod package while Stellaris is running.

Mod script, definitions, and generated gameplay overlay files must be treated as launch-time inputs.
They are loaded by Stellaris for the active session and must not be expected to reload automatically while a campaign is already running.

The companion app should prepare staged output first:

```text
analysis result
-> validated bounded strategic profile
-> staged generated mod files
-> optional backup of previous generated state
-> replace generated mod layer only after Stellaris exits
```

Generated mod state should remain:

* campaign-scoped
* schema-versioned
* reversible
* small
* explainable
* safe to delete

The base mod should remain stable.
Generated campaign overlays should be isolated from hand-authored mod logic where possible.

## Generated Campaign Library

The generated overlay may contain rules for multiple known local campaigns at the same time.

Each campaign ruleset must be guarded by its campaign marker, so loading all known rulesets does not mean all rulesets are active.

The companion app owns generated campaign library maintenance.

The generated overlay is a regenerated snapshot, not an append-only history.

On update, the companion app should:

* build a complete fresh generated overlay from the durable on-disk archive/memory and currently local save campaigns
* compile only validated Strategic Nexus DSL rules into generated mod files
* validate the generated files
* replace the previous generated overlay as a whole while Stellaris is closed
* discard old generated rules that are no longer part of the current active library

It must not keep appending new event/rule blocks forever.
The number of generated events/rules should remain bounded by the current active campaign library, not by total historical analysis count.

It should:

* scan local Stellaris save campaign directories
* detect newly added or manually restored known campaigns
* add or refresh generated rules for campaigns that are present locally
* remove inactive generated rules for campaigns whose saves are no longer present locally
* keep deeper Strategic Nexus archive/memory separately from the active generated mod library
* rebuild the generated overlay when local campaign availability changes

Important distinction:

```text
local save campaign missing
-> generated mod rules for that campaign may be removed from the active mod overlay
-> archived Strategic Nexus memory remains on disk outside the active mod unless explicitly deleted

known campaign save reappears locally
-> companion app can regenerate or restore that campaign ruleset into the active generated overlay
```

This allows the mod to load a bounded library of currently playable campaign rules while preventing unbounded growth.
The active generated mod library is a rebuildable projection of the durable on-disk Strategic Nexus archive/memory, not the source of truth.

The current local harness for the first read-only inventory step is:

```text
Strategic Nexus.exe --scan-save-campaigns <save_root> <inventory_output.json>
```

It scans campaign subdirectories and loose `.sav` files without reading or editing save contents.

The output is a deterministic JSON inventory for future companion-app library maintenance.
Each entry includes a deterministic anchor save fingerprint based on a selected local `.sav` file name, byte count, and read-only content hash.
This can help the companion app recognize local save availability changes more reliably than folder names alone.

The current local harness for discovering likely Windows Stellaris save roots is:

```text
Strategic Nexus.exe --discover-stellaris-save-roots <output.json>
```

It checks common user Documents, OneDrive Documents/Dokumenty, and Steam Cloud userdata locations. Steam Cloud lookup uses the Steam registry install path when available and reports candidate paths with an `exists` flag.
It does not create directories, move saves, or assume that a missing candidate is an error.

The current local harness for comparing two read-only save inventories is:

```text
Strategic Nexus.exe --diff-save-campaigns <previous_save_root> <current_save_root> <diff_output.json>
```

It reports added, removed, changed, and unchanged campaign entries from directory/file metadata only.
This is useful for detecting local save availability changes, but it is not final campaign identity resolution.
Renames without a stable save-content fingerprint may appear as one removed entry and one added entry.

Recommended v0 policy:

* include rulesets only for locally present Stellaris save campaigns
* cap the number or size of active generated campaign rulesets
* keep marker guards mandatory for every campaign ruleset
* treat cleanup as generated-overlay maintenance, not memory deletion

The current local harness for planning the bounded active campaign library is:

```text
Strategic Nexus.exe --plan-campaign-library <save_root> <max_campaigns> <plan_output.json>
```

It scans local save availability, requires an anchor fingerprint for inclusion, applies the active campaign count limit, and writes an auditable include/skip plan.
This plan is a staging decision for generated overlay maintenance, not a deletion command for durable memory.

The current local harness for compiling a generated overlay through that active library plan is:

```text
Strategic Nexus.exe --compile-campaign-library-overlay <input.dsl> <save_root> <max_campaigns> <output_dir>
```

It parses and validates DSL, but only compiles rules whose `campaign` id matches an included local campaign key from the active library plan.
Rules for absent or skipped campaigns are ignored for that generated overlay snapshot.
The output directory also receives `strategic_nexus_campaign_library_plan.json` for auditability.

Development-only console reload commands or script test commands must not be used as the production update mechanism.
They are not a safe basis for multiplayer, Ironman-adjacent expectations, launcher-managed mod state, or automated companion-app behavior.

Production rule:

```text
while Stellaris runs -> capture stable autosaves only
after Stellaris exits -> verify archive and analyze captured autosaves
-> stage generated overlay while Stellaris is closed
-> launch or reload the save in a new session
-> consume the already-loaded overlay during that session
```

---

# What Carries Forward From The Old Architecture

Keep:

* bounded `strategy_dimensions`
* schema validation
* campaign and empire identity metadata
* sequence and freshness concepts
* strategic memory rules
* personality drift rules
* deterministic fallback behavior
* local LLM output validation
* compact audit records
* bridge-core validation logic as an offline validator

Reframe:

* daemon work becomes offline/session-end analysis by default
* bridge output becomes a generated-mod preparation artifact
* priority queues schedule offline empire analysis, not live gameplay responses
* audit records explain why the next-session overlay changed

Remove from production assumptions:

* live LLM decisions entering the current play session
* mod-authored requests to an external process during normal gameplay
* synchronous strategic refresh while Stellaris is running
* any requirement that the mod read arbitrary external files

Keep these only as local development harnesses when useful:

* one-shot request JSON workflows
* explicit test fixtures
* bridge-core effect batch smoke tests
* deterministic v0 pipeline tests

---

# Local LLM Boundary

Local LLM analysis may run after play.

The LLM receives only bounded, extracted summaries unless a future local-only diagnostic mode explicitly needs a narrow save excerpt.

The LLM should not receive:

* full raw saves by default
* private unrelated user files
* arbitrary mod folders
* raw generated scripts to execute
* command strings

The LLM may produce:

* empire personality updates
* memory summaries
* bounded strategy dimensions
* doctrine inertia adjustments
* campaign-specific strategic profiles

All output remains untrusted until validated.

---

# Multiplayer Rule

For multiplayer, only the host campaign archive should drive Strategic Nexus analysis.

Stellaris multiplayer campaign capability is determined by how the campaign was created.
A single-player-founded campaign should not be treated as MP-ready.
A multiplayer-founded campaign may still have only one current human player and can still be MP-capable.

For MP-capable campaigns, SNC should generate validated rules for all known campaign empires, because players can later take over different empires through the normal host/lobby flow.
At runtime, rules for a currently human-controlled empire must not activate; rules may activate only for AI-controlled empires under the campaign/empire marker guards.
For single-player-founded campaigns, the player empire is expected to stay fixed after campaign creation and should remain a do-not-apply target for gameplay-affecting generated rules.

Clients should receive the same generated mod package for the next session when checksum-sensitive files are involved.

The app must not create per-client divergent gameplay-affecting generated files during a multiplayer campaign unless the packaging process explicitly preserves identical host/client mod content.

The companion app must eventually provide an explicit multiplayer package/export workflow for generated overlays.
That workflow must prove or preserve byte-identical gameplay-affecting files for host and clients before MP use.

---

# Open Questions

These must be solved before implementation:

* reliable detection that Strategic Nexus is enabled for the current playset
* exact campaign identity fingerprint
* exact archive directory layout
* which generated mod files are checksum-sensitive
* how to package identical multiplayer generated overlays
* whether save parsing can recover all needed empire identity fields safely
* which save fields can safely support integrated empire-state inputs such as ethics distribution, resources, income, capability, diplomatic state, reputation evidence, and internal pressure evidence
* which 3A/3B outputs must remain summaries only until parser coverage and confidence are good enough for generated overlay rules

Partially answered by current v0 harnesses:

* archive and generated overlay staging have local test layouts, but the final production app layout is not frozen
* generated events, scripted effects, and scripted triggers are treated as gameplay-affecting/checksum-sensitive in v0
* mod refresh is architecturally a post-session complete replacement snapshot while Stellaris is closed; the remaining product decision is how much user review/confirmation the companion app should require before publishing it

---

# Design Goal

Strategic Nexus should feel like:

```text
a civilization memory and strategy layer that learns between play sessions
```

not:

```text
a live controller trying to steer the current simulation from outside the game
```
