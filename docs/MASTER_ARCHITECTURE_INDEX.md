# Strategic Nexus - Master Architecture Index

## Purpose

This document is the canonical navigation layer for Strategic Nexus architecture.

It exists so future development does not depend on:

* one long chat thread
* implicit memory
* scattered Markdown files
* undocumented subsystem relationships

Use this document first when re-orienting the project.

---

# Hard Safety Rule

Strategic Nexus must not do anything illegal or against OpenAI rules.

If a proposed feature, tool, research direction, repository commit, GitHub push, or architecture path could violate law, GitHub rules, third-party rights, platform terms, OpenAI policy, or safe-use boundaries, it is out of scope and must be replaced with a safer design.

Treat GitHub content as potentially public even while the repository is private.

Maintain a local working copy as a backup and continuity anchor, subject to the same safety boundaries.

No architecture change may be accepted if it would violate, weaken, bypass, disable, or narrow the project safety audit. Redesign the architecture until `tools/run_safety_audit.ps1` passes with no findings.

Canonical safety document:

* [SAFETY_SCOPE.md](SAFETY_SCOPE.md)

---

# Current Project Status

Strategic Nexus is in early v0 architecture work.

Verified:

* singleplayer scripted mod receiver PoC
* multiplayer scripted state synchronization PoC
* modular `strategy_dimensions` receiver PoC
* bridge-core payload validation
* allowlisted effect batch generation
* sequence replay protection
* first deterministic v0 `X -> Y -> U -> payload -> effect batch` local pipeline test

In progress:

* core runtime skeleton
* modular strategy dimension pipeline
* schema evolution rules
* host-authoritative multiplayer architecture
* offline campaign analysis architecture for autosave archiving and next-session mod refresh
* local owner Task Board / tray helper for Codex coordination
* production companion tray app prototype (`StrategicNexusCompanionTray.exe`)

Not started:

* real LLM integration
* full save parser production pipeline integration
* ministerial reasoning pipeline implementation
* strategic memory library implementation
* real gameplay balancing effects
* production companion tray app release UX polish and lifecycle completeness

---

# Canonical Architecture

Core documents:

