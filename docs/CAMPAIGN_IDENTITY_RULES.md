# Strategic Nexus — Campaign Identity And Payload Validation Rules

## Core Rule

Strategic Nexus decisions must belong to a specific campaign and empire.

The system must never apply:
- decisions from another campaign
- decisions from another galaxy
- decisions from another empire
- replayed outdated payloads

Every strategic payload must carry identity metadata.

---

# Required Identity Metadata

Every bounded decision payload should include:

```json
{
  "campaign_id": "galaxy_7A91C2",
  "galaxy_seed": 88374621,
  "save_identity_hash": "A19F-77CC-92B1",
  "empire_id": 12,
  "sequence_id": 81,
  "source_save_year": 2247,
  "source_save_month": 6
}
```

The exact schema may evolve later.
The architectural requirement is mandatory.

---

# Campaign Identity

Campaign identity should uniquely identify a running campaign.

Possible sources:
- galaxy seed
- generated campaign UUID
- stable save identity hash
- combined metadata hash

The goal is preventing:
- cross-campaign payload reuse
- stale payload contamination
- accidental application to another galaxy

---

# Empire Identity

Every decision belongs to one empire only.

Bridge/mod must verify:
- empire id exists
- empire id matches payload target
- empire identity is valid for current campaign

Rejected behavior:

```text
Empire A payload
→ accidentally applied to Empire B
```

---

# Sequence Rules

Every empire decision stream should use increasing sequence ids.

Example:

```json
{
  "empire_id": 12,
  "sequence_id": 81
}
```

Bridge must reject:
- lower sequence ids
- replayed payloads
- duplicated sequence reuse

This prevents stale overwrite problems.

---

# Save Freshness Metadata

Decisions should include:
- source save year
- source save month
- source timestamp if available

Old decisions are allowed.
But their authority may weaken over time.

Old does not mean invalid.

---

# Rejected Model

Rejected architecture:

```text
decision.json
→ blindly execute
```

This is unsafe.

Problems:
- stale payload reuse
- wrong campaign contamination
- wrong empire application
- replay attacks
- inconsistent strategic state

---

# Required Validation Flow

Required validation model:

```text
read payload
→ validate schema
→ validate campaign id
→ validate empire id
→ validate sequence id
→ validate doctrine id
→ validate payload integrity
→ apply bounded action
```

Invalid payload:
→ fallback

---

# Fallback Rules

The bridge/mod must fallback if:
- campaign id mismatch
- empire id mismatch
- corrupt payload
- malformed JSON
- unknown doctrine id
- invalid sequence id
- replayed old payload

Fallback must be safe and deterministic.

---

# Multiplayer Rule

In multiplayer:

- only host runs daemon
- only host validates payloads
- clients never process payload files
- clients receive synchronized game state only

This prevents divergent local state.

---

# Anti-Cheat Principle

The player must never experience:
- cross-save contamination
- impossible strategic reactions
- reused hidden state from another campaign

Strategic Nexus must always behave like:
- a civilization inside the current galaxy
- with memory tied to that campaign only

Not:
- a universal persistent omniscient AI.
