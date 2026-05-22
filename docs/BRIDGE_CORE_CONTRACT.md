# Bridge Core Contract

## Purpose

The bridge core validates daemon output before any safe scripted receiver or development harness consumes Strategic Nexus values.

The main payload model is modular strategy dimensions, not a monolithic `doctrine_id`.

This contract exists to let the LLM express nuanced strategy while keeping the bridge safe, bounded, inspectable, and deterministic enough for multiplayer.

## Payload

```json
{
  "schema_version": 1,
  "sequence_id": 1,
  "created_unix_ms": 0,
  "ttl_ms": 30000,
  "bridge_available": true,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "strategy_dimensions": {
    "military_posture": "defensive",
    "fleet_philosophy": "carrier",
    "border_strategy": "fortress_border",
    "expansion_policy": "cautious",
    "diplomatic_behavior": "paranoid",
    "research_bias": "strike_craft",
    "economic_focus": "alloy_focus",
    "crisis_policy": "prepare",
    "espionage_policy": "gather_military_intel"
  },
  "confidence": 0.68,
  "fallback_required": false
}
```

## Allowed Fields

- `schema_version`: integer contract version
- `sequence_id`: monotonic integer used to reject replay/stale reuse
- `created_unix_ms`: creation timestamp in Unix milliseconds
- `ttl_ms`: payload time-to-live in milliseconds
- `bridge_available`: bool
- `campaign_id`: stable campaign identity string
- `empire_id`: target empire identity string
- `strategy_dimensions`: object with bounded enum values
- `confidence`: numeric value clamped to `0.0 - 1.0`
- `fallback_required`: bool

## Forbidden Fields

Forbidden:

- `doctrine_id`
- raw LLM text
- raw action text
- script text
- command strings
- debug-interface command strings
- fleet targets
- build orders
- save fragments
- tactical targets

The bridge core must not accept fields such as:

- `doctrine_id`
- `command`
- `command_string`
- `script`
- `script_text`
- `console_command`
- `raw_text`
- `llm_text`
- `fleet_target`
- `build_order`
- `save_fragment`

## Strategy Dimensions

Each domain has a fixed vocabulary.

`military_posture`:

- `passive`
- `defensive`
- `balanced`
- `aggressive`
- `existential`

`fleet_philosophy`:

- `balanced`
- `artillery`
- `carrier`
- `missile`
- `torpedo`
- `picket`
- `swarm`
- `crisis_defense`

`border_strategy`:

- `open_border`
- `mobile_response`
- `fortress_border`
- `chokepoint_hardening`
- `deep_space_projection`

`expansion_policy`:

- `isolationist`
- `cautious`
- `opportunistic`
- `expansionist`
- `conquest_driven`

`diplomatic_behavior`:

- `cooperative`
- `cautious`
- `manipulative`
- `paranoid`
- `hegemon_seeking`
- `vassalizing`
- `honor_bound`
- `treacherous`

`research_bias`:

- `balanced`
- `economy`
- `military_industry`
- `shields`
- `armor`
- `missiles`
- `strike_craft`
- `point_defense`
- `sensors_intel`
- `megastructure`
- `crisis_preparation`

`economic_focus`:

- `balanced`
- `recovery`
- `alloy_focus`
- `research_focus`
- `unity_focus`
- `naval_capacity`
- `megastructure_focus`
- `fortress_economy`

`crisis_policy`:

- `ignore`
- `observe`
- `prepare`
- `contain`
- `total_mobilization`
- `survival_at_all_costs`

`espionage_policy`:

- `ignore`
- `defensive_counterintel`
- `target_rivals`
- `target_hegemon`
- `gather_military_intel`
- `destabilize_threats`

## Output Action Enum

```cpp
enum class PredefinedGameAction
{
    UseFallback = 0,
    ApplyStrategyDimensions
};
```

Any consumer of `ApplyStrategyDimensions` must map it to predefined scripted events/effects only.

It must not execute arbitrary command text.

## Validation Rules

- unknown `schema_version` -> fallback
- stale payload -> fallback for now
- missing file -> fallback
- invalid JSON -> fallback
- missing `campaign_id` or `empire_id` -> fallback
- missing `strategy_dimensions` -> fallback
- no valid strategy dimensions -> fallback
- invalid domain value -> ignore that domain
- invalid entire payload -> fallback
- `confidence` clamped to `0.0 - 1.0`
- `fallback_required = true` -> fallback
- `sequence_id <= last accepted sequence_id` -> fallback
- forbidden raw/action/command/legacy fields -> fallback
- no raw text action fields
- no command string fields
- no monolithic `doctrine_id`

Fallback output must map to:

```cpp
PredefinedGameAction::UseFallback
```

## Partial Update Rule

A payload may update only some domains.

Domains not present should keep previous valid values in the higher-level state manager.

The bridge core only validates the current payload. It does not own long-term doctrine inertia, campaign memory, or previous accepted state yet.

## MP Rule

Bridge execution is host-only.

Clients must not run the bridge.
Clients must not generate strategic decisions.
Clients only receive synchronized game state from Stellaris.

## Current Implementation

Bridge core skeleton lives in:

```text
src/bridge_core/
```

It does not interact with the running Stellaris process.
It does not modify the game executable or process state.
It only parses, validates, clamps, and maps bounded modular strategy dimensions to a predefined action enum.

## Current Pipeline Boundary

The standalone bridge core now supports a one-shot pipeline:

```text
decision.json
    -> DecisionFileReader
    -> BridgeValidator
    -> ScriptIntentMapper
    -> EffectBatchEmitter
    -> EffectBatchFileWriter
    -> SequenceStateStore
```

The pipeline may write:

- an allowlisted effect batch file
- a local last-accepted `sequence_id` state file

It still does not interact with the running Stellaris process.
It still does not execute the generated batch.

Replay rule:

- valid accepted payload advances sequence state
- repeated or older sequence id is rejected
- invalid/replayed payload clears stale effect batch output

CLI harness:

```powershell
dist/bridge_core_test.exe --pipeline <decision.json> <effect_batch_output> <sequence_state> <now_unix_ms>
```

Main application CLI:

```powershell
x64/Debug/Strategic Nexus.exe --bridge-pipeline <decision.json> <effect_batch_output> <sequence_state> <now_unix_ms>
```

Visual Studio project integration:

- bridge core library sources are compiled into `Strategic Nexus.vcxproj`
- `src/bridge_core/main.cpp` remains excluded because it is the standalone test harness entry point
- the main application exposes bridge pipeline behavior through `--bridge-pipeline`