* [PROJECT_VISION.md](PROJECT_VISION.md) - project philosophy and hard boundaries
* [ARCHITECTURE.md](ARCHITECTURE.md) - four-layer architecture and fail-safe split
* [V0_SCOPE_AND_PIPELINE_PLAN.md](V0_SCOPE_AND_PIPELINE_PLAN.md) - bounded v0 scope and offline validator pipeline boundary
* [OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md](OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md) - revised session-to-session architecture, autosave archiving, campaign analysis, and next-session mod refresh
* [CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md](CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md) - target release orchestrator architecture for minimal mandatory user interaction
* [STELLARIS_DISTRIBUTION_AND_SAVE_ROOTS.md](STELLARIS_DISTRIBUTION_AND_SAVE_ROOTS.md) - verified non-Steam distribution surfaces and provider-neutral save-root contract
* [SAVE_ENTRY_POINT_AND_BRANCH_RULES.md](SAVE_ENTRY_POINT_AND_BRANCH_RULES.md) - entry-point-scoped generated rules and branch/reload handling for captured autosaves
* [LOCAL_LLM_INTEGRATION_CONTRACT.md](LOCAL_LLM_INTEGRATION_CONTRACT.md) - local model weights, runtime, companion, validation, reduced-mode, and mod boundary contract
* [META_RULE_LANGUAGE_AND_COMPILER.md](META_RULE_LANGUAGE_AND_COMPILER.md) - bounded DSL and deterministic compiler from LLM proposals to generated mod overlay
* [GENERATED_OVERLAY_LAYOUT_CONTRACT.md](GENERATED_OVERLAY_LAYOUT_CONTRACT.md) - v0 generated overlay file layout and staging contract
* [GENERATED_OVERLAY_GAMEPLAY_ACCEPTANCE_PLAN.md](GENERATED_OVERLAY_GAMEPLAY_ACCEPTANCE_PLAN.md) - bounded functional acceptance plan for proving generated overlay gameplay effect in v0 domains
* [MULTIPLAYER_SEASON_ORCHESTRATOR.md](MULTIPLAYER_SEASON_ORCHESTRATOR.md) - host-coordinated low-friction multiplayer season/package architecture
* [LEGACY_RUNTIME_ARCHITECTURE_INTERPRETATION_RULES.md](LEGACY_RUNTIME_ARCHITECTURE_INTERPRETATION_RULES.md) - how to read older daemon, bridge, and cadence documents after the offline architecture shift
* [DOCUMENTATION_CONSISTENCY_AUDIT.md](DOCUMENTATION_CONSISTENCY_AUDIT.md) - focused audits for contradictions between architecture documents
* [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md) - implementation order and milestone status
* [V0_SPRINT_MODE.md](V0_SPRINT_MODE.md) - temporary acceleration rules for reaching the first real-game v0 validation loop
* [PROJECT_ARCHITECTURE_INDEX_RULES.md](PROJECT_ARCHITECTURE_INDEX_RULES.md) - rules for maintaining this index
* [PROJECT_OWNER_COLLABORATION_MODEL.md](PROJECT_OWNER_COLLABORATION_MODEL.md) - project owner and Codex collaboration model, Free Work expectations, and owner input checklist
* [ENGINEERING_RULES.md](ENGINEERING_RULES.md) - practical engineering rules for development
* [CODEX_PROJECT_MEMORY_RULES.md](CODEX_PROJECT_MEMORY_RULES.md) - durable Markdown memory rules for Codex
* [CODEX_WORK_CONTINUITY_NOTES.md](CODEX_WORK_CONTINUITY_NOTES.md) - lightweight Codex continuity ledger to avoid repeated maintenance work after context loss
* [DATA_ANALYTICS_CONTEXT.md](DATA_ANALYTICS_CONTEXT.md) - reusable data-source, privacy, provenance, and analysis workflow context for future data work
* [CODEX_TOOL_INSTALLATION_AUTONOMY_RULES.md](CODEX_TOOL_INSTALLATION_AUTONOMY_RULES.md) - rules for autonomous installation of useful development tools
* [EMERGENCY_UI_AUTOMATION_RULES.md](EMERGENCY_UI_AUTOMATION_RULES.md) - rules for last-resort mouse/keyboard/screenshot automation
* [FREE_WORK_AND_USAGE_BUDGET_RULES.md](FREE_WORK_AND_USAGE_BUDGET_RULES.md) - rules for autonomous Codex work based on declared usage budget
* [AUTOMATION_CANDIDATES.md](AUTOMATION_CANDIDATES.md) - proposed recurring Codex automation candidates and recommended cadences
* [DEV_DIARY.md](DEV_DIARY.md) - daily human-readable development diary for project continuity and external feedback
* [CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md](CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md) - campaign-empire personality creation, evolution, and LLM conversation boundaries

Core rule:

```text
Vanilla Stellaris AI executes gameplay.
Strategic Nexus influences bounded strategic state.
Strategic Nexus Companion (SNC) archives autosaves with read-only access.
SNC is provider-neutral: it detects Stellaris and save roots, not Steam as a required runtime.
SNC acts as a campaign orchestrator and should automate safe staging decisions where confidence is high.
Local analysis updates campaign-scoped memory between play sessions.
The generated mod overlay applies bounded strategic state on the next launch.
Generated rules are entry-point scoped: they must match the loaded save state, not merely the latest captured autosave.
The LLM proposes only bounded DSL rules; the compiler validates and translates them.
LLM model weights are external user-selected dependencies and are never distributed with the mod.
The local model runtime is replaceable and is not a safety boundary.
The companion validates all model output before any generated overlay can be accepted.
The mod applies safe scripted behavior and fallback logic.
```

---

# Safe Mod And Daemon Architecture

Primary documents:

* [SAFETY_SCOPE.md](SAFETY_SCOPE.md)
* [AUTOMATION_REQUIREMENT.md](AUTOMATION_REQUIREMENT.md)

Implemented code:

* `src/bridge_core/`
* `tools/build_bridge_core.ps1`
* `tools/run_bridge_core_tests.ps1`
* `tools/run_all_tests.ps1`
* `.github/workflows/windows-tests.yml`

Status:

* bridge-core validation exists
* effect batch generation exists
* structurally malformed bridge decision payloads fail closed
* bridge decision payloads require campaign and empire identity fields
* oversized numeric bridge payload fields fail closed instead of throwing parser exceptions
* bridge effect batch writes and cleanup preserve directory targets
* bridge pipeline does not advance sequence when effect batch write fails
* bridge pipeline clears stale effect batch output when decision payload reading fails
* bridge pipeline clears effect batch output if sequence state write fails
* lower-level game-process modification and proprietary executable analysis are out of scope

