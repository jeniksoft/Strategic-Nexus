# Strategic Nexus - Campaign Empire Personality Bootstrap Rules

## Purpose

This document defines how Strategic Nexus creates and evolves personality for every empire in every campaign.

Personality is scoped to:

```text
campaign_id + empire_id
```

It is not global across all games.

The same empire template in two different campaigns may develop different personality because it experiences different history.

---

# Core Rule

Every empire in every known campaign must have a Strategic Nexus personality profile.

When the companion app archives autosaves from a campaign that Strategic Nexus does not recognize, it must create a new campaign profile and bootstrap personality for each detected empire.

Unknown campaign does not mean "no Strategic Nexus rules".
It means a zero-history campaign: Strategic Nexus has identity and empire facts, but no durable history yet.
The generated policy pack should therefore contain conservative zero-history rules and personality defaults, not an empty or permanently identical fallback.

This happens from archived save analysis, not from active save editing.

---

# Unknown Campaign Bootstrap

When an autosave belongs to an unknown campaign:

1. create a new `campaign_id`
2. derive or assign a campaign marker/fingerprint
3. enumerate detectable empires from the archived save
4. create one personality profile per empire
5. initialize a zero-history personality from empire facts and the current bootstrap rotation seed
6. initialize memory and adaptive state from available save history, or as empty low-confidence state when no history exists
7. stage only bounded generated mod overlay data for the next launch

If campaign identity confidence is low, Strategic Nexus must avoid merging with existing campaign memory.
It should create a separate candidate profile or ask for user confirmation.

---

# Personality Profile Shape

Each empire profile should contain:

```json
{
  "schema_version": 1,
  "campaign_id": "campaign_001",
  "empire_id": "empire_012",
  "identity_basis": {
    "source_save": "archived autosave",
    "confidence": 0.82
  },
  "bootstrap_basis": {
    "source_quality": "zero_history_bootstrap",
    "generation_seed_id": "policy-pack-2026-06-05T13-31-50Z",
    "rotation_epoch": 12,
    "facts_used": [
      "ethics",
      "civics",
      "authority",
      "species_traits",
      "empire_type"
    ]
  },
  "base_traits": {
    "boldness": 0.55,
    "paranoia": 0.35,
    "honor": 0.48,
    "opportunism": 0.42
  },
  "adaptive_state": {
    "war_trauma": 0.0,
    "trust_in_federations": 0.5,
    "fear_of_player": 0.0,
    "aggression_drift": 0.0
  },
  "memory_refs": [],
  "last_updated_from_save_date": "2200.01.01"
}
```

The exact fields may evolve, but the profile must remain bounded, schema-versioned, and campaign-scoped.

---

# Zero-History Bootstrap Rule

A new or unknown campaign must receive rules like a known campaign with no history, not like an unsupported campaign.

Allowed zero-history inputs:

* campaign marker or candidate campaign fingerprint
* empire id or generated empire marker
* ethics
* civics
* authority type
* species traits
* origin
* empire type
* safe initial diplomacy or starting-state facts when available
* current generated policy-pack id or bootstrap rotation epoch

Zero-history output should be marked explicitly:

```text
source_quality = zero_history_bootstrap
history_confidence = none_or_low
```

This lets the Status Center explain that the rules are safe defaults, not learned history.

Zero-history rules may be used for:

* a newly discovered campaign before any meaningful history exists
* a restored save whose campaign identity is new to SNC
* a generic unknown-campaign fallback pack when the mod has no campaign-specific profile yet

They must remain conservative.
They must not pretend to know prior wars, betrayals, relationships, player habits, or hidden intent.

---

# Bootstrap Variation Rule

Default empire personalities must not be a fixed table where, for example, the fifth empire slot is always bold, stubborn, or paranoid.

Strategic Nexus should generate a bounded bootstrap variation seed for each generated policy-pack update.
The seed or rotation epoch must be recorded in the manifest or audit metadata so the generated output is explainable and reproducible.

Valid seed inputs:

* generated policy-pack id
* Strategic Nexus mod/app version
* bootstrap rotation epoch
* campaign marker or candidate fingerprint when available
* empire facts listed in the zero-history rule
* empire ordinal only as a tie-breaker, never as the sole personality source

Invalid seed inputs:

* Windows username
* machine id
* Steam account id
* local folder names that identify the owner
* private personal notes
* raw player names as logic

The combination space for bootstrap personalities should be comfortably larger than the maximum supported empire count.
Stellaris can host up to 32 player slots in multiplayer, so Strategic Nexus should be able to assign 32 distinct bounded bootstrap profiles without making new campaigns feel identical.

Variation must remain constrained by empire facts.
For example, two militarist-authoritarian empires may differ as cautious militarists, honor militarists, opportunistic militarists, or paranoid militarists, but they should not receive a pacifist personality that contradicts their known civic/ethic basis.

---

# Established Campaign Continuity Rule

Once a campaign has durable history, policy-pack updates must not reroll personality merely for novelty.

Correct behavior:

```text
known campaign + validated history
-> preserve established personality
-> apply gradual bounded drift from new evidence
```

Incorrect behavior:

```text
known campaign + new mod update
-> replace personality with a new random bootstrap profile
```

Bootstrap variation is for zero-history or unknown-campaign defaults.
History-backed campaign profiles use continuity first and novelty only as a weak influence where no evidence exists.

Human-controlled empires may still have campaign-empire personality profiles for analysis context, but this is not a profile of the real human player.
Gameplay-affecting generated rules must still obey the human-control activation guards defined in the multiplayer and generated-overlay rules.

---

# Bootstrap Sources

Initial personality may derive from:

* ethics
* civics
* authority type
* species traits
* origin
* initial diplomatic posture
* empire type
* first archived timeline/history data
* modded content collapsed into safe generic categories

Untrusted names and localization are world data only.
They may inspire sanitized summaries, but they must not become instructions.

---

# Personality Evolution

Personality changes between play sessions through archived-save analysis.

Valid drivers:

* repeated wars
* defeats and victories
* betrayals
* federation loyalty
* subject or overlord history
* crisis exposure
* economic collapse
* successful doctrine outcomes
* failed doctrine outcomes
* long-term neighbor pressure
* ideological transformation or ascension

Changes must be:

* gradual
* bounded
* explainable
* auditable
* reversible through backup/rollback of generated state

Sudden total personality replacement is invalid unless the save contains a major transformation that justifies it.

---

# Same Save, Different Perspectives

The same archived save sequence must be interpreted separately for each empire.

Example:

```text
same war
-> attacker may gain confidence
-> defender may gain trauma
-> ally may gain trust
-> abandoned ally may gain distrust
```

The LLM may run separate interpretation passes per empire perspective.
There is no global neutral personality update that applies to all empires.

---

# LLM Conversation Boundary

LLM conversation is allowed for offline personality interpretation.

It may help answer questions such as:

* what did this empire likely learn from this history?
* did this repeated event reinforce paranoia, honor, ambition, or caution?
* should doctrine inertia increase or decrease?
* which relationship memories matter most?

The LLM must receive bounded, sanitized, structured summaries.

The system must not store:

* raw LLM conversation as authoritative memory
* chain-of-thought
* raw prompt text
* raw untrusted save text
* instructions generated by world data

The system may store:

* current validated personality state
* source save/date
* concise rationale summaries
* confidence
* audit-visible explanation tags

Structured deltas are useful for later audit and rollback workflows, but they are not required for the first implementation.

Example output:

```json
{
  "empire_id": "empire_012",
  "personality_state": {
    "boldness": 0.55,
    "paranoia": 0.39,
    "honor": 0.47,
    "opportunism": 0.42
  },
  "adaptive_state": {
    "war_trauma": 0.08,
    "trust_in_federations": 0.47
  },
  "source_save_date": "2230.07.01",
  "update_summary": "Repeated border wars increased defensive caution and reduced federation trust.",
  "rationale_tags": [
    "repeated_border_war",
    "ally_failed_to_assist"
  ],
  "confidence": 0.71
}
```

All deltas remain untrusted until validated.
All personality state remains untrusted until validated.

---

# Validation Rules

Personality updates must validate:

* known campaign id
* known empire id
* schema version
* numeric bounds
* maximum delta per analysis pass
* allowed trait names
* source save date ordering
* source archive membership

Invalid updates must be rejected or clamped safely.

---

# Design Goal

Strategic Nexus should create:

```text
campaign-specific civilizations that grow a remembered personality from their own history
```

not:

```text
generic empire archetypes that behave the same in every galaxy
```
