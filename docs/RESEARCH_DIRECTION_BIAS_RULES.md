# Strategic Nexus — Research Direction Bias Rules

## Core Rule

Strategic Nexus may influence research direction.

Strategic Nexus must not directly optimize the technology tree.

The LLM may recommend strategic research bias, but it must not behave like a perfect tech picker.

---

# Allowed Behavior

Strategic Nexus may increase or decrease broad research priorities.

Examples:

- missile technology bias
- strike craft technology bias
- shield technology bias
- armor technology bias
- point defense bias
- starbase defense bias
- economy scaling bias
- naval capacity bias
- crisis preparation bias
- espionage/intel bias
- federation/diplomacy bias
- megastructure preparation bias

This is allowed because it represents long-term strategic planning.

---

# Forbidden Behavior

Strategic Nexus must not:

- pick exact technology choices as a perfect optimizer
- use hidden enemy tech data without intel
- instantly hard-counter unseen player research
- solve the full tech tree
- force deterministic best-in-slot research
- override vanilla research behavior completely
- choose research from god-view save data

Forbidden example:

```text
Player secretly switched to carrier warfare.
AI instantly prioritizes every perfect anti-carrier technology without observing it.
```

Allowed example:

```text
Empire has high military intel and observed repeated carrier-heavy enemy fleets.
It gradually increases point defense and strike craft counter research weighting.
```

---

# Strategic Bias, Not Direct Control

Correct model:

LLM
→ recommends research direction bias
→ bounded doctrine payload
→ scripted/mod layer adjusts weights
→ vanilla AI still makes final research choice

Rejected model:

LLM
→ directly selects exact technology
→ vanilla AI bypassed

Strategic Nexus remains a strategy layer, not a research micromanager.

---

# Evidence Requirement

Research bias should be based on legitimate evidence.

Allowed evidence:

- empire personality
- ethics and civics
- current strategic doctrine
- known threats
- observed enemy fleet doctrine
- recent war losses
- crisis status
- military intel
- economic weakness
- strategic goals
- historical memory

Not allowed:

- hidden enemy tech
- hidden ship designs
- hidden economy details
- full save truth without intel

---

# Research Bias Payload

Example payload:

```json
{
  "empire_id": 12,
  "research_bias": {
    "missiles": 0.25,
    "strike_craft": 0.40,
    "point_defense": 0.30,
    "economy": 0.10
  },
  "confidence": 0.68,
  "evidence": [
    "observed_enemy_carrier_usage",
    "recent_battle_losses",
    "high_military_intel"
  ]
}
```

The values are directional weights, not hard commands.

---

# Inertia And Cooldown

Research direction must change slowly.

Suggested defaults:

- minor research bias adjustment: 3–5 years
- major research direction shift: 5–10 years
- full strategic research pivot: 10+ years

This prevents the AI from acting like an instant counter-solver.

---

# Personality Influence

Research bias should be filtered through empire identity.

Examples:

- Technocratic empire reacts with research adaptation.
- Militarist empire prioritizes weapons and naval capacity.
- Pacifist empire favors defensive and economic research.
- Paranoid empire favors sensors, starbases, and defense.
- Opportunist empire favors fast military exploitation.
- Spiritualist empire may resist purely technocratic optimization.

This creates believable variation between empires.

---

# Fallback Behavior

If evidence is weak or intel is insufficient:

- keep current bias
- reduce confidence
- use balanced research direction
- prefer personality-consistent defaults

Do not force a counter-research plan without evidence.

---

# Anti-Cheat Principle

The player must feel:

“this empire adapted its research priorities after learning from war and intel”

not:

“the AI read my hidden technology choices from the save.”
