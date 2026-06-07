# Strategic Nexus - Reactive Policy Pack Architecture

## Purpose

This document defines the approved near-realtime architecture for Strategic Nexus.

The goal is to make Strategic Nexus visibly affect a long single Stellaris session without using unsafe or unsupported live game integration.

Strategic Nexus still must not inject live LLM decisions into an already-running game.

The approved path is:

```text
archived saves / durable memory / entry-point facts
-> bounded LLM strategy proposal
-> validated Strategic Nexus DSL
-> deterministic reactive policy pack compiler
-> generated overlay loaded by Stellaris before play
-> ordinary Stellaris on_actions/events/triggers select among precompiled policy branches during play
```

This is not realtime inference.

It is precompiled event-driven strategy.

---

# Core Decision

Strategic Nexus should move from a purely session-to-session overlay toward a precompiled reactive policy pack.

The current offline analysis loop remains the foundation.

The improvement is that the generated overlay should contain multiple safe strategic contingencies, not only one current-session recommendation.

During an active game session, the mod may react quickly by selecting one of those already-loaded branches through normal Stellaris script.

The companion app and LLM may observe, archive, analyze, and prepare the next pack, but they do not modify the running game.

---

# Legal And Safety Boundary

Allowed:

* ordinary Stellaris mod files
* generated overlay files loaded before the session
* on_actions, events, scripted triggers, scripted effects, flags, variables, and allowlisted modifiers
* read-only autosave capture
* reading ordinary game logs for diagnostics when available
* compiling validated DSL into deterministic mod script

Forbidden:

* loading external binary code into the running game process
* inspecting or changing the running game's address space
* patching `stellaris.exe`
* live hot-swapping already-loaded gameplay definitions as a production transport
* sending commands into the running game process
* treating raw LLM output as gameplay script
* per-tick or combat-frame LLM inference
* client-local LLM decisions in multiplayer

If a future official API or documented modding feature provides a safer runtime update path, it may be evaluated separately.
Until then, live update means only "the loaded mod reacts to in-game events", not "SNC injects new decisions into the active process".

---

# Runtime Model

## Companion App

SNC owns:

* save-root discovery
* live autosave capture
* archive verification
* post-session analysis
* local LLM model setup and execution
* bounded decision-input construction
* DSL validation and compiler invocation
* generated overlay staging and publishing while Stellaris is closed
* Status Center reporting

During `stellaris.exe` play, SNC may:

* preserve stable autosaves
* update status heartbeats
* read safe local diagnostic signals
* prepare non-active candidate analysis in private staging

During `stellaris.exe` play, SNC must not:

* publish new active generated gameplay files
* block the game on inference
* treat an LLM answer as immediately game-active
* modify active saves

## Base Mod Policy Kernel

The handwritten base mod should contain a stable reactive policy kernel.

The kernel owns:

* low-frequency dispatcher events
* event-family dispatch points
* marker guards
* safe fallback behavior
* allowlisted modifiers and bias hooks
* generated policy-pack loading surfaces

The base mod should be small and stable.

## Generated Reactive Policy Pack

The generated overlay owns the campaign-specific policy pack.

It may contain:

* entry-point-scoped campaign guards
* empire guards
* strategy state variables
* event-family policy branches
* monthly reconciliation branches
* bounded intensity and duration values
* audit metadata in the generated manifest

It must remain:

* deterministic
* manifest-bound
* complete-replacement
* validated before publish
* small enough to keep mod load and monthly/event overhead acceptable

---

# Unknown Campaign Bootstrap Policy Pack

The generated overlay should include a conservative zero-history bootstrap policy path for campaigns that do not yet have durable Strategic Nexus history.

Unknown campaign must mean:

```text
campaign facts known or partially known
+ zero history
-> bounded bootstrap personality
-> conservative reactive policy branches
```

not:

```text
no rules
```

and not:

```text
the same default archetype table forever
```

For each generated policy-pack update, SNC should record a bootstrap rotation seed or epoch in the manifest.
That seed may vary the default empire personality mix for new or unknown campaigns while staying deterministic and reproducible for the generated pack.

The bootstrap policy pack may use:

* empire facts such as ethics, civics, authority, species traits, origin, and empire type
* campaign marker or candidate fingerprint when available
* generated policy-pack id or bootstrap rotation epoch
* empire ordinal only as a tie-breaker

It must not use:

* local username
* machine id
* account id
* raw private folder names
* personal profile data about the human player

Known campaigns with validated history must not be rerolled by this mechanism.
Their policy branches should use the established campaign-empire personality and gradual validated drift.

The Status Center should expose whether a ruleset is:

```text
history_backed
zero_history_bootstrap
generic_unknown_campaign_fallback
```

so the owner can tell whether Strategic Nexus is using learned history or safe defaults.

---

# Event Families

The compiler should expose allowlisted Strategic Nexus event families rather than raw Stellaris script names from the LLM.

Initial useful event families:

```text
session_start
monthly_strategy_tick
war_started
war_ended
country_attacked
fleet_destroyed
planet_conquered
first_contact_started
first_contact_finished
economy_deficit_detected
crisis_pressure_detected
```

The compiler maps those families to verified Stellaris on_actions, scripted triggers, or low-frequency dispatcher events.

The LLM may propose:

```text
when event_family = war_started
prefer military_posture defensive intensity 0.8
```

The LLM must not propose:

```text
raw_on_action = on_war_beginning
raw_effect = arbitrary Stellaris script
```

Raw script selection remains compiler-owned.

---

# Policy Branch Shape

A reactive policy branch should answer:

* which campaign and entry point is this safe for?
* which empire is it for?
* which event family can activate it?
* which validated conditions must be true?
* which bounded strategy dimensions change?
* how strong and how long is the change?
* what fallback applies when guards do not match?

Illustrative DSL direction:

```text
campaign "campaign_id" {
  entry_point "save_fingerprint" {
    empire "empire_id" {
      policy "war_defense_01" {
        event = war_started
        when personality.paranoia >= medium
        prefer military_posture defensive intensity 0.8
        prefer doctrine_inertia high
        duration = 24_months
        confidence = 0.74
        rationale = "Repeated border pressure favors defensive wartime posture."
      }
    }
  }
}
```

This is illustrative.
Exact syntax belongs to `META_RULE_LANGUAGE_AND_COMPILER.md`.

---

# Update Cadence Levels

Strategic Nexus should distinguish four levels:

```text
Level 0 - post-session only:
  Analyze after play; next launch gets one updated policy state.

Level 1 - precompiled reactive pack:
  Next launch gets multiple validated branches; the active session reacts through ordinary mod script.

Level 2 - observer-assisted next-pack preparation:
  SNC observes autosaves/logs during play and prepares the next pack, but activation waits for a safe publish boundary.

Level 3 - explicit checkpoint/reload loop:
  The user can deliberately save, exit/reload, or restart a session to apply a freshly generated pack.
```

Approved v0 direction:

```text
Level 1 first, Level 2 as companion maturity grows.
```

Do not pursue a hidden Level 4 that injects new decisions into the active game.

---

# Multiplayer Rule

Multiplayer remains host-authoritative.

The host prepares or exports the reactive policy pack.

All clients must receive byte-identical gameplay-affecting generated files when multiplayer checksum requires it.

Clients must not run their own local LLM to generate different policy packs for the same session.

If policy-pack identity, manifest hash, campaign marker, or package provenance is ambiguous, multiplayer mode must fall back or require explicit host action.

---

# V0 Implementation Direction

The next v0 implementation work should prove the smallest useful reactive loop:

```text
validated DSL with event-family branch
-> compiler emits generated trigger/effect branch
-> base on_action dispatcher calls generated branch
-> local test verifies branch selection text/manifest
-> manual Stellaris acceptance verifies visible scripted effect or log marker
```

Preferred first branches:

1. `monthly_strategy_tick` because the current PoC already uses monthly country pulses.
2. `war_started` or `country_attacked` because they prove event-driven reaction beyond passive monthly cadence.

Do not add broad strategy complexity before this small reactive contract is verified.

Current verified base dispatcher state:

* `monthly_strategy_tick` is wired through the handwritten monthly pulse path.
* `war_started` and `country_attacked` are routed through low-frequency proxy events in `mod/strategic_nexus_poc/common/on_actions/strategic_nexus_poc_on_actions.txt`.
* `tools/run_v0_pipeline_tests.ps1` already asserts both the generated reactive dispatcher text and the base kernel `on_actions` wiring for those allowlisted families.

---

# Acceptance Criteria

The reactive policy-pack architecture is considered implemented enough for v0 when:

* DSL can represent an allowlisted event family.
* Compiler rejects unknown event families and raw script names.
* Generated overlay manifest records policy-pack/event-family coverage.
* Generated script remains small and deterministic.
* A local regression test verifies accepted and rejected event-family branches.
* A manual Stellaris test or scripted mod-side smoke test verifies at least one in-session branch activation.
* Status Center can explain whether the active generated overlay is post-session-only or reactive-policy-pack capable.

Until those conditions pass, Strategic Nexus should describe the current build as session-to-session with reactive-policy-pack work in progress.
