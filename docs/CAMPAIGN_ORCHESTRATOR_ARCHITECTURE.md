# Strategic Nexus - Campaign Orchestrator Architecture

## Purpose

This document defines the target release architecture for minimizing mandatory user interaction.

The goal is that a normal user should mostly just play Stellaris.

Strategic Nexus should quietly preserve campaign history, analyze the latest play session, and prepare the next-session generated overlay without requiring routine manual decisions.
The generated overlay should evolve into a reactive policy pack so the next active session can respond to major events through already-loaded normal mod script.

This document refines, but does not replace, `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md`.

---

# Core Principle

Strategic Nexus should behave as a campaign orchestrator, not as a live game controller.

The orchestrator coordinates:

* local save discovery
* read-only autosave archiving
* latest-session delta extraction
* durable campaign and empire memory
* validated DSL rule generation
* deterministic generated overlay compilation
* safe next-launch staging
* precompiled reactive policy-pack coverage for major event families

It must not:

* inspect Stellaris memory
* patch the game
* edit active saves
* update active mod files while Stellaris is running
* require live LLM decisions during gameplay
* require routine user choices before every session

---

# Naming

The release companion application is named:

```text
Strategic Nexus Companion (SNC)
```

`SNC` is the short product/development abbreviation for the companion app.

SNC is distinct from the local development Task Board used by Codex and the project owner.

---

# Target Release Loop

```text
Windows starts
-> Strategic Nexus Companion (SNC) may start if the user enabled it
-> app watches local save/playset signals at low frequency
-> before Stellaris launch, app stages the best known safe generated overlay
-> player plays normally
-> app archives stable autosaves with read-only access
-> player exits Stellaris
-> app verifies the archived session
-> deterministic extractor builds a latest-session delta ledger
-> local LLM interprets bounded empire-specific briefs
-> validator accepts only structured memory/personality/rule deltas
-> compiler emits a complete replacement generated reactive policy pack snapshot
-> next launch uses the updated overlay
-> during play, ordinary mod script selects among already-loaded policy branches
```

The app should prefer automatic safe action over asking the user.

User interaction is reserved for ambiguity, missing permissions, failed validation, multiplayer packaging, or product settings.

---

# Product Boundary: Task Board Is Development-Only

The local Task Board under `tools/dev_attention/` is a development coordination tool for the project owner and Codex.

It is not part of the release product.

It should not be treated as the future end-user UX.

Release builds may have a similar concept, but it must be productized separately.

Recommended release name:

```text
Status Center
```

The Status Center is not a task board.

It is a quiet user-facing place for:

* current campaign status
* archive health
* last analysis result
* next overlay readiness
* warnings and recoverable problems
* optional settings
* multiplayer package/export status
* multiplayer friend/package sync status
* copyable multiplayer package/invite status text

It should not ask the user to manage routine development tasks.

For multiplayer season design, see `MULTIPLAYER_SEASON_ORCHESTRATOR.md`.

---

# Release Companion Lifecycle

Strategic Nexus Companion (SNC) should be a persistent background assistant, not a tool the user must remember before every Stellaris session.

First-install default:

```text
start_with_windows = false
```

The app may strongly recommend enabling start-with-Windows during first-run setup, but it must not silently enable it.

Normal lifecycle:

```text
Windows starts
-> companion starts if the user enabled start-with-Windows
-> user may open/close the window without stopping the background app
-> user may hard-close/exit the app explicitly
-> if not explicitly exited, the app remains available until OS shutdown/restart/logoff
```

The app must distinguish:

* window close -> minimize to tray / continue background work
* explicit user exit -> stop and do not auto-restart
* OS shutdown, restart, or logoff -> stop cleanly; next start follows the start-with-Windows setting
* unexpected crash -> restart only through a bounded crash policy

The app should not fight Windows shutdown or restart.

It should also not disappear silently after a recoverable crash if the user reasonably expects it to be running before Stellaris starts.

---

# Crash Restart Policy

Unexpected crashes may be restarted automatically, but only with backoff and a loop guard.

Required behavior:

