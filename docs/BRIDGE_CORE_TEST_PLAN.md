# Bridge Core Test Plan

## Scope

These tests cover only the standalone bridge core.

They do not interact with the running Stellaris process.
They do not automate the console.
They do not use named pipes into the game.
They do not integrate an LLM.

## Build Harness

Example MSVC command from the repository root:

```powershell
cl /std:c++20 /EHsc /I src/bridge_core src/bridge_core/*.cpp /Fe:bridge_core_test.exe
```

If another C++20 compiler is used, compile all `.cpp` files in `src/bridge_core`.

## Harness Usage

```powershell
.\bridge_core_test.exe resources\decision_valid_dimensions.json 20000 0
```

Arguments:

- decision file path
- optional `now_unix_ms`
- optional `last_sequence_id`

The sample files use small deterministic timestamps so tests can pass a fixed `now_unix_ms`.

## Test Cases

### Valid Modular Payload

Input:

```text
resources/decision_valid_dimensions.json
```

Expected:

- `valid=true`
- `valid_dimensions=9`
- `invalid_dimensions=0`
- `confidence=0.68`
- `action=ApplyStrategyDimensions`
- `script_event=strategic_nexus.1400`
- mapped script flags are printed for currently supported dimensions
- individual dimensions are printed

### Valid Receiver Dimensions Payload

Input:

```text
resources/decision_valid_receiver_dimensions.json
```

Expected:

- `valid=true`
- `valid_dimensions=2`
- `mapped_dimensions=2`
- `script_event=strategic_nexus.1400`
- `script_global_flag=strategic_nexus_military_posture_defensive`
- `script_global_flag=strategic_nexus_research_bias_economy`
- `effect_batch_valid=true`
- `effect_batch_line=effect set_global_flag = strategic_nexus_bridge_available`
- `effect_batch_line=effect set_global_flag = strategic_nexus_military_posture_defensive`
- `effect_batch_line=effect set_global_flag = strategic_nexus_research_bias_economy`
- `effect_batch_line=effect set_country_flag = strategic_nexus_bridge_available`
- `effect_batch_line=effect set_country_flag = strategic_nexus_military_posture_defensive`
- `effect_batch_line=effect set_country_flag = strategic_nexus_research_bias_economy`
- `effect_batch_line=event strategic_nexus.1400`
- equivalent country flags are printed

### Partial Modular Payload

Input:

```text
resources/decision_partial_dimensions.json
```

Expected:

- `valid=true`
- `valid_dimensions=2`
- `action=ApplyStrategyDimensions`
- absent domains are not printed

### Invalid Domain Value

Input:

```text
resources/decision_invalid_dimension_value.json
```

Expected:

- `valid=true`
- invalid domain is ignored
- valid domain is preserved
- `valid_dimensions=1`
- `invalid_dimensions=1`
- `action=ApplyStrategyDimensions`

### No Valid Dimensions

Input:

```text
resources/decision_no_valid_dimensions.json
```

Expected:

- `valid=false`
- `reason=no valid strategy dimensions`
- `action=UseFallback`

### Stale Payload

Input:

```text
resources/decision_stale.json
```

Expected:

- `valid=false`
- `reason=stale payload`
- `action=UseFallback`

### Missing File

Input:

```text
resources/missing_decision.json
```

Expected:

- `valid=false`
- `reason=missing file`
- `action=UseFallback`

### Malformed JSON

Input:

```text
resources/decision_malformed.json
```

Expected:

- `valid=false`
- `action=UseFallback`

### Structurally Malformed Complete JSON

Input:

```text
resources/decision_malformed_complete.json
```

Expected:

- `valid=false`
- `reason=malformed file`
- `action=UseFallback`

### Missing Schema Version

Input:

```text
resources/decision_missing_schema_version.json
```

Expected:

- `valid=false`
- `reason=missing schema_version`
- `action=UseFallback`

### Missing Identity Fields

Input:

