# Strategic Nexus - Civilization Relation Model Rules

# Relation Importance Budget Rule

Civilizations must not track all other empires equally.

Each civilization should maintain only a limited set of:

* major rivals
* major allies
* existential threats
* strategically important powers
* emotionally significant civilizations

Low-relevance empires should decay into:

* simplified summaries
* weak emotional state
* low strategic attention

This is required for:

* performance
* token budget control
* believable political focus
* scalable galaxy simulation

Civilizations should care most about:

* neighbors
* rivals
* allies
* hegemonic threats
* recent enemies
* ideological opponents

not every empire equally.

---

# Emotional Abstraction Rule

Civilization emotions must remain bounded and abstracted.

Allowed relationship dimensions:

* trust
* distrust
* fear
* respect
* hostility
* dependence
* suspicion
* ideological_alignment

These should remain:

* bounded
* low-detail
* strategically interpretable
* slowly evolving

Strategic Nexus is NOT a detailed character roleplay simulator.

---

# Emotional Inertia Rule

Emotional relationships must evolve slowly.

Civilizations should not:

* rapidly love
* rapidly hate
* rapidly forgive
* rapidly forget

Relationship state should have:

* inertia
* decay
* reinforcement
* historical persistence

This creates believable geopolitical continuity.

---

# Relationship Decay Rule

Relationships decay without interaction.

Without:

* diplomacy
* wars
* espionage
* alliances
* trade
* border tension
* federation cooperation

relationship intensity should gradually weaken.

Examples:

* admiration fades
* fear weakens
* hostility softens
* trust erodes

unless reinforced by new events.

---

# Idealization And Demonization Rule

Low-information civilizations may:

* idealize allies
* mythologize federations
* exaggerate threats
* demonize rivals

especially under:

* low intel
* paranoia
* propaganda-like strategic interpretation
* trauma

However:
these interpretations must remain:

* bounded
* evidence-linked
* uncertainty-aware

Civilizations must not hallucinate arbitrary narratives without supporting events.

---

# Asymmetric Relationship Rule

Relationships are not automatically mutual.

Example:

* Empire A fears Empire B
* Empire B barely notices Empire A

This asymmetry is desirable and realistic.

Civilization perception should remain empire-scoped.

---

# Subjective Predictive Profile Rule

Relationship memory must answer two different questions:

* what happened between us
* what do we expect them to do next

Each civilization may maintain subjective predictive profiles for important other empires.

These profiles are observer-target scoped:

```text
campaign_id + observer_empire_id + target_empire_id
```

They are not objective labels on the target empire.

Allowed predictive dimensions include:

* keeps agreements
* breaks agreements
* attacks weakness
* defends allies
* abandons allies
* joins stronger coalitions
* respects strength
* escalates border tension
* seeks revenge
* trades reliably
* exploits distractions

Personality must affect interpretation.

Examples:

* a paranoid observer may treat one betrayal as strong evidence of future betrayal
* an honor-bound observer may weight oath-breaking more heavily than border friction
* a pragmatic observer may still cooperate with a predictable traitor against a greater threat
* a militarist observer may respect an aggressive rival without trusting it

Predictive profiles should drive targeted strategic rules only when confidence and evidence are sufficient.
Weak profiles should remain summaries, not generated overlay behavior.

---

# Capability Constraint Rule

Emotional hostility does not override strategic reality.

A civilization may:

* hate a hegemon
* fear a rival
* distrust an ally

while still:

* avoiding war
* appeasing
* cooperating temporarily
* remaining diplomatically cautious

Capability constraints always remain important.

---

# Strategic Readability Rule

Civilizations should remain strategically readable.

Avoid:

* excessive emotional complexity
* giant narrative histories
* unstable psychological simulation

Prefer:

* a few strong rivalries
* a few strong alliances
* a few persistent fears
* a few meaningful historical traumas

The system should generate believable geopolitical behavior without exploding complexity.