* record crash reason, timestamp, app version, and recent operation
* restart after a short delay for the first unexpected crash
* increase delay after repeated crashes
* stop restarting after a small repeated-crash threshold
* show a Status Center warning when crash-loop protection disables restart
* never restart after explicit user exit
* never restart during normal OS shutdown/restart/logoff

Recommended initial thresholds:

```text
restart_attempts_per_10_minutes <= 3
backoff = 10s, 60s, 5m
after_threshold = disable auto-restart until user starts app again
```

The goal is to protect autosave history without creating an annoying crash loop.

Yes, software that repeatedly opens just to fall over is technically persistent, but not in the way humans enjoy.

---

# Crash Report Policy

When the app repeatedly crashes or hits a serious recoverable fault, it should prepare a diagnostic support report for the developer.

Developer support contact:

```text
support@jeniksoft.cz
```

The app must not send the report automatically.

Required behavior:

* prepare a local report preview
* explain what categories of data are included
* allow the user to approve or cancel sending
* allow the user to copy/export the report manually
* avoid including raw saves by default
* redact or summarize paths, usernames, IPs, machine identifiers, and other personal data where possible
* include explicit user consent before sending

Crash reporting is a release legal/privacy surface, not only a technical logging feature.

The implementation should assume public release and EU/GDPR-style consent expectations.

---

# Status Taxonomy

The Status Center should expose high-level product status, not developer task-board internals.

## Archive Status

Archive status tells the user whether Strategic Nexus is preserving campaign history correctly.

It should answer:

* is the app watching the right Stellaris save folders?
* was the current or latest session detected?
* were autosaves copied read-only into the Strategic Nexus archive?
* did copied saves pass manifest/hash verification?
* when was the last successful archive update?
* did any autosave copy fail because the file was locked, incomplete, missing, or unstable?

Example states:

```text
Archive OK
Waiting for Stellaris
Archiving current session
Last autosave missed
Archive warning
Archive blocked by permissions
```

## Generated Overlay Status

Generated overlay status tells the user whether the next launch has a safe Strategic Nexus mod update staged.

It should answer:

* which campaign rules are currently included in the active generated overlay?
* was the overlay rebuilt from validated DSL and durable memory?
* does the generated manifest match the generated files?
* is the staged overlay safe for next launch?
* is Stellaris currently running, preventing staging?
* is multiplayer packaging/export required before play?
* did validation reject generated rules and fall back safely?
* is the active overlay post-session-only or reactive-policy-pack capable?

Example states:

```text
Overlay ready
Overlay fallback active
Overlay waiting for Stellaris to close
Overlay validation failed
MP package required
Manifest mismatch
```

## Status Center

Status Center is the release UI surface that combines archive status, generated overlay status, analysis status, settings, warnings, and support actions.

Initial Status Center wording, grouping, severity levels, and state names may be chosen by Codex during implementation.

The project owner will refine the product language later from real use.

Codex should not block implementation on owner approval for ordinary Status Center presentation details when the underlying architecture is clear.

Codex must still flag status design that seems misleading, too noisy, legally risky, or likely to hide important archive/overlay failure.

It should be quiet by default.

It should become visible or notify the user only when:

* user action is required
* history capture is at risk
* repeated crashes disabled auto-restart
* overlay staging failed or is unsafe
* multiplayer package/export must be confirmed
* multiplayer friend sync needs confirmation or reports a package mismatch
* support report approval is needed

It should not behave like the development Task Board.

---

# Local Model Manager

Strategic Nexus must not distribute LLM model weights with the Stellaris mod.

Reasons:

* model files are large and impractical for mod distribution
* model licenses differ and may restrict commercial, hosted, derivative, or other use cases
* some models require the individual user to accept license or gated-access terms
* the project must not silently bundle a model whose terms are unclear or incompatible with the release mode
* Paradox modding terms should be treated as making the Stellaris mod itself non-commercial unless separate explicit permission says otherwise

The companion app should own local model setup through a Model Manager.

The Model Manager should:

* detect local hardware capability such as CPU, RAM, GPU, VRAM, disk space, and operating system
* recommend a model based on hardware fit, expected quality, runtime cost, and license compatibility
* let the user choose another supported model when they understand the tradeoff
* download or connect the selected model only after explicit user action
* use official model sources, official runtimes, or a documented user-provided local path
* preserve license metadata, model id, version, source URL, local path, and content hash in local state
* support changing the selected model during use
* revalidate outputs after a model change when cached analysis or generated overlays may depend on old behavior
* show Status Center warnings when no supported model is installed, the model is missing, the model license is incompatible with the selected mode, or the model cannot run on the current hardware

The companion app may automate download, installation, runtime wiring, and configuration after the user selects a model and confirms the action.
This automation is a convenience layer; it must not bypass license acceptance, gated access, authentication, or user consent.

Canonical details for the model weights, runtime, companion, validation, reduced-mode, and mod boundary are in [LOCAL_LLM_INTEGRATION_CONTRACT.md](LOCAL_LLM_INTEGRATION_CONTRACT.md).

## Local LLM Ecosystem Layers

The local LLM ecosystem has four separate layers.

They must not be collapsed into one product component:

```text
model weights
-> model runtime
-> Strategic Nexus Companion
-> Strategic Nexus mod and generated overlay
```

Layer responsibilities:

* Model weights are third-party or user-provided artifacts. Strategic Nexus records metadata and verifies local availability, but does not distribute them with the mod.
* Model runtime is the local execution engine or API wrapper. It loads the selected model and returns text or structured candidate output. The runtime is replaceable.
* Strategic Nexus Companion owns model selection, license-aware setup, hardware fit checks, prompt construction, offline inference calls, output validation, cache invalidation, and Status Center readiness.
* Strategic Nexus mod owns only normal Stellaris mod files, stable scripted fallback behavior, generated overlay files, and launcher-visible mod state.

Trust boundary:

```text
local saves / archive summaries / prompts / model output = untrusted or semi-trusted input
schema validation / allowlists / DSL validator / manifest verifier / package verifier = trusted gates
generated overlay accepted by validators = bounded trusted artifact
active Stellaris session = never directly controlled by the LLM
```

The model runtime may fail, hallucinate, ignore instructions, produce malformed output, or produce plausible but unsafe recommendations.
Strategic Nexus must never treat model output as trusted merely because it came from a supported local model.

Model output may only become gameplay-affecting after all applicable gates pass:

* bounded schema parse succeeds
* unsupported fields are rejected
* values are allowlisted and clamped
* DSL validation succeeds
* generated files are compiled deterministically
* generated overlay manifest verification succeeds
* publishing happens only while Stellaris is closed
* multiplayer package verification passes when MP use is relevant

If any gate fails, the companion app should keep or restore safe fallback behavior and surface an ordinary Status Center explanation.

Recommended model catalog fields:

```text
model_id
display_name
provider
runtime
source_url
license_name
license_url
use_policy_url
commercial_allowed
requires_user_license_acceptance
requires_login_or_gated_access
recommended_min_ram_gb
recommended_min_vram_gb
recommended_gpu_class
quality_tier
speed_tier
privacy_mode
status
last_license_review_date
```

Allowed catalog status values:

```text
approved_for_noncommercial_mod_use
approved_for_local_experimental_use
requires_user_license_acceptance
restricted
unsupported
```

The default recommendation must prefer models whose terms allow the intended local, non-commercial mod-assistant use.
Models with unclear terms, incompatible terms, missing license metadata, or required gated approval must not be silently selected as the default.

If no compatible local model is installed, Strategic Nexus should remain safe but reduced:

* archive and verification can still run
* deterministic validators can still run
* existing generated overlays remain usable if already verified
* new LLM interpretation should not run
* Status Center should explain that Strategic Nexus is not fully functional until a supported model is installed

The release product language should describe the model dependency similarly to an external runtime dependency:

```text
Strategic Nexus needs a supported local AI model for offline strategy analysis.
The model is not included with the mod.
You can choose and install a supported model from the companion app.
```

---

# Policy Kernel Model

The base mod should contain a stable policy kernel.

The kernel owns:

