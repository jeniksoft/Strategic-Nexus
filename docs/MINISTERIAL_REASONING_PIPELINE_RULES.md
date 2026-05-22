# Strategic Nexus — Ministerial Reasoning Pipeline Rules

## Core Rule

Strategic Nexus should prefer:

* serial domain-specific reasoning passes
* modular agenda evaluation
* bounded policy review

instead of:

* one massive monolithic civilization prompt

Civilizations should reason more like:

* distributed state institutions
* ministries
* policy departments
* cabinet review systems

not:

* one giant omniscient reasoning entity

---

# Design Philosophy

Real governments do not process all strategic domains simultaneously inside one unified thought process.

Instead:

* specialized institutions evaluate their own domains
* proposals are reviewed collectively
* conflicts are negotiated
* policies evolve incrementally

Strategic Nexus should follow similar principles.

This improves:

* scalability
* auditability
* stability
* performance
* strategic continuity

---

# Ministerial Reasoning Principle

Each strategic domain may reason independently.

Possible ministries:

* military ministry
* economy ministry
* diplomacy ministry
* intelligence ministry
* internal governance ministry
* faction ministry
* scientific ministry
* strategic memory analysis ministry

Each ministry receives:

* bounded domain-specific context
* relevant memory retrieval
* current strategic state
* existing agenda
* cabinet feedback if applicable

Ministries should not receive the entire civilization context.

---

# Ministry Proposal Rule

Each ministry should produce:

* agenda proposals
* suggested modifications
* risk assessment
* confidence estimation
* expected consequences
* strategic rationale

Ministries may also recommend:

* no change
* partial change
* delayed action
* further observation

No change is a valid strategic decision.

---

# Ministerial Brief Extractor / Clerk Layer

Between ministry input preparation and cabinet review there may be a non-strategic extraction layer.

The clerk layer does not primarily summarize ministry reasoning output.

Instead, it reduces the ministry input context into a compact cabinet-readable input brief.

Pipeline:

```text
X1..Xn = ministry input contexts
Y1..Yn = ministry outputs / agenda proposals
U1..Un = clerk-reduced ministry input briefs

cabinet receives:
Y1..Yn + U1..Un
```

The ministry performs domain reasoning over X and produces Y.

The clerk compresses X into U.

The cabinet evaluates Y in light of U without reprocessing full X.

This allows the cabinet to understand what led the ministry to its proposal without repeating ministry-level reasoning.

This layer behaves like:

* a clerk
* a compiler
* a document normalizer
* an administrative compressor

not like:

* a ministry
* a cabinet member
* a strategist
* a policy advisor
* an autonomous agent

Its task is to transform large ministry input contexts into standardized compact cabinet-readable input briefs.

The clerk layer does not decide.

The clerk layer does not:

* redo ministry reasoning
* improve proposals
* invent missing arguments
* add strategic interpretation
* optimize proposals for cabinet approval
* choose between policy options
* rewrite ministry intent
* prioritize based on hidden context

The clerk layer only extracts, reduces, and normalizes information present in the ministry input context according to explicit extraction rules.

---

# Clerk Context Isolation Rule

The clerk layer should receive minimal context.

Allowed context:

* extraction rules
* output schema
* formatting constraints
* current ministry input

The clerk layer should not receive:

* long-term empire memory
* cabinet private context
* other ministry conversations
* hidden strategic goals
* ideology weighting
* historical trauma memory
* gameplay state outside the provided ministry input

Reason:
additional context may corrupt the clerk role.

A clerk with strategic context may begin to interpret, correct, prioritize, or politically reshape the ministry input brief.

The ideal clerk is a worker without opinion.

---

# Clerk Extraction Rule

The clerk should extract:

* proposal summary
* requested decision
* stated reasons
* explicit risks
* explicit uncertainties
* alternatives mentioned
* rejected alternatives
* expected consequences
* confidence level
* dependencies
* conflicts mentioned by the ministry
* missing data explicitly not provided
* unchanged agenda if no change is recommended

The clerk should preserve fixed output ordering.

The clerk should not dynamically reorder sections according to perceived importance.

Importance is strategic interpretation unless explicitly stated by the ministry or mechanically derived by fixed schema rules.

---

# Deterministic Brief Ordering Rule

Cabinet proposal briefs should preserve deterministic section ordering.

The clerk layer should not dynamically reorder sections based on:

* perceived importance
* emotional weighting
* urgency interpretation
* strategic preference
* estimated relevance
* rhetorical emphasis

Reason:
dynamic ordering introduces hidden interpretation bias.

Two identical ministry input contexts should produce structurally identical cabinet briefs.

This improves:

* auditability
* replay consistency
* multiplayer synchronization
* debugging
* proposal comparison
* cabinet stability

The clerk layer should follow a fixed canonical ordering.

Example:

```text
1. requested_action
2. current_agenda_state
3. stated_reasons
4. expected_consequences
5. explicit_risks
6. explicit_uncertainties
7. alternatives_considered
8. rejected_alternatives
9. dependencies
10. strategic_conflicts
11. missing_information
12. confidence_estimate
```

The ordering itself should not imply strategic priority.

Cabinet interpretation occurs after extraction.

The clerk layer should remain structurally neutral.

---

# Deterministic Serialization Principle

Proposal briefs should be serializable into stable structured formats.

Possible formats:

* JSON
* compact schema payloads
* deterministic markdown
* replay-safe policy records

Avoid:

* freeform formatting drift
* stylistic restructuring
* emotionally weighted summaries
* dynamically reordered narratives

Stable serialization improves:

* replay validation
* diff comparison
* deterministic testing
* multiplayer consistency
* long-term archival stability

The architecture should prefer:

```text
ministry reasoning variability
+
clerk serialization stability
```

rather than:

```text
variability at every layer
```

---

# Clerk Non-Invention Rule

The clerk may write:

```text
The ministry states X.
```

The clerk may not write:

```text
X is probably important.
```

unless importance was explicitly stated in the ministry input or mechanically derived by a fixed schema rule.

If required information is absent, the clerk must write:

```text
Not specified in ministry input.
```

No silent completion is allowed.

This preserves uncertainty and prevents administrative hallucination.

---

# Clerk Strategic Purpose

The clerk layer exists primarily to reduce cabinet compute cost and context load.

Its purpose is not archival completeness.

Its purpose is bounded cabinet awareness.

The cabinet must know enough about ministry inputs to evaluate ministry outputs, but it must not repeat ministry-level reasoning.

The clerk layer therefore provides compressed evidence context.

It allows cabinet review to compare:

```text
ministry output Y
against
reduced ministry input brief U
```

without requiring:

```text
cabinet reprocessing full ministry input X
```

This preserves:

* low hardware requirements
* bounded cabinet context
* cabinet lightweightness
* ministry/cabinet separation
* performance stability
* multiplayer scalability
* replay tractability

---

# Clerk Audit Rule

Each cabinet brief should be traceable back to the source ministry input.

The system should be able to verify that the clerk did not introduce new strategic claims that were not present in the ministry source.

Possible validation checks:

* source-backed claim verification
* missing-field detection
* fixed schema validation
* section ordering validation
* non-invention checks
* diff review between ministry input context and clerk brief

The clerk layer should behave closer to deterministic middleware than to a creative LLM agent.

---

# Clerk LLM Use Rule

The clerk layer may use an LLM when the extraction task requires human-like judgment about relevance.

This is acceptable because Strategic Nexus uses LLMs specifically for:

* contextual judgment
* imperfect information interpretation
* human-like prioritization
* nuanced relevance filtering

However, clerk LLM usage must remain constrained.

The clerk LLM may:

* identify which input facts are relevant to the ministry proposal
* compress ministry input context into a cabinet-readable brief
* preserve explicit uncertainty
* preserve stated evidence
* produce a fixed-schema brief

The clerk LLM must not:

* decide policy
* improve the ministry proposal
* add unsupported arguments
* invent missing evidence
* optimize the proposal for cabinet approval
* use hidden strategic context outside the ministry input

When a deterministic extractor is sufficient, prefer deterministic extraction.

When relevance judgment matters, a small or medium LLM may be used, but it must be matched to the task size and bounded by schema validation.

The goal is not to turn the LLM into a mechanical automaton.

The goal is to use human-like judgment only where it adds strategic value, while keeping authority boundaries intact.

---

# Stateless Clerk Rule

The clerk layer should operate as a fresh isolated task whenever possible.

The clerk should receive only:

* extraction rules
* output schema
* formatting constraints
* current ministry input context X

The clerk should not receive:

* persistent conversation history
* long-term strategic memory
* cabinet context
* other ministry context
* previous clerk interpretations
* hidden strategic priorities

This keeps the clerk role administrative.

A stateless clerk may use language judgment on the provided data, but lacks the context required to become a strategist.

---

# Clerk Non-Memory Rule

Clerk work should not become strategic memory.

Clerk-generated briefs may be stored only as:

* temporary cabinet input
* replay evidence
* validation artifact
* short-lived audit record

They should not become:

* long-term empire memory
* doctrine memory
* relationship memory
* trauma memory
* personality memory
* strategic precedent

Reason:
administrative extraction is not civilization interpretation.

