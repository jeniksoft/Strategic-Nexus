# Strategic Nexus — Probabilistic Decision Sampling Rules

## Core Rule

Strategic Nexus civilizations should not behave as perfectly deterministic strategic solvers.

Instead, civilizations should operate using:

* bounded probabilistic reasoning
* weighted strategic preferences
* controlled nondeterministic behavior
* personality-constrained decision sampling

Civilizations should feel:

* believable
* imperfect
* adaptive
* psychologically continuous

not:

* mechanically deterministic
* mathematically rigid
* perfectly optimized

---

# Design Philosophy

Human strategic behavior is not perfectly deterministic.

Given the same situation:

* similar civilizations may react differently
* the same civilization may react differently at different times
* uncertainty influences decisions
* personality influences decisions
* emotional state influences decisions

Strategic Nexus should simulate:

* strategic preference distributions
* bounded uncertainty
* controlled variation

rather than:

* single guaranteed outputs

---

# Distribution Output Principle

The LLM should preferably output:

* weighted strategic preferences
* probability distributions
* strategic confidence ranges

instead of:

* single hardcoded mandatory choices

Example:

```json id="u5a8km"
{
  "military_posture": {
    "defensive": 0.55,
    "balanced": 0.30,
    "aggressive": 0.15
  }
}
```

The daemon or decision sampler then selects the final bounded action.

---

# Separation Of Reasoning And Sampling Rule

The LLM should:

* reason
* evaluate
* estimate strategic preference

The daemon should:

* validate
* normalize
* apply constraints
* perform controlled sampling
* enforce architecture rules

The LLM itself should not directly execute uncontrolled random behavior.

---

# Bounded Decision Space Rule

Sampling may only occur within:

* validated schema values
* approved doctrine enums
* bounded strategic categories
* legal gameplay actions

The system must never sample:

* undefined actions
* unrestricted commands
* invalid schema outputs
* unsafe runtime behavior

Validation layers remain authoritative.

---

# Personality Continuity Rule

Probabilistic behavior must remain bounded by civilization identity.

Civilizations may vary:

* tactically
* emotionally
* strategically

but must remain recognizable over time.

Examples:

Allowed:

* militarist empire alternates between defensive buildup and offensive pressure

Forbidden:

* fanatic pacifist suddenly behaving like genocidal fanatic militarist without historical cause

Probability sampling must preserve long-term identity continuity.

---

# Ethics Constraint Rule

Ethics constrain probability distributions.

Examples:

* pacifists heavily weight defensive choices
* militarists heavily weight aggressive postures
* xenophobes heavily weight suspicion
* spiritualists heavily weight psionic directions

Low-probability edge behavior may exist,
but ethics must strongly shape distributions.

---

# Memory Influence Rule

Civilization memory should influence probability weights.

Examples:

* repeated betrayal increases suspicion weighting
* traumatic defeat increases defensive weighting
* successful expansion increases aggressive confidence
* economic collapse increases stability preference

Memory should shape future probability distributions gradually over time.

---

# Confidence And Sampling Rule

High-confidence strategic situations may produce:

* narrow distributions
* more deterministic outcomes

Uncertain situations may produce:

* wider distributions
* greater behavioral variation

Example:

Clear existential threat:
→ mostly defensive response

Ambiguous diplomacy:
→ wider range of possible reactions

---

# Strategic Inertia Rule

Probability distributions should evolve slowly.

Avoid:

* massive distribution swings every update
* unstable personality drift
* chaotic strategic oscillation

Civilizations should exhibit:

* behavioral continuity
* strategic inertia
* recognizable patterns

over long campaigns.

---

# Extreme Action Gate Rule

High-impact strategic actions require additional gating.

Examples:

* ethics reform
* ascension direction shift
* existential wars
* federation betrayal
* government transformation
* genocidal escalation

These actions should require:

* strategic justification
* threshold checks
* historical pressure
* additional validation

Low random probability alone is insufficient.

---

# Decision Sampling Layer Rule

Decision sampling should occur in a dedicated controlled layer.

Responsibilities:

* normalization
* validation
* weighted sampling
* cooldown enforcement
* strategic gating
* replay logging
* multiplayer synchronization

Sampling logic must not remain implicit inside raw prompts.

---

# Seeded Sampling Rule

Strategic Nexus should support optional deterministic sampling seeds.

Possible uses:

* debugging
* replay analysis
* regression testing
* multiplayer synchronization
* developer reproducibility

Normal gameplay may still include nondeterministic variation.

---

# Replay Compatibility Rule

Replay systems should record:

* probability distributions
* selected outcomes
* sampling seed
* gating decisions
* validation results

Replay systems do not require identical future inference output.

They require:

* explainability
* auditability
* bounded behavioral continuity

---

# Multiplayer Rule

In multiplayer:

* sampling occurs only on host
* selected decisions become authoritative
* clients do not independently sample outcomes

This preserves:

* synchronization stability
* deterministic runtime state
* replay consistency

---

# Anti-Solver Principle

Strategic Nexus civilizations should not always choose:

* mathematically optimal
* perfectly efficient
* globally dominant
* meta-optimal strategies

Civilizations should instead exhibit:

* hesitation
* ideology
* fear
* ambition
* trauma
* imperfect information
* strategic personality

Controlled imperfection is desirable.

---

# Strategic Diversity Rule

Probability distributions should help prevent:

* civilization homogenization
* strategy convergence
* repetitive meta behavior
* identical empire evolution

Different civilizations should naturally diverge over time.

---

# Controlled Randomness Principle

Randomness must remain:

* bounded
* explainable
* identity-consistent
* strategically meaningful

Avoid:

* chaotic randomness
* personality destruction
* irrational spam behavior
* unstable doctrine collapse

Randomness is a flavoring mechanism, not a replacement for strategy.

---

# Future Evolution Rule

Probability systems may evolve over time through:

* telemetry
* testing
* campaign analysis
* balance refinement
* retrieval improvements
* doctrine tuning

Initial implementations should remain conservative.

Behavioral complexity may gradually increase as the architecture matures.

---

# Design Goal

Strategic Nexus civilizations should behave like:

```text id="1t8mwr"
bounded historical civilizations making probabilistic strategic decisions under uncertainty
```

not:

```text id="w7n2cz"
perfect deterministic optimization solvers executing mathematically ideal strategies
```

Controlled probabilistic reasoning is one of the core mechanisms that makes civilizations feel alive rather than algorithmic.
