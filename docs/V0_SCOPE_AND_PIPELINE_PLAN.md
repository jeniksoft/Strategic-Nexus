# Strategic Nexus - v0 Scope And Pipeline Plan

## Purpose

This document defines the first implementation boundary after the proof-of-concept phase.

v0 must create a small end-to-end technical skeleton.

It must not attempt full Strategic Nexus intelligence.

---

# v0 Core Goal

Build a minimal, testable pipeline:

```text
empire snapshot
-> ministry input context X
-> ministry output Y
-> clerk-reduced input brief U
-> lightweight cabinet review
-> validated strategy_dimensions payload
-> bridge-core effect batch
```

The goal is architectural proof, not strategic depth.

---

# v0 Non-Goals

Do not implement in v0:

* full save parsing
* full LLM memory library
* full ministerial government
* long-term relationship modeling
* personality drift
* ethics reform logic
* war theater control
* custom networking
* direct tactical control
* save editing

If a system is needed for shape, use a small stub or fixed sample input.

---

# Initial Strategy Domains

v0 uses only two strategy domains:

```text
military_posture:
  defensive
  aggressive

research_bias:
  economy
  military_industry
```

These already match the current modular strategy dimension direction.

Do not add more domains until the end-to-end pipeline is stable.

Note:
`military_industry` is the bridge-contract-safe value for military research bias.
Avoid the shorter `military` value in payloads because the current bridge validator does not accept it.

---

# First Pipeline Objects

## X - Ministry Input Context

`X` is the bounded input for one ministry.

Initial fields:

```json
{
  "schema_version": 1,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "ministry": "military",
  "turn_context": {
    "year": 2230,
    "is_at_war": false,
    "strategic_pressure": 0.35
  },
  "known_facts": [],
  "uncertainties": [],
  "current_agenda": {}
}
```

For v0 this may come from sample JSON, not real save parsing.

## Y - Ministry Output / Agenda Proposal

`Y` is the ministry proposal.

Initial fields:

```json
{
  "schema_version": 1,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "ministry": "military",
  "proposal_id": "proposal_001",
  "requested_changes": {
    "military_posture": "defensive"
  },
  "stated_reasons": [],
  "explicit_risks": [],
  "explicit_uncertainties": [],
  "confidence": 0.6
}
```

The v0 ministry may initially be deterministic or fixture-driven.

The interface must allow later LLM replacement.

## U - Clerk-Reduced Input Brief

`U` is a compact brief derived from `X`, not from hidden context.

Initial fields:

```json
{
  "schema_version": 1,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "ministry": "military",
  "source_context_id": "context_001",
  "relevant_facts": [],
  "explicit_uncertainties": [],
  "missing_information": [],
  "compression_notes": []
}
```

The v0 clerk should be stateless.

It may be deterministic first.

If LLM extraction is later used, it must obey the same schema.

---

# Cabinet Review

The v0 cabinet receives:

```text
Y + U
```

It does not receive full `X`.

Initial cabinet behavior:

* validate proposal shape
* compare requested change against current agenda
* accept safe known enum values
* preserve existing agenda when confidence is too low
* emit final bounded `strategy_dimensions`

The v0 cabinet may initially be deterministic.

The interface must allow later LLM replacement.

---

# Final v0 Payload

The final payload must remain compatible with bridge-core direction:

```json
{
  "schema_version": 1,
  "sequence_id": 1,
  "created_unix_ms": 0,
  "ttl_ms": 30000,
  "campaign_id": "sample_campaign",
  "empire_id": "empire_001",
  "strategy_dimensions": {
    "military_posture": "defensive",
    "research_bias": "economy"
  },
  "fallback_required": false
}
```

Invalid output must fallback safely.

---

# Persistence In v0

Persist only:

* sequence state
* last valid final payload
* minimal audit record
* validation errors
* replay input/output fixture paths

Do not persist yet:

* memory books
* strategic episodes
* relationship memory
* personality drift
* cabinet conversation history
* clerk conversation history

---

# Testing Requirements

v0 tests must cover:

* valid `X -> Y -> U -> payload`
* invalid enum fallback
* low confidence preserves current agenda
* missing optional fields degrade safely
* stale payload rejection remains intact
* bridge-core tests still pass

Testing should be local and deterministic where possible.

---

# Implementation Order

1. Create schema/data structs for `X`, `Y`, and `U`.
2. Add fixture JSON files.
3. Add deterministic ministry stub.
4. Add stateless clerk reducer.
5. Add lightweight cabinet evaluator.
6. Emit final `strategy_dimensions` payload.
7. Reuse bridge-core pipeline for validation and effect batch generation.
8. Add tests.

Current progress:

* `src/strategic_pipeline/` contains the first deterministic v0 pipeline skeleton.
* `resources/ministry_context_military_defensive.json` and `resources/ministry_context_military_aggressive.json` provide sample `X` fixtures.
* `resources/ministry_context_research_economy.json` and `resources/ministry_context_research_military_industry.json` provide sample research ministry fixtures.
* `Strategic Nexus.exe --v0-pipeline <input_context> <decision_output> <sequence_id> <created_unix_ms> <ttl_ms> [audit_output]` emits a bridge-compatible decision JSON and can optionally emit a compact audit JSON.
* V0 decision and audit outputs are written through temporary files and replacement to avoid partial active outputs.
* V0 output failure tests verify a directory path is rejected without deleting the directory.
* Audit output failures are reported but do not invalidate an otherwise valid decision output.
* `tools/run_v0_pipeline_tests.ps1` verifies fixture `X -> Y -> U -> payload -> bridge-core effect batch` and audit record emission.
* V0 pipeline tests parse generated decision and audit JSON outputs before bridge handoff checks.
* `tools/run_all_tests.ps1` runs both bridge-core and v0 pipeline regression tests.
* The v0 skeleton is deterministic and does not use an LLM yet.
* Edge-case fixtures verify low-confidence agenda preservation, missing optional fields, escaped JSON string handling, and invalid current agenda fallback.
* Read-failure tests verify missing ministry input, malformed input, and unsupported schema fail closed without decision or audit output.
* Ministry input untrusted text is currently bounded to 512 characters per string and 16 entries per string array.
* Untrusted input tests verify ministry input arrays are bounded before audit and future LLM context use.
* Cabinet enum validation uses the same bridge-core strategy dimension parsers as runtime payload ingestion.
* Both initial v0 domains, `military_posture` and `research_bias`, are covered by local pipeline tests.
* The v0 pipeline now computes a deterministic processing priority score from war state, strategic pressure, and explicit uncertainty count.
* A first deterministic processing queue can order ministry input contexts by priority while preserving stable order inside ties.
* `Strategic Nexus.exe --v0-priority-queue <output_json> <input_context>...` writes a compact queue JSON for local scheduler testing.
* Priority scoring is audit-only for single v0 pipeline runs; queue ordering exists as a local skeleton but is not yet connected to the live daemon loop.
* The audit record contains the ministry input context, processing priority score, ministry output, clerk-reduced input brief, and final payload.

Validation note:
Preserving an existing agenda under low confidence must still validate the existing agenda values.
Low confidence must not allow invalid current strategy values to pass through to bridge-core.

---

# Open Questions

These do not block initial skeleton implementation:

* exact real save parser fields
* final LLM provider
* real gameplay effect balancing
* long-term memory schema

These must be decided before production gameplay:

* checksum-safe scripted mod packaging rules
* checksum-safe release packaging rules
* real empire identity mapping
* real campaign identity source
