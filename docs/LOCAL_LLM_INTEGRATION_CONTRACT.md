# Strategic Nexus - Local LLM Integration Contract

## Purpose

This document is the canonical contract for local LLM integration in Strategic Nexus.

It defines how model weights, local runtimes, the Strategic Nexus Companion, validators, generated overlays, and the Stellaris mod are separated.

It does not define strategic decision semantics. Use:

* [LLM_DECISION_CONTRACT.md](LLM_DECISION_CONTRACT.md) for bounded LLM recommendation behavior
* [LLM_CONTEXT_BUDGET_RULES.md](LLM_CONTEXT_BUDGET_RULES.md) for what context the LLM may receive
* [META_RULE_LANGUAGE_AND_COMPILER.md](META_RULE_LANGUAGE_AND_COMPILER.md) for the validated DSL and compiler path
* [GENERATED_OVERLAY_LAYOUT_CONTRACT.md](GENERATED_OVERLAY_LAYOUT_CONTRACT.md) for generated overlay file layout

---

# Core Contract

Strategic Nexus must not distribute LLM model weights with the Stellaris mod.

The user chooses and installs a supported local model through the companion app or by providing a documented local path.

The local model runtime is a replaceable execution layer, not a safety boundary.

The Strategic Nexus Companion owns model selection, setup, runtime wiring, readiness reporting, prompt/context assembly, output validation, cache invalidation, and reduced-mode behavior.

The Stellaris mod consumes only validated generated overlay artifacts.

The Stellaris mod must never consume raw model output, prompts, completions, local runtime responses, or free-form generated text.

---

# Layer Model

The local LLM ecosystem has separate layers:

```text
user-selected model weights
-> local model runtime
-> Strategic Nexus Companion
-> schema validation / allowlists / DSL compiler / manifest verifier
-> validated generated overlay
-> Strategic Nexus Stellaris mod
```

These layers must not be collapsed into one product component.

## Model Weights

Model weights are external artifacts selected by the user.

Strategic Nexus may store metadata about the selected model, but it must not bundle model weights into:

* the Stellaris mod package
* Steam Workshop or Paradox mod distribution
* generated overlay packages
* multiplayer overlay handoff packages
* GitHub release artifacts for the mod

Model weights may be large, license-restricted, gated, or unsuitable for redistribution.

## Local Model Runtime

The local model runtime loads the selected model and returns candidate output.

The runtime may be:

* an installed local inference backend
* a local server chosen by the user
* a documented runtime launched or configured by the companion app

The runtime is replaceable.

The runtime is not trusted to enforce Strategic Nexus safety rules.

## Strategic Nexus Companion

The companion app is the operational and validation boundary.

It owns:

* supported-model catalog
* license and use-policy metadata
* hardware recommendation
* explicit user model selection
* model download or local-path wiring when allowed
* local runtime configuration
* bounded prompt/context assembly
* offline inference calls
* parsing model output
* rejecting unsupported output
* invalidating stale caches after model changes
* generated overlay candidate validation
* Status Center model readiness and reduced-mode reporting

## Validators And Compiler

Validators and compilers are the trust gates.

They must:

* parse structured output
* reject unsupported fields
* clamp or reject out-of-range values
* accept only allowlisted strategy dimensions
* reject raw Stellaris script from the model
* compile only validated Strategic Nexus DSL
* verify generated overlay manifests
* verify multiplayer packages when relevant

## Stellaris Mod

The mod is intentionally smaller and dumber than the companion app.

It owns:

* stable handwritten fallback behavior
* generated overlay files already accepted by validators
* ordinary Stellaris scripted effects, triggers, events, and modifiers
* safe behavior when generated input is absent or rejected

The mod must not:

* contain model weights
* call local model runtimes
* parse prompts
* accept raw model output
* trust arbitrary generated text
* run independent client-side LLM decisions in multiplayer

---

# Distribution And Licensing

The supported-model catalog must be curated.

Unknown, unclear, gated, incompatible, or legally ambiguous model terms must not be treated as safe defaults.

Every supported model entry should include enough metadata for the companion app to explain why it is allowed or blocked in the current release mode.

Recommended catalog status values:

```text
approved_for_noncommercial_mod_use
approved_for_local_experimental_use
requires_user_license_acceptance
requires_login_or_gated_access
restricted
unsupported
```

Strategic Nexus must not bypass:

* model license acceptance
* gated-access approval
* authentication
* provider terms
* user consent

If terms are unclear, the safe result is reduced mode or explicit owner review, not silent installation.

---

# Model Catalog Fields

Each supported-model catalog entry should include:

```text
model_id
display_name
provider
runtime
source_url
license_name
license_url
use_policy_url
requires_user_license_acceptance
requires_login_or_gated_access
recommended_min_ram_gb
recommended_min_vram_gb
quality_tier
speed_tier
privacy_mode
status
last_license_review_date
notes
```

The catalog must be treated as policy data.

Changing catalog status for a model can change what users are allowed to install through the companion app.

---

# Local Model State

The companion app should persist local model state separately from the mod.
The default local model state path used by SNC is
`dist/private_reports/snc_local_model_state.json` unless the owner explicitly
chooses a different documented path.

The first implementation path is a companion/CLI prepare step:

```text
Strategic Nexus.exe --prepare-local-llm-model <model-id> <state-json> <accept-license|no-license> <download|no-download> [runtime-url]
```

The prepare step may only operate on a supported catalog model, currently an
Ollama model id such as `ollama:llama3.2:3b`. It must not download anything
unless the user has explicitly accepted the provider terms and the caller
passes `download`. If the runtime is missing, the model is not installed, or
the state file cannot be written, SNC remains in reduced mode.

Recommended state fields:

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

Recommended status values:

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

The model state is local operational state, not gameplay truth.

---

# Trust Boundary

All model output is untrusted until validated.

This remains true even when:

* the model is local
* the model was recommended by Strategic Nexus
* the runtime is supported
* the user intentionally selected the model
* previous outputs from the same model were valid

Allowed promotion path:

```text
model output
-> parse bounded candidate
-> reject unsupported fields
-> validate allowlisted strategy dimensions
-> compile through Strategic Nexus DSL
-> verify generated overlay manifest
-> publish only while Stellaris is closed
-> verify MP package when multiplayer use is relevant
```

Forbidden promotion paths:

```text
model output -> Stellaris script
model output -> generated overlay without DSL validation
model output -> active game runtime
model output -> multiplayer client-specific gameplay state
model output -> save editing
```

Raw prompt text, raw completion text, hidden chain-of-thought, and arbitrary free-form model text must not become gameplay files.

---

# Reduced Mode

Strategic Nexus must have a useful reduced mode when no supported local model is installed or ready.

Reduced mode still allows:

* autosave archive
* deterministic archive verification
* manifest verification
* generated overlay verification
* existing verified generated overlays to remain active
* Status Center diagnostics

Reduced mode disables:

* new LLM interpretation
* new model-derived generated overlay candidates
* model-dependent cache refresh

Status Center must plainly explain why reduced mode is active.

Examples:

```text
No supported local model is installed.
Selected model is missing.
Selected model requires user license acceptance.
Selected model is incompatible with available hardware.
Local runtime failed.
Selected model changed; previous analysis must be revalidated.
```

---

# Model Switching

Changing the selected model during use is allowed.

After a model change, model-dependent analysis must be considered stale until revalidated or regenerated.

At minimum, the companion app should invalidate:

* pending model-derived recommendations
* generated overlay candidates not yet accepted
* cached interpretation summaries that depend on the old model
* Status Center model readiness derived from the old model

Audit records should include:

```text
old_model_id
old_model_hash
new_model_id
new_model_hash
changed_at
affected_campaign_ids
revalidation_status
```

Model switching must not silently rewrite active gameplay-affecting overlays.

---

# Multiplayer Boundary

Clients must not independently run LLM analysis for gameplay-affecting multiplayer decisions.

The multiplayer path is:

```text
host/coordinator prepares one validated generated overlay package
-> package manifest verifies checksum-relevant generated files
-> participants install the same package
-> normal Stellaris/Steam lobby and checksum behavior remain the join gate
```

Model runtime output is never the multiplayer authority.

The validated package manifest is the authority for generated gameplay-affecting files.

---

# Privacy Boundary

Local-first analysis is the default.

The companion app should avoid sending save-derived context outside the local machine unless a future explicit opt-in feature says otherwise.

Model setup metadata may include URLs, licenses, hashes, and local paths, but generated gameplay artifacts should not contain raw prompts, raw model outputs, personal notes, or private file paths.

---

# Non-Goals

This contract does not authorize:

* bundling LLM weights with the mod
* treating model output as trusted
* cloud inference as the default production path
* local model fine-tuning as a required path
* raw save dumps into broad prompts
* raw LLM Stellaris script generation
* live LLM control of a running Stellaris session
* client-side independent LLM decisions in multiplayer
* save editing
* proprietary executable analysis

---

# Implementation Gates

Before Model Manager behavior is treated as production-ready, the project should have:

* supported-model catalog schema
* license/use-policy metadata handling
* hardware fit checks
* reduced-mode Status Center state
* model-ready Status Center state
* model-changed cache invalidation
* runtime failure handling
* tests for unsupported, missing, and license-blocked models
* tests proving raw model output cannot bypass schema, DSL, manifest, and package gates

This contract is architecture-level until those gates are implemented and verified.
