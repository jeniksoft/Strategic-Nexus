# Strategic Nexus - Generated Overlay Layout Contract

## Purpose

This document defines the first v0 generated overlay file layout.

It records what the current local compiler harness stages from validated Strategic Nexus DSL.

The layout is a contract for tests and future companion-app staging work, not final gameplay balancing.

---

# Production Boundary

Generated overlay files are next-session artifacts.

They must be written only while Stellaris is closed.

They must be generated from validated Strategic Nexus DSL, not raw LLM text or raw Stellaris script.

The active generated overlay is a complete replacement snapshot.

---

# Current CLI Harness

The current local harness is:

```text
Strategic Nexus.exe --compile-generated-overlay <input.dsl> <output_dir>
Strategic Nexus.exe --verify-generated-overlay <output_dir>
```

It:

1. reads DSL from `<input.dsl>`
2. parses the bounded campaign/empire/rule shape
3. validates identifiers, ministries, preferences, conditions, confidence, intensity, and budgets
4. compiles deterministic generated overlay text
5. stages files under `<output_dir>`

If parsing or validation fails, it returns failure and does not write overlay files.

The verify command:

1. reads `<output_dir>/strategic_nexus_generated_manifest.json`
2. validates that the manifest has balanced JSON delimiters
3. reads every file listed in the manifest
4. recomputes deterministic `fnv1a64` hashes and byte counts
5. fails closed if a file is missing, changed, path-unsafe, classified incorrectly, unknown to the v0 generated layout, or mismatched

The verifier is a package consistency helper.

It does not prove that the game will accept the mod, only that the generated overlay files still match their manifest.

Functional behavior verification is a separate requirement.
Manifest verification, path safety, deterministic hashes, and successful staging do not prove that the generated mod does what the architecture intended.

Before any generated overlay is considered gameplay-ready, the project should have at least one of:

* a deterministic mod-side smoke harness
* a local scripted test that checks the intended generated trigger/effect path
* a documented manual Stellaris acceptance test with expected observations

Until then, generated overlay output should be described as contract-verified, not gameplay-verified.

---

# v0 File Layout

The current staged layout is:

```text
<output_dir>/
  strategic_nexus_generated_manifest.json
  events/
    strategic_nexus_generated_events.txt
  common/
    scripted_effects/
      strategic_nexus_generated_effects.txt
    scripted_triggers/
      strategic_nexus_generated_triggers.txt
```

These filenames are intentionally stable for v0 tests.

Future layout changes must update:

* this document
* `src/generated_overlay/`
* `tests/generated_overlay_contract_test.cpp`
* `tools/run_v0_pipeline_tests.ps1`

---

# File Responsibilities

## Generated Manifest

`strategic_nexus_generated_manifest.json` is the deterministic overlay snapshot manifest.

It records:

* manifest schema version
* generator id
* complete replacement snapshot marker
* campaign ids represented by the snapshot
* generated gameplay file paths
* checksum relevance classification
* file byte counts
* deterministic file content hashes

The v0 hash is `fnv1a64`.

This is an audit and byte-identity helper, not a cryptographic security boundary.

For multiplayer packaging, the manifest is the first contract the companion app can use to compare generated gameplay-affecting files across host/export/client package flows.

`--verify-generated-overlay` is the current local harness for this comparison.

The manifest itself should be distributed with the generated overlay package for diagnostics.

The gameplay-affecting generated files listed inside the manifest are the checksum-sensitive payload.

## Generated Events

`events/strategic_nexus_generated_events.txt` is the dispatcher surface.

It should remain small.

It should call generated scripted effects at controlled low-frequency moments.

Generated events must not become an append-only behavior history.

---

## Generated Scripted Effects

`common/scripted_effects/strategic_nexus_generated_effects.txt` applies bounded campaign/empire strategic state.

Effects should:

* check generated triggers before applying state
* select allowlisted base behavior or flags
* be idempotent where practical
* avoid unbounded stacking

---

## Generated Scripted Triggers

`common/scripted_triggers/strategic_nexus_generated_triggers.txt` owns cheap eligibility checks.

In v0 this means:

* campaign marker guard
* empire marker guard

Future conditions may be added only if they remain cheap and validated.

---

# Checksum And Multiplayer Rule

Generated overlay files are gameplay-affecting unless proven otherwise.

For v0, the generated files under `events/`, `common/scripted_effects/`, and `common/scripted_triggers/` are classified as:

```text
gameplay_affecting
```

The manifest is classified as a diagnostic/package verification file.

It should be kept identical across packages when practical, but the strict gameplay checksum requirement applies to the generated gameplay files it lists.

The verifier enforces this v0 classification for known generated gameplay file paths.
If a generated manifest marks one of those paths as anything other than `gameplay_affecting`, or lists an unknown generated overlay file path, verification fails closed.

For multiplayer, every participant must use byte-identical generated gameplay files.

The companion app must eventually provide export/package verification before generated overlays are considered multiplayer-ready.

---

# Non-Goals

The current v0 layout does not yet define:

* final gameplay balancing values
* final allowlisted modifier catalog
* final multiplayer export archive format
* launcher integration
* campaign library cleanup implementation

---

# Design Goal

The generated overlay should be:

```text
small, deterministic, marker-guarded, and replaceable
```

not:

```text
an accumulating pile of generated gameplay script
```
