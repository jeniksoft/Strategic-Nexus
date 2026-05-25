# Strategic Nexus - Attention And Review Cadence Rules

## Core Rule

Civilizations do not continuously think about everything at once.

Strategic Nexus must simulate limited strategic attention.

In the revised offline companion architecture, review cadence means offline analysis priority over archived saves and generated-overlay refresh planning.
It does not mean live LLM review during active gameplay.

The daemon must decide:

* what currently matters
* what deserves reevaluation
* what may be ignored temporarily
* what may decay into the background

The LLM should receive an active strategic attention window, not the entire civilization state every cycle.

---

# Design Philosophy

Human decision-making is selective.

Civilizations:

* forget
* deprioritize
* ignore distant issues
* focus on immediate threats
* react to recent crises
* overlook low-relevance information

This is:

* realistic
* strategically interesting
* computationally necessary

Strategic Nexus should simulate selective strategic attention, not omnipresent perfect awareness.

---

# Attention Budget Principle

Every civilization has limited:

* attention
* processing focus
* strategic bandwidth
* context budget

The daemon should prioritize:

* urgent topics
* recent events
* nearby threats
* active wars
* economic crises
* strategic transitions

and deprioritize:

* inactive distant empires
* ancient irrelevant events
* low-threat regions
* stale diplomatic relationships

---

# Review Cadence Rule

Different systems should update at different speeds.

Strategic systems are not equally urgent.

---

## War And Crisis Review

Highest priority.

Review frequency:

* every new save if actively at war
* highest queue priority

Focus:

* strategic fronts
* theater priorities
* defense pressure
* fleet survival
* chokepoints
* crisis containment

Context should remain:

* narrow
* current
* tactical-summary focused

Realtime combat control remains forbidden.

---

## Military Doctrine Review

Medium-high priority.

Review frequency:

* every few years
* after major wars
* after catastrophic defeat
* after major military transition

Focus:

* doctrine effectiveness
* fleet philosophy
* strategic weaknesses
* military adaptation

---

## Economic And Research Review

Medium priority.

Review frequency:

* periodic
* after major collapse
* after major expansion
* after strategic transition

Focus:

* sustainability
* industrial capability
* technology direction
* infrastructure readiness

---

## Diplomacy Review

Medium-low priority.

Review frequency:

* after alliances
* after betrayal
* after federation changes
* after wars
* after major geopolitical shifts

Focus:

* trust
* rivalries
* alliances
* hegemon balance
* diplomatic opportunity

---

## Relationship And Emotional Review

Low priority.

Review frequency:

* infrequent
* after meaningful interaction
* after major trauma
* after long-term geopolitical shifts

Focus:

* fear
* trust
* rivalry
* ideological hostility
* strategic respect

Relationship state should evolve slowly.

---

## Personality Review

Very low priority.

Review frequency:

* rare
* after civilization-defining events
* after ascension
* after catastrophic trauma
* after centuries of geopolitical pressure

Personality should remain highly stable.

---

# Attention Trigger Rule

Some events may temporarily increase attention priority.

Examples:

* border invasion
* crisis appearance
* betrayal
* federation collapse
* economic crash
* fleet destruction
* hegemon expansion
* sudden technological leap

This may:

* move topics into active attention
* increase review frequency
* increase strategic pressure

---

# Context Activation Rule

Information does not need to remain permanently active to exist.

Civilizations may:

* know something historically
* but not actively consider it currently

This is realistic behavior.

The daemon may:

* reactivate dormant memories
* resurface old rivalries
* reconnect historical trauma
  when new events justify it.

---

# Forgetting And Deprioritization Rule

Without reinforcement:

* memories fade
* rivalries weaken
* fear softens
* distant threats lose attention
* old diplomatic issues become secondary

This is desirable.

Strategic Nexus should simulate:

* selective memory
* political distraction
* shifting strategic focus

not perfect eternal awareness.

---

# Token Budget Rule

Attention systems are also a performance mechanism.

The daemon should:

* reduce irrelevant context
* compress stale relationships
* limit active strategic entities
* avoid prompt bloat

The system must remain scalable for:

* large galaxies
* multiplayer
* long campaigns

---

# Anti-Omniscience Principle

Civilizations should not constantly analyze the entire galaxy.

They should:

* focus locally
* focus emotionally
* focus strategically
* focus situationally

This creates:

* believable mistakes
* strategic blind spots
* delayed reactions
* geopolitical surprises

These are desirable outcomes.

---

# Design Goal

Strategic Nexus civilizations should feel like:

```text id="x0b6cj"
limited political entities with selective attention and shifting strategic focus
```

not:

```text id="5s8lwo"
perfect galaxy-wide superintelligences evaluating every variable continuously
```

Selective attention is one of the key systems required for:

* believable civilization behavior
* performance scalability
* strategic diversity
* anti-optimizer behavior
* long-term maintainability
