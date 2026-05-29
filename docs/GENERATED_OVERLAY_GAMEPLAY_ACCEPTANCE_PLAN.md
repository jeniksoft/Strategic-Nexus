# Generated Overlay Gameplay Acceptance Plan (v0)

## Purpose

Close the current verification gap: generated overlay files are deterministic and fail-closed, but gameplay effect is not yet verified in a Stellaris-like execution surface.

## Scope

This v0 plan verifies only bounded strategic behavior already present in generated DSL/overlay contracts:

- `military_posture`
- `research_bias`

No new strategic domains are introduced here.

## Acceptance Preconditions

1. Build and local tests pass:
   - `cmd /c tools/run_v0_pipeline_tests.cmd`
2. A validated DSL input is available:
   - `resources/generated_overlay_valid.dsl`
3. Overlay can be compiled and verified:
   - `Strategic Nexus.exe --compile-generated-overlay ...`
   - `Strategic Nexus.exe --verify-generated-overlay ...`

## Functional Acceptance Cases

### Case A: Defensive military posture

Input:

- DSL rule selecting `military_posture = defensive`

Expected result:

- Generated scripted artifacts contain the defensive branch and do not emit aggressive branch effects for the same rule evaluation path.

### Case B: Aggressive military posture

Input:

- DSL rule selecting `military_posture = aggressive`

Expected result:

- Generated scripted artifacts contain the aggressive branch and do not emit defensive branch effects for the same rule evaluation path.

### Case C: Economy research bias

Input:

- DSL rule selecting `research_bias = economy`

Expected result:

- Generated scripted artifacts contain economy-focused scripted value toggles and no military-industry fallback for that same accepted decision.

### Case D: Military research bias

Input:

- DSL rule selecting `research_bias = military`

Expected result:

- Generated scripted artifacts contain military-focused scripted value toggles and no economy fallback for that same accepted decision.

## Fail-Closed Acceptance Cases

### Case E: Invalid tactical-style domain

Input:

- Known-invalid DSL fixture (`resources/generated_overlay_invalid.dsl`)

Expected result:

- Validation/compiler rejects input.
- No partially accepted gameplay-affecting files are published as valid output.

### Case F: Manifest drift before publish

Input:

- Mutate any generated gameplay-affecting file after compile.

Expected result:

- `--verify-generated-overlay` fails closed.
- Publish path must be blocked.

## Evidence To Record

For each accepted run, record:

- command lines used
- output paths
- pass/fail result
- manifest hash (`generated_overlay_manifest_hash`) where available
- short note describing observed strategic effect branch

Store evidence as local report artifacts under `dist/private_reports/`.

## Done Criteria

The generated overlay gameplay verification gap is considered covered for v0 when:

1. Cases A-D pass with reproducible evidence.
2. Cases E-F fail closed as expected.
3. The result is visible in owner-facing status/reporting as `gameplay acceptance: verified for v0 domains`.

Until then, generated overlays remain contract-verified but not fully gameplay-verified.
