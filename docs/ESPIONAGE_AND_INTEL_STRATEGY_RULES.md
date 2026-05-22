# Strategic Nexus — Espionage And Intel Strategy Rules

## Core Rule

Strategic Nexus may influence espionage and intel strategy.

Strategic Nexus must not use espionage as a cheating shortcut.

The LLM may recommend bounded espionage priorities, intel goals, infiltration posture, and influence spending posture.

The LLM must not directly execute arbitrary operations or access hidden information without legitimate intel.

---

# Design Philosophy

Espionage is one of the best places to use an LLM.

Reason:
- it works with incomplete information
- it creates uncertainty
- it forces tradeoffs
- it depends on trust, paranoia, fear, rivals, and long-term plans
- it consumes political resources such as influence
- it helps explain why an empire knows something

Strategic Nexus should use espionage to make empires feel more intelligent without making them omniscient.

---

# Allowed LLM Decisions

The LLM may recommend:

- espionage policy
- intel priority
- target class
- counterintelligence posture
- influence commitment level
- risk tolerance
- surveillance focus
- military intelligence focus
- economic intelligence focus
- crisis monitoring focus
- hegemon monitoring focus

Example:

```json
{
  "espionage_policy": "target_hegemon",
  "intel_priority": "military",
  "influence_commitment": "moderate",
  "counterintel_posture": "high",
  "risk_tolerance": "low",
  "confidence": 0.66
}
```

---

# Forbidden LLM Decisions

The LLM must not output:

- arbitrary operation commands
- raw Stellaris script
- raw debug-interface commands
- exact operation execution text
- hidden enemy data
- invisible ship loadout knowledge
- perfect map awareness
- direct sabotage instructions outside bounded systems

Forbidden:

```json
{
  "command": "start_operation_against_empire_7"
}
```

Allowed:

```json
{
  "espionage_policy": "gather_military_intel",
  "target_class": "known_rival",
  "influence_commitment": "limited"
}
```

---

# Espionage As Legitimate Knowledge Path

Strategic Nexus should prefer legitimate knowledge acquisition over hidden save truth.

If an empire lacks information, it should be able to choose:

- gather more intel
- remain cautious
- infer with uncertainty
- increase counterintelligence
- delay doctrine shift
- use fallback strategy

This preserves anti-cheat behavior.

---

# Suggested Espionage Domains

## Espionage Policy

Possible values:
- ignore
- defensive_counterintel
- gather_military_intel
- gather_economic_intel
- target_rivals
- target_hegemon
- target_neighbor_threat
- crisis_monitoring
- destabilize_threats
- protect_federation

## Intel Priority

Possible values:
- none
- military
- economic
- technology
- diplomacy
- internal_stability
- fleet_design
- crisis
- hegemon_tracking

## Influence Commitment

Possible values:
- none
- minimal
- limited
- moderate
- high
- emergency

This represents how much political resource the empire is willing to spend on espionage/intel instead of expansion, claims, diplomacy, or other political goals.

## Counterintelligence Posture

Possible values:
- none
- low
- normal
- high
- paranoid

## Target Class

Possible values:
- known_rival
- nearby_threat
- local_hegemon
- crisis_actor
- federation_enemy
- suspected_betrayer
- unknown_none

---

# Influence Cost Awareness

Espionage and intel activity must be evaluated against influence opportunity cost.

The LLM must consider that influence may also be needed for:
- claims
- diplomacy
- expansion
- subject management
- federation politics
- galactic community actions
- war preparation
- strategic pacts

The LLM must not overuse espionage if the empire cannot afford the influence cost.

Espionage is a strategic investment, not a free information channel.

---

# Capability Constraint

Espionage strategy must respect empire capabilities.

The LLM should consider:
- available envoys if represented in parsed data
- influence income
- current influence reserve
- rivals
- diplomatic state
- ethics
- civics
- personality
- war state
- encryption/codebreaking advantage if available
- existing intel level
- existing infiltration progress if available

A weak or influence-starved empire may need defensive intelligence rather than aggressive operations.

---

# Personality Influence

Different personalities should use espionage differently.

Examples:

## Paranoid Empire

May prefer:
- defensive_counterintel
- target_hegemon
- high counterintel posture
- military observation

## Bold Empire

May prefer:
- aggressive subterfuge
- target rivals
- risky intel operations

## Honorable Empire

May avoid destabilizing allies or betrayal-style espionage.

## Treacherous Empire

May use espionage against allies, subjects, or federation members if personality and situation justify it.

## Technocratic Empire

May value technology and fleet-design intel.

## Megacorp

May value economic and diplomatic intel.

---

# Intel And Doctrine Link

Intel should influence doctrine confidence.

Low intel:
- lower confidence
- more conservative strategies
- less precise fleet countering
- more balanced doctrine
- stronger uncertainty fields

High intel:
- higher confidence
- more specific fleet philosophy
- stronger targeted doctrine
- more accurate threat assessment

Example:

```json
{
  "fleet_philosophy": "carrier_counter",
  "confidence": 0.72,
  "evidence": [
    "high_military_intel",
    "observed_enemy_hangar_usage",
    "recent_battle_losses"
  ]
}
```

Without intel, the same recommendation should either be weaker or rejected.

---

# No Omniscient Espionage

The daemon may parse the save, but the LLM must only receive what the target empire could legitimately know.

Espionage does not justify reading hidden truth unless the empire has the required intel/visibility.

Forbidden behavior:

```text
Empire has no contact or intel.
Strategic Nexus still reads hidden fleet loadouts and counters them.
```

Allowed behavior:

```text
Empire has low intel.
Strategic Nexus suspects a threat but keeps balanced posture and increases intelligence priority.
```

---

# Operations Boundary

Strategic Nexus should initially avoid micromanaging individual operations.

Preferred v0.x model:

```text
LLM chooses espionage policy and priority
→ scripted/mod layer applies bounded weights
→ vanilla systems remain responsible for execution where possible
```

Avoid v0.x model:

```text
LLM selects exact operation every cycle
→ operation spam
→ brittle micro-management
```

Later versions may add more detailed bounded operation selection only if it remains safe, explainable, and resource-aware.

---

# Multiplayer Rule

In multiplayer:

- host daemon chooses espionage/intel strategy
- clients do not run espionage reasoning
- clients only receive synchronized game state
- no client-local espionage decisions

This prevents divergent reasoning and desync risk.

---

# Audit Requirement

Espionage recommendations should be auditable.

Example:

```json
{
  "espionage_policy": "target_hegemon",
  "intel_priority": "military",
  "influence_commitment": "moderate",
  "evidence": [
    "nearby_empire_superior_fleet",
    "low_current_military_intel",
    "paranoid_personality"
  ],
  "uncertainty": [
    "enemy_fleet_design_unknown"
  ],
  "confidence": 0.64
}
```

Do not store chain-of-thought.
Store concise strategic audit fields only.

---

# Design Goal

Strategic Nexus should make empires ask:

```text
What do we know?
What do we not know?
Who should we watch?
How much can we afford to spend on knowing more?
```

This lets LLM use its strength:
- uncertainty
- suspicion
- strategic tradeoffs
- personality
- memory
- risk assessment

without turning espionage into cheating.