---

# Payload And Schema Contracts

Primary documents:

* [BRIDGE_CORE_CONTRACT.md](BRIDGE_CORE_CONTRACT.md)
* [BRIDGE_CORE_TEST_PLAN.md](BRIDGE_CORE_TEST_PLAN.md)
* [PAYLOAD_LIFECYCLE_RULES.md](PAYLOAD_LIFECYCLE_RULES.md)
* [SCHEMA_VERSIONING_RULES.md](SCHEMA_VERSIONING_RULES.md)
* [MODULAR_STRATEGY_DIMENSIONS.md](MODULAR_STRATEGY_DIMENSIONS.md)
* [LLM_DECISION_CONTRACT.md](LLM_DECISION_CONTRACT.md)

Current v0 payload direction:

```text
modular strategy dimensions
not one monolithic doctrine_id
```

Initial implemented dimensions:

* `military_posture`
* `research_bias`

Current v0 pipeline:

* `src/strategic_pipeline/`
* `resources/ministry_context_military_defensive.json`
* `resources/ministry_context_military_aggressive.json`
* `resources/ministry_context_research_economy.json`
* `resources/ministry_context_research_military_industry.json`
* `resources/ministry_context_unknown_preserve_agenda.json`
* `resources/ministry_context_missing_optional_fields.json`
* `resources/ministry_context_escaped_strings.json`
* `resources/ministry_context_invalid_current_agenda.json`
* `resources/ministry_context_malformed.json`
* `resources/ministry_context_unsupported_schema.json`
* `resources/ministry_context_untrusted_bounds.json`
* `resources/generated_overlay_valid.dsl`
* `resources/generated_overlay_invalid.dsl`
* `src/generated_overlay/`
* `tests/generated_overlay_contract_test.cpp`
* `tools/run_v0_pipeline_tests.ps1`
* `tools/run_all_tests.ps1`
* `.github/workflows/windows-tests.yml`

The v0 pipeline is deterministic and uses fixture input only.
It does not parse saves and does not call an LLM yet.
The test suite verifies military posture, research bias, low-confidence preservation, missing optional fields, escaped JSON string handling, invalid agenda fallback, malformed input rejection, read-failure handling, untrusted input bounds, and compact audit record emission.

Architectural update:

* the v0 pipeline remains useful as a bounded offline validator and generated-overlay preparation path
* production architecture should not assume realtime LLM decisions can enter a running game
* future work should prefer session-end archived-save analysis and next-session mod refresh
* future generated overlays should be produced from validated Strategic Nexus DSL, not raw LLM script
* `src/generated_overlay/` contains the first DSL parser, validator, and deterministic overlay compiler contract skeleton
* `--compile-generated-overlay` stages generated event/effect/trigger files from validated DSL for local testing

Current v0 untrusted input limits:

* ministry input strings: 512 characters
* ministry input string arrays: 16 entries

Validation ownership:

* bridge-core strategy dimension parsers are the canonical enum validation source
* v0 cabinet validation reuses bridge-core parsers to avoid payload contract drift
* v0 ministry input reads escaped JSON strings before sanitizing untrusted text
* v0 decision and audit outputs use atomic temp-file replacement
* audit output failure is non-fatal when the decision output is valid
* common atomic writes reject directory targets without deleting them
* common atomic writes use Windows replace-existing file moves instead of remove-then-rename replacement

Hard boundaries:

* no raw LLM text
* no arbitrary command strings
* no direct tactical control
* no save fragments
* no fleet targets
* no unrestricted script text

Audit boundary:

* v0 audit records may persist `X`, `Y`, `U`, and the final payload
* v0 audit records must remain compact developer diagnostics
* v0 audit records are not strategic memory books

---

# Multiplayer

Primary documents:

* [MULTIPLAYER_REQUIREMENTS.md](MULTIPLAYER_REQUIREMENTS.md)
* [POC_A_MULTIPLAYER_TEST.md](POC_A_MULTIPLAYER_TEST.md)
* [POC_A_MULTIPLAYER_RESULTS.md](POC_A_MULTIPLAYER_RESULTS.md)
* [MP_HOST_AUTHORITATIVE_PAYLOAD_POC.md](MP_HOST_AUTHORITATIVE_PAYLOAD_POC.md)

