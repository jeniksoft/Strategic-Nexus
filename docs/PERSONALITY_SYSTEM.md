# Personality System

## Campaign Scope

Every personality belongs to exactly one empire in exactly one campaign.

Identity key:

```text
campaign_id + empire_id
```

When Strategic Nexus detects an archived autosave from an unknown campaign, it must create a new personality profile for every detectable empire in that campaign.

Unknown campaign is treated as a zero-history campaign, not as a campaign without rules.
Each detected empire receives a bounded bootstrap personality that is marked as zero-history and later becomes history-backed only after validated archive evidence exists.

The same empire template in a different campaign is a different personality instance.

See `CAMPAIGN_EMPIRE_PERSONALITY_BOOTSTRAP_RULES.md` for bootstrap and evolution rules.

---

## Zero-History Variation

Default personalities must vary across generated policy-pack updates.

The variation must be deterministic, manifest-visible, and constrained by empire facts such as ethics, civics, authority, species traits, origin, and empire type.
It must not be a fixed slot table where the same empire ordinal always receives the same personality.

Once a campaign has durable history, the established campaign-empire personality must not be rerolled on every mod update.
History-backed continuity wins over bootstrap novelty.

Human-controlled empire personality is an empire-state interpretation, not a personal profile of the real player.

- cruelty
- paranoia
- opportunism
- honor
- fanaticism
- greed
- diplomatic tendency

These represent civilization identity.

Example:

{
  "boldness": 0.82,
  "paranoia": 0.44,
  "honor": 0.18
}

---

## Adaptive State

Traits influenced by history:
- trust
- fear
- trauma
- aggression drift
- federation loyalty
- hatred
- isolationism

These evolve dynamically.

Example:

{
  "fear_of_player": 0.77,
  "war_trauma": 0.61,
  "trust_in_federations": 0.12
}

---

## Historical Memory

Important events are stored as abstract historical entries.

Example:

{
  "year": 2331,
  "event": "federation_betrayal",
  "actor": "Empire_12",
  "target": "Empire_04",
  "severity": 0.82
}

---

## Behavioral Goal

The player should feel:
- civilizations have character
- civilizations remember history
- civilizations react consistently
- civilizations possess strategic intent

NOT:
- random behavior
- unstable personality shifts
- artificial scripted reactions
