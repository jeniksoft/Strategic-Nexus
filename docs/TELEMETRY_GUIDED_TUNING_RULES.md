# Strategic Nexus - Telemetry Guided Tuning Rules

## Core Rule

Strategic Nexus may use telemetry, replay records, campaign outcomes, and audit history to tune guessed constants over time.

Telemetry may guide tuning.

Telemetry must not silently rewrite architecture during active gameplay.

The project must prefer:

```text
observed campaign behavior
-> tuning proposal
-> review
-> versioned configuration change
-> regression validation
```

not:

```text
observed campaign behavior
-> uncontrolled self-modification
```

---

# Design Philosophy

Many early Strategic Nexus constants are necessarily estimates.

Examples:

* strategic pressure thresholds
* ministry priority weights
* doctrine inertia values
* ethics bias strength
* personality bias strength
* memory decay rates
* relationship decay rates
* confidence thresholds
* fallback thresholds
* review cadence timing
* probabilistic sampling weights

These values should improve through real campaign observation.

However:
uncontrolled automatic tuning can destabilize civilization behavior, break multiplayer assumptions, and make regressions hard to reproduce.

Telemetry-guided tuning is desirable.

Silent live self-reconfiguration is not.

---

# Learning Layer Separation Rule

Strategic Nexus must keep different learning layers separate.

## Campaign Runtime Learning

Allowed during gameplay.

This means:

* an empire learns from its own campaign history
* memory influences future decisions
* doctrine inertia changes from local experience
* trauma, trust, distrust, confidence, and strategic precedent evolve

This changes civilization state.

It does not change global architecture, global constants, or prompt policy.

## Update-Time Tuning

Allowed during mod, daemon, or tuning profile updates.

This means:

* constants are adjusted
* thresholds are tuned
* sampling weights are refined
* review cadence is changed
* validation-safe scoring rules are improved

This changes system behavior for a reviewed version.

## Update-Time LLM Policy Learning

Allowed during mod, daemon, or prompt policy updates.

This means:

* ministry instructions may be improved
* cabinet evaluation rubrics may be refined
* strategic review guidelines may be updated
* prompt templates may gain lessons from previous campaigns
* regression replay results may change role instructions

This does not require local model fine-tuning.

Preferred path:

```text
campaign audits
-> summarized lessons
-> prompt policy/rubric updates
-> versioned release
-> regression validation
```

Local LLM fine-tuning is not the assumed default path because it is hardware-expensive and harder to validate.

Instruction and rubric evolution is the preferred practical learning path.

---

# What Telemetry May Tune

Telemetry may inform tuning of bounded constants such as:

* when an empire should become defensive or aggressive
* how strongly war increases processing priority
* how fast stale doctrine confidence decays
* how much trauma reinforces defensive behavior
* how quickly relationships decay without interaction
* how often ministries should review stable domains
* how wide probabilistic decision distributions should be
* how strongly capability constraints suppress risky plans
* how quickly low-importance memories leave active context

These are strategic behavior parameters.

They are not unrestricted code changes.

---

# Required Evidence

Tuning should be based on evidence such as:

* replay records
* regression save results
* campaign audit summaries
* selected payload history
* rejected payload reasons
* fallback counts
* desync or stability reports
* doctrine success and failure summaries
* memory retrieval effectiveness
* performance telemetry

Subjective gameplay feel matters,
but it should be captured as a tuning note rather than hidden intuition.

---

# Review Before Apply Rule

Telemetry should produce proposed changes, not automatic final changes.

Before changing constants, the project should ask:

* what behavior looked wrong
* what evidence supports the change
* which constants are affected
* which systems may regress
* which tests or replay saves should be rerun

Large behavior changes require stronger evidence.

---

# Versioned Tuning Rule

Tuned values should be versioned.

Preferred:

* named configuration files
* schema-versioned tuning profiles
* changelog entries
* replay-compatible audit records

Avoid:

* magic constants scattered through code
* undocumented threshold changes
* silent model-dependent behavior drift

---

# Update-Time Tuning Rule

Strategic Nexus may apply telemetry-derived tuning during controlled updates.

Allowed update points:

* mod version update
* daemon version update
* tuning profile update
* developer-reviewed configuration update
* explicitly selected experimental tuning profile

The forbidden behavior is silent live mutation during an active campaign session.

Runtime telemetry may:

* record observations
* mark candidate issues
* suggest future tuning
* adjust safe temporary load modes

Runtime telemetry must not silently:

* permanently alter strategy constants without review
* bypass regression testing
* rewrite validation rules
* mutate schema contracts
* change multiplayer authority rules

When tuning changes are applied through a mod or daemon update, they must be:

* versioned
* documented
* compatible where feasible
* regression-tested
* reversible through profile/configuration where practical

---

# Local-First Rule

Telemetry-guided tuning must respect privacy rules.

Default:

```text
local only
```

Remote aggregate learning is opt-in only.

Raw saves, raw prompts, custom names, multiplayer identifiers, and personal data must not be uploaded automatically.

---

# Regression Rule

Any tuning change that affects strategic behavior should be validated against regression cases.

Useful checks:

* bridge payload validity
* fallback behavior
* multiplayer safety assumptions
* personality continuity
* ethics consistency
* performance impact
* long-campaign stability

The project should prefer slower, auditable improvement over fast unstable tuning.

---

# Human Gameplay Feedback Rule

Human feedback is valid tuning evidence.

Examples:

* empires feel too passive
* wars feel unfocused
* personalities feel too similar
* trauma dominates for too long
* memory seems irrelevant
* strategy changes too frequently
* AI still feels like a solver

Such feedback should be converted into:

* concrete observations
* suspected affected constants
* proposed tuning experiments
* regression checks

---

# Multiplayer Rule

In multiplayer:

* tuning evaluation occurs on host/developer side
* clients do not tune independently
* selected constants must remain deterministic for the session
* synchronized game state remains authoritative

Runtime tuning must never create client-side divergence.

---

# Design Goal

Strategic Nexus should behave like:

```text
a system that learns how to tune its strategic parameters from observed campaigns under developer control
```

not:

```text
a self-mutating black box whose behavior drifts without explanation
```

Telemetry should make each reviewed mod update smarter over time while preserving control, auditability, and multiplayer safety.