```text
resources/decision_missing_identity.json
```

Expected:

- `valid=false`
- `reason=missing campaign_id`
- `action=UseFallback`

### Invalid Oversized Number

Input:

```text
resources/decision_invalid_number.json
```

Expected:

- `valid=false`
- `action=UseFallback`
- parsing fails closed without crashing

### Confidence Out Of Range Clamps

Input:

```text
resources/decision_confidence_out_of_range.json
```

Expected:

- `valid=true`
- `confidence=1`
- `action=ApplyStrategyDimensions`

### Fallback Required

Input:

```text
resources/decision_fallback_required.json
```

Expected:

- `valid=false`
- `reason=fallback required by payload`
- `action=UseFallback`

### Invalid Effect Batch Directory Target

Input:

```text
resources/decision_fallback_required.json
```

Expected:

- `valid=false`
- `effect_batch_written=false`
- output directory target is preserved

### Valid Effect Batch Directory Target

Input:

```text
resources/decision_valid_receiver_dimensions.json
```

Expected:

- `valid=true`
- `effect_batch_written=false`
- output directory target is preserved

### Replayed Sequence

Input:

```text
resources/decision_valid_dimensions.json
```

Command:

```powershell
.\bridge_core_test.exe resources\decision_valid_dimensions.json 20000 1
```

Expected:

- `valid=false`
- `reason=replayed or old sequence_id`
- `action=UseFallback`

### Legacy Doctrine Id Rejected

Input:

```text
resources/decision_legacy_doctrine_id_rejected.json
```

Expected:

- `valid=false`
- `reason=forbidden executable, raw text, or legacy doctrine field`
- `action=UseFallback`

### Forbidden Command Field

Input:

```text
resources/decision_forbidden_command.json
```

Expected:

- `valid=false`
- `reason=forbidden executable, raw text, or legacy doctrine field`
- `action=UseFallback`

## Pass Criteria

- valid modular payload maps only to `ApplyStrategyDimensions`
- valid modular payload maps to a predefined script intent, not command text
- partial payloads are accepted
- invalid domain values are ignored per-domain
- payloads with zero valid dimensions fallback
- stale payload falls back
- malformed JSON falls back
- missing `schema_version` falls back
- missing identity fields fall back
- invalid oversized numeric fields fall back without crashing
- missing file falls back
- confidence is clamped
- replayed sequence is rejected
- `doctrine_id` is rejected
- command/script/raw-text fields are rejected
- no lower-level game-process modification, debug-interface automation, custom networking, or LLM integration exists in this bridge core

## Script Intent Mapper Scope

The first mapper intentionally supports only a tiny mod-side receiver surface:

- `military_posture = defensive`
- `military_posture = aggressive`
- `research_bias = economy`
- `research_bias = military_industry`

The mapper emits only predefined flag identifiers and the predefined receiver event `strategic_nexus.1400`.

It does not emit debug-interface command strings.
It does not emit script text.
It does not execute anything in Stellaris.
It only prepares an auditable intent for a future host-only bridge adapter.

## Effect Batch Emitter Scope

The bridge core can also emit a predefined effect batch from a validated `ScriptIntent`.

This batch is not daemon-provided command text.
It is generated only from allowlisted flags and a predefined receiver event.

Allowed output shape:

```text
effect set_global_flag = strategic_nexus_bridge_available
effect set_global_flag = strategic_nexus_military_posture_defensive
effect set_global_flag = strategic_nexus_research_bias_economy
effect set_country_flag = strategic_nexus_bridge_available
effect set_country_flag = strategic_nexus_military_posture_defensive
effect set_country_flag = strategic_nexus_research_bias_economy
event strategic_nexus.1400
```

The emitter does not:

- read daemon command strings
- accept script text fields
- call Stellaris
- interact with the running Stellaris process
- automate the console

It exists to define a safe allowlisted artifact for local validation and scripted receiver testing.

## Effect Batch File Writer