* handwritten dispatcher events
* stable scripted effects
* stable scripted triggers
* allowlisted modifiers or bias hooks
* fallback behavior
* marker guard logic

The generated overlay should stay small.
Reactive policy-pack output is allowed, but it must remain a compact set of validated contingency branches rather than a generated script archive.

The generated overlay owns:

* campaign marker expectations
* empire policy selections
* entry-point-scoped reactive policy branches
* bounded intensity/cadence values
* compact generated triggers/effects when needed
* audit-friendly metadata

This is preferred over generating many unique event blocks.

The generated overlay is a complete replacement snapshot, not an append-only history.

---

# Season Delta Ledger

The normal analysis input should be a latest-session delta ledger, not raw full campaign history.

Pipeline:

```text
verified archived autosaves from latest session
-> deterministic extractor
-> season delta ledger
-> empire-scoped briefs
-> LLM interpretation
-> validated structured deltas
-> durable memory update
-> generated overlay snapshot
```

The ledger should contain factual, bounded, extracted observations.

Examples:

* wars started or ended
* borders changed
* alliances formed or broke
* subject relationships changed
* major losses or gains
* economic or military trend summaries
* unknown or low-confidence gaps

The LLM should interpret meaning and strategy from bounded briefs.

The LLM should not parse raw saves directly in the normal path.

Current v0 implementation note:

```text
Strategic Nexus.exe --build-season-delta-ledger <session_archive_dir> <campaign_id> <output_json>
```

This produces a metadata-only ledger from a verified autosave archive.
It is useful for proving pipeline shape, but it is not yet the real strategic extractor.

The companion skeleton can also emit a conservative empire brief:

```text
Strategic Nexus.exe --build-empire-brief-from-archive <session_archive_dir> <campaign_id> <empire_id> <output_json>
```

This brief declares missing parser information instead of inventing empire state.

---

# Automatic Staging Policy

The orchestrator may stage generated overlays automatically when all required safety conditions pass.

Required conditions:

* Stellaris is not running
* generated overlay validates successfully
* active campaign library plan is within limits
* marker guards exist for campaign-specific rules
* generated gameplay files match their manifest
* no known multiplayer package mismatch exists
* no low-confidence campaign merge would occur
* for multiplayer host rotation, latest handoff provenance is known or the reduced confidence is explicit

If confidence is high:

```text
stage automatically
record a Status Center entry
```

If confidence is low:

```text
do not apply risky campaign-specific behavior
use base fallback
show a Status Center warning
ask only if user action is genuinely needed
```

---

# Local Campaign Library Policy

The active generated mod library should include only campaigns that are currently loadable locally, unless the user explicitly pins a campaign.

Rules:

* local save missing -> remove that campaign ruleset from active generated overlay
* Strategic Nexus archive/memory remains on disk
* known save reappears -> regenerate the campaign ruleset from durable memory
* unknown save appears -> create a new campaign profile after safe identity checks
* ambiguous match -> do not merge aggressively

The active generated library is a rebuildable projection of durable memory.

It is not the source of truth.

---

# User Interaction Model

Mandatory interaction should be rare.

The user may need to act when:

* first-run setup asks whether to enable start-with-Windows
* a campaign identity match is ambiguous
* multiplayer generated overlay export/import is required
* save roots cannot be found
* filesystem permissions block archive or staging
* validation fails repeatedly
* the user wants to review or disable an automatic update

The Status Center should explain issues in ordinary product language.

It should not expose developer internals unless diagnostic mode is enabled.

---

# Safety And Legality Boundary

This architecture intentionally stays inside ordinary modding and local companion-app file handling.

Allowed:

* ordinary mod files
* generated mod overlay files staged while Stellaris is closed
* launcher-visible mod/playset configuration where supported
* read-only save copying
* local archive and memory files
* local analysis after play

Forbidden:

* game memory inspection
* executable patching
* save editing
* bypassing launcher or game security
* hidden gameplay automation through debug interfaces
* live LLM control of a running game

---

# Design Goal

Strategic Nexus should feel like:

```text
a quiet campaign memory and strategy assistant that prepares the next session
```

not:

```text
a tool the player must babysit before every launch
```
