# Strategic Nexus - LLM Decision Contract

## Core Rule

The LLM is a strategic reasoning layer.

The LLM must never directly control Stellaris.

The LLM must never output executable game commands.

The LLM must only output bounded, validated, structured strategic recommendations.

---

# Role Of The LLM

The LLM exists to interpret strategic context.

It should reason about:
- empire personality
- known threats
- incomplete intel
- historical memory
- diplomatic context
- military posture
- strategic opportunities
- strategic risks
- long-term doctrine direction

It should not act as:
- a tactical controller
- a fleet micro AI
- a ship designer optimizer
- a tech tree solver
- a direct game command generator
- an omniscient save-file analyzer

---

# Required Output Format

The LLM must return structured JSON only.

No markdown.
No prose outside JSON.
No executable script.
No debug-interface command text.
No Stellaris effect text.
No raw mod script.

Example:

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
  "fallback_required": false,

  "evidence": [
    "recent_border_war_loss",
    "observed_enemy_carrier_usage",
    "high_military_intel"
  ],

  "uncertainty": [
    "enemy_exact_ship_design_unknown",
    "economy_estimate_low_confidence"
  ],

  "personality_influence": [
    "paranoid",
    "risk_averse"
  ]
}
```

---

# Forbidden Output

The LLM must never output:

- arbitrary command strings
- Stellaris debug-interface commands
- scripted effects
- event calls
- fleet targets
- planet build orders
- ship component layouts
- save fragments
- executable code
- raw hidden reasoning logs as gameplay payload
- instructions to modify memory
- instructions to edit saves

Forbidden example:

```json
{
  "command": "effect set_country_flag = strategic_nexus_attack_player"
}
```

Allowed example:

```json
{
  "strategy_dimensions": {
    "military_posture": "aggressive",
    "border_strategy": "mobile_response"
  }
}
```

---

# Bounded Decision Space

The LLM must choose only from known bounded enum values.

Unknown values must be rejected or ignored by the bridge.

The LLM may reason freely internally, but its output must be bounded.

Allowed:

```json
"fleet_philosophy": "carrier"
```

Forbidden:

```json
"fleet_philosophy": "invent a new mixed plasma carrier doomstack strategy"
```

---

# Evidence Requirement

Every major strategic recommendation should include evidence.

Evidence should come from:
- known intel
- observed behavior
- historical memory
- timeline events
- diplomacy
- sensor-visible state
- empire personality
- prior doctrine success or failure

Evidence must not come from:
- hidden save truth
- unknown empires
- invisible ship loadouts
- unseen economy details
- omniscient galaxy state

---

# Uncertainty Requirement

The LLM must explicitly represent uncertainty.

If information is incomplete, the LLM should say so in the structured output.

Example:

```json
"uncertainty": [
  "enemy_fleet_loadout_not_fully_known",
  "distant_hegemon_strength_estimate_uncertain"
]
```

Uncertainty should reduce confidence and prevent extreme pivots.

---

# Confidence Requirement

Every output must include confidence.

Suggested interpretation:

```text
0.00-0.30 = weak confidence
0.31-0.60 = moderate confidence
0.61-0.80 = strong confidence
0.81-1.00 = very strong confidence
```

Low confidence should cause:
- smaller changes
- partial updates
- conservative strategy
- preservation of previous doctrine

High confidence may allow:
- stronger doctrine adjustment
- higher intensity
- faster strategic shift

Confidence must not bypass doctrine inertia rules.

---

# Fallback Requirement

The LLM may request fallback.

Fallback should be used when:
- context is insufficient
- intel is too weak
- save data is inconsistent
- empire state is unsupported
- no safe recommendation exists
- schema compatibility is uncertain

Example:

```json
{
  "fallback_required": true,
  "fallback_reason": "insufficient_intel"
}
```

Fallback is safe behavior, not failure.

---

# Empire Perspective Rule

The LLM must reason only from the perspective of the target empire.

The LLM must not know:
- all empires in the galaxy
- hidden player status
- unknown empires
- hidden fleet positions
- hidden technologies
- hidden economy details

The LLM receives an EmpireScopedKnowledge snapshot and must treat it as the full known world of that empire.

---

# No Human Targeting Rule

The LLM must not be told which empire is controlled by a human player unless the game itself exposes that knowledge as normal gameplay state.

The LLM may react to:
- military strength
- expansion speed
- threat level
- diplomatic behavior
- war history
- known aggression
- hegemonic growth

It must not react to:
- "this is the human"
- "focus the player"
- "anti-player coalition"

The player is just another empire.

---

# Personality Influence

The LLM must apply empire personality as a strategic filter.

Personality should influence:
- risk tolerance
- betrayal willingness
- trust
- paranoia
- doctrine inertia
- reaction to defeat
- reaction to weakness
- diplomatic behavior
- military aggression

The same facts may produce different decisions for different personalities.

Example:

```text
Paranoid empire sees superior neighbor
-> fortress_border + defensive posture

