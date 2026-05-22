# Strategic Nexus — Empire Knowledge Model

## Core Architecture Rule

There must NEVER be a single global LLM knowledge context.

Rejected architecture:

```text
Global galaxy context
→ one galactic AI brain
→ all empires reason from combined knowledge
```

Required architecture:

```text
Empire-scoped reasoning
→ each empire has isolated knowledge
→ each empire reasons separately
→ information only spreads through legitimate game systems
```

Strategic Nexus must simulate civilizations with limited knowledge, not a hidden omniscient optimizer.

---

# Core Philosophy

Strategic Nexus must reason from the perspective of civilizations inside the galaxy.

It must not reason from:

- complete save truth
- omniscient galaxy state
- hidden information unavailable to the empire
- combined knowledge from unrelated empires
- developer-level knowledge of the full campaign

The system simulates:

- strategic institutions
- intelligence limitations
- historical memory
- political bias
- imperfect inference
- regional awareness
- limited diplomatic knowledge

It must not simulate universal strategic optimization.

---

# Known Empire Set

Each empire maintains its own known empire list.

Example:

```json
{
  "observer_empire": 12,
  "known_empires": [2, 5, 19],
  "unknown_empires_exist": true
}
```

Unknown empires must not appear in strategic reasoning until discovered through legitimate game systems.

---

# Unknown Empire Rule

If an empire has not:

- established communications
- gained sensor visibility
- gained sufficient intel
- completed first contact
- learned of existence through diplomacy
- learned of existence through federation/alliance information sharing

then that empire must not appear inside strategic reasoning.

Correct behavior:

- isolated civilizations remain strategically isolated
- empires underestimate the galaxy
- regional power structures emerge naturally
- late hegemon discovery becomes possible
- first contact changes strategic context

Forbidden behavior:

- instant galaxy awareness
- perfect threat ranking
- automatic anti-player balancing
- global hidden knowledge sharing
- knowing every empire from game start

---

# EmpireScopedKnowledge

Each empire receives its own filtered strategic context.

The save parser must:

- parse newest save
- build visibility model
- build intel model
- build diplomatic knowledge model
- build known empire list
- build sensor visibility model
- build memory model from that empire perspective
- filter hidden information
- generate empire-specific knowledge snapshot

The parser must NEVER expose full galaxy truth directly to the LLM.

---

# Information Categories

## Own State

An empire always knows:

- its own economy
- its own planets
- its own fleets
- its own ship designs
- its own technology
- its own diplomacy
- its own war state
- its own strategic history

## Public Or Diplomatic State

An empire may know:

- known diplomatic relations
- wars involving known empires
- federations it knows about
- subjects and overlords it knows about
- galactic community information
- rivalries and pacts it knows about

## Sensor State

An empire may know:

- visible fleets
- visible systems
- visible starbases
- observed movement
- nearby military posture

Only if sensor visibility or legitimate sharing allows it.

## Intel State

An empire may know deeper information if intel level allows it.

Examples:

- relative economy
- relative technology
- relative fleet power
- planet details
- ship loadout hints
- internal state
- military doctrine indicators

## Historical Memory

An empire may remember:

- wars
- betrayals
- border conflicts
- humiliations
- fleet wipes
- crisis damage
- federation history
- old doctrine outcomes
- repeated enemy behavior

Memory must remain empire-scoped.

---

# Allowed Inference

Strategic Nexus may infer:

- aggressive tendencies
- carrier doctrine preference
- defensive posture
- economic dominance
- crisis preparation
- opportunistic behavior
- likelihood of war
- likely strategic intent

Inference is allowed.

Omniscience is forbidden.

Inference must include uncertainty and evidence when possible.

---

# Forbidden Information Usage

The system must NEVER use:

- hidden fleet compositions without intel
- hidden ship loadouts without intel
- hidden technologies without intel
- hidden economy details without intel
- hidden fleet positions outside visibility
- direct save truth as perfect knowledge
- exact hidden player optimization
- instant perfect counter generation
- knowledge from unknown empires
- knowledge from unrelated empire contexts

---

# Continuous Asynchronous Processing

Strategic Nexus daemon is asynchronous.

It is not synchronized to Stellaris runtime ticks.

The daemon:

- processes newest save
- selects next empire
- generates bounded strategic output
- stores result
- moves to next empire

The game never waits for the daemon.

---

# Empire Strategy Queue

Correct model:

```text
newest save
→ build empire priority queue
→ process empire A
→ process empire B
→ process empire C
→ continue until all processed
→ restart from newest save
```

High-priority empires may update more frequently.

Priority examples:

- active war
- crisis border
- hegemon growth
- collapsing federation
- fleet wipe
- rapid economic change
- new first contact
- new rival
- major loss
- major opportunity

---

# Save Source Of Truth

Newest save is always the authoritative source of truth for current state.

Old saves may be used only for:

- historical comparison
- memory persistence
- strategic continuity
- doctrine history
- detecting change over time

The daemon must not generate new decisions from outdated saves if a newer save is available.

---

# Decision Continuity

Old decisions are not invalid merely because they are old.

If no newer valid decision exists:

```text
old decision
→ reduced authority
→ lower confidence
→ continue until replaced
```

This represents strategic inertia and institutional continuity.

---

# Multiplayer Rule

In multiplayer:

- daemon runs only on host
- clients do not run LLM
- clients do not process empire reasoning
- clients do not read decision payloads
- clients receive synchronized game state only

Strategic Nexus remains host-authoritative.

---

# Design Goal

The player should feel:

```text
Each civilization sees a different galaxy.
Each civilization fears different threats.
Each civilization misunderstands reality in its own way.
```

Not:

```text
One hidden superintelligence controls all empires.
```
