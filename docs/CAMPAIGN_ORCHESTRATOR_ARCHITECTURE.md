# Strategic Nexus - Campaign Orchestrator Architecture

## Purpose

This document defines the target release architecture for minimizing mandatory user interaction.

The goal is that a normal user should mostly just play Stellaris.

Strategic Nexus should quietly preserve campaign history, analyze the latest play session, and prepare the next-session generated overlay without requiring routine manual decisions.

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

It must not:

* inspect Stellaris memory
* patch the game
* edit active saves
* update active mod files while Stellaris is running
* require live LLM decisions during gameplay
* require routine user choices before every session

---

# Target Release Loop

```text
Windows starts
-> Strategic Nexus companion app may start if the user enabled it
-> app watches local save/playset signals at low frequency
-> before Stellaris launch, app stages the best known safe generated overlay
-> player plays normally
-> app archives stable autosaves with read-only access
-> player exits Stellaris
-> app verifies the archived session
-> deterministic extractor builds a latest-session delta ledger
-> local LLM interprets bounded empire-specific briefs
-> validator accepts only structured memory/personality/rule deltas
-> compiler emits a complete replacement generated overlay snapshot
-> next launch uses the updated overlay
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
* copyable multiplayer package/invite status text

It should not ask the user to manage routine development tasks.

For multiplayer season design, see `MULTIPLAYER_SEASON_ORCHESTRATOR.md`.

---

# Release Companion Lifecycle

The release companion app should be a persistent background assistant, not a tool the user must remember before every Stellaris session.

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
* support report approval is needed

It should not behave like the development Task Board.

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

The generated overlay owns:

* campaign marker expectations
* empire policy selections
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
