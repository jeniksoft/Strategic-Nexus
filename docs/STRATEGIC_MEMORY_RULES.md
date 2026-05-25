# Strategic Nexus - Strategic Memory Rules

## Core Rule

Strategic Nexus civilizations must possess strategic memory.

Civilizations should:

* remember wars
* remember betrayals
* remember alliances
* remember humiliation
* remember existential threats
* remember successful doctrines
* remember failed doctrines

The LLM must not behave like a stateless optimizer.

Strategic memory is one of the primary reasons to use an LLM-based architecture.

---

# Design Philosophy

Memory creates civilization continuity.

Without memory:

* empires become interchangeable
* behavior becomes purely reactive
* diplomacy loses meaning
* personality collapses into optimization
* history becomes irrelevant

Strategic Nexus should create civilizations that feel historically aware.

---

# Memory Layers

Strategic memory should exist in multiple layers.

## Short-Term Memory

Represents:

* recent wars
* recent defeats
* recent diplomatic events
* current crisis reactions
* current strategic pressure

Characteristics:

* changes quickly
* high emotional intensity
* strong influence on immediate doctrine

Example:

* recent border invasion
* emergency economic collapse
* recent betrayal

---

## Medium-Term Memory

Represents:

* recurring rivalries
* repeated strategic lessons
* long conflicts
* alliance trust evolution
* regional geopolitical patterns

Characteristics:

* moderate persistence
* influences doctrine inertia
* influences diplomacy

Example:

* repeated wars with same hegemon
* successful federation cooperation
* repeated piracy pressure

---

## Long-Term Civilization Memory

Represents:

* civilization identity
* cultural paranoia
* historical trauma
* ideological hostility
* strategic traditions
* historical doctrine preference

Characteristics:

* changes very slowly
* partially persistent across rulers
* deeply affects personality

Example:

* civilization historically fears extermination
* civilization glorifies expansion
* civilization distrusts federations
* civilization prefers fortress defense

---

# Timeline As Memory Anchor

The Stellaris save timeline/history acts as the canonical portable memory anchor.

Timeline events should be interpreted into:

* trauma
* rivalry
* trust
* doctrine lessons
* strategic identity
* civilization narratives

The daemon may extend memory,
but the save timeline remains the portable historical backbone.

---

# Memory Sources

Strategic memory may be derived from:

* wars
* defeats
* victories
* crises
* diplomacy
* betrayals
* federation history
* subject history
* espionage
* border pressure
* fleet losses
* economic collapse
* successful doctrines
* failed doctrines
* timeline events
* strategic transitions

---

# Memory Interpretation Rule

Memory is not raw event storage.

The daemon should interpret events into strategic meaning.

Bad:

```text id="5ny55t"
War declared at year 2231.
```

Good:

```text id="k8f53a"
Empire suffered repeated border aggression from neighboring hegemon and developed defensive paranoia.
```

Memory should describe:

* meaning
* strategic lesson
* emotional impact
* civilization adaptation

Not raw logs.

---

# Memory Must Be Empire-Scoped

Each empire has its own interpretation of history.

The same event may produce different memories.

Example:

Empire A:

* remembers betrayal
* develops paranoia

Empire B:

* remembers successful expansion
* develops confidence

There is no global objective civilization memory.

---

# Personality Interaction Rule

Personality affects memory interpretation.

Examples:

## Paranoid Personality

May:

* remember betrayals longer
* overestimate threats
* reinforce defensive doctrine

## Bold Personality

May:

* recover quickly from defeat
* underestimate danger
* seek revenge

## Honor-Bound Personality

May:

* strongly value alliances
* remember diplomatic betrayal intensely

## Opportunistic Personality

May:

* reinterpret defeats pragmatically
* abandon old grudges faster

---

# Doctrine Memory Rule

Empires should remember strategic outcomes.

Examples:

* successful carrier warfare
* failed missile doctrine
* effective fortress defense
* economic collapse from over-militarization

Memory should influence future strategic recommendations.

The LLM should not repeatedly retry catastrophically failed transitions without strong justification.

---

# Trauma Rule

Some events should create strategic trauma.

Possible trauma sources:

* existential wars
* genocide attempts
* crisis devastation
* repeated betrayal
* catastrophic collapse
* humiliation
* near extinction

Trauma may influence:

* paranoia
* militarization
* isolationism
* crisis preparation
* distrust
* doctrine rigidity

Trauma should decay slowly.

Some trauma may become civilization-defining.

---

# Trust And Distrust Memory

Empires should remember:

* reliable allies
* repeated betrayal
* opportunistic behavior
* military support
* abandonment during war
* federation loyalty

Trust should evolve slowly over time.

Trust should not instantly reset.

---

# Memory Decay Rule

Not all memory lasts forever.

Strategic memory should gradually decay unless reinforced.

Fast-decaying:

* temporary economic pressure
* minor diplomatic incidents

Slow-decaying:

* repeated wars
* betrayal
* crisis trauma
* extermination threats

Civilization identity memories may persist for centuries.

---

# Reinforcement Rule

Repeated events reinforce memory.

Example:

Single border war:
-> moderate caution

Five repeated invasions:
-> entrenched defensive paranoia

Repeated reinforcement should:

* strengthen confidence
* strengthen trauma
* strengthen rivalry
* strengthen distrust
* strengthen doctrine inertia

---

# Memory Compression Rule

The daemon should compress memory into:

* strategic summaries
* doctrine lessons
* relationship summaries
* civilization narratives

The LLM should not receive:

* giant raw historical logs
* full raw timelines
* unbounded event spam

Memory must remain:

* token-efficient
* strategically meaningful
* interpretable

---

# Memory Persistence Rule

Strategic memory should survive:

* save rename
* machine transfer
* daemon restart
* daemon memory loss when possible

If daemon memory is lost:

* bootstrap memory reconstruction should occur from save timeline/history.

---

# Session Delta Update Rule

Normal memory updates should process the latest play session, not the entire campaign from scratch.

The durable memory store represents older history.
New autosaves from the latest session provide the delta.

Correct flow:

```text
previous memory
+ latest session autosaves
-> updated memory
```

This prevents analysis time from growing linearly with the total age of the galaxy.

Full reconstruction from all available saves may be supported as an explicit recovery or maintenance operation, but it is not the default post-session workflow.

---

# Memory And Uncertainty

Memories may be:

* inaccurate
* biased
* outdated
* incomplete

The LLM should not treat memory as perfect truth.

Civilizations may:

* overreact
* misremember
* cling to old fears
* ignore new evidence

This is desirable behavior.

---

# Anti-Optimizer Principle

Memory exists to prevent pure optimization behavior.

Strategic Nexus should create civilizations that:

* carry historical baggage
* fear old enemies
* repeat traditions
* overlearn trauma
* distrust rivals
* value old alliances

Perfect adaptation is NOT the goal.

Believable historical continuity IS the goal.

---

# Multiplayer Rule

In multiplayer:

* memory generation occurs on host
* clients do not generate independent memory
* synchronized strategic state remains authoritative

This prevents divergent civilization histories.

---

# Auditability Rule

Strategic memory changes should be auditable.

Examples:

* new trauma source
* doctrine lesson update
* alliance trust change
* rivalry escalation

Store:

* concise summaries
* source events
* confidence
* decay state

Do NOT store chain-of-thought.

---

# Design Goal

Strategic Nexus civilizations should feel like:

```text id="6n2ljn"
historical civilizations shaped by memory, trauma, ideology, and strategic experience
```

not:

```text id="db1f8j"
stateless optimization algorithms recalculating every turn
```

Memory is one of the central pillars of the entire Strategic Nexus architecture.
