# Strategic Nexus - Schema Versioning Rules

## Core Rule

All Strategic Nexus payloads, bridge contracts, and strategic schemas must be versioned.

The architecture must support:
- gradual evolution
- backward compatibility
- partial compatibility
- append-only expansion when possible
- safe fallback behavior

The system must survive:
- daemon updates
- mod updates
- bridge updates
- Stellaris updates
- long-running campaigns
- multiplayer synchronization requirements

---

# Required Schema Version Field

Every payload must include:

```json
{
  "schema_version": 1
}
```

Schema version is mandatory.

Payloads without schema_version are invalid unless explicitly supported by compatibility rules.

---

# Design Philosophy

Strategic Nexus schemas are expected to evolve over time.

The architecture must assume:
- new domains will be added
- new enums will appear
- old enums may be deprecated
- some systems will become optional
- bridge capabilities may expand gradually

Schema evolution is normal and expected.

---

# Append-Only Preference

Whenever possible:

- add fields
- add domains
- add enum values

Avoid:
- renaming fields
- changing semantic meaning
- destructive rewrites
- incompatible payload redesigns

Preferred evolution:

```text
v1
-> add optional field
-> v2
```

Rejected evolution:

```text
v1
-> completely different payload structure
-> v2
```

---

# Unknown Field Rule

Unknown fields must not crash the system.

Correct behavior:

```text
unknown field
-> ignore safely
-> preserve known behavior
```

This allows newer daemon versions to communicate with older bridge/mod versions more safely.

---

# Missing Field Rule

Missing optional fields must not invalidate an otherwise safe payload.

Correct behavior:

```text
missing field
-> use previous value
or
-> use safe default
```

Rejected behavior:

```text
missing field
-> reject entire payload
```

---

# Unknown Enum Rule

Unknown enum values must not crash the system.

Correct behavior:

```text
unknown enum
-> ignore that domain
or
-> fallback that domain only
```

Do not invalidate the entire payload unless safety requires it.

---

# Enum Expansion Playbook

When Strategic Nexus adds a new enum value:

* add the value append-only when possible
* keep existing values accepted for older consumers
* document which consumer owns the new value mapping
* preserve domain-level fallback behavior for unsupported values
* add a regression test for the new value before the roadmap slice is marked done

When an enum value is deprecated or renamed:

* keep the old value readable for compatibility where safe
* map it through an explicit migration rule when a rename is unavoidable
* fail closed for the affected domain if the old and new meanings would be ambiguous

The bridge core, compiler, and generated-overlay validator should all agree on the same allowlist and fallback semantics before a new enum is considered production-ready.

---

# Partial Compatibility Rule

A payload may still be partially valid even if some domains are unsupported.

Example:

```json
{
  "schema_version": 3,
  "strategy_dimensions": {
    "military_posture": "defensive",
    "fleet_philosophy": "carrier",
    "new_future_domain": "experimental_value"
  }
}
```

Older bridge version:

```text
known domains
-> apply

unknown domains
-> ignore safely
```

---

# Domain Expansion Rule

New strategic domains may be added over time.

Example:

v1:
- military_posture
- fleet_philosophy
- diplomatic_behavior

v2:
+ espionage_policy
+ crisis_policy

v3:
+ internal_stability_policy

Older payloads remain valid.

---

# Enum Expansion Rule

New enum values may be added gradually.

Example:

v1:

```text
fleet_philosophy:
- balanced
- artillery
- carrier
```

v2:

```text
fleet_philosophy:
- balanced
- artillery
- carrier
- missile
- torpedo
```

Old payloads remain valid.

---

# Deprecated Value Rule

Deprecated enums or domains should remain readable for compatibility.

Correct behavior:

```text
deprecated value
-> migration mapping
or
-> compatibility fallback
```

Rejected behavior:

```text
old enum
-> hard crash
```

---

# Payload Migration Rule

If migration is needed:

```text
old schema
-> migration layer
-> normalized internal representation
```

Migration should happen:
- inside daemon
- or inside bridge core

Until a real migration registry exists, old artifacts should keep failing closed in a way that preserves safe fallback behavior and gives the owner an explicit degraded-state signal rather than pretending the payload is fully migrated.

## Migration Registry Contract

The project does not yet have a full migration registry.

When the registry lands, it should:

- live inside daemon or bridge core, not gameplay script
- normalize old artifacts into the current internal representation
- emit owner-visible degraded-state warnings when only partial compatibility is possible
- keep old campaign artifacts readable with safe fallback when a migration path is unavailable
- add regression tests for old-artifact migration and partial compatibility

Until then, prefer safe fallback over silent acceptance.

Not inside gameplay script layer.

---

# Save Compatibility Rule

Campaign continuity is more important than perfect modernization.

Old campaigns should continue functioning whenever reasonably possible.

Correct behavior:

```text
old save
-> degraded compatibility if needed
-> continue campaign safely
```

Rejected behavior:

```text
old save
-> completely unusable after update
```

---

# Multiplayer Rule

Multiplayer requires consistent schema interpretation.

Host authoritative rule:
- host validates payloads
- clients do not process daemon payloads
- clients receive synchronized game state only

Bridge compatibility mismatches should:
- fallback safely
- preserve synchronization
- avoid desync risk

---

# Bridge Strictness Rule

Bridge validation should be stricter than daemon generation.

Reason:
- daemon may evolve faster
- bridge touches game runtime
- runtime safety is higher priority

The bridge should reject:
- unsafe payloads
- malformed payloads
- unsupported critical structures

while still tolerating:
- unknown optional fields
- future-compatible extensions

---

# Version Negotiation Philosophy

The system should prefer:

```text
graceful degradation
```

over:

```text
hard incompatibility
```

Strategic Nexus should continue operating with reduced capability whenever safe.

---

# Design Goal

The architecture should support years of gradual evolution without requiring:

- full redesign
- campaign reset
- hard migration break
- complete payload rewrite
- rigid preset architecture

Strategic Nexus is intended to evolve incrementally over time.