The bridge core may write a valid effect batch to disk for lab transport tests or future adapter handoff.

Rules:

- write only allowlisted generated batch lines
- write atomically through a temp file
- create parent directories when needed
- clear stale output when the batch is invalid
- never write daemon-provided command text
- never write raw LLM text

Expected valid output path in smoke tests:

```text
dist/test_effect_batch_valid.txt
```

Invalid payload test:

```text
dist/test_effect_batch_stale.txt
```

The invalid payload test must delete the stale file.

## Pipeline Smoke Test

The bridge core has a one-shot pipeline mode:

```powershell
dist/bridge_core_test.exe --pipeline resources/decision_valid_receiver_dimensions.json dist/pipeline_effect_batch.txt dist/pipeline_sequence_state.txt 20000
```

Expected valid run:

- `pipeline_accepted=true`
- `pipeline_reason=accepted`
- `pipeline_sequence_id=20`
- `pipeline_previous_sequence_id=0`
- `pipeline_current_sequence_id=20`
- `pipeline_effect_batch_written=true`
- `pipeline_sequence_advanced=true`

Expected effect batch write failure:

- `pipeline_accepted=false`
- `pipeline_reason=failed to write output atomically`
- `pipeline_effect_batch_written=false`
- `pipeline_sequence_advanced=false`
- sequence state is not advanced
- effect batch directory target is preserved

Expected replay run with the same payload:

- `pipeline_accepted=false`
- `pipeline_reason=replayed or old sequence_id`
- `pipeline_previous_sequence_id=20`
- `pipeline_current_sequence_id=20`
- `pipeline_effect_batch_written=false`
- `pipeline_sequence_advanced=false`
- stale effect batch file is cleared

Expected malformed payload read:

- `pipeline_accepted=false`
- `pipeline_reason=malformed file`
- `pipeline_effect_batch_written=false`
- `pipeline_sequence_advanced=false`
- stale effect batch file is cleared
- sequence state is not advanced

Expected sequence write failure:

- `pipeline_accepted=false`
- `pipeline_reason=failed to replace sequence state`
- `pipeline_effect_batch_written=false`
- `pipeline_sequence_advanced=false`
- effect batch output is cleared
- sequence parent file target is preserved

This proves the bridge core can act as a safe one-shot daemon-to-transport step before any native runtime adapter exists.

## Main Application Pipeline Check

The Visual Studio project builds bridge core library files into the main application.

Manual smoke command:

```powershell
x64/Debug/Strategic Nexus.exe --bridge-pipeline resources/decision_valid_receiver_dimensions.json dist/app_pipeline_effect_batch.txt dist/app_pipeline_sequence_state.txt 20000
```

Expected first run:

- `pipeline_accepted=true`
- `pipeline_reason=accepted`
- `pipeline_sequence_advanced=true`

Expected second run with the same sequence state:

- `pipeline_accepted=false`
- `pipeline_reason=replayed or old sequence_id`
- `pipeline_sequence_advanced=false`

Do not add `src/bridge_core/main.cpp` to the main Visual Studio project because the project already has `Strategic Nexus.cpp` as its application entry point.

## Local Verification

Verified locally on 2026-05-20 with Visual Studio C++ toolchain.

Passed cases:

- full modular payload -> `ApplyStrategyDimensions`
- partial modular payload -> `ApplyStrategyDimensions`
- invalid single domain ignored while valid domain is preserved
- zero valid dimensions -> `UseFallback`
- stale payload -> `UseFallback`
- fallback required -> `UseFallback`
- malformed payload -> `UseFallback`
- missing `schema_version` -> `UseFallback`
- missing file -> `UseFallback`
- confidence `1.74` clamped to `1`
- replayed `sequence_id` rejected
- legacy `doctrine_id` rejected
- forbidden `command` field rejected

Schema version field:

- bridge core now requires `schema_version`
- legacy `version` without `schema_version` is rejected
