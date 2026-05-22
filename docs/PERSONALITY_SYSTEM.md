# Personality System
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