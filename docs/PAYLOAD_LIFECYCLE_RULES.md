# Strategic Nexus — Payload Lifecycle Rules

## Core Rule

Every strategic payload has a lifecycle.

Payloads are not simply:

- valid
- invalid

A payload may also become:

- stale
- decayed
- replaced
- replayed
- corrupted
- fallback-required

The bridge and scripted layer must treat these states differently.

---

# Required Payload Identity

Every payload must contain enough identity metadata to prevent cross-campaign or wrong-empire contamination.

Required fields should include:

```json
{
  "schema_version": 1,
  "campaign_id": "campaign_hash_or_uuid",
  "galaxy_seed": 123456,
  "empire_id": 12,
  "sequence_id": 81,
  "source_save_year": 2247,
  "source_save_month": 6,
  "created_unix_ms": 0
}
```

The exact schema may evolve, but identity validation is mandatory.

---

# VALID

A payload is VALID if:

- schema is correct
- campaign id matches
- empire id matches
- sequence id is newer than the last accepted sequence for that empire
- doctrine id is known
- bounded values are in valid range or safely clamped
- payload integrity passes validation

VALID payloads may influence strategic doctrine.

---

# STALE

A payload is STALE if:

- it is old
- but structurally valid
- campaign id matches
- empire id matches
- no newer accepted replacement exists

STALE does NOT mean invalid.

Correct behavior:

```text
stale payload
→ reduced authority
→ lower confidence
→ continue using until replaced
```

Rejected behavior:

```text
stale payload
→ immediate fallback
```

Old strategy should weaken, not vanish.

---

# DECAYED

A payload is DECAYED if:

- it remains valid
- but confidence or intensity weakened over time

Example design guide:

```text
0–5 years old
→ full strength

5–15 years old
→ reduced influence

15+ years old
→ weak but active
```

Decayed payloads represent:

- aging doctrine
- outdated assumptions
- institutional inertia
- slow strategic drift

---

# REPLACED

A payload is REPLACED if:

- a newer valid payload exists
- campaign id matches
- empire id matches
- newer sequence id exists

Replaced payloads no longer influence doctrine.

They may remain in:

- history logs
- memory summaries
- debugging records
- strategic audit trail

---

# REPLAYED

A payload is REPLAYED if:

- lower sequence id arrives
- identical old payload is resent
- stale overwrite is attempted
- sequence id was already accepted

Replay attempts must be rejected.

Correct behavior:

```text
replayed payload
→ reject payload
→ preserve current accepted doctrine
```

Do not fallback just because an old replay arrives if a better current doctrine exists.

---

# CORRUPTED

A payload is CORRUPTED if:

- malformed JSON
- invalid schema
- missing required fields
- invalid enum values
- invalid numeric ranges that cannot be clamped safely
- broken serialization
- unreadable file
- wrong encoding

Corrupted payloads trigger fallback or preservation of previous valid doctrine.

---

# FALLBACK_REQUIRED

A payload explicitly requests fallback if:

```json
{
  "fallback_required": true
}
```

This may represent:

- daemon uncertainty
- unsupported game state
- insufficient evidence
- failed reasoning
- bridge safety condition
- unknown doctrine state

Fallback is safe continuity, not failure.

---

# Validation Flow

Required validation flow:

```text
read payload
-> validate schema
-> validate campaign id
-> validate empire id
-> validate sequence id
-> validate strategy dimensions
-> validate bounded numeric ranges
-> classify payload lifecycle state
-> map to predefined action
-> apply bounded action
```

Current bridge-core pipeline flow:

```text
read decision file
-> read last accepted sequence state
-> validate schema and sequence
-> map strategy_dimensions to ScriptIntent
-> emit allowlisted effect batch
-> write effect batch atomically
-> advance sequence state only after successful write
```

Replay safety:

```text
replayed payload
-> reject payload
-> clear stale effect batch output
-> preserve last accepted sequence state
```

---

# Failure Rules

If validation fails:

```text
invalid payload
→ fallback or preserve previous valid doctrine
```

Failure must NEVER:

- corrupt save
- intentionally crash game
- desync multiplayer
- execute arbitrary commands
- apply payload to wrong empire
- apply payload to wrong campaign

---

# Preserve Previous Doctrine Rule

When a new payload is invalid, corrupted, replayed, or wrong-campaign:

```text
bad incoming payload
→ ignore it
→ preserve previous accepted valid doctrine if available
```

Do not erase a good doctrine because a bad payload arrived.

Fallback is used only if no safe previous doctrine exists or if explicit fallback is required.

---

# Sequence Rules

Every empire decision stream uses increasing sequence ids.

Example:

```json
{
  "empire_id": 12,
  "sequence_id": 81
}
```

The bridge must reject:

- lower sequence ids
- duplicated sequence ids
- old replay attempts
- payloads from another campaign

---

# Bounded Values

Payload values must be bounded.

Examples:

```json
{
  "strategic_pressure": 0.74,
  "doctrine_intensity": 0.62,
  "confidence": 0.68
}
```

Rules:

- clamp floats to valid ranges
- reject unknown enum values
- reject unbounded text commands
- reject arbitrary script strings
- reject raw LLM text

---

# Multiplayer Rule

In multiplayer:

- host validates payloads
- clients never process payload files
- clients never run daemon
- clients never generate or deliver payloads
- clients receive synchronized game state only

This preserves deterministic multiplayer state.

---

# Design Goal

Civilizations should:

- maintain strategic continuity
- degrade gracefully
- slowly adapt
- preserve doctrine inertia
- survive daemon delay
- survive invalid payloads

They should not:

- instantly reset
- oscillate constantly
- hard fallback on delay
- behave like stateless optimizers
- lose doctrine because daemon is slow
