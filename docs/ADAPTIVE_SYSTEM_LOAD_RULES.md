# Strategic Nexus - Adaptive System Load Rules

## Core Rule

Gameplay performance has priority over strategic freshness.

Strategic Nexus must adapt its processing load to available hardware resources and current game performance.

The system must never assume unlimited CPU, RAM, GPU, VRAM, disk I/O, or inference capacity.

Performance is a product-quality requirement across the whole project, not only a gameplay safeguard.
The mod, generated overlay, companion app, save parser, offline analysis, automations, and owner-facing tools should be designed so that routine use feels fast.
If a feature is correct but slow enough to waste user time, cause lag, delay gameplay flow, or make maintenance unpleasant, treat that as a design problem to measure and fix.

In the revised offline companion architecture, references to daemon scheduling should be read as companion/offline-worker scheduling unless a section explicitly describes a development harness.
They must not be read as permission for live LLM decisions to enter an already-running Stellaris session.

---

# Design Philosophy

Strategic Nexus runs alongside Stellaris.

Stellaris is already performance-heavy, especially during:

* late game
* large galaxies
* many empires
* large wars
* fleet combat
* crisis events
* heavy modlists
* multiplayer sessions

Strategic Nexus must therefore behave like a polite background system.

It should improve strategic behavior without noticeably degrading the game experience.

---

# Performance Priority Rule

If there is a conflict between:

```text
better strategic freshness
```

and:

```text
smooth gameplay
```

then smooth gameplay wins.

Strategic Nexus may delay, skip, compress, or degrade strategic processing to preserve gameplay performance.

---

# Adaptive Load Principle

The daemon should adapt to:

* CPU usage
* memory pressure
* disk I/O pressure
* inference latency
* save parsing time
* game speed degradation
* active multiplayer state
* galaxy size
* empire count
* active war count
* crisis activity
* user-configured performance mode

The daemon should not operate at fixed maximum workload.

---

# Load Modes

Strategic Nexus should support multiple load modes.

These modes should become explicit daemon scheduler concepts, not only configuration text.

Recommended implementation model:

```cpp
enum class LoadMode
{
    HighQuality,
    Balanced,
    PerformanceProtection,
    EmergencyMinimal
};
```

Every daemon scheduling decision should be able to inspect the current load mode.

## High Quality Mode

Use when resources are available.

Behavior:

* deeper reasoning
* more empires processed
* richer memory updates
* more frequent relationship review
* larger context summaries

## Balanced Mode

Default mode.

Behavior:

* priority-based processing
* normal context compression
* normal review cadence
* stable gameplay preference

## Performance Protection Mode

Use when the system is under pressure.

Behavior:

* reduce LLM calls
* process only high-priority empires
* shorten prompts
* skip low-priority memory updates
* skip relationship review
* preserve last valid decisions

## Emergency Minimal Mode

Use when performance is severely constrained.

Behavior:

* stop non-critical inference
* keep only fallback/last-known-good doctrine
* process only critical war/crisis states if possible
* disable optional subsystems
* avoid gameplay degradation

---

# Priority Degradation Order

When resources are limited, disable or delay systems in this order:

1. Relationship emotional updates
2. Long-term memory refinement
3. Low-priority diplomacy review
4. Low-priority economy review
5. Non-urgent espionage planning
6. Low-priority peaceful empires
7. Medium-priority strategic refresh
8. High-priority war/crisis empires last

War/crisis processing has higher priority,
but even war/crisis processing must remain non-realtime.

---

# Task Cost And Priority Rule

Every major daemon task should declare:

* priority
* estimated cost
* allowed load modes
* timeout budget
* fallback behavior
* whether it is skippable

Example task categories:

* empire strategic refresh
* war/crisis review
* memory refinement
* relationship update
* diplomacy review
* economy review
* save parsing
* payload validation

The scheduler must not treat all work as equally important.

