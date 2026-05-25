# Strategic Nexus - Legacy Runtime Architecture Interpretation Rules

## Purpose

Older Strategic Nexus documents were written when the project still explored realtime daemon and bridge delivery.

The production direction has changed.

Strategic Nexus is now a session-to-session offline companion architecture.

This document defines how to interpret older runtime terms without deleting useful validated work.

---

# Canonical Production Direction

For production gameplay, assume:

```text
companion app archives autosaves during play
-> offline analysis runs after Stellaris exits
-> compiler generates a bounded mod overlay
-> generated overlay is used on the next launch
```

Do not reinterpret older runtime language as permission to send new LLM decisions into an already-running game.

---

# Legacy Term Mapping

## Daemon

Older docs often say `daemon`.

In current production architecture, read this as one of:

* companion app background worker
* offline post-session analysis worker
* local development harness
* scheduler for work that does not block active gameplay

Do not read it as:

* live gameplay controller
* live LLM-to-game transport
* required process launched by the mod
* system that rewrites behavior during a running Stellaris session

---

## Bridge

Older docs often say `bridge` or `bridge core`.

The bridge-core validation work remains useful as:

* schema validation
* bounded value validation
* replay and stale-output protection
* deterministic artifact generation
* local development harness

For production, it must not be treated as proof that a live transport into the running game exists or should be built.

---

## Runtime Cadence

Older docs may mention monthly, yearly, priority-queue, or repeated review cadence.

In current architecture, this means:

* offline analysis cadence over archived autosaves
* generated-overlay refresh cadence between sessions
* low-frequency scripted consumption of already-generated values

It does not mean:

* repeated LLM decisions during active gameplay
* per-tick or per-day mod decisions
* continuous live strategy replacement

---

## Generated Output

Generated output now means a full replacement snapshot for the active generated mod library.

It does not mean an append-only pile of new events, modifiers, or rules.

Old generated rules that are not present in the next snapshot are removed from the active overlay.

Durable Strategic Nexus archive and memory remain on disk outside the active generated mod library.

---

# Conflict Rule

If an older document appears to conflict with offline companion architecture, prefer:

1. `TECHNICAL_RUNTIME_LIMITATIONS.md`
2. `OFFLINE_CAMPAIGN_ANALYSIS_ARCHITECTURE.md`
3. `META_RULE_LANGUAGE_AND_COMPILER.md`
4. this document

Then preserve the older document only as conceptual background, implementation detail, or development harness guidance.

---

# Design Goal

Keep useful validation and scheduler work from the old architecture, while preventing architecture drift back into realtime LLM control.