The system may validate that the clerk output followed schema and did not invent claims.

The system does not need to preserve detailed clerk process history as meaningful civilization memory.

---

# Existing Agenda Principle

Civilizations already possess active agendas.

New proposals should compete against:

* current policy
* strategic inertia
* implementation cost
* instability risk
* doctrine continuity

The system must not assume:

* every cycle requires major change
* every ministry must propose reform

Existing stable agendas are often preferable.

---

# Cabinet Review Rule

A cabinet review layer evaluates:

* ministry proposals
* policy conflicts
* strategic consistency
* feasibility
* civilization-wide coherence

The cabinet should NOT receive:

* raw save data
* massive ministry context
* full memory archives
* complete internal reasoning traces

The cabinet receives:

* policy summaries
* proposal briefs
* confidence estimates
* risk summaries
* strategic conflicts
* projected outcomes

Cabinet review remains lightweight.

---

# Cabinet Role Principle

The cabinet behaves more like:

* a prime minister
* ruling council
* executive strategic coordinator

not:

* a second omniscient super-intelligence layer

The cabinet evaluates:

* whether proposed changes fit the current civilization trajectory
* whether changes conflict with other agendas
* whether the current policy is preferable to reform

The cabinet should compare:

* proposed agenda
  vs
* existing agenda under current conditions

---

# Incremental Change Rule

Civilizations should evolve incrementally.

The architecture should prefer:

* partial reform
* selective adjustment
* bounded policy shifts
* gradual doctrine evolution

Avoid:

* total policy replacement every cycle
* civilization personality resets
* simultaneous radical change across all domains

Strategic inertia is desirable.

---

# Partial Acceptance Rule

Cabinet review may:

* approve some ministry proposals
* reject others
* preserve existing agendas elsewhere
* accept partial modifications

Example:

```text id="g8w3fa"
military proposal rejected
economy proposal accepted
diplomacy partially modified
intelligence unchanged
```

Civilizations should evolve unevenly and organically.

---

# Revision Feedback Rule

If a proposal is rejected:

* the ministry may receive cabinet feedback
* only the affected agenda should be reconsidered
* unaffected agendas should remain stable

Cabinet feedback may include:

* strategic concerns
* conflict explanations
* implementation risks
* ideological contradictions
* economic concerns

The ministry re-evaluates:

* its own proposal
* cabinet feedback
* existing strategic context

The entire civilization should not rerun full reasoning.

---

# Feedback Accumulation Rule

Cabinet feedback may accumulate through limited revision cycles.

Example:

* proposal rejected
* ministry revises
* cabinet requests adjustment
* ministry revises again

However:
revision loops must remain bounded.

---

# Revision Limit Rule

The system must prevent infinite policy negotiation loops.

Possible limits:

* maximum revision count
* maximum retries per agenda
* timeout-based fallback
* confidence threshold stop

If consensus fails:

* preserve existing agenda
* skip reform
* apply only safe partial changes

Strategic continuity is preferable to unstable endless negotiation.

---

# Policy Persistence Rule

Rejected reform proposals do NOT erase existing policy.

If no improved agenda is accepted:

* previous stable agenda remains active

This prevents:

* policy collapse
* strategic vacuum
* civilization instability
* random behavioral drift

---

# Change Budget Rule

Civilizations should maintain bounded change velocity.

Too many simultaneous reforms may cause:

* strategic instability
* identity drift
* incoherent behavior
* unrealistic volatility

The cabinet may limit:

* number of accepted reforms
* severity of changes
* simultaneous doctrine shifts

Civilizations should feel historically continuous.

---

# Ministry Specialization Rule

Different ministries may use:

* different prompts
* different retrieval priorities
* different reasoning depth
* different cadence
* different lightweight models in the future

This improves:

* scalability
* optimization
* hardware efficiency

Not every strategic domain requires equal reasoning complexity.

---

# Ministry Priority Scheduling Rule

Ministries should not all run with equal priority.

The daemon should prioritize ministries according to:

* current galaxy state
* empire condition
* active wars
* nearby crises
* economic instability
* diplomatic shocks
* faction instability
* espionage relevance
* recent strategic failures
* current attention window

Examples:

* active war increases military and intelligence priority
* economic collapse increases economy priority
* federation crisis increases diplomacy priority
* faction unrest increases internal governance priority
* suspicious neighbor buildup increases intelligence priority

Low-priority ministries may be:

* skipped
* delayed
* run with a weaker model
* run with shorter context
* preserved through existing agenda state

This preserves performance while allowing the civilization to focus on what currently matters.

---

# Asynchronous Pipeline Rule

Ministerial reasoning should remain asynchronous where possible.

