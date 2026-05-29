# Architecture Overview

## Core Principle

Strategic Nexus must not do anything illegal or against OpenAI rules.

If a proposed architecture path could violate law, third-party rights, platform terms, OpenAI policy, or safe-use boundaries, it is out of scope.

The external AI layer NEVER directly controls the game.

It only:
- analyzes
- interprets
- recommends
- modifies strategic priorities

The safe production architecture has four layers:

1. Vanilla Stellaris mod
2. Companion app
3. Offline campaign analysis pipeline
4. Generated campaign-specific mod overlay

The vanilla mod owns all in-game behavior.
Strategic Nexus Companion (SNC), the release companion app, observes the play session lifecycle and archives autosaves with read-only access.
The offline analysis pipeline reasons after play and prepares bounded campaign-specific state.
The generated mod overlay influences the next play session through normal mod scripting.
The LLM proposes bounded Strategic Nexus DSL rules, not raw Stellaris script.
A deterministic compiler validates and translates those rules into the generated overlay.

Legacy daemon and file-bridge workflows remain useful as local development harnesses and validators.
They are not the production assumption for sending new LLM decisions into an already-running game.

The companion app must not aggressively poll the game or inspect game state every frame.
It may detect a Stellaris session and copy stable autosave files into an archive.
It waits until the game exits before running heavyweight analysis or refreshing generated mod files.
The mod must not be responsible for launching the daemon process.
The design must not assume the vanilla mod can read arbitrary external files.

The companion app has its own lifecycle as an independent background application:

```text
Windows/user starts Strategic Nexus Companion (SNC)
    -> app idles in tray
    -> app detects Stellaris session with Strategic Nexus enabled
    -> app archives stable autosaves using read-only access
    -> app waits for Stellaris to exit
    -> app analyzes archived saves locally
    -> app stages generated mod overlay for the next launch
    -> app returns to idle
```

This avoids launch permissions, antivirus friction, sandbox assumptions, and mod API hacks.

Correct flow:

```text
Vanilla Stellaris Mod
    -> runs scripted fallback behavior by default
    -> uses generated campaign-specific overlay values when present

Companion App
    -> detects safe session lifecycle signals
    -> archives autosaves without modifying active saves
    -> closes to tray and avoids gameplay interference

Offline Campaign Analysis
    -> analyzes archived save snapshots after play
    -> interprets the same history from each empire perspective
    -> updates campaign-scoped memory and personality state

Generated Mod Overlay
    -> maps only bounded validated values to scripted mod data
    -> is compiled from validated Strategic Nexus DSL
    -> updates only between sessions
    -> fails closed to vanilla scripted fallback
```

Incorrect flow:

```text
offline worker continuously scans runtime game state
offline worker blocks gameplay while waiting for inference
offline worker modifies saves or running game internals directly
offline analysis replaces vanilla AI
generated overlay exposes raw AI output or tactical commands
mod depends on external file reads
companion app edits active autosaves
companion app modifies active mod files while Stellaris is running
local LLM output enters the current play session directly
```

The game remains the master.
The companion app and offline analysis remain advisory.
The mod never waits synchronously for daemon output.
If analysis or generated overlay refresh fails, the mod continues with fallback scripted behavior.

No AI replacement.
No save editing.
No direct tactical control.
No fleet control.
No arbitrary command strings from local analysis.
No raw LLM text enters the game.

Ironman compatibility is not required for v0.x.
Do not optimize v0.x for achievements.
Non-Ironman runtime integration is acceptable when bounded and safe.

---

## Major Systems

### 1. Session And Autosave Observer
Responsible for:
- detecting Stellaris play sessions
- detecting safe local signs that Strategic Nexus is enabled
- archiving autosaves with read-only access
- waiting for files to become stable before copying
- closing to tray without affecting gameplay

### 2. Galaxy State Extraction
Responsible for:
- parsing archived save data
- extracting strategic information
- generating compact summaries

### 3. Historical Memory System
Responsible for:
- storing important historical events
- reputation persistence
- geopolitical history
- civilization memory

### 4. Personality System
Responsible for:
- stable personality traits
- behavior consistency
- strategic interpretation filters

### 5. Doctrine Planner
Responsible for:
- selecting strategic doctrines
- long-term strategic priorities
- geopolitical goals

### 6. LLM Client
Responsible for:
- local inference communication
- prompt management
- response validation
- local model readiness through the companion Model Manager
- refusing new LLM interpretation when no supported model is installed

LLM model weights are not distributed with the Strategic Nexus Stellaris mod.
The companion app owns model selection, installation assistance, local runtime wiring, and readiness reporting.
The user must choose a supported model, and the companion app may recommend one based on hardware capability and license compatibility.

