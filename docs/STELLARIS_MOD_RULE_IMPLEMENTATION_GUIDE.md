# Stellaris Mod Rule Implementation Guide

## Purpose

This guide is the practical handoff for workers and foreground chats that need to implement Strategic Nexus rules inside a Stellaris mod.

Strategic Nexus should influence Stellaris through ordinary mod files only:

```text
SNC autosave/archive analysis
-> bounded local LLM proposal
-> validated Strategic Nexus DSL
-> deterministic generated overlay
-> verified install into a normal Stellaris mod directory
-> Stellaris loads the mod before play
-> ordinary on_actions/events/scripted_effects select precompiled branches during play
```

The local model output is never trusted as Stellaris script. It is input to a validator/compiler.

## Hard Boundary

Allowed:

* normal Stellaris mod folders and descriptors
* on_actions, events, scripted_effects, scripted_triggers, static_modifiers, flags, variables, and localization
* read-only save/autosave capture
* post-play analysis and next-launch mod refresh
* precompiled reactive policy branches selected by in-game script

Forbidden:

* patching or inspecting `stellaris.exe`
* process memory reads/writes
* binary injection or external DLL hooks
* hidden console command transport as a production path
* live LLM decisions entering an already running game
* raw LLM text copied into mod script
* client-local LLM decisions in multiplayer

If a future official Stellaris/Paradox runtime API exists, evaluate it separately. Until then, "near realtime" means precompiled reactive script, not live game-process control.

## Local Evidence Checked

Checked on 2026-06-08 against a local Stellaris 4.3-era install and the local Strategic Nexus PoC mod.

Primary local evidence:

* `<Stellaris install>/common/on_actions/99_README_ON_ACTIONS.txt`
* `<Stellaris install>/common/on_actions/00_on_actions.txt`
* `<Stellaris install>/events/focus_events_1.txt`
* `<Stellaris install>/common/personalities/00_personalities.txt`
* `<Paradox user data>/Stellaris/mod/strategic_nexus_poc`
* `src/generated_overlay/OverlayCompiler.cpp`
* `src/generated_overlay/DslValidator.cpp`
* `tests/generated_overlay_contract_test.cpp`

External references for orientation:

