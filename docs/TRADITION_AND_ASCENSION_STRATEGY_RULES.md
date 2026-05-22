# Strategic Nexus — Tradition And Ascension Strategy Rules

## Core Rule

Traditions and Ascension Perks are long-term civilization commitments.

Strategic Nexus must treat them as:

* high-inertia decisions
* civilization identity commitments
* strategic path dependencies
* long-term ideological direction

Traditions and Ascension Perks must NOT be treated as:

* short-term optimizations
* rapidly adjustable strategy layers
* reactive tactical tools

---

# Design Philosophy

Traditions and Ascension Perks are among the most civilization-defining systems in Stellaris.

They strongly influence:

* economy
* military capability
* technological direction
* ideology
* diplomacy
* expansion style
* long-term empire evolution

This makes them highly suitable for LLM-guided strategic planning.

---

# Long-Term Commitment Rule

Traditions and Ascension Perks are effectively irreversible.

Once selected:

* they become part of civilization identity
* they constrain future strategic evolution
* they influence future recommendations
* they become historical commitments

The LLM must account for:

* existing traditions
* existing perks
* existing ascension direction
* long-term compatibility

The system must avoid:

* incoherent strategic pivots
* contradictory progression
* meta-style reactive build swapping

---

# Path Dependency Rule

Past strategic choices constrain future choices.

Example:

A civilization heavily invested into:

* unity
* psionics
* fortress defense

should not suddenly behave like:

* hyper-aggressive synthetic expansionists

unless:

* major historical transformation occurred
* civilization identity drift is justified
* long-term transition pressure accumulated

Strategic evolution should feel historically continuous.

---

# Allowed Strategic Influence

Strategic Nexus may influence:

* tradition priority
* long-term ascension direction
* ideological strategic focus
* doctrine compatibility
* strategic specialization preference

Example:

```json id="2s4v8w"
{
  "tradition_priority": [
    "discovery",
    "prosperity",
    "supremacy"
  ],

  "ascension_direction": "psionic"
}
```

The LLM should primarily recommend:

* direction
* preference
* strategic alignment

not raw unrestricted gameplay commands.

---

# Forbidden Behavior

The LLM must NOT:

* constantly re-evaluate traditions aggressively
* optimize every choice for short-term efficiency
* ignore previous strategic commitments
* create incoherent build paths
* behave like a multiplayer meta optimizer

Strategic Nexus civilizations should feel:

* ideological
* historically shaped
* path dependent
* imperfect

not:

* mechanically optimized build calculators

---

# Tradition Strategy Domains

Possible future domains:

## tradition_focus

Possible values:

* economic_growth
* military_supremacy
* technological_acceleration
* defensive_survival
* diplomatic_integration
* unity_cohesion
* expansionist_growth
* crisis_preparation

---

## ascension_direction

Possible values:

* psionic
* synthetic
* biological
* hybrid_unknown
* megastructure_focus
* federation_focus
* imperial_hegemony

---

# Capability Constraint Rule

Tradition and perk planning must respect:

* economy
* unity generation
* research capability
* infrastructure
* empire size
* strategic environment
* military pressure
* crisis state

A civilization may desire:

* technological ascension

but lack:

* research capability
* economic stability
* safety to pursue it

The LLM must consider realistic execution feasibility.

---

# Personality Interaction Rule

Different civilizations should prefer different strategic paths.

Examples:

## Spiritual Civilization

May prefer:

* unity traditions
* psionic direction
* ideological stability
* defensive continuity

---

## Technocratic Civilization

May prefer:

* discovery
* technological acceleration
* synthetic direction
* research specialization

---

## Militarist Civilization

May prefer:

* supremacy
* aggressive expansion
* military economy
* fleet doctrine synergy

---

## Megacorp

May prefer:

* trade
* diplomacy
* economic scaling
* soft-power influence

---

## Hive Mind

May prefer:

* rapid adaptation
* biological specialization
* expansion sustainability

---

# Memory Interaction Rule

Civilizations should remember:

* successful tradition paths
* failed strategic specialization
* traumatic overextension
* successful defensive doctrines
* economic collapse from poor scaling

Historical experience should influence future strategic preference.

---

# Strategic Inertia Rule

Tradition and ascension planning should evolve slowly.

Review frequency should remain:

* low-frequency
* milestone-driven
* historically grounded

The system should avoid:

* constant reevaluation
* unstable strategic identity
* rapid ideological oscillation

---

# War Pressure Rule

War and crisis may influence future tradition/perk preference.

Examples:

* repeated invasions → defensive traditions
* technological inferiority → research focus
* economic collapse → prosperity focus
* crisis trauma → survival-oriented perks

However:
temporary pressure should not instantly override civilization identity.

---

# Anti-Optimizer Principle

The strongest theoretical build is not automatically the correct civilization choice.

Examples:

* ideological rigidity
* trauma
* paranoia
* honor culture
* strategic fear
* tradition inertia

may all produce:

* imperfect
* suboptimal
* historically believable
  choices.

This is desirable behavior.

---

# Multiplayer Rule

In multiplayer:

* host performs strategic tradition reasoning
* clients do not generate independent strategic planning
* synchronized state remains authoritative

This preserves:

* consistency
* multiplayer safety
* deterministic synchronization

---

# Auditability Rule

Tradition and perk decisions should be auditable.

Examples:

* why a direction was preferred
* why a perk path was rejected
* what historical factors influenced the choice
* what capability constraints existed

Audit summaries should remain:

* concise
* structured
* strategically readable

Do NOT store chain-of-thought.

---

# Future Expansion Rule

This system is intended to integrate later with:

* strategic memory
* civilization personality
* espionage interpretation
* war planning
* diplomacy
* crisis behavior
* doctrine systems

Tradition planning should become one of the major long-term identity systems of Strategic Nexus.

---

# Design Goal

Strategic Nexus civilizations should feel like:

```text id="5v8a2n"
historical civilizations following long-term ideological and strategic development paths
```

not:

```text id="4e2k0f"
stateless meta-build optimizers recalculating perfect perk choices every few years
```

Traditions and Ascension Perks are one of the strongest long-term identity anchors in the entire architecture.

---

# Ethics Appendix

# Ethics Priority Rule

Ethics are primary civilization constraints and strategic preference drivers.

Ethics must strongly influence:

* tradition preference
* ascension direction
* war philosophy
* diplomacy
* federation behavior
* espionage behavior
* economic priorities
* crisis response
* doctrine selection
* strategic risk tolerance

Ethics are not cosmetic flavor.

They are one of the core strategic identity systems of a civilization.

---

# Ethics Inertia Rule

Ethics have higher inertia than normal strategic doctrine.

Doctrine may:

* adapt
* evolve
* react to circumstances

Ethics should remain:

* highly persistent
* civilization-defining
* historically stable

A civilization may temporarily behave defensively,
but a militarist civilization should still remain fundamentally militaristic.

---

# Ethics And Strategic Preference Examples

## Militarist

Possible tendencies:

* offensive doctrine preference
* military buildup
* lower war aversion
* strategic intimidation
* supremacy traditions
* aggressive rivalry posture

---

## Pacifist

Possible tendencies:

* defensive doctrine
* alliance preference
* appeasement behavior
* infrastructure investment
* fortress strategy
* war exhaustion sensitivity

---

## Xenophile

Possible tendencies:

* federation preference
* diplomacy focus
* trust growth
* trade cooperation
* multicultural integration
* alliance stability

---

## Xenophobe

Possible tendencies:

* border defensiveness
* suspicion
* strategic isolation
* territorial rigidity
* distrust of outsiders
* reduced alliance preference

---

## Materialist

Possible tendencies:

* research prioritization
* synthetic preference
* technological escalation
* optimization behavior
* technocratic development

---

## Spiritualist

Possible tendencies:

* unity focus
* psionic preference
* ideological rigidity
* tradition preservation
* cultural continuity

---

## Egalitarian

Possible tendencies:

* federation support
* decentralized cooperation
* alliance-oriented diplomacy
* internal stability preference

---

## Authoritarian

Possible tendencies:

* hegemonic ambition
* centralized strategic planning
* subject management focus
* power projection

---

# Ethics Interaction Rule

Ethics may reinforce or conflict with:

* personality
* memory
* trauma
* doctrine
* current strategic pressure

Examples:

Militarist + trauma:
→ revenge-oriented militarization

Pacifist + existential crisis:
→ defensive mobilization without aggressive expansion

Materialist + technological inferiority:
→ intensified research focus

Spiritualist + synthetic threat:
→ ideological hostility

Civilizations should feel internally coherent.

---

# Ethics Constraint Rule

Ethics should constrain otherwise optimal decisions.

Examples:

* pacifists may avoid efficient aggressive wars
* xenophobes may reject beneficial diplomacy
* spiritualists may distrust synthetic ascension
* egalitarians may resist oppressive subject systems

This is desirable behavior.

Strategic Nexus prioritizes:

* believable civilization identity
* historical continuity
* ideological consistency

over:

* pure optimization

---

# Ethics And Long-Term Identity

Ethics should become one of the strongest long-term anchors for:

* civilization personality
* strategic direction
* geopolitical behavior
* alliance patterns
* doctrine evolution
* ascension planning

Civilizations with different ethics should gradually feel strategically different even under similar external conditions.

---

# Ethics Appendix Design Goal

Strategic Nexus civilizations should feel like:

```text id="2kn4xp"
ideological civilizations shaped by persistent ethical worldviews
```

not:

```text id="1o6wqy"
generic optimization systems with cosmetic ethics labels
```
