# Strategic Nexus — LLM Context Budget Rules

## Core Rule

Context is a filtered strategic briefing, not a save dump.

The LLM must never receive:

* the full save
* the full galaxy state
* unrestricted hidden information
* unfiltered raw parsed data

The LLM must receive only:

* filtered
* bounded
* empire-scoped
* strategically relevant
* uncertainty-aware
  information.

The goal is NOT to maximize information quantity.

The goal IS to maximize strategic relevance while preserving:

* anti-cheat principles
* personality
* uncertainty
* believable civilization behavior
* multiplayer safety
* performance
* future scalability

---

# Empire-Scoped Context Rule

Every LLM request belongs to exactly one empire.

The LLM must reason only from the perspective of the target empire.

Allowed context:

* empire identity
* ethics
* civics
* government type
* personality profile
* known empires
* known threats
* visible military information
* available intel
* strategic memory
* current doctrine state
* economic capability
* technological capability

Forbidden context:

* full galaxy truth
* hidden fleet positions
* hidden ship loadouts
* hidden economy data
* hidden technologies
* unknown empires without contact
* unrestricted save parsing output

The LLM must never behave like a galaxy-wide omniscient observer.

---

# Context Budget Principle

Local LLM context is limited.

The daemon must aggressively compress and summarize information before sending it to the model.

Do NOT send:

* raw save sections
* complete planet lists
* complete fleet lists
* complete ship lists
* complete system lists
* raw event spam
* large debug logs
* parser internals

Instead send:

* strategic summaries
* threat summaries
* economic summaries
* memory summaries
* doctrine summaries
* capability summaries
* uncertainty summaries

---

# Structured Context Layout

The prompt/context should use stable logical sections.

Recommended structure:

## A. Empire Identity

Defines:

* ethics
* civics
* government
* personality
* ascension direction
* civilization style

## B. Current Capability

Defines:

* economy strength
* alloy capacity
* energy stability
* research capability
* naval capability
* infrastructure maturity
* strategic resources
* logistics state
* recovery state

## C. Known Galaxy

Defines:

* known empires
* known threats
* diplomacy
* visible military state
* intel-derived information
* crisis awareness

## D. Strategic Memory

Defines:

* historical wars
* betrayals
* alliances
* humiliations
* trauma
* successful doctrines
* failed doctrines
* timeline-derived memory

## E. Current Strategic State

Defines:

* current doctrine dimensions
* doctrine inertia
* recent strategic changes
* current strategic pressure

## F. Current Pressure

Defines:

* immediate threats
* expansion pressure
* crisis pressure
* economic instability
* political instability
* military weakness

## G. Allowed Strategic Output

Defines:

* allowed domains
* allowed enum values
* partial update permissions
* schema version
* validation constraints

---

# Strategic Relevance Rule

Every piece of context must justify its existence.

Good context:

* “neighbor fleet superior”
* “energy deficit worsening”
* “military intel low”
* “recent federation betrayal”
* “crisis approaching border”
* “alloy production insufficient for carrier transition”

Bad context:

* asteroid lists
* irrelevant systems
* decorative planet data
* raw save internals
* hidden unknown empires
* unnecessary low-level details

If information does not help strategic reasoning, it should usually not enter the prompt.

---

# Uncertainty Representation Rule

The context must explicitly represent uncertainty.

The LLM must know:

* what is confirmed
* what is estimated
* what is unknown

Example:

Confirmed:

* neighboring empire fleet superior

Estimated:

* neighboring economy likely strong

Unknown:

* exact fleet composition
* hidden reserve fleets
* internal instability level

Uncertainty should influence:

* confidence
* doctrine inertia
* risk tolerance
* strategic conservatism

---

# Capability Constraint Context Rule

The context must include realistic empire capability limits.

The LLM must know:

* economic limits
* influence limits
* industrial limits
* technological limits
* logistics limits
* infrastructure maturity
* transition difficulty

The best theoretical strategy may not be executable.

The LLM must choose:

* realistic
* sustainable
* civilization-compatible
  strategies.

---

# Personality Preservation Rule

The context must preserve civilization identity.

The prompt must include:

* ethics
* civics
* personality
* historical memory
* trust/distrust
* paranoia
* trauma
* strategic culture

Without this:
all empires converge toward optimizer behavior.

Strategic Nexus must produce distinct civilizations, not identical strategic solvers.

---

# Timeline Compression Rule

Raw timeline history should not be sent directly.

The daemon should compress timeline history into:

* strategic memory
* trauma summaries
* rivalry summaries
* doctrine lessons
* trust changes
* civilization narratives

Bad:

```text
200 raw timeline events
```

Good:

```text
Empire suffered two border wars against hegemon neighbor and now strongly prefers defensive borders.
```

---

# Intel-Limited Strategy Rule

The LLM must not make precise strategic counters without sufficient intel.

Example:

If fleet composition is unknown:

* avoid hard counter specialization
* prefer balanced doctrine
* increase intel priority
* increase caution

The LLM should reason within information limits.

---

# Partial Expansion Rule

The context architecture must support gradual expansion.

Example:

v0:

* military_posture
* research_bias

Later:

* espionage_policy
* fleet_philosophy
* border_strategy
* diplomatic_behavior
* crisis_policy

New sections must be additive.

Old prompts must remain partially compatible when possible.

---

# Auditability Rule

Every LLM decision must be reproducible from:

* context snapshot
* schema version
* empire id
* campaign id
* strategic memory state
* uncertainty state
* capability state

The system should store:

* concise summaries
* evidence
* confidence
* uncertainty

Do NOT store chain-of-thought.

---

# Performance Rule

The daemon should minimize:

* token waste
* repeated data
* irrelevant details
* redundant summaries

The LLM should receive:

* compressed strategic state
* not simulation-level raw data

Strategic Nexus is a strategic interpretation layer, not a full galaxy simulator prompt.

---

# Anti-Cheat Principle

The daemon may parse the save.

The LLM must NOT receive unrestricted hidden truth.

The daemon acts as:

* filter
* compressor
* perspective limiter
* uncertainty generator

This preserves:

* fairness
* immersion
* believable intelligence
* strategic fog-of-war

---

# Modding And Runtime Constraint Awareness

Research and community evidence strongly suggest:

* most Stellaris scripting is loaded at startup
* runtime communication is limited
* event-driven architecture is preferred
* predefined events/effects are safer than arbitrary execution

Relevant references:

Event modding overview:
https://stellaris.fandom.com/wiki/Event_modding

General modding tutorial:
https://stellaris.fandom.com/wiki/Modding_tutorial

Discussion of scripting/runtime limitations:
https://www.reddit.com/r/Stellaris/comments/168oe4k/

Paradox scripting performance discussion:
https://www.reddit.com/r/Stellaris/comments/ilpztj/

These constraints reinforce the need for:

* compressed context
* bounded schemas
* modular strategic dimensions
* empire-scoped reasoning

---

# Design Goal

The LLM should feel like:

```text
a strategic advisory mind inside a civilization
```

not:

```text
a save-file supercomputer with perfect information
```

The model should receive:

* the right information
* at the right abstraction level
* from the right perspective
* with the right uncertainty
* inside a bounded strategic framework.
