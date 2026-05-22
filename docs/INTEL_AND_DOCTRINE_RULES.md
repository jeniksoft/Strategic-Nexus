# Strategic Nexus — Intel And Doctrine Rules

## Core Principle

Strategic Nexus must never behave as an omniscient AI.

The system may only reason about:
- observed information
- inferred information
- remembered historical information

The system must never use hidden save data as perfect knowledge.

The goal is:
“civilizations making strategic decisions from imperfect information”
not:
“AI reading the entire save as a cheating oracle.”

---

# Knowledge Model

Strategic Nexus reasoning must operate from the perspective of a specific empire.

The save parser must eventually support:

```text
save data
→ empire knowledge filtering
→ empire strategic snapshot
→ doctrine reasoning
```

The correct architecture is NOT:

```text
save parser
→ full galaxy truth
→ perfect counters
```

The correct architecture IS:

```text
save parser
→ filtered empire perspective
→ bounded strategic reasoning
```

---

# Allowed Information Sources

## Observed Information

Strategic Nexus may reason about information that the empire could reasonably know through normal gameplay systems.

Examples:

- military intel
- espionage
- diplomatic visibility
- visible fleets
- sensor range observations
- known technologies
- known diplomatic relations
- recent wars
- recent fleet engagements
- galactic resolutions
- crisis events
- federation behavior
- rival declarations
- public fleet power
- observed starbase composition
- known ascension paths if revealed
- observed strike craft usage
- observed missile usage
- observed shield or armor bias if visible in combat

---

## Inferred Information

Strategic Nexus may create uncertain strategic conclusions from historical or observed behavior.

Examples:

- “this empire prefers carrier warfare”
- “this empire favors defensive wars”
- “this empire is economically dominant”
- “this empire rushes alloy production”
- “this empire is likely preparing for war”
- “this empire appears technologically advanced”
- “this empire adapts rapidly after defeat”
- “this empire is opportunistic”
- “this empire behaves aggressively toward weaker neighbors”

Inference is allowed because human players also infer behavior from patterns.

---

## Historical Memory

Strategic Nexus may remember:
- past wars
- betrayal
- repeated aggression
- successful doctrines
- failed doctrines
- federation history
- crisis preparation
- military humiliation
- border conflicts
- ideological conflicts

This memory is empire-perspective memory, not objective galaxy truth.

---

# Forbidden Information Usage

Strategic Nexus must NEVER use:

- hidden fleet compositions without intel
- hidden ship loadouts without intel
- hidden technologies without intel
- hidden economy details without intel
- hidden fleet positions outside observation
- direct save truth for perfect counters
- exact player build optimization from save parsing
- instant reaction to hidden changes
- “god view” reasoning

Forbidden example:

```text
Player switched to plasma weapons.
AI instantly hard-counters before observing this.
```

Allowed example:

```text
Empire repeatedly lost armor-heavy battles against the player.
Empire gradually increases anti-armor doctrine weighting.
```

---

# Fleet Doctrine Philosophy

Strategic Nexus may influence:
- fleet doctrine
- strategic composition direction
- military posture
- doctrine weighting

Strategic Nexus must NOT micromanage:
- exact component layouts
- exact slot usage
- exact ship designer optimization
- tactical combat targeting

---

# Allowed Fleet Doctrine Presets

Examples:

- Carrier Doctrine
- Missile Doctrine
- Torpedo Doctrine
- Artillery Doctrine
- Defensive Picket Doctrine
- Balanced Fleet Doctrine
- Crisis Defense Doctrine
- Raider Doctrine
- Fortress Doctrine

The LLM selects strategic doctrine direction.
The scripted layer translates doctrine into predefined bounded presets.

---

# Research Direction Philosophy

Strategic Nexus may influence:
- strategic research direction
- technology category weighting
- doctrine-aligned priorities

Examples:

- prioritize strike craft technologies
- prioritize missile technologies
- prioritize shield technologies
- prioritize armor technologies
- prioritize starbase defense
- prioritize economic scaling
- prioritize crisis preparation

Strategic Nexus must NOT:
- perfectly optimize tech trees
- directly choose hidden hard counters
- instantly react to unseen enemy changes

---

# Strategic Reasoning Contract

All strategic doctrine decisions should eventually support:

- confidence
- evidence sources
- uncertainty
- fallback behavior

Example:

```json
{
  "fleet_doctrine": "carrier_counter",
  "confidence": 0.63,
  "evidence": [
    "recent_battle_losses",
    "observed_enemy_hangar_usage",
    "high_military_intel"
  ],
  "fallback_required": false
}
```

Fallback example:

```json
{
  "fleet_doctrine": "balanced",
  "confidence": 0.18,
  "fallback_required": true
}
```

---

# Anti-Cheat Principle

Strategic Nexus must feel:
- adaptive
- intelligent
- historical
- emotional
- political

But never:
- omniscient
- perfect
- impossible to deceive

The player must feel:
“the AI learned about me”
not:
“the AI illegally read my save.”