Possible flow:

```text id="m6d1sr"
retrieve ministry input context X
→ ministry reasoning over X
→ ministry output Y
→ clerk reduction of X into U
→ proposal validation
→ cabinet review of Y + U
→ partial revision if needed
→ bounded acceptance
→ payload generation
```

This pipeline supports:

* adaptive load control
* modular retries
* selective skipping
* performance scaling

---

# Auditability Rule

The system should preserve:

* ministry proposals
* clerk-generated briefs
* cabinet objections
* accepted changes
* rejected changes
* revision history
* policy continuity

This improves:

* debugging
* replay analysis
* strategic explainability

---

# Strategic Personality Rule

The ministerial architecture should reinforce:

* civilization personality
* doctrine continuity
* ideological consistency
* historical inertia
* strategic habits

Civilizations should evolve through:

* institutional friction
* compromise
* gradual adaptation

not:

* instant global strategic rewrites

---

# Performance Rule

The ministerial architecture exists partly to reduce:

* context size
* inference cost
* VRAM pressure
* giant prompt complexity
* gameplay stutter

Smaller bounded reasoning passes are preferred over massive unified inference.

---

# Design Goal

Strategic Nexus civilizations should behave like:

```text id="z3v8rq"
complex historical governments evolving through specialized institutional reasoning and bounded cabinet coordination
```

not:

```text id="p4k7nd"
single gigantic omniscient prompts attempting to solve the entire civilization at once
```

Institutional strategic reasoning is a foundational scalability and realism mechanism of the architecture.

---

# Cabinet Lightweight Constraint Appendix

## Core Constraint

The cabinet review layer must remain lightweight.

The architecture must avoid evolving the cabinet into:

* a second full civilization reasoning pass
* a giant super-prompt
* an omniscient strategic solver
* a duplicate of ministry reasoning

If this occurs:

* hardware cost rises dramatically
* context size explodes
* architectural layering collapses
* performance advantages disappear

---

# Cabinet Scope Limitation Rule

The cabinet should evaluate:

* ministry summaries
* strategic conflicts
* feasibility concerns
* policy coherence
* civilization trajectory

The cabinet should NOT:

* fully recompute ministry analysis
* deeply reanalyze raw data
* rerun full strategic simulations
* receive massive memory context

Cabinet review remains:

* policy-level
* comparative
* bounded
* lightweight

---

# Proposal Brief Rule

Ministries should provide concise proposal briefs.

Clerk-generated input briefs may accompany proposal briefs when ministries process larger input contexts.

Possible contents:

* proposal summary
* expected outcome
* strategic rationale
* confidence estimate
* implementation cost
* major risks
* conflicts with existing policy

Avoid:

* giant reasoning traces
* raw context dumps
* full memory replay
* excessive internal chain-of-thought style expansion

---

# Differential Evaluation Rule

The cabinet should primarily evaluate:

```text id="j5n8dt"
difference between current agenda and proposed agenda
```

not:

* recompute civilization strategy from scratch

The cabinet asks:

* Is the proposed change better than maintaining the current policy?
* Does the proposal conflict with civilization direction?
* Does the proposal introduce instability?
* Is the proposal affordable and coherent?

This keeps review bounded.

---

# Limited Revision Principle

Cabinet objections should remain targeted.

If revision is needed:

* only affected ministries should reconsider
* only affected policy domains should rerun reasoning

Avoid:

* civilization-wide reruns
* global strategic recomputation
* cascading full-pipeline retries

Localized correction is preferred.

---

# Bounded Negotiation Rule

Government negotiation loops must remain bounded.

Possible safeguards:

* maximum revision rounds
* revision cooldowns
* diminishing reconsideration priority
* fallback to existing agenda

If consensus cannot be reached efficiently:

* preserve current stable agenda
* defer reform
* apply only safe partial changes

Stability is preferable to infinite strategic debate.

---

# Strategic Inertia Reinforcement Rule

The cabinet should naturally reinforce:

* continuity
* institutional inertia
* strategic persistence
* policy conservatism under uncertainty

Large-scale change should require:

* strong evidence
* sustained pressure
* strategic justification

This helps civilizations feel:

* historically grounded
* politically realistic
* psychologically continuous

rather than:

* hyperreactive
* chaotic
* constantly reinvented

---

# Performance Preservation Rule

The ministerial architecture exists partly to:

* minimize inference spikes
* reduce VRAM pressure
* reduce giant prompt complexity
* improve gameplay smoothness

Cabinet inflation into a giant reasoning layer violates the architecture's performance goals.

The cabinet must remain:

* smaller
* lighter
* more abstract
  than ministry-level reasoning.