Bold empire sees superior neighbor
-> opportunistic expansion or preemptive militarization
```

---

# Strategic Memory Influence

The LLM must use historical memory when available.

Memory may include:
- wars
- betrayals
- humiliations
- repeated defeats
- crisis trauma
- old alliances
- successful doctrines
- failed doctrines
- timeline events

Memory must be empire-scoped.

Memory must not become global hidden knowledge.

---

# No Perfect Optimization Rule

The LLM must not always choose the mathematically best answer.

It should produce believable civilization behavior.

Allowed:
- imperfect inference
- delayed adaptation
- ideological bias
- fear
- overconfidence
- underestimation
- trauma-based overreaction

Forbidden:
- instant perfect counters
- solver-like behavior
- exact hidden information exploitation
- universally optimal strategy

---

# Empire Capability Constraint Rule

Strategy dimensions must be evaluated through empire capability constraints.

The LLM must not choose strategy only because it is theoretically optimal.

The LLM must evaluate whether the empire can realistically:
- finance the strategy
- transition into the strategy
- technologically support the strategy
- politically sustain the strategy
- industrially execute the strategy
- survive the transition period

Strategic recommendations must account for:
- economy
- ethics
- civics
- government type
- species traits
- ascension path
- industrial capacity
- naval infrastructure
- strategic resources
- unity generation
- technology level
- empire size
- stability
- logistics
- current wars
- recovery state

---

# Strategy Feasibility Principle

A strategy may be:
- theoretically powerful
- but economically unrealistic
- too slow to implement
- politically unstable
- technologically unreachable
- incompatible with empire identity

The LLM must prefer strategies that are realistically executable by the current civilization.

---

# Transition Cost Rule

Changing strategic dimensions has cost.

The LLM must consider:
- infrastructure conversion cost
- alloy cost
- research cost
- transition time
- fleet replacement time
- economic disruption
- doctrine retraining
- strategic vulnerability during transition

The LLM should avoid unrealistic rapid pivots unless:
- existential threat exists
- collapse is imminent
- crisis state is active

---

# Empire Identity Constraint

Different empire archetypes should naturally prefer different strategic dimensions.

Examples:

Spiritualist empires may prefer:
- unity stability
- defensive doctrine
- slower technological transitions

Megacorps may prefer:
- economic leverage
- trade protection
- proxy influence
- specialized fleets

Hive Minds may prefer:
- aggressive recovery
- swarm warfare
- rapid replacement doctrine

Machine Intelligences may prefer:
- long-term industrial scaling
- megastructure focus
- strategic patience

The same strategic situation may produce different valid strategies for different civilizations.

---

# Economic Sustainability Rule

A strategy is not valid if the empire cannot sustain it.

Example:

```text
carrier_focus
```

may require:
- advanced technology
- alloy surplus
- strike craft infrastructure
- strong economy
- long production cycles

While:

```text
missile_swarm
```

may be:
- cheaper
- faster
- easier to mass produce
- suitable for emergency defense

The LLM must evaluate:
- short-term survivability
- medium-term sustainability
- long-term viability

---

# Time-To-Effect Rule

Some strategies produce results quickly.
Others require decades.

The LLM must account for:
- implementation speed
- urgency
- strategic timing
- crisis pressure
- economic recovery time

Empires under immediate threat may require:
- fast deployable doctrine
- emergency militarization
- defensive fallback strategy

Even if long-term alternatives are theoretically superior.

---

# Anti-Optimizer Principle

Strategic Nexus must not behave like a perfect strategy solver.

The LLM should produce believable civilization behavior constrained by:
- economics
- institutions
- ideology
- logistics
- historical momentum
- industrial reality
- technological maturity

The strongest theoretical strategy is not always the correct strategic choice for a specific civilization.

---

# Partial Update Rule

The LLM may update only some strategy dimensions.

Missing dimensions should usually preserve previous valid values.

Example:

```json
{
  "strategy_dimensions": {
    "military_posture": "existential",
    "crisis_policy": "total_mobilization"
  }
}
```

This allows gradual strategic drift.

---

# Auditability Requirement

Every LLM decision must be auditable.

The system should store:
- input snapshot id
- campaign id
- empire id
- output payload
- confidence
- evidence
- uncertainty
- fallback reason if any
- schema version
- timestamp/source save year

Do not store private chain-of-thought.

Store concise strategic explanation fields only.

---

# Bridge Responsibility

The bridge must not trust the LLM.

The bridge must validate:
- schema_version
- campaign_id
- empire_id
- sequence_id
- enum values
- numeric ranges
- fallback_required
- payload lifecycle state

Invalid output must not reach gameplay logic.

---

# Scripted Layer Responsibility

The scripted layer should only receive bounded state.

It should never receive:
- raw LLM text
- command strings
- arbitrary doctrine names
- unvalidated values

The scripted layer maps safe state to safe Stellaris behavior.

---

# Multiplayer Rule

In multiplayer:

- LLM runs only on host
- clients never run LLM
- clients never process LLM payloads
- clients receive synchronized game state only

This prevents divergent reasoning and desync.

---

# Design Goal

The LLM should make each empire feel like:

```text
a civilization with memory, personality, imperfect intel, and strategic culture
```

not like:

```text
a perfect optimizer reading the save file
```

Strategic Nexus exists because LLMs can interpret context, uncertainty, history, and personality.

That potential must be used through bounded structured decisions, not unrestricted commands.