Status:

* host-triggered scripted state synchronization has been verified
* host-authoritative payload model is viable at scripted state level
* clients must not run LLM
* clients must not generate strategic decisions

Architecture rule:

```text
host/coordinator prepares byte-identical generated overlay before launch -> ordinary scripted mod state synchronizes -> clients observe normal MP state
```

Target release direction:

```text
host orchestrator builds and verifies one canonical MP generated overlay package
-> players install the identical package
-> players join through normal Stellaris/Steam lobby flow
-> checksum/load-order compatibility remains the primary join gate
```

---

# PoC And Test Assets

Primary documents:

* [POC_A_MULTIPLAYER_TEST.md](POC_A_MULTIPLAYER_TEST.md)
* [POC_A_MULTIPLAYER_RESULTS.md](POC_A_MULTIPLAYER_RESULTS.md)
* [ON_GAME_START_COUNTRY_SP_TEST.md](ON_GAME_START_COUNTRY_SP_TEST.md)

Mod assets:

* `mod/strategic_nexus_poc/`

Status:

* SP PoC A is fully verified
* MP scripted synchronization is verified
* observer mode is useful for long-run behavior observation, not bridge proof

---

# Empire Knowledge And Anti-Omniscience

Primary documents:

* [EMPIRE_KNOWLEDGE_MODEL.md](EMPIRE_KNOWLEDGE_MODEL.md)
* [NO_GLOBAL_GALACTIC_AI_BRAIN.md](NO_GLOBAL_GALACTIC_AI_BRAIN.md)
* [IMPERFECT_INTELLIGENCE_RULES.md](IMPERFECT_INTELLIGENCE_RULES.md)
* [INTEL_AND_DOCTRINE_RULES.md](INTEL_AND_DOCTRINE_RULES.md)
* [ESPIONAGE_AND_INTEL_STRATEGY_RULES.md](ESPIONAGE_AND_INTEL_STRATEGY_RULES.md)
* [LLM_CONTEXT_BUDGET_RULES.md](LLM_CONTEXT_BUDGET_RULES.md)

Core rule:

```text
Every LLM request belongs to exactly one empire perspective.
No global omniscient galaxy brain.
```

Status:

* rules exist
* narrow player-empire save headline parser exists
* production parser/filter integration into the archive-to-brief pipeline is not started

---

# LLM Reasoning Architecture

Primary documents:

* [LOCAL_LLM_INTEGRATION_CONTRACT.md](LOCAL_LLM_INTEGRATION_CONTRACT.md)
* [LLM_DECISION_CONTRACT.md](LLM_DECISION_CONTRACT.md)
* [LLM_CONTEXT_BUDGET_RULES.md](LLM_CONTEXT_BUDGET_RULES.md)
* [LLM_MODEL_SELECTION_AND_TASK_FIT_RULES.md](LLM_MODEL_SELECTION_AND_TASK_FIT_RULES.md)
* [LLM_PERSONALITY_DECISION_RULES.md](LLM_PERSONALITY_DECISION_RULES.md)
* [MINISTERIAL_REASONING_PIPELINE_RULES.md](MINISTERIAL_REASONING_PIPELINE_RULES.md)
* [PROBABILISTIC_DECISION_SAMPLING_RULES.md](PROBABILISTIC_DECISION_SAMPLING_RULES.md)
* [CODEX_MODEL_USAGE_RULES.md](CODEX_MODEL_USAGE_RULES.md)
* [FREE_WORK_AND_USAGE_BUDGET_RULES.md](FREE_WORK_AND_USAGE_BUDGET_RULES.md)

Important distinction:

* `CODEX_MODEL_USAGE_RULES.md` governs Codex development behavior
* `FREE_WORK_AND_USAGE_BUDGET_RULES.md` governs Codex autonomous work intensity from user-declared remaining usage budget
* `LOCAL_LLM_INTEGRATION_CONTRACT.md` governs future Strategic Nexus model weights, runtime, companion, validation, reduced-mode, and mod separation
* `LLM_MODEL_SELECTION_AND_TASK_FIT_RULES.md` governs future Strategic Nexus daemon/model selection

Current target:

```text
small bounded v0 reasoning pipeline
not full ministerial government simulation yet
```

---

# Ministerial Pipeline

Primary document:

* [MINISTERIAL_REASONING_PIPELINE_RULES.md](MINISTERIAL_REASONING_PIPELINE_RULES.md)

Conceptual model:

```text
X = ministry input context
Y = ministry output / agenda proposal
U = clerk-reduced ministry input brief

cabinet evaluates Y + U
```

Status:

* architecture rules exist
* implementation is not started
* v0 should use only a minimal subset

Recommended v0 subset:

* one or two ministry domains
* stateless clerk interface
* lightweight cabinet comparator
* no persistent ministry personality

---

# Memory Systems

Primary documents:

* [STRATEGIC_MEMORY_RULES.md](STRATEGIC_MEMORY_RULES.md)
* [STRATEGIC_MEMORY_LIBRARY_RULES.md](STRATEGIC_MEMORY_LIBRARY_RULES.md)
* [TIMELINE_MEMORY_ANCHOR_RULES.md](TIMELINE_MEMORY_ANCHOR_RULES.md)
* [SAVE_FORMAT.md](SAVE_FORMAT.md)
* [CAMPAIGN_IDENTITY_RULES.md](CAMPAIGN_IDENTITY_RULES.md)

Core direction:

```text
save timeline is canonical portable historical anchor
daemon memory enriches but does not own campaign identity
```

Status:

* conceptual architecture exists
* offline autosave archive architecture exists
* production memory library is not started
* v0 should only persist minimal last-known-good state and audit snapshots

---

# Personality, Ethics, Politics, And Relations

Primary documents:

* [PERSONALITY_SYSTEM.md](PERSONALITY_SYSTEM.md)
* [CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md](CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md)
* [LLM_PERSONALITY_DECISION_RULES.md](LLM_PERSONALITY_DECISION_RULES.md)
* [ETHICS_AND_FACTION_GOVERNANCE_RULES.md](ETHICS_AND_FACTION_GOVERNANCE_RULES.md)
* [TRADITION_AND_ASCENSION_STRATEGY_RULES.md](TRADITION_AND_ASCENSION_STRATEGY_RULES.md)
* [CIVILIZATION_RELATION_MODEL_RULES.md](CIVILIZATION_RELATION_MODEL_RULES.md)
* [DOCTRINE_INERTIA_RULES.md](DOCTRINE_INERTIA_RULES.md)

Status:

* architecture rules exist
* campaign-empire personality bootstrap rules exist
* internal pressure, strategic reputation, and doctrine reform cost are approved architecture layers for later strategic memory work
* subsystem schemas are implementation boundaries; final interpretation must reconcile one integrated campaign-empire state
* implementation is not started
* v0 should not implement full politics/personality drift

---

# Strategy Domains

Primary documents:

* [MODULAR_STRATEGY_DIMENSIONS.md](MODULAR_STRATEGY_DIMENSIONS.md)
* [RESEARCH_DIRECTION_BIAS_RULES.md](RESEARCH_DIRECTION_BIAS_RULES.md)
* [WAR_THEATER_PRIORITY_RULES.md](WAR_THEATER_PRIORITY_RULES.md)
* [ESPIONAGE_AND_INTEL_STRATEGY_RULES.md](ESPIONAGE_AND_INTEL_STRATEGY_RULES.md)
* [TRADITION_AND_ASCENSION_STRATEGY_RULES.md](TRADITION_AND_ASCENSION_STRATEGY_RULES.md)

Initial v0 domains:

* `military_posture`
* `research_bias`

Future domains:

* border strategy
* fleet philosophy
* diplomacy
* crisis policy
* espionage policy
* internal stability
* war theater priorities

---

# Performance And Runtime Limits

Primary documents:

* [TECHNICAL_RUNTIME_LIMITATIONS.md](TECHNICAL_RUNTIME_LIMITATIONS.md)
* [ADAPTIVE_SYSTEM_LOAD_RULES.md](ADAPTIVE_SYSTEM_LOAD_RULES.md)
* [ATTENTION_AND_REVIEW_CADENCE_RULES.md](ATTENTION_AND_REVIEW_CADENCE_RULES.md)
* [EMPIRE_PROCESSING_PRIORITY_QUEUE.md](EMPIRE_PROCESSING_PRIORITY_QUEUE.md)
* [LLM_MODEL_SELECTION_AND_TASK_FIT_RULES.md](LLM_MODEL_SELECTION_AND_TASK_FIT_RULES.md)

