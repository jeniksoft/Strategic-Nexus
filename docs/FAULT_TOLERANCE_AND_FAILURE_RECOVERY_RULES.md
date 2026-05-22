# Strategic Nexus — Fault Tolerance And Failure Recovery Rules

## Core Rule

Strategic Nexus must assume failures are inevitable.

The architecture must remain resilient against:

* software crashes
* hardware instability
* multiplayer interruptions
* corrupted data
* interrupted writes
* daemon failure
* OS instability
* unexpected shutdowns
* partial updates
* networking problems
* invalid payloads
* human mistakes

The system must fail safely.

---

# Design Philosophy

The project must assume:

* players will crash
* systems will freeze
* power will fail
* saves may corrupt
* mods may mismatch
* updates may interrupt
* hardware may behave unpredictably

Strategic Nexus must therefore prioritize:

* graceful degradation
* recoverability
* data integrity
* bounded failure
* restart safety
* multiplayer stability

Perfect reliability is impossible.

Safe recovery is mandatory.

---

# Fail-Safe Principle

If anything goes wrong:

* the game must continue functioning
* vanilla gameplay must survive
* multiplayer must remain stable if possible
* the mod must degrade safely
* civilization state should recover gracefully

Strategic Nexus must never become:

* a single point of catastrophic failure
* a save destroyer
* a permanent campaign corrupter

---

# Failure Categories

The architecture must account for multiple classes of failure.

---

## Hardware Failures

Examples:

* power outage
* PSU instability
* overheating
* unstable RAM
* faulty SSD/HDD
* GPU driver crash
* thermal shutdown
* machine restart
* blue screen
* silent data corruption

Possible consequences:

* corrupted save
* partial write
* daemon interruption
* broken payload output
* stale synchronization state

---

## Operating System Failures

Examples:

* forced Windows update
* driver crash
* process termination
* antivirus interference
* filesystem lock
* sleep/hibernate interruption
* permissions failure
* storage exhaustion
* cloud sync conflict

Possible consequences:

* interrupted save writes
* payload corruption
* daemon crash
* incomplete batch generation

---

## Multiplayer Failures

Examples:

* Wi-Fi disconnect
* packet loss
* temporary internet outage
* client timeout
* host migration failure
* desync
* delayed synchronization
* stale client state

Possible consequences:

* inconsistent strategic state
* stale doctrine propagation
* partial synchronization

Host authority must remain canonical.

---

## Mod And Runtime Failures

Examples:

* broken scripted event
* invalid enum
* schema mismatch
* payload version mismatch
* corrupted batch file
* failed bridge delivery
* missing scripted target
* outdated mod version

Possible consequences:

* ignored payload
* fallback doctrine
* invalid strategic state

---

## Human Errors

Examples:

* deleting files
* moving saves
* renaming campaigns
* partial Git updates
* incorrect mod load order
* incompatible mod combinations
* accidental overwrite
* manual file editing mistakes

The architecture must tolerate common user mistakes where possible.

---

# Atomic Write Rule

Critical files must use atomic write behavior whenever possible.

Preferred behavior:

1. write temporary file
2. validate
3. rename/replace final file

On Windows, replacement should use the operating system's replace-existing file move behavior where available.

Avoid:

* overwriting active files directly
* partially written payloads
* incomplete save replacement

Interrupted writes must not destroy valid previous state.

---

# Validation Before Apply Rule

All strategic payloads must be validated before use.

Validation should include:

* schema version
* enum validity
* bounds checking
* corruption detection
* sequence ordering
* campaign identity
* empire identity
* timestamp sanity

Invalid payloads must be rejected safely.

---

# Last Known Good State Rule

The system should preserve:

* last valid payload
* last valid doctrine state
* last valid strategic snapshot

If new data fails:

* fallback to previous stable state
* avoid undefined behavior

Civilizations should degrade gracefully rather than collapse randomly.