Expensive low-priority tasks should be delayed or skipped before they affect gameplay performance.

---

# Skippable Work Rule

Most Strategic Nexus work must be skippable.

If a task is skipped:

* last known good state should remain valid
* confidence may decay slowly
* update cadence may stretch
* no invalid state should be generated
* no emergency fallback should occur unless state is actually invalid

Skipped work is not a failure.

It is a normal adaptive behavior.

---

# Scheduler Contract Rule

The daemon scheduler must be designed around bounded work units.

Each work unit should answer:

* What empire does this belong to?
* What system does this update?
* How urgent is it?
* How expensive is it expected to be?
* Can it be skipped?
* What happens if it times out?
* What state remains valid if it does not run?

This prevents the daemon from becoming an uncontrolled background workload.

The scheduler must be able to preserve campaign continuity even when only a small subset of strategic systems are processed.

---

# Minimum Strategic Continuity Rule

When processing is reduced:

* keep last valid strategy
* decay confidence slowly
* avoid hard reset
* avoid random fallback if previous state is valid

Old valid strategic state is better than no strategic state.

---

# Inference Budget Rule

The daemon should track:

* average inference time
* max inference time
* queue length
* failed inference count
* timeout count
* tokens per decision if available
* decisions per in-game year

If inference becomes too slow:

* reduce context
* reduce review frequency
* process fewer empires
* prioritize critical situations

---

# Save Parsing Budget Rule

Save parsing may become expensive in large galaxies.

The daemon should:

* avoid unnecessary full reparses where possible
* cache stable summaries
* invalidate caches carefully
* parse only needed sections where feasible
* degrade gracefully if parsing is too slow

The game must never wait for save parsing.

---

# Multiplayer Load Rule

In multiplayer:

* host performance is critical
* host/coordinator companion work must be conservative
* clients do not run LLM
* desync safety has priority over strategic depth

If multiplayer performance drops:

* reduce Strategic Nexus load first
* preserve synchronization
* keep gameplay stable

---

# Active Combat Protection Rule

During large visible fleet combat or heavy war load, Strategic Nexus should avoid expensive processing spikes.

Reason:

* Stellaris already becomes more demanding during battles
* frame stutter degrades player experience
* tactical moments should remain responsive

The daemon may:

* defer low-priority inference
* reduce background processing
* process only already queued lightweight tasks

---

# User Configuration Rule

Users should eventually be able to select performance policy.

Possible settings:

* quality
* balanced
* performance
* minimal

Default should be:

* balanced

The system should prefer safe automatic adaptation, but user control is valuable.

---

# No Blocking Rule

Strategic Nexus must never block:

* game simulation
* save loading
* multiplayer synchronization
* user input
* combat responsiveness

All heavy work must remain asynchronous.

---

# Watchdog Rule

The daemon should detect overload.

Possible overload indicators:

* inference timeout
* queue grows too large
* repeated missed update windows
* save parsing takes too long
* memory pressure rises
* CPU remains saturated
* emergency mode triggered repeatedly

If overload is detected:

* log concise warning
* reduce workload
* preserve last valid state
* avoid retry storms

---

# Anti-Runaway Rule

The system must prevent runaway behavior.

Avoid:

* infinite retry loops
* repeated failing inference
* unlimited queue growth
* unbounded memory accumulation
* unbounded prompt growth
* repeated parse storms

Runaway behavior is a critical failure.

---

# Performance Audit Rule

Strategic Nexus should record lightweight performance telemetry.

Useful fields:

* mode
* queue length
* processed empires
* skipped empires
* average inference time
* average parse time
* fallback count
* overload count

Telemetry must remain concise.

No giant logs.

---

# Design Goal

Strategic Nexus should behave like:

```text
an adaptive background strategic system that protects gameplay smoothness
```

not:

```text
a heavyweight AI process that makes Stellaris slower and less playable
```

The best strategic AI is useless if the player experience degrades.
