# Strategic Nexus - LLM Model Selection And Task Fit Rules

## Core Rule

Strategic Nexus must choose LLM capability according to task difficulty and strategic risk.

The system should use:

* enough intelligence to handle the task well
* enough context to preserve meaning
* enough reasoning depth to avoid costly mistakes

but not:

* maximum model capability for every task
* maximum context for every task
* expensive reasoning for mechanical work

Model selection is part of architecture.

---

# Design Philosophy

LLMs are valuable because they provide human-like strategic judgment.

This is not a bug.

Strategic Nexus uses LLMs because they can reason about:

* uncertainty
* priorities
* political meaning
* memory
* personality
* ideology
* tradeoffs
* imperfect information

The goal is not to turn the LLM into a rigid optimizer or a deterministic parser.

The goal is to use human-like judgment where it matters while keeping outputs bounded, validated, and safe.

---

# Task Fit Principle

Each LLM task should declare its required capability level.

Possible levels:

* mechanical
* lightweight judgment
* domain reasoning
* cabinet reasoning
* high-risk architecture reasoning

Different tasks may use different models, context sizes, temperature settings, and reasoning depth.

One model configuration should not be assumed optimal for all work.

---

# Mechanical Task Rule

Mechanical tasks should avoid expensive LLM reasoning when possible.

Examples:

* JSON validation
* schema normalization
* enum validation
* bounds checking
* deterministic formatting
* duplicate detection
* sequence validation

Preferred implementation:

* normal code
* deterministic parser
* schema validator
* small model only if text judgment is unavoidable

---

# Lightweight Judgment Rule

Some tasks require limited human-like relevance judgment.

Examples:

* clerk input brief extraction
* relevance filtering
* short summary compression
* uncertainty preservation
* selecting salient evidence from bounded input

These tasks may use a smaller or medium LLM.

The model should be strong enough to preserve meaning, but not so large that it becomes a major performance cost.

---

# Domain Reasoning Rule

Ministry-level reasoning may require stronger judgment.

Examples:

* military posture review
* economic strategy review
* diplomatic policy review
* internal governance review
* intelligence priority review

These tasks should use a model capable of:

* interpreting context
* weighing uncertainty
* preserving personality
* respecting ethics
* considering capability limits
* producing bounded proposals

The model does not need to solve the entire civilization.

It only needs to reason well about its domain.

---

# Cabinet Reasoning Rule

Cabinet review may require higher-level judgment, but it must remain lightweight.

The cabinet model should evaluate:

* proposal coherence
* conflicts between ministries
* feasibility
* civilization trajectory
* change budget
* strategic inertia

The cabinet should not receive full ministry input contexts.

The cabinet should not become a giant second civilization reasoning pass.

---

# High-Risk Reasoning Rule

Use the strongest available reasoning only for high-risk tasks.

Examples:

* schema redesign
* architecture blockers
* multiplayer synchronization design
* security boundary design
* prompt injection defense
* memory architecture design
* safe daemon-to-mod architecture design

These are expensive mistakes.

Higher intelligence is justified when failure could corrupt saves, break multiplayer, weaken safety, or distort long-term architecture.

---

# Adaptive Model Selection Rule

Model selection should adapt to:

* current load mode
* hardware pressure
* multiplayer state
* task priority
* war/crisis urgency
* queue length
* timeout history
* inference latency

If the system is under pressure:

* reduce low-priority LLM calls
* use smaller models for lightweight tasks
* shorten context
* skip non-urgent reviews
* preserve last known good state

Gameplay performance remains higher priority than strategic freshness.

---

# Human-Like Judgment Preservation Rule

Do not over-mechanize tasks that exist specifically to use LLM judgment.

Bad:

```text
force every relevance decision into rigid numeric rules
```

Better:

```text
allow bounded LLM judgment, then validate and serialize output safely
```

Strategic Nexus should remain:

* bounded
* auditable
* safe

but also:

* human-like
* interpretive
* historically aware
* strategically nuanced

---

# Validation Authority Rule

Even when a strong model is used, validation remains external.

The model may judge.

The daemon must validate.

The bridge must enforce.

The mod must apply only bounded scripted behavior.

No model, regardless of capability, becomes an authority layer above schema, safety, synchronization, or runtime constraints.

---

# Cost Awareness Rule

Every LLM call has cost.

Cost may include:

* CPU usage
* GPU usage
* VRAM pressure
* latency
* battery/power usage
* heat
* multiplayer host slowdown
* opportunity cost for other daemon tasks

The system must spend intelligence intentionally.

---

# Design Goal

Strategic Nexus should behave like:

```text
a system that spends model intelligence where human-like judgment matters
```

not:

```text
a system that wastes maximum reasoning on every mechanical step
```

Correct model sizing is required for performance, scalability, and believable civilization reasoning.
