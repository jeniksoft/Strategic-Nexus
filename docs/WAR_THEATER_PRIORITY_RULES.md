# Strategic Nexus - War Theater Priority Rules

## Core Rule

Strategic Nexus may influence war theater priorities.

Strategic Nexus must NOT directly control fleets or tactical combat.

The LLM may define:

* strategic war goals
* theater priorities
* target importance
* operational focus
* strategic pressure direction

Vanilla AI remains responsible for:

* fleet movement
* pathfinding
* combat execution
* tactical engagement
* retreat behavior
* ship combat control

Strategic Nexus provides strategic intent, not realtime battlefield control.

---

# Design Philosophy

Vanilla Stellaris AI often lacks coherent strategic war intent.

Strategic Nexus should improve:

* war focus
* theater prioritization
* strategic pressure
* target selection philosophy
* operational consistency

without replacing:

* vanilla combat systems
* tactical AI
* realtime gameplay behavior

---

# Allowed Strategic Influence

Strategic Nexus may influence:

* strategic attack direction
* defensive priorities
* chokepoint importance
* industrial target preference
* ally support focus
* crisis containment focus
* war exhaustion strategy
* territorial priority
* infrastructure targeting
* fleet concentration philosophy

---

# Forbidden Strategic Influence

Strategic Nexus must NOT:

* move fleets directly
* issue tactical orders
* micro-manage combat
* control individual ships
* force realtime retreat behavior
* override combat simulation
* perform realtime battlefield analysis
* create per-tick war AI

The project must remain strategic, not tactical.

---

# War Strategy Domains

Possible future bounded domains:

## war_strategy

Possible values:

* defensive_hold
* border_push
* chokepoint_control
* capital_strike
* economic_disruption
* claims_focus
* ally_support
* exhaustion_strategy
* crisis_containment

---

## operational_focus

Possible values:

* defend_core_worlds
* secure_shipyards
* isolate_enemy_front
* raid_outer_systems
* support_allies
* preserve_fleet_strength
* rapid_claim_capture

---

## theater_priority_profile

Possible values:

* defensive
* balanced
* aggressive
* opportunistic
* desperate_survival

---

# Target Priority Weights

Strategic Nexus may eventually generate bounded target weights.

Example:

```json
{
  "target_priorities": {
    "chokepoints": 0.9,
    "shipyards": 0.8,
    "capital_worlds": 0.4,
    "claimed_systems": 0.7,
    "ally_defense": 0.6,
    "industrial_worlds": 0.75
  }
}
```

These are strategic weights only.

The system must NOT become:

* direct tactical targeting
* fleet scripting
* deterministic combat control

---

# Capability Constraint Rule

War planning must account for:

* economy
* fleet strength
* reinforcement capability
* logistics
* recovery state
* naval capacity
* war exhaustion
* industrial sustainability

The strongest theoretical attack is not always realistic.

Civilizations should:

* retreat strategically
* consolidate
* defend infrastructure
* preserve fleets
  when necessary.

---

# Intel Constraint Rule

War planning must respect intel limitations.

Low intel:

* less specialized war planning
* more defensive caution
* more balanced priorities

High intel:

* stronger operational focus
* more accurate strategic targeting
* better theater specialization

Strategic Nexus must never use hidden information for perfect war planning.

---

# Personality Interaction

Different civilizations should wage war differently.

Examples:

## Paranoid Civilization

May prefer:

* fortress defense
* chokepoint holding
* defensive concentration

## Bold Civilization

May prefer:

* deep strikes
* aggressive offensives
* risky advances

## Opportunistic Civilization

May:

* exploit weak fronts
* abandon losing allies
* attack exhausted rivals

## Honor-Bound Civilization

May:

* prioritize ally defense
* avoid betrayal behavior
* defend federation territory

## Machine Intelligence

May:

* preserve industrial efficiency
* avoid unnecessary attrition
* maintain strategic patience

---

# War Queue Priority Rule

Empires currently at war may receive:

* higher daemon queue priority
* more frequent strategic refresh
* faster theater reevaluation

However:
updates still remain asynchronous and low-frequency.

Realtime battlefield AI remains forbidden.

---

# Performance Constraint

War systems must respect:

* Stellaris CPU limitations
* multiplayer synchronization
* daemon performance budget
* save parsing cost

Strategic Nexus must never:

* continuously analyze combat
* run realtime war simulation
* perform per-day tactical planning

Strategic updates remain limited and strategic in scale.

---

# Multiplayer Rule

In multiplayer:

* host remains authoritative
* clients do not run war reasoning
* synchronized strategic state remains canonical

No client-side war AI divergence is allowed.

---

# Future Expansion Rule

War theater systems are considered:

* advanced-stage architecture
* post-core implementation
* late roadmap functionality

Implementation order:

1. stable daemon
2. stable payload schema
3. stable strategy dimensions
4. multiplayer validation
5. memory/personality systems
6. bounded war theater priorities

War systems should NOT be implemented early.

---

# Design Goal

Strategic Nexus wars should feel like:

```text
civilizations pursuing strategic military intent
```

not:

```text
realtime tactical AI micro-management
```

The project succeeds if wars become:

* more believable
* more focused
* more thematic
* more historically coherent

while preserving:

* vanilla gameplay execution
* performance
* multiplayer stability
* bounded architecture
