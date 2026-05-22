# Save Data Philosophy

## Core Principle

The LLM should NEVER receive full raw save data.

Instead:
- strategic abstraction
- compressed summaries
- historical significance
- geopolitical context

must be extracted.

---

## Correct Approach

Good:

{
  "player": {
    "fleet_power": 920000,
    "technology_rank": 1,
    "alliance_members": 0
  }
}

Bad:
- entire save dumps
- excessive tactical details
- planet-by-planet spam
- unnecessary numeric noise

---

## Important Data Types

### Strategic Power
- fleet strength
- economic ranking
- technology ranking
- diplomatic influence

### Personality Data
- civilization archetype
- historical behavior
- adaptive state

### Historical Events
- betrayals
- major wars
- crisis participation
- federations
- genocides
- megastructures

### Galactic Context
- crisis progression
- hegemonic threats
- alliance blocs
- instability level

---

## LLM Responsibility

The LLM performs:
- interpretation
- strategic abstraction
- doctrine selection
- geopolitical reasoning

The LLM does NOT:
- micro-manage fleets
- optimize build orders
- control tactics
- manipulate game memory