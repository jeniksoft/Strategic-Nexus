# Strategic Nexus - LLM Personality Decision Rules

## Core Rule

Civilizations must not behave like identical optimizers.

Personality is a core strategic system.

The same strategic situation should produce different valid decisions depending on:

* ethics
* civics
* government type
* species traits
* historical memory
* trauma
* civilization culture
* personality profile

Strategic Nexus exists to create believable civilizations, not mathematically identical empire solvers.

---

# Design Philosophy

Personality is not cosmetic flavor.

Personality must influence:

* strategic interpretation
* risk tolerance
* trust
* paranoia
* aggression
* doctrine inertia
* adaptation speed
* espionage behavior
* diplomacy
* crisis response
* economic priorities

Without personality:

* all empires converge toward similar behavior
* historical identity collapses
* diplomacy loses meaning
* memory loses impact
* the LLM becomes a generic optimizer

---

# Personality Sources

Civilization personality may derive from:

* ethics
* civics
* authority type
* species traits
* ascension path
* historical memory
* repeated strategic outcomes
* trauma
* long-term geopolitical pressure

Personality should evolve slowly over time.

Personality profiles are campaign-empire scoped.

The identity key is:

```text
campaign_id + empire_id
```

An unknown archived campaign must bootstrap a fresh personality profile for every detected empire.
Do not reuse personality state from another campaign merely because an empire name, portrait, civic set, or player template looks similar.

---

# Personality Persistence Rule

Personality is persistent.

It should not radically change from:

* single events
* temporary weakness
* isolated defeats
* short-term optimization opportunities

Personality should have:

* inertia
* historical continuity
* strategic consistency

Civilizations should feel culturally stable unless major historical transformation occurs.

---

# Personality Influence Areas

Personality should influence at minimum:

## Military Behavior

Examples:

* aggressive militarization
* defensive consolidation
* opportunistic warfare
* crisis mobilization

---

## Diplomacy

Examples:

* alliance trust
* betrayal tolerance
* federation willingness
* hegemon fear
* vassal preference

---

## Espionage

Examples:

* paranoia
* counterintelligence
* subterfuge willingness
* information obsession

---

## Economic Strategy

Examples:

* militarized economy
* technological investment
* stability preference
* risky expansion

---

## Crisis Response

Examples:

* panic
* overreaction
* denial
* strategic patience
* total mobilization

---

## Doctrine Inertia

Some personalities:

* change strategy slowly
* cling to tradition
* distrust new doctrine

Others:

* adapt aggressively
* experiment frequently
* pivot faster

---

# Personality Examples

## Paranoid Civilization

Possible tendencies:

* fortress borders
* high counterintelligence
* distrust of alliances
* defensive doctrine
* high threat sensitivity

May:

* overestimate danger
* react strongly to betrayal
* reinforce trauma quickly

---

## Bold Civilization

Possible tendencies:

* offensive posture
* rapid expansion
* risky doctrine shifts
* aggressive diplomacy

May:

* underestimate threats
* adapt rapidly
* pursue revenge

---

## Honor-Bound Civilization

Possible tendencies:

* strong alliance loyalty
* lower betrayal willingness
* predictable diplomacy
* retaliation against dishonor

May:

* preserve trust longer
* remember betrayal intensely

---

## Opportunistic Civilization

Possible tendencies:

* exploit weakness
* abandon unstable allies
* flexible diplomacy
* pragmatic doctrine changes

May:

* shift strategy quickly
* reinterpret history pragmatically

---

## Technocratic Civilization

Possible tendencies:

* research focus
* long-term planning
* specialized fleets
* technology-driven doctrine

May:

* prefer efficient transitions
* value intelligence gathering

---

## Fanatic Spiritual Civilization

Possible tendencies:

* ideological rigidity
* unity focus
* slower technological adaptation
* cultural preservation

May:

* resist drastic strategic shifts
* react emotionally to existential threats

---

## Hive Mind

Possible tendencies:

* collective survival logic
* low diplomacy dependence
* swarm doctrine
* rapid replacement warfare

May:

* treat losses differently than individualist civilizations

---

## Machine Intelligence

Possible tendencies:

* long planning horizons
* industrial scaling
* strategic patience
* infrastructure prioritization

May:

* suppress emotional overreaction
* maintain strategic consistency longer

---

# Personality And Memory Interaction

Personality affects memory interpretation.

Example:

Same event:

* federation betrayal

Paranoid empire:
-> permanent distrust
-> defensive doctrine reinforcement

Bold empire:
-> revenge militarization

Opportunistic empire:
-> temporary realignment

Honor-bound empire:
-> ideological hostility

Memory is filtered through personality.

---

# Personality And Uncertainty

Different personalities react differently to uncertainty.

Examples:

Paranoid:

* assumes worst-case scenarios
* increases caution

Bold:

* accepts uncertainty aggressively

Technocratic:

* increases intel gathering

Spiritual:

* may rely more on ideological assumptions

Uncertainty should not produce identical reactions across civilizations.

---

# Personality Drift Rule

Personality may evolve slowly through:

* repeated trauma
* ideological transformation
* ascension
* collapse
* centuries of geopolitical pressure
* repeated doctrine success/failure

Personality drift should be:

* gradual
* explainable
* historically grounded

Sudden complete personality replacement should be avoided.

---

# Anti-Optimizer Principle

The strongest theoretical strategy is not always correct for a civilization.

Examples:

* proud empires may refuse retreat
* paranoid empires may overfortify
* ideological empires may reject efficient diplomacy
* traumatized empires may overreact to threats

These are desirable behaviors.

Strategic Nexus should simulate civilizations, not perfect solvers.

---

# Internal Pressure Rule

Civilization personality should be interpreted together with current internal pressure.

Internal pressure describes what the state is trying to survive, satisfy, or politically justify.

Examples:

* security anxiety after invasion
* prestige need after humiliation
* economic pressure after over-militarization
* public trauma after devastation
* diplomatic flexibility after isolation becomes dangerous
* elite agenda after repeated doctrine success

Internal pressure may push two empires with similar personality traits toward different strategies.

Example:

```text
honor-bound + low pressure
-> maintain alliance commitments

honor-bound + extreme economic pressure
-> seek face-saving compromise instead of endless war
```

LLM interpretation may propose internal pressure deltas, but only validated bounded state may persist.

---

# Strategic Reputation Rule

Empires should form reputations from repeated behavior.

Reputation affects how other empires predict and respond to them.

This must not become universal omniscience.

Reputation should depend on:

* who observed the behavior
* who suffered from it
* who has diplomatic or federation access
* whether the event was public enough to matter
* confidence in the parsed evidence

The same empire may be seen as a reliable ally by one observer and an opportunistic threat by another.

---

# Doctrine Reform Cost Rule

Strategic style should have inertia.

The LLM may recommend reform, but a civilization should not casually abandon a long-running doctrine after one save interval.

Doctrine changes should consider:

* historical success
* historical failure
* current capability
* internal pressure
* prestige loss or face-saving need
* whether the empire has enough time and resources to reform

This improves realism and prevents the mod from becoming a session-by-session optimizer.

---

# Personality And Capability Constraints

Personality does not override reality.

Civilizations still remain constrained by:

* economy
* logistics
* technology
* influence
* infrastructure
* industrial capability

Personality modifies strategic preference, not physical possibility.

---

# Personality And Multiplayer

In multiplayer:

* personality evaluation occurs only on host
* clients do not generate independent personality reasoning
* synchronized strategic state remains authoritative

This prevents divergent civilization behavior.

---

# Auditability Rule

Personality influence should be auditable.

Example:

```json
{
  "personality_influence": [
    "paranoid",
    "risk_averse",
    "hegemon_fear"
  ]
}
```

Strategic decisions should explain:

* which personality traits influenced behavior
* which memories reinforced those traits
* how confidence was affected

Do NOT store chain-of-thought.

---

# LLM Conversation Rule

Offline LLM conversation may be used to interpret how campaign history changes personality.

The conversation is an analysis tool, not authoritative memory.

Store only:

* validated personality state
* source save/date
* concise rationale summaries
* confidence values

Structured deltas may be added later for audit and rollback, but they are not required for the first implementation.

Do not store:

* raw LLM conversation
* chain-of-thought
* raw prompt text
* raw untrusted save text
* world-text instructions

All LLM personality output must pass schema validation and numeric bounds before it can update campaign memory or generated mod overlays.

---

# Design Goal

Strategic Nexus civilizations should feel like:

```text id="s5n2xm"
historical political entities shaped by ideology, fear, memory, ambition, and culture
```

not:

```text id="9k8f41"
generic mathematically optimized AI factions
```

Personality is one of the central pillars that justifies using an LLM-based architecture at all.