The model catalog must not treat unclear, gated, or incompatible model licenses as safe defaults.
If no supported model is installed, Strategic Nexus runs in reduced mode: archive and deterministic verification can still operate, but new LLM interpretation is disabled and Status Center must report the missing model.

Canonical local model/runtime/companion/mod boundary:
[LOCAL_LLM_INTEGRATION_CONTRACT.md](LOCAL_LLM_INTEGRATION_CONTRACT.md)

The local LLM ecosystem is layered:
- model weights: external artifacts chosen by the user, never bundled with the mod
- model runtime: replaceable local execution backend
- companion app: model setup, prompt construction, offline inference, validation, cache invalidation, and Status Center reporting
- Stellaris mod: stable policy kernel, validated generated overlay, and fallback behavior

The LLM client does not create trusted gameplay state by itself.
Model output is always untrusted until it passes schema validation, allowlists, DSL validation, deterministic compilation, manifest verification, and any relevant multiplayer package verification.
The generated mod integration layer must consume only validator-accepted artifacts, never raw model output.

### 7. Generated Mod Integration Layer
Responsible for:
- applying strategic weights safely
- communicating with Stellaris scripting systems
- graceful fallback behavior
- treating generated overlay values as optional hints, not authoritative commands
- refreshing generated files only while Stellaris is not running

### 8. Offline Strategic Worker
Responsible for:
- waiting idle until a play session ends or work is requested explicitly
- reading archived save metadata
- running strategic analysis outside active gameplay
- writing generated overlay data atomically enough for the next launch
- avoiding realtime polling loops

### 9. Offline Validator / Development Harness
Responsible for:
- reading response data in local development workflows
- validating a tiny set of bounded strategic values
- exposing only allowlisted scripted artifacts or receiver state
- hiding analysis/file failures from the scripted mod layer
- falling back to scripted defaults when unavailable

In production, this layer is primarily a validator and generated-overlay preparation boundary.
It must not be treated as proof that live LLM decisions can enter the running game.

The validator and generated overlay must not expose:
- raw prompts
- raw LLM output
- save data
- tactical orders
- fleet control
- economic micro-management

Initial bounded value contract:

```json
{
  "bridge": {
    "available": true,
    "doctrine": "balance_against_hegemon",
    "strategic_pressure": 0.74,
    "fallback_required": false
  }
}
```

Production work runs after the play session or through explicit local development commands.
It is not a live daemon-to-game transport.

---

## Fail-Safe Architecture

If the companion app or offline analysis:
- crashes
- freezes
- times out
- produces invalid output

Then:
- the game continues normally
- vanilla AI continues functioning
- no save corruption occurs

---

## Initial MVP Scope

Version 0.1:
- autosave archiving proof
- archived save parsing
- strategic summary generation
- campaign identity fingerprinting
- empire-scoped memory and personality summaries
- bounded generated overlay output
- safe next-session mod refresh

No realtime control.
No tactical overrides.
No direct fleet management.
No synchronous gameplay blocking.
No active save modification.
No active mod update while Stellaris is running.

The first MVP may run as:

```text
1. Stellaris creates autosaves during play
2. companion app copies stable autosaves into an archive
3. player exits Stellaris
4. offline analysis extracts campaign and empire summaries
5. validator emits bounded generated overlay state
6. companion app refreshes generated mod files for the next launch
7. mod uses generated values if valid, otherwise scripted fallback
```

Current request-file MVP:

```text
Strategic Nexus.exe --request resources/strategic_request.example.json
```

This is a local development harness, not the production gameplay delivery model.

Current sleepy daemon MVP:

```text
Strategic Nexus.exe --daemon exchange
```

This is also a local development harness.
The production direction is offline archived-save analysis and next-session overlay refresh.

Expected exchange directory:

```text
exchange/
    request.json
    response.json
    daemon.lock
    daemon.log
```

This exchange directory is not a vanilla mod API contract.
It is for the external daemon, optional bridge, and local debugging.

`daemon.lock` is a runtime heartbeat marker, not a permanent startup gate.
If the daemon crashes or the machine loses power, a leftover lock file must not block the next daemon start.
On startup, the daemon may overwrite a previous lock marker.
On clean shutdown, it removes its lock marker.

Responses are written through a temporary file and then published to `response.json`.
This prevents the mod from reading a half-written response during normal operation.

Responses include:
- `status`
- `request.request_id`
- `request.trigger`
- `decision` on success
- `bridge` bounded-value payload on success
- `error.message` on failure