Core rule:

```text
gameplay performance beats strategic freshness
Strategic Nexus is asynchronous and low-frequency
```

Status:

* rules exist
* deterministic v0 priority scorer exists
* deterministic v0 processing queue skeleton exists
* local v0 priority queue CLI harness exists
* offline-worker scheduler integration is not started

---

# Safety, Privacy, And Security

Primary documents:

* [PROMPT_INJECTION_AND_UNTRUSTED_INPUT_RULES.md](PROMPT_INJECTION_AND_UNTRUSTED_INPUT_RULES.md)
* [PRIVACY_AND_LEARNING_RULES.md](PRIVACY_AND_LEARNING_RULES.md)
* [FAULT_TOLERANCE_AND_FAILURE_RECOVERY_RULES.md](FAULT_TOLERANCE_AND_FAILURE_RECOVERY_RULES.md)
* [DROP_IN_DROP_OUT_COMPATIBILITY.md](DROP_IN_DROP_OUT_COMPATIBILITY.md)
* [MOD_INTEROPERABILITY_AND_GAMEPLAY_DEGRADATION_RULES.md](MOD_INTEROPERABILITY_AND_GAMEPLAY_DEGRADATION_RULES.md)
* [GAME_VERSION_AND_DLC_RESILIENCE_RULES.md](GAME_VERSION_AND_DLC_RESILIENCE_RULES.md)
* [STELLARIS_MECHANICS_KNOWLEDGE_RULES.md](STELLARIS_MECHANICS_KNOWLEDGE_RULES.md)

Core rules:

* local-first by default
* remote learning is opt-in only
* all player/mod text is untrusted
* failures must degrade safely
* saves must not be corrupted

---

# Testing And Validation

Primary documents:

* [TESTING_AND_VALIDATION_ARCHITECTURE_RULES.md](TESTING_AND_VALIDATION_ARCHITECTURE_RULES.md)
* [TELEMETRY_GUIDED_TUNING_RULES.md](TELEMETRY_GUIDED_TUNING_RULES.md)
* [BRIDGE_CORE_TEST_PLAN.md](BRIDGE_CORE_TEST_PLAN.md)
* [POC_A_MULTIPLAYER_RESULTS.md](POC_A_MULTIPLAYER_RESULTS.md)

Current automated tests:

* bridge core build/test script exists
* payload validation tests exist
* effect batch generation tests exist

Local owner coordination tooling:

* `tools/dev_attention/user_task_board.ps1` provides a local Windows Task Board / tray helper for owner-facing tasks and reports
* this is development coordination tooling, not the production Strategic Nexus companion app
* real production companion app lifecycle, autosave watching, analysis orchestration, and overlay staging remain separate implementation work

Future test assets:

* regression saves
* replay records
* hostile input tests
* long-run MP stability tests

Tuning rule:

* telemetry may guide future constant/bias/cadence tuning
* telemetry must produce reviewed, versioned tuning changes, not uncontrolled live self-modification

---

# v0 Implementation Priority

Current owner priority:

Reach the first functional v0 that can be tested in real Stellaris sessions as soon as safely possible.
The project should optimize for the shortest real-game validation loop, not for polishing already usable pieces.
If a component is good enough for the first real-game test, record follow-up polish separately and move to the next missing end-to-end v0 piece.

Do next:

1. Close the minimum real-game validation loop: save/session archive -> analysis -> validated v0 output/overlay/package/status -> next owner-run Stellaris session.
2. Keep the verified archive spine aligned with the offline architecture.
3. Feed the narrow save parser summary into the season delta ledger, empire brief, and ministry input harnesses only as far as needed for first v0 testing.
4. Add functional generated-overlay acceptance evidence for the current v0 domains.
5. Extend companion-app-safe staging and Status Center visibility without adding realtime game control.
6. Keep integration limited to supported mod scripting, local archives, generated overlay files, and bounded JSON payloads.

Do not start yet:

* full memory library
* full ministerial government simulation
* personality drift
* war theater control
* raw LLM integration into game runtime
* non-blocking polish before the first real-game v0 validation loop exists

---

# Maintenance Rule

When adding a new major architecture document:

1. Add it to this index.
2. Assign it to a subsystem category.
3. Mark whether it is conceptual, prototype, implemented, or verified.
4. Note implementation impact if it changes v0 scope.