---

# Idempotency Rule

Repeated application of the same payload should not corrupt state.

The architecture should tolerate:

* duplicate processing
* delayed processing
* replay after restart
* partial synchronization retries

This is critical for:

* multiplayer stability
* crash recovery
* daemon restart safety

---

# Sequence Safety Rule

Payloads must include:

* sequence_id
* timestamp
* campaign_id

Older payloads must never overwrite newer valid state.

The system must prevent:

* stale rollback corruption
* out-of-order updates
* delayed replay corruption

---

# Campaign Identity Rule

Campaign identity must survive:

* save rename
* machine transfer
* daemon restart
* file relocation

Strategic state must bind to:

* campaign identity
* civilization identity

not fragile filenames.

---

# Watchdog Principle

Critical systems should detect stalled behavior.

Possible watchdog targets:

* daemon freeze
* stalled payload generation
* invalid update cadence
* repeated corruption
* bridge inactivity

The system should prefer:

* controlled fallback
* safe restart
* graceful degradation

over:

* undefined behavior

---

# Safe Fallback Rule

If Strategic Nexus becomes unavailable:

* the game must remain playable
* vanilla AI should continue functioning
* civilizations should preserve last stable doctrine if possible

The mod should degrade into:

* static strategic state
* or vanilla behavior

not catastrophic failure.

---

# Corruption Isolation Rule

Corruption should remain localized whenever possible.

Examples:

* one broken empire payload should not destroy all empires
* one corrupted save parse should not erase strategic memory database
* one invalid enum should not crash multiplayer

Failures should remain compartmentalized.

---

# Timeout Rule

LLM inference and daemon operations must have timeouts.

The game must never:

* wait indefinitely
* freeze for inference
* block simulation

If timeout occurs:

* skip update
* use fallback
* retry later

Asynchronous safety has higher priority than perfect update freshness.

---

# Partial Availability Principle

Strategic Nexus should tolerate partial system availability.

Examples:

* some empires updated
* others stale
* some memories unavailable
* some payloads rejected

This is acceptable.

The system must not require:

* perfect synchronization
* perfect freshness
* complete galaxy updates every cycle

---

# Attention To Save Integrity

Save corruption is one of the highest-risk failure categories.

Possible causes:

* interrupted writes
* mod mismatch
* version mismatch
* power failure
* forced shutdown
* unstable storage
* cloud sync conflict

The architecture should:

* avoid direct save modification when possible
* preserve backup state
* avoid destructive overwrite behavior

---

# Determinism And Multiplayer Rule

Host authority remains canonical.

Clients must:

* never independently reason strategically
* never generate divergent payloads
* never own civilization state

This minimizes:

* desync risk
* divergent simulation
* synchronization corruption

---

# Logging And Diagnostics Rule

The system should preserve:

* concise error logs
* fallback reasons
* corruption detection
* payload rejection causes
* restart causes
* timeout events

Avoid:

* giant uncontrolled logs
* raw save spam
* uncontrolled debug flooding

---

# Performance Failure Rule

Performance collapse is also considered a failure mode.

The architecture must avoid:

* runaway context growth
* realtime inference
* unbounded memory growth
* uncontrolled prompt expansion
* infinite retry loops

Graceful degradation is preferable to runaway processing.

---

# Human Recovery Principle

Users should be able to recover campaigns manually if necessary.

Examples:

* disable daemon
* remove invalid payload
* revert to previous save
* clear temporary runtime state
* continue with vanilla AI

Strategic Nexus should never permanently trap campaigns in unrecoverable states.

---

# Design Goal

Strategic Nexus should behave like:

```text id="j9p2xl"
a fault-tolerant strategic overlay capable of surviving imperfect real-world conditions
```

not:

```text id="6t5vne"
a fragile experimental system that collapses when anything unexpected happens
```

Robustness and graceful recovery are core architectural requirements of the entire project.
