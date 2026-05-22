# Strategic Nexus — Modular Strategy Dimensions

## Core Rule

Strategic Nexus should prefer modular bounded dimensions over monolithic presets.

The LLM should not choose one large rigid doctrine preset.

Instead, the LLM should select bounded values across multiple strategic domains.

This gives the LLM expressive freedom while keeping the system safe, inspectable, debuggable, and deterministic enough for multiplayer.

---

# Rejected Model

Rejected architecture:

```text
LLM selects one giant doctrine preset
→ preset controls everything
→ rigid behavior
→ limited variation
→ hard to debug
→ too many preset combinations over time
```

This eventually creates either:
- too few boring archetypes
- or hundreds of unmaintainable presets

Both are bad.

---

# Required Model

Required architecture:

```text
LLM selects bounded values in separate domains
→ domains combine into strategic behavior
→ scripted layer applies safe effects
→ vanilla AI remains execution layer
```

Example:

```json
{
  "military_posture": "defensive",
  "fleet_philosophy": "carrier",
  "border_strategy": "fortress",
  "research_bias": "strike_craft",
  "diplomatic_behavior": "paranoid",
  "expansion_policy": "cautious",
  "crisis_policy": "prepare"
}
```

This creates a rich strategic identity without allowing unbounded behavior.

---

# Why Modular Dimensions

Modular bounded dimensions allow:

- more varied empire behavior
- better LLM expressiveness
- safer payload validation
- easier debugging
- easier balancing
- clearer logs
- less preset explosion
- better multiplayer safety
- gradual doctrine drift
- empire personality expression

The LLM gets room to reason.
The bridge and mod still receive bounded enums.

---

# Bounded Vocabulary Rule

Each domain must have a small fixed vocabulary.

Good:

```text
fleet_philosophy:
- balanced
- artillery
- carrier
- missile
- torpedo
- picket
```

Bad:

```text
fleet_philosophy:
- arbitrary free text from LLM
```

No raw text strategic commands may enter the game.

---

# Suggested Strategy Domains

## Military Posture

Controls general war readiness.

Possible values:
- passive
- defensive
- balanced
- aggressive
- existential

## Fleet Philosophy

Controls high-level fleet composition preference.

Possible values:
- balanced
- artillery
- carrier
- missile
- torpedo
- picket
- swarm
- crisis_defense

## Border Strategy

Controls territorial defense style.

Possible values:
- open_border
- mobile_response
- fortress_border
- chokepoint_hardening
- deep_space_projection

## Expansion Policy

Controls colonization and conquest pressure.

Possible values:
- isolationist
- cautious
- opportunistic
- expansionist
- conquest_driven

## Diplomatic Behavior

Controls diplomatic stance.

Possible values:
- cooperative
- cautious
- manipulative
- paranoid
- hegemon_seeking
- vassalizing
- honor_bound
- treacherous

## Research Direction Bias

Controls broad research priority direction.

Possible values:
- balanced
- economy
- military_industry
- shields
- armor
- missiles
- strike_craft
- point_defense
- sensors_intel
- megastructure
- crisis_preparation

## Economic Focus

Controls broad economy priorities.

Possible values:
- balanced
- recovery
- alloy_focus
- research_focus
- unity_focus
- naval_capacity
- megastructure_focus
- fortress_economy

## Crisis Policy

Controls response to crisis-level threats.

Possible values:
- ignore
- observe
- prepare
- contain
- total_mobilization
- survival_at_all_costs

## Espionage And Intel Policy

Controls spy/intel behavior.

Possible values:
- ignore
- defensive_counterintel
- target_rivals
- target_hegemon
- gather_military_intel
- destabilize_threats

## Internal Stability Policy

Controls response to internal weakness.

Possible values:
- ignore
- economic_recovery
- defensive_consolidation
- diplomatic_shelter
- militarized_recovery
- unity_stabilization

---

# Domain Change Speed

Not all domains should change at the same speed.

Fast-changing domains:
- military_posture
- border_strategy
- crisis_policy

Medium-changing domains:
- fleet_philosophy
- research_bias
- economic_focus
- espionage_policy

Slow-changing domains:
- diplomatic_behavior
- expansion_policy
- internal_civilization_style
- personality-derived behavior

Civilization identity must change slowly.

Operational posture may change faster.

---

# Personality Integration

Personality modifies domain choices.

Example:

```json
{
  "base_personality": "paranoid",
  "military_posture": "defensive",
  "border_strategy": "fortress_border",
  "diplomatic_behavior": "cautious",
  "espionage_policy": "defensive_counterintel"
}
```

Same fleet style may still produce different civilization behavior when combined with other dimensions.

---

# Payload Example

Example bounded modular payload:

```json
{
  "schema_version": 1,
  "campaign_id": "campaign_hash_or_uuid",
  "empire_id": 12,
  "sequence_id": 81,

  "strategy_dimensions": {
    "military_posture": "defensive",
    "fleet_philosophy": "carrier",
    "border_strategy": "fortress_border",
    "expansion_policy": "cautious",
    "diplomatic_behavior": "paranoid",
    "research_bias": "strike_craft",
    "economic_focus": "alloy_focus",
    "crisis_policy": "prepare",
    "espionage_policy": "gather_military_intel"
  },

  "confidence": 0.68,
  "fallback_required": false
}
```

---

# Validation Rule

Each domain value must be validated against a known enum.

Invalid domain value:
→ ignore that domain or fallback that domain

Invalid entire payload:
→ preserve previous valid strategy if possible

Never execute arbitrary strategy text.

---

# Partial Update Rule

A payload may update only some domains.

Example:

```json
{
  "strategy_dimensions": {
    "military_posture": "existential",
    "crisis_policy": "total_mobilization"
  }
}
```

Domains not present should keep previous valid values.

This allows gradual strategic adjustment.

---

# Doctrine Inertia Rule

Modular dimensions still require inertia.

The LLM must not flip every dimension constantly.

Each domain should have:
- current value
- previous value
- confidence
- intensity
- age
- change pressure
- inertia/cooldown

This prevents optimizer-like behavior.

---

# Scripted Layer Mapping

The scripted layer should translate modular dimensions into safe game behavior.

Examples:

- `military_posture = defensive`
  → increase defensive doctrine weights

- `fleet_philosophy = carrier`
  → increase carrier-style fleet preference

- `research_bias = strike_craft`
  → increase strike craft research direction

- `border_strategy = fortress_border`
  → increase starbase/chokepoint priorities

- `diplomatic_behavior = paranoid`
  → increase distrust and defensive pact preference

The LLM does not directly manipulate fleets, ships, economy, or diplomacy.

It only selects bounded strategic direction.

---

# Anti-Cheat Principle

Modular strategy must still respect empire knowledge.

The LLM may choose dimensions only from:
- known information
- inferred information
- remembered history
- legitimate intel
- empire personality

It must not use:
- hidden save truth
- unknown empire data
- invisible fleet loadouts
- perfect counter information

---

# Design Goal

Strategic Nexus should feel like many distinct civilizations making layered strategic choices.

Not like:
- one global doctrine picker
- a perfect optimizer
- a rigid preset system
- an omniscient save solver

The LLM should have expressive freedom inside bounded modular domains.
