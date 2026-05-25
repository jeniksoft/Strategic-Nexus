# Strategic Nexus - Doctrine Inertia Rules

## Core Rule

Strategic Nexus must not behave like an instant optimizer.

Empires must not change strategic doctrine every time a slightly better option appears.

Strategic doctrine changes must have:
- inertia
- delay
- commitment
- cultural resistance
- political resistance
- historical bias

The goal is to simulate civilizations, not a perfect solver.

---

# Rejected Behavior

Forbidden pattern:

new save
-> LLM finds slightly better doctrine
-> empire immediately switches doctrine
-> next save changes again

This creates:
- unstable AI
- optimizer behavior
- artificial feeling
- unrealistic fleet and research shifts
- player perception of cheating

---

# Required Behavior

Strategic Nexus doctrines must persist until there is a strong reason to change.

Allowed doctrine change reasons:

- major military defeat
- repeated losses against the same enemy doctrine
- crisis arrival
- existential threat
- major geopolitical shift
- new hegemon appears
- federation collapse
- ideology-driven escalation
- long-term strategic review
- high-confidence intel update

Weak signals must not cause immediate doctrine switching.

---

# Doctrine Commitment

Each empire should maintain doctrine commitment metadata.

Example:

```json
{
  "empire_id": 12,
  "current_doctrine": "defensive_posture",
  "doctrine_started_year": 2241,
  "minimum_commitment_years": 10,
  "commitment_strength": 0.78,
  "change_pressure": 0.34
}
```

Doctrine change should require `change_pressure` to exceed commitment threshold.

---

# Doctrine Cooldowns

Each doctrine should have a minimum lifetime.

Suggested defaults:

- minor posture adjustment: 3-5 years
- fleet doctrine shift: 10-15 years
- research direction shift: 5-10 years
- ideology-level grand strategy shift: 20+ years

These are design defaults, not fixed final numbers.

---

# Strategic Inertia Sources

Doctrine inertia may come from:

- empire personality
- ethics
- civics
- government type
- recent victories
- recent defeats
- institutional pride
- military tradition
- ruler agenda
- historical trauma
- paranoia
- fanaticism
- bureaucracy

Examples:

- Militarist empire may resist abandoning artillery doctrine.
- Pacifist empire may delay offensive doctrine even under pressure.
- Paranoid empire may overcommit to fortress doctrine.
- Fanatic purifier may refuse diplomatic containment doctrine.
- Technocratic empire may prefer research bias over immediate militarization.

---

# Partial Adaptation

Doctrine changes should not always be binary.

Preferred model:

- keep main doctrine
- add secondary pressure
- slowly adjust weights
- gradually drift toward new doctrine

Example:

```json
{
  "primary_doctrine": "carrier_doctrine",
  "secondary_doctrine_pressure": {
    "point_defense": 0.35,
    "torpedo_response": 0.22
  }
}
```

This allows adaptation without unrealistic instant switching.

---

# Old Decisions

Old decisions are not invalid merely because they are old.

Stale doctrine should:
- lose intensity slowly
- remain active until replaced
- represent institutional inertia

It should not instantly fall back unless invalid or corrupt.

---

# LLM Instruction

The LLM must be instructed that doctrine changes are expensive.

It should prefer:
- explanation
- confidence
- evidence
- gradual adaptation
- inertia-aware recommendations

The LLM must not recommend doctrine change without sufficient evidence.

---

# Anti-Cheat Principle

Fast perfect adaptation looks like cheating.

Slow imperfect adaptation feels like civilization behavior.

Strategic Nexus should produce:
- believable strategic drift
- delayed response
- doctrine loyalty
- institutional mistakes
- historical continuity

Not:
- instant hard counters
- perfect solver response
- doctrine spam
- tactical optimization loop