* [Stellaris Wiki: Modding](https://stellaris.paradoxwikis.com/Modding)
* [Stellaris Wiki: Modding tutorial](https://stellaris.paradoxwikis.com/Modding_tutorial)
* [Stellaris Wiki: Event modding](https://stellaris.paradoxwikis.com/Event_modding)
* [Stellaris Wiki: Scopes](https://stellaris.paradoxwikis.com/Scopes)
* [Stellaris Wiki: Effects](https://stellaris.paradoxwikis.com/Effects)
* [Stellaris Wiki: Conditions](https://stellaris.paradoxwikis.com/Conditions)

Local install files are the implementation source of truth. Wiki pages are references, not the acceptance gate.

## Stellaris Mod Location Contract

On Windows, Stellaris user mods are normally installed under:

```text
<Paradox user data>/Stellaris/mod/
```

A local mod normally has two descriptor surfaces:

```text
<Paradox user data>/Stellaris/mod/strategic_nexus_poc.mod
<Paradox user data>/Stellaris/mod/strategic_nexus_poc/descriptor.mod
```

The external `.mod` descriptor points the launcher to the actual mod folder. The local PoC uses an absolute `path="..."` descriptor and `supported_version="4.3.*"`.

Workers must not assume Steam is required. Distribution/provider detection belongs to `STELLARIS_DISTRIBUTION_AND_SAVE_ROOTS.md`. The mod implementation contract is provider-neutral once the Stellaris user data root is known.

## Current Strategic Nexus Mod Surfaces

The existing local PoC proves these surfaces:

```text
common/on_actions/strategic_nexus_poc_on_actions.txt
events/strategic_nexus_poc_events.txt
common/static_modifiers/strategic_nexus_poc_modifiers.txt
localisation/english/strategic_nexus_poc_l_english.yml
descriptor.mod
```

The PoC wires:

```text
on_game_start_country -> strategic_nexus.1201
on_monthly_pulse_country -> strategic_nexus.1202, strategic_nexus.1300, strategic_nexus.1302
```

It also proves that normal script can use:

```text
set_global_flag
set_country_flag
remove_country_flag
set_variable
check_variable
add_modifier
remove_modifier
log
country_event = { id = ... }
```

This is enough for a safe generated policy kernel.

## Vanilla Hook Evidence

`common/on_actions/99_README_ON_ACTIONS.txt` states that on_actions fire events automatically and that pulse on_actions exist, including country monthly pulses. It also states that custom on_actions can be defined and fired through `fire_on_action`.

Useful verified vanilla on_actions:

```text
on_game_start_country
on_monthly_pulse_country
on_yearly_pulse_country
on_war_beginning
on_war_ended
on_war_won
on_war_lost
on_country_attacked
```

Relevant scope notes from vanilla comments:

```text
on_yearly_pulse_country: this = country
on_war_ended: Root = loser, From = main winner
on_country_released_in_war: Root = new country, From = forcing country
```

Workers must verify scope for every newly mapped event family before enabling it. If scope is uncertain, keep the family disabled or route through a harmless diagnostic event first.

## Event Shape

Vanilla files use hidden triggered events like:

```text
country_event = {
    id = focus.5
    hide_window = yes
    is_triggered_only = yes

    trigger = {
        is_country_type = default
    }

    immediate = {
        # scripted work
    }
}
```

Strategic Nexus generated events should follow the same shape:

```text
namespace = strategic_nexus_generated

country_event = {
    id = strategic_nexus_generated.1
    hide_window = yes
    is_triggered_only = yes

    trigger = {
        is_country_type = default
        is_ai = yes
    }

    immediate = {
        strategic_nexus_generated_monthly_strategy_tick_dispatch = yes
    }
}
```

Use `is_ai = yes` for gameplay-affecting AI rules. This also enforces the multiplayer rule: if an empire is currently player-controlled, Strategic Nexus rules do not activate for that empire.

## Generated Overlay Layout

Generated overlay files must mirror the Stellaris mod tree:

```text
events/strategic_nexus_generated_events.txt
common/scripted_effects/strategic_nexus_generated_effects.txt
common/scripted_triggers/strategic_nexus_generated_triggers.txt
strategic_nexus_generated_manifest.json
```

Future base kernel files should be handwritten and stable:

```text
common/on_actions/strategic_nexus_policy_kernel_on_actions.txt
events/strategic_nexus_policy_kernel_events.txt
common/static_modifiers/strategic_nexus_policy_kernel_modifiers.txt
localisation/english/strategic_nexus_policy_kernel_l_english.yml
```

Do not use `replace_path` for generated overlay files unless the design explicitly intends to replace a whole vanilla directory. Prefer additive namespaced files.

## Critical Publish Rule

`dist/private_reports/snc_generated_overlay_active` is an internal verified snapshot. It is not automatically visible to Stellaris.

For gameplay, SNC must install the verified generated files into the actual mod folder before the next launch:

```text
verified active snapshot
-> manifest check
-> copy/sync only generated files into <mod_root>/
-> leave descriptor, localization, and handwritten base kernel intact
-> launch/next load uses the updated files
```

The generated install step must be complete replacement for generated files only, not for the whole mod root.

Publish must refuse to run while `stellaris.exe` is active. Stellaris script definitions are loaded by the game; do not rely on hot-swapping files during play.

## Current Compiler Contract

Current code emits:

```text
events/strategic_nexus_generated_events.txt
common/scripted_effects/strategic_nexus_generated_effects.txt
common/scripted_triggers/strategic_nexus_generated_triggers.txt
strategic_nexus_generated_manifest.json
```

Current rule guards:

```text
has_global_flag = strategic_nexus_campaign_<campaign_marker>
has_country_flag = strategic_nexus_empire_<empire_id>
has_global_flag = strategic_nexus_known_save_fingerprint_<value>
has_global_flag = strategic_nexus_known_save_date_<value>
has_global_flag = strategic_nexus_known_rule_scope_<value>
```

Current allowed DSL source quality:

```text
history_backed
zero_history_bootstrap
generic_unknown_campaign_fallback
```

Current allowed event families:

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

Current allowed preferences:

```text
military_posture = defensive | aggressive
research_bias = economy | military_industry
doctrine_inertia = low | medium | high
```

Current compiler gap:

* `monthly_strategy_tick` has a generated dispatch surface.
* Other event-family effects are generated when rules exist.
* The base kernel must not call an effect that may be absent.

The next robust implementation should either:

1. emit no-op dispatch effects for every base-kernel event family, or
2. keep the base kernel limited to always-present dispatchers.

Prefer option 1. A stable base kernel can safely call every allowlisted dispatcher only when the generated overlay always provides no-op fallbacks.

## Base Kernel Target

The handwritten base kernel should map Stellaris on_actions to Strategic Nexus event families.

Initial safe kernel:

```text
on_monthly_pulse_country = {
    events = {
        strategic_nexus_kernel.10
    }
}
```

Future event families after scope verification:

```text
on_war_beginning = { events = { strategic_nexus_kernel.20 } }
on_war_ended = { events = { strategic_nexus_kernel.21 } }
on_country_attacked = { events = { strategic_nexus_kernel.30 } }
```

Kernel events should be hidden, triggered-only, country-scoped where possible, and player-safe:

```text
namespace = strategic_nexus_kernel

country_event = {
    id = strategic_nexus_kernel.10
    hide_window = yes
    is_triggered_only = yes

    trigger = {
        is_country_type = default
        is_ai = yes
    }

    immediate = {
        strategic_nexus_generated_monthly_strategy_tick_dispatch = yes
    }
}
```

For war/event scopes where `root` may not be the desired country, first add diagnostic `log` events and inspect logs before applying gameplay-affecting branches.

## Marker And Identity Layer

Generated rules are intentionally inert until campaign/empire/entry-point markers match.

SNC or the base mod must set these markers before dispatch can apply:

```text
strategic_nexus_campaign_<campaign_marker>
strategic_nexus_empire_<empire_id>
strategic_nexus_known_save_fingerprint_<value>
strategic_nexus_known_save_date_<value>
strategic_nexus_known_rule_scope_<value>
```

Implementation requirement:

* Do not infer the active campaign from the latest captured autosave only.
* Build entry-point-scoped markers for every loadable save that the user may resume.
* Captured autosaves are historical evidence unless they are also still loadable entry points.
* Branch/reload histories must not poison the wrong entry-point rules.

Unknown campaign support:

* Generate `zero_history_bootstrap` or `generic_unknown_campaign_fallback` rules.
* Keep them conservative.
* Use deterministic bootstrap rotation seed/epoch.
* Never use username, machine id, account id, raw folder names, or private user profile details.

## Multiplayer Rule

Generate rules for every known empire in an MP campaign, but activate only for empires controlled by AI at runtime.

Practical v0 gate:

```text
trigger = {
    is_country_type = default
    is_ai = yes
}
```

This means:

* AI empires can receive Strategic Nexus strategic bias.
* A human-controlled empire is skipped.
* If a human later takes control of an AI empire, the rule should no longer fire while `is_ai = no`.

The host/coordinator remains the authority for generated MP packages. Clients must not run their own local LLM to produce divergent rules.

## Human-Control Guarded MP Rules

The 15F guard rule is a gameplay activation rule, not just a status flag.

Implementation shape:

```text
generated rule emitted for every known campaign empire
-> runtime guard checks `is_ai = yes`
-> AI-controlled empires may receive the bounded Strategic Nexus effect
-> human-controlled empires stay inert and fail closed
```

Working rules:

* use the current controller state from ordinary Stellaris script, not from client-local LLM output
* do not key activation on player names, Steam identity, or private profile data
* if controller state is uncertain, skip activation rather than guessing
* keep `human_control_guard_state` as owner-facing visibility only; it does not replace the runtime guard
* prefer one small diagnostic or bias effect first, then expand only after the guard proves stable in logs and saves

## Gameplay Effect Style

For v0, prefer tiny, bounded, inspectable effects:

```text
remove_country_flag = strategic_nexus_pref_military_posture_aggressive
set_country_flag = strategic_nexus_pref_military_posture_defensive
```

Use flags and variables as Strategic Nexus strategy state. Let stable handwritten script translate that state into small gameplay biases.

Avoid:

* direct fleet micro
* raw target selection
* arbitrary event chains
* high-frequency per-object loops
* destructive or irreversible effects

When visible in-game evidence is needed, use harmless diagnostic modifiers with localization, then remove or gate them before production.

## Validation And Acceptance

Before publish:

1. Parse DSL.
2. Validate allowlisted ministries, event families, source qualities, preferences, conditions, intensity, and confidence.
3. Reject unsupported runtime conditions before compile.
4. Compile generated files.
5. Verify generated manifest hashes.
6. Install only verified generated files into the mod root.
7. Refuse publish if Stellaris is running.

After game run:

1. Inspect `<Paradox user data>/Stellaris/logs/error.log`.
2. Inspect `<Paradox user data>/Stellaris/logs/game.log` or relevant logs for `Strategic Nexus` lines.
3. Confirm no missing scripted effect/trigger errors.
4. Confirm expected harmless marker or policy state.
5. Record evidence under `dist/private_reports/`.

## Worker-Ready Next Steps

Do these in order:

1. Add generated no-op dispatcher effects for all base-kernel event families.
2. Add a stable handwritten base kernel file that maps `on_monthly_pulse_country` to `monthly_strategy_tick`.
3. Add generated overlay install/sync from verified active snapshot into the actual mod root.
4. Add publish refusal while `stellaris.exe` is running.
5. Add log-check tooling for Strategic Nexus errors after a game run.
6. Add diagnostic owner test: monthly pulse sets one harmless visible marker for an AI country.
7. Only after monthly works, add `war_started`, `war_ended`, and `country_attacked` with scope-specific diagnostics.
8. Add the first human-control guarded gameplay branch with `is_ai = yes` and a fail-closed regression test.

Do not mark gameplay integration done until Stellaris has loaded the installed mod files and the logs/marker prove the branch executed.
