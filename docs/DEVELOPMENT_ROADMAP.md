# Strategic Nexus — Development Roadmap

## Purpose

This document defines the implementation order of Strategic Nexus.

The project must evolve incrementally.

Architecture stability is more important than immediate strategic complexity.

New systems should be added gradually using already established patterns.

The roadmap is a living document and must be continuously updated.

Use `MASTER_ARCHITECTURE_INDEX.md` as the canonical navigation layer for project architecture.

Use `V0_SCOPE_AND_PIPELINE_PLAN.md` as the current implementation boundary for the first daemon/pipeline skeleton.

---

# Status Definitions

## NOT_STARTED

The system or milestone does not exist yet.

## IN_PROGRESS

Partial implementation exists.
Testing or architecture work is ongoing.

## IMPLEMENTED

The system exists and basic functionality is verified.

## VERIFIED

The system has been tested and validated successfully.

## BLOCKED

Work cannot continue until another dependency is solved.

---

# Development Philosophy

Strategic Nexus must be built in layers.

Do NOT attempt:

* full strategic complexity immediately
* massive doctrine sets immediately
* huge prompt systems immediately
* full empire simulation immediately

Instead:

* build minimal stable loops
* verify architecture
* expand incrementally
* preserve compatibility
* keep systems modular
* keep systems bounded

---

# Rule Of Expansion

Start with:

* small enum sets
* small strategy domains
* small payload schemas
* small memory systems

Expand later using the same architectural patterns.

The architecture must be additive and extensible.

---

# Current Development Priorities

## 1. Core Runtime Skeleton

Status:
IN_PROGRESS

Goal:
Create minimal stable end-to-end architecture.

Required:

* parser skeleton
* daemon skeleton
* payload schema
* payload validation
* campaign identity handling
* sequence handling
* fallback handling
* audit logging
* bounded enum validation
* doctrine persistence

Notes:
Do not optimize strategic intelligence yet.
Optimize architecture stability.

Current implementation boundary:
Follow `V0_SCOPE_AND_PIPELINE_PLAN.md`.

The first pipeline should prove the small `X -> Y -> U -> payload` flow before expanding strategic complexity.

---

## 2. Minimal Strategy Dimensions

Status:
IN_PROGRESS

Goal:
Verify that LLM decisions can safely alter empire strategic behavior.

Initial domains:

* military_posture
* research_bias

Initial enum count:
2 values per domain only.

Example:

military_posture:

* defensive
* aggressive

research_bias:

* economy
* military

Notes:
More enums and domains will be added later using the same schema.

Current progress:
Bridge core can validate modular `strategy_dimensions` payloads and reject legacy `doctrine_id`.
Bridge core one-shot pipeline can emit an allowlisted effect batch and persist replay-protection sequence state.
The scripted PoC receiver is implemented for `military_posture` and `research_bias`; real gameplay effects are not implemented yet.
The first deterministic v0 strategic pipeline skeleton can transform fixture ministry input `X` into ministry proposal `Y`, clerk brief `U`, final `strategy_dimensions` payload, and bridge-core effect batch.
V0 pipeline tests now cover military posture, research bias, low-confidence agenda preservation, missing optional fields, invalid current agenda fallback, malformed input rejection, read-failure handling, and untrusted input bounds.
The v0 pipeline can emit a compact audit record for `X`, `Y`, `U`, and the final payload.
The v0 pipeline computes an audit-visible deterministic processing priority score.
A first local processing queue skeleton can order ministry contexts by priority and emit a compact queue JSON, but live daemon scheduling is not connected yet.
V0 ministry input text is bounded before audit or future LLM context use.
V0 cabinet enum validation reuses bridge-core strategy dimension parsing to avoid contract drift.
V0 decision and audit files are written through atomic temp-file replacement.
Audit output failure does not invalidate an otherwise valid v0 decision output.
Common atomic file writes reject directory targets without deleting them.
Bridge pipeline failure tests now cover effect batch write failure, malformed payload cleanup, replay cleanup, and sequence write failure cleanup.
`tools/run_all_tests.ps1` now runs the local bridge-core and v0 pipeline regression tests together.
GitHub Actions Windows CI runs the same local regression suite on push and pull request.

---

## 3. Empire-Scoped Context Pipeline

Status:
NOT_STARTED

Goal:
Ensure each empire receives isolated knowledge context.

Required:

* known empire filtering
* intel filtering
* sensor filtering
* anti-omniscience validation
* empire-specific snapshots

Notes:
No global galaxy AI brain allowed.

---

## 4. Save Timeline Memory Bootstrap

Status:
NOT_STARTED

