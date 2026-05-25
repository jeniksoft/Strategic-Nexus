# Strategic Nexus - Empire Processing Priority Queue

## Core Rule

Strategic Nexus daemon must not process empires only by simple round-robin.

Round-robin is allowed as a fallback, but the main scheduler must support priority escalation.

Some empires need strategic updates more urgently than others.

---

# Reason

Strategic Nexus is asynchronous.

The daemon:
- reads the newest save
- processes empires one by one
- generates bounded strategy decisions
- stores results for later host-side application

The game does not wait for the daemon.

Because processing is asynchronous, the daemon must decide which empire should be analyzed next.

---

# Rejected Model

Rejected scheduler:

```text
Empire 1
Empire 2
Empire 3
Empire 4
repeat forever
```

This ignores strategic urgency.

Problems:
- empires in active wars may wait too long
- crisis-front empires may be updated too late
- collapsing empires may not adapt
- hegemon threats may be detected slowly
- important events are treated like inactive empires

---

# Required Model

Required scheduler:

```text
newest save
-> build empire priority scores
-> process highest-priority empire
-> update decision
-> process next priority empire
-> periodically refresh priority from newest save
```

Round-robin may still be used inside equal priority groups.

---

# Priority Sources

The following should increase empire processing priority:

## War State

- empire is at war
- empire recently entered war
- empire recently lost war
- empire lost large fleet power
- empire borders active war zone
- empire is defending against stronger enemy

## Crisis State

- crisis has spawned
- empire borders crisis territory
- empire lost systems to crisis
- empire has strong anti-crisis potential
- empire controls key chokepoints near crisis path

## Hegemony Detection

- nearby empire is rapidly growing
- player or AI empire becomes dominant
- federation becomes too strong
- subject bloc expands quickly
- local balance of power collapses

## Diplomacy Shifts

- federation formed
- federation collapsed
- vassalization wave detected
- defensive pact network changed
- rival declared
- major betrayal detected
- galactic community law changed

## Internal Instability

- economy rank dropped sharply
- fleet rank dropped sharply
- tech rank changed significantly
- empire lost capital or core worlds
- empire suffered humiliation or major defeat

## Strategic Opportunity

- nearby rival weakened
- war target exposed
- crisis distracted enemies
- hegemon engaged elsewhere
- federation ally became vulnerable

---

# Priority Score Example

Example internal scheduler score:

```json
{
  "empire_id": 12,
  "base_priority": 0.25,
  "war_priority": 0.40,
  "crisis_priority": 0.00,
  "hegemon_priority": 0.30,
  "diplomatic_priority": 0.15,
  "total_priority": 1.10
}
```

This is not a final schema. It is a design guide.

---

# Update Frequency Tiers

Suggested tiers:

## Critical

Process as soon as possible.

Examples:
- active war
- crisis border
- fleet wipe
- capital threatened
- hegemon at border

## High

Process soon.

Examples:
- major diplomatic change
- strong rival nearby
- recent defeat
- rapid economic shift

## Normal

Process in ordinary queue.

Examples:
- stable empire
- no major conflict
- moderate relevance

## Low

Process rarely.

Examples:
- isolated empire
- inactive distant empire
- fallen empire in passive state
- no contact with relevant powers

---

# Newest Save Rule

Every processing pass must use the newest available save.

Old saves may be used only for historical memory comparison.

Source of truth:
- newest save

Historical memory:
- previous strategic snapshots
- previous decisions
- remembered wars
- remembered betrayals
- old doctrine history

The daemon must not generate new decisions from an outdated save if a newer save is available.

---

# No Runtime Synchronization Requirement

The daemon does not need to know exact Stellaris runtime time.

It only needs:
- save timestamp
- in-game year/month from save
- campaign identity
- empire identity
- previous decision metadata

This preserves loose coupling between game and daemon.

---

# Decision Replacement

New decisions replace older decisions for the same empire only if:

- campaign id matches
- empire id matches
- sequence id is newer
- payload is valid
- doctrine id is known

Old valid decisions remain usable until replaced.

---

# Stale Does Not Mean Invalid

Stale decisions should not be rejected just because they are old.

Instead:

```text
old decision
-> lower intensity
-> lower confidence
-> keep doctrine until replaced
```

Invalid decisions:
- corrupt payload
- wrong campaign id
- wrong empire id
- unknown doctrine id
- malformed JSON
- replayed old sequence id

Old but valid decisions:
- remain active
- slowly weaken
- represent institutional inertia

---

# Multiplayer Rule

In multiplayer:

- daemon runs only on host
- priority queue exists only on host
- clients do not process empires
- clients do not run LLM
- clients receive synchronized game state from Stellaris

This prevents divergent local reasoning and desync risk.

---

# Design Philosophy

The scheduler should make Strategic Nexus feel like a strategic staff system.

Important empires get reviewed first.

Stable empires can wait.

This creates:
- believable delay
- natural strategic inertia
- lower compute pressure
- no need for realtime AI
- better compatibility with local LLM limits
