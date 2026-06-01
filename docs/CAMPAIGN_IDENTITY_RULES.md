# Strategic Nexus - Campaign Identity And Payload Validation Rules

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
- archived first-save hash as a local fallback
- stable player empire and empire roster fingerprint when available

The goal is preventing:
- cross-campaign payload reuse
- stale payload contamination
- accidental application to another galaxy

Campaign identity must be derived without editing saves.

If identity confidence is low, Strategic Nexus should create a new campaign profile or ask the user before merging histories.
It must not silently merge two campaigns because they share a filename or player empire name.

Ordinary Stellaris mod script should not be assumed to expose the active save filename, campaign id, archive path, or galaxy seed.
Campaign recognition inside the mod should use persistent scripted markers that the base mod placed into normal save state earlier.
The companion app may combine those markers with external archived-save metadata and hashes.

Campaign-specific generated rules must be gated by marker checks.
If the loaded save does not expose the expected marker, the generated campaign rules must be ignored and the base mod must use fallback behavior.

Campaign-specific generated rules are not enough by themselves.
Rules should also be scoped to a known save entry point or compatible save-state range.
The user can load an older manual save, autosave, or ironman save after SNC generated new rules.
If the loaded save does not match the expected entry-point fingerprint, in-game date/state, or marker facts, rules produced from a later branch must remain inert.

The active generated mod overlay may contain rules for multiple known campaigns, but each ruleset must remain marker-guarded.
It may also contain multiple rulesets for different known entry points within the same campaign, as long as each ruleset is bounded and clearly gated.
The companion app should remove campaign rules from the active generated overlay when the corresponding local Stellaris save campaign is no longer present.
This cleanup must not delete archived Strategic Nexus memory unless explicitly requested.
Strategic Nexus campaign memory is durable on-disk app state; the active generated mod overlay is only a rebuildable projection of that memory.
Generated campaign rules must be published as a complete replacement snapshot.
The app must not append new rules indefinitely on each analysis run.

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
-> accidentally applied to Empire B
```

The same archived save sequence may be analyzed once per empire perspective.
This is required because each empire interprets history differently.
Shared source data does not imply shared memory or shared personality.

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
-> blindly execute
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
-> validate schema
-> validate campaign id
-> validate empire id
-> validate sequence id
-> validate doctrine id
-> validate payload integrity
-> apply bounded action
```

Invalid payload:
-> fallback

---

# Fallback Rules

The bridge/mod must fallback if:
- campaign id mismatch
- campaign marker mismatch
- campaign marker missing when campaign-specific rules require it
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
- generated gameplay-affecting mod overlays must remain byte-identical for host and clients unless a future package format proves a safe split

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
