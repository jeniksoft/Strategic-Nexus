# Strategic Nexus - Technical Runtime Limitations

## Core Rule

Strategic Nexus is NOT a realtime AI system.

Strategic Nexus must remain:

* asynchronous
* strategic
* low-frequency
* performance-aware
* multiplayer-safe
* session-to-session by default

The project must avoid realtime inference and continuous gameplay control.

---

# Design Philosophy

Strategic Nexus exists to provide:

* civilization-level reasoning
* strategic adaptation
* memory
* personality
* doctrine evolution
* strategic interpretation

It does NOT exist to:

* micro-manage fleets
* perform realtime combat AI
* replace tactical gameplay
* execute per-tick reasoning
* constantly react every game day

Strategic Nexus is a strategic overlay system, not a realtime control layer.

The preferred production model is offline campaign analysis:

```text
archive autosaves during play
-> analyze after play
-> update generated mod overlay for the next launch
```

---

# Stellaris Performance Constraint

Stellaris is already heavily CPU-bound, especially during:

* late game
* large galaxies
* high empire counts
* fleet combat
* pathfinding
* crisis events
* heavy modded gameplay

Realtime LLM inference would:

* reduce simulation speed
* increase frame stutter
* worsen late-game slowdown
* increase multiplayer desync risk
* create unstable timing behavior

Strategic Nexus must therefore minimize runtime overhead aggressively.

---

# Hard Runtime Rule

Strategic Nexus must never require realtime inference.

Forbidden architectures:

* per-day inference
* per-tick inference
* combat-frame inference
* realtime fleet control
* live combat decision loops
* synchronous blocking AI generation
* live LLM decisions entering the already-running game

The daemon must remain decoupled from Stellaris simulation timing.

---

# Update Frequency Rule

Strategic updates are intentionally slow.

For the revised architecture, the default update cadence is per play session, not per active game tick.

Generated mod overlays should normally update only after Stellaris exits and before the next launch.

During active `stellaris.exe` play, the companion app should preserve stable autosaves and status only. Post-play analysis and generated rule refresh start after the game exits and the capture archive is verified.

Older notes that mention in-game monthly updates should be treated as a possible scripted fallback cadence for already-generated values, not as a requirement for live LLM refresh.

Generated mod files are launch-time inputs.
Strategic Nexus must not expect Stellaris to automatically reload changed mod files during an active gameplay session.
Developer console reload/testing commands are not a production transport path.

Current target:

* maximum one generated strategic overlay refresh per play session by default
* optional scripted in-game cadence may consume already-generated values at low frequency

This may be slower under:

* large galaxies
* heavy wars
* low-end hardware
* multiplayer load

This is acceptable.

Civilizations should evolve gradually.

---

# Asynchronous Processing Rule

The daemon or offline worker operates asynchronously.

The worker:

* does not control Stellaris time
* does not synchronize to ticks
* does not require immediate response

The worker:

* parses archived saves
* processes empires progressively
* generates bounded strategic updates
* delivers updates for a later session

Strategic Nexus is eventually-consistent, not realtime-consistent.

The default offline analysis input is the latest play session's archived autosaves plus prior durable memory.
It should not reprocess all historical autosaves after every session.
This keeps analysis cost bounded for long-running galaxies.

The app must assume the machine may be restarted between sessions.
Durable archive and memory state must survive restart.
Optional start-with-Windows support exists to reduce the chance that autosaves are missed before the app is launched manually.

---

# War Priority Exception

Empires currently at war may receive:

* higher queue priority
* more frequent strategic refresh
* faster theater reevaluation

However:

Even wartime updates remain bounded and asynchronous.

Strategic Nexus still avoids:

* realtime combat logic
* fleet micro-management
* tactical reaction loops

---

# Doctrine Inertia Benefit

Slow update frequency is a feature, not a flaw.

Benefits:

* strategic inertia
* believable civilizations
* less optimizer behavior
* smoother multiplayer behavior
* lower CPU usage
* reduced strategic thrashing

Civilizations should not instantly pivot strategy every few days.

---

# Runtime Responsibility Split

## Daemon

Responsible for:

* strategic reasoning
* memory interpretation
* doctrine evolution
* strategic planning
* bounded payload generation

In the revised production architecture, this role runs primarily after the play session.

## Companion App

Responsible for:

* detecting the play session lifecycle
* archiving autosaves with read-only access
* staying out of active gameplay
* staging generated mod overlay updates after Stellaris exits

## Bridge

Responsible for:

* payload validation
* bounded runtime delivery
* predefined event triggering
* synchronization safety

## Vanilla AI

Responsible for:

* fleet movement
* tactical combat
* pathfinding
* economic execution
* realtime gameplay systems

Strategic Nexus modifies strategic direction, not realtime control.

---

# Multiplayer Constraint

Multiplayer stability has higher priority than strategic complexity.

Rules:

* daemon runs only on host
* clients do not run LLM
* clients do not process payloads
* synchronized game state remains authoritative

Realtime distributed AI is explicitly out of scope.

---

# Performance Budget Principle

Every new feature must justify:

* CPU usage
* memory usage
* token usage
* save parsing cost
* synchronization overhead

The project should prefer:

* compressed strategic summaries
* bounded updates
* low-frequency doctrine evolution

over:

* continuous simulation analysis
* huge prompt contexts
* rapid update loops

---

# Scalability Goal

Strategic Nexus should remain usable on:

* large galaxies
* long campaigns
* multiplayer sessions
* heavily populated late games

The project must avoid becoming:

* a simulation killer
* a CPU bottleneck
* a desync generator

---

# Future Expansion Constraint

Future systems must respect this limitation.

Including:

* memory systems
* espionage systems
* war planning
* doctrine systems
* diplomacy systems

No future feature may assume realtime inference capability.

---

# Design Goal

Strategic Nexus should feel like:

```text
slow-moving strategic civilization intelligence
```

not:

```text
a realtime tactical AI controller
```

The project succeeds if civilizations:

* evolve gradually
* react believably
* maintain continuity
* preserve performance
* remain multiplayer-safe