Goal:
Use Stellaris timeline/history as portable memory anchor.

Required:

* timeline parsing
* historical event extraction
* bootstrap memory reconstruction
* campaign continuity after daemon loss

Notes:
Save portability is mandatory.

---

## 5. Multiplayer Host-Authoritative Validation

Status:
IN_PROGRESS

Goal:
Verify multiplayer synchronization architecture.

Required:

* host-only daemon
* synchronized scripted state
* client passive observation
* desync resistance
* fallback handling

Notes:
Clients must never process LLM payloads.

Current progress:
Scripted synchronized-state behavior is verified in MP.
Host-only daemon decisions are still outside production gameplay. Future MP work must stay within normal scripted state synchronization.

---

## 6. Safe Mod Integration

Status:
IN_PROGRESS

Goal:
Keep Strategic Nexus integrated through supported mod scripting, local files, and bounded JSON payloads.

Allowed targets:

* scripted mod receiver behavior
* local daemon request/response files
* bridge-core validation and allowlisted artifacts
* manual development harnesses for private testing

Forbidden:

* lower-level modification of the running game process
* proprietary executable analysis
* arbitrary command execution
* save editing
* combat patching
* direct AI replacement

Notes:
Lower-level game integration is out of scope for this repository.

Current progress:
The project now treats bridge-core output as a validation artifact and keeps gameplay delivery in the scripted mod layer or explicit development harnesses.

---

## 7. Modular Strategy Dimension Expansion

Status:
NOT_STARTED

Goal:
Expand strategy dimensions gradually.

Future domains may include:

* border_strategy
* fleet_philosophy
* diplomatic_behavior
* crisis_policy
* espionage_policy
* economic_focus
* internal_stability_policy

Notes:
Do not expand aggressively before v0 architecture stabilizes.

---

## 8. Espionage And Intel Layer

Status:
NOT_STARTED

Goal:
Use espionage/intel as legitimate information acquisition path.

Required:

* intel-aware reasoning
* uncertainty handling
* influence cost awareness
* espionage priorities
* counterintelligence posture

Notes:
Espionage must never become cheating.

---

## 9. Personality And Civilization Identity

Status:
NOT_STARTED

Goal:
Make civilizations strategically distinct.

Required:

* personality traits
* doctrine inertia
* risk tolerance
* trust/distrust
* trauma memory
* strategic culture

Notes:
Different civilizations should react differently to the same situation.

---

## 10. Strategic Memory System

Status:
NOT_STARTED

Goal:
Create long-term strategic continuity.

Required:

* memory summaries
* doctrine success/failure memory
* trauma persistence
* trust persistence
* memory decay
* historical interpretation

Notes:
Civilizations should feel historically aware.

---

## 11. Capability-Constrained Strategic Reasoning

Status:
NOT_STARTED

Goal:
Ensure strategies are realistic for the civilization.

Required:

* economy-aware reasoning
* infrastructure-aware reasoning
* technology-aware reasoning
* influence-aware reasoning
* logistics-aware reasoning
* transition-cost reasoning

Notes:
Theoretical optimality is not enough.
Strategies must be realistically executable.

---

## 12. Schema Evolution And Compatibility

Status:
IN_PROGRESS

Goal:
Allow safe long-term evolution of the architecture.

Required:

* schema versioning
* backward compatibility
* unknown field handling
* partial compatibility
* migration rules
* enum expansion safety

Notes:
The project must survive years of iteration.

Current progress:
Schema versioning rules exist.
Bridge core now requires `schema_version`.
Compatibility and migration layers are not fully implemented yet.

---

## 13. Advanced Strategic Behavior

Status:
NOT_STARTED

Goal:
Allow more sophisticated strategic interpretation.

Possible future areas:

* strategic deception
* proxy warfare
* alliance manipulation
* strategic bluffing
* ideological behavior
* long-term hegemon balancing
* adaptive federation strategy

Notes:
Only after core architecture is stable.

---

## 14. Advanced Runtime Integration

Status:
BLOCKED

Goal:
Create stable automated runtime bridge.

Dependencies:

* validated architecture
* stable payload schema
* stable strategy dimensions system
* verified multiplayer behavior
* verified fallback handling

Notes:
Must remain bounded and safe.

---

# Updating This Document

This document must evolve continuously.

Rules:

* update status fields regularly
* add new milestones only when justified
* avoid roadmap explosion
* preserve development order clarity
* preserve architecture-first philosophy

---

# Core Development Principle

Strategic Nexus should evolve like a civilization:

* slowly
* modularly
* persistently
* safely
* adaptively

Do not rush complexity before the architecture is stable.
