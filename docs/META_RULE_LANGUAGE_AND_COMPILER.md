# Strategic Nexus - Meta Rule Language And Compiler

## Purpose

This document defines the Strategic Nexus meta rule language and compiler boundary.

The LLM may propose campaign and empire rules in a small bounded DSL.

The LLM must not generate Stellaris script directly.

The compiler converts validated DSL programs into generated Stellaris mod overlay files.

---

# Core Rule

Strategic Nexus DSL is a safe declarative subset of the mod script behavior Strategic Nexus is allowed to generate.

It should be expressive enough to generate the required Strategic Nexus mod scripts, but it must not provide any capability outside the allowed generated overlay boundary.

The DSL is not a general programming language.

Forbidden DSL features:

* loops
* recursion
* arbitrary functions
* arbitrary file paths
* arbitrary commands
* raw Stellaris script blocks
* runtime code generation
* fleet micro-control
* tactical orders
* save editing

Allowed DSL features:

* campaign-scoped rules
* entry-point-scoped rules
* empire-scoped rules
* allowlisted event-family policy branches
* bounded ministry domains
* bounded strategic preferences
* marker-guarded activation
* personality and memory conditions from validated state
* controlled intensity values
* controlled duration/cadence values
* concise rationale metadata for audit

---

# Authority Boundary

The LLM writes proposals.

A proposal becomes usable only after:

```text
parse
-> semantic validation
-> budget validation
-> numeric clamp
-> deterministic compilation
-> generated overlay validation
```

If any stage fails, the invalid rule is skipped.

The compiler must not reinterpret invalid LLM output into unrelated behavior.

After a DSL program is validated, clamped, and compiled, the generated overlay is the next-session artifact.
The runtime mod does not ask the LLM to revise behavior during the active session.
It may, however, select among already-compiled reactive policy branches through ordinary Stellaris script.

---

# Design Philosophy

The DSL lets the LLM act like an empire player or strategist inside the bounded Strategic Nexus architecture.

The LLM may use:

* empire personality
* empire memory
* known facts
* uncertainty
* strategic culture
* ministry perspective

to propose strategic rules.

The compiler and validator remain authoritative.

The LLM is not allowed to:

* bypass schemas
* invent new mod script commands
* add new runtime authority
* generate raw event text
* generate hidden communication paths
* replace vanilla AI

---

# Ministry Domains

DSL rules are grouped by ministry-like domains.

Initial v0 domains:

* `military_ministry`
* `research_ministry`

Future domains may include:

* `diplomacy_ministry`
* `espionage_ministry`
* `economy_ministry`
* `internal_stability_ministry`
* `crisis_ministry`
* `tradition_ministry`

Domains are strategic and coarse.
They must not become micromanagement surfaces.

---

# v0 Rule Shape

The exact syntax may evolve, but the v0 DSL should remain close to this shape:

```text
campaign "campaign_id" {
  empire "empire_id" {
    rule "rule_id" {
      ministry = military_ministry
      when personality.paranoia >= medium
      when memory.has_tag repeated_border_war
      prefer military_posture defensive intensity 0.7
      prefer doctrine_inertia high
      duration = next_session
      confidence = 0.72
      rationale = "Repeated border wars reinforced defensive paranoia."
    }
  }
}
```

This example is illustrative, not final syntax.

Required concepts:

* campaign id
* entry-point guard when available
* empire id
* rule id
* ministry
* optional allowlisted event family
* optional validated conditions
* bounded strategic actions
* intensity/confidence where relevant
* duration/cadence where relevant
* rationale summary for audit

---

# Conditions

Allowed condition inputs must come from validated Strategic Nexus state.

Examples:

* campaign marker state
* empire identity
* personality bands
* memory tags
* current strategic dimensions
* known war state
* known relationship state
* known or inferred threat state
* uncertainty state

Forbidden condition inputs:

* hidden full save truth
* raw save text
* arbitrary player names as instructions
* unknown empires outside target perspective
* fleet-level tactical state outside allowed strategic summaries
* raw Stellaris script conditions from the LLM
* raw on_action names from the LLM

---

# Actions

Allowed actions must map to bounded Strategic Nexus domains.

Initial v0 actions:

* set or prefer `military_posture`
* set or prefer `research_bias`
* set or prefer `doctrine_inertia`
* apply an allowlisted personality/strategy bias tag

Actions must compile into one or more of:

* generated scripted triggers
* generated scripted effects
* generated dispatcher events
* allowlisted base modifiers
* allowlisted flags or variables

Reactive policy branches must use allowlisted Strategic Nexus event-family names.
The compiler owns the mapping from event family to concrete Stellaris script surfaces.
The LLM must not provide raw on_action names, event ids, scripted effect bodies, or arbitrary Stellaris script.

The LLM must not define raw modifier values in v0.
The generated DSL should select from allowlisted modifier/bias names defined by the base mod or compiler configuration.

---

# Compiler Target

The compiler should generate a complete replacement snapshot of the generated overlay.

Preferred v0 backend:

```text
base handwritten on_actions
-> base handwritten dispatcher event
-> generated campaign/empire scripted triggers
-> generated campaign/empire scripted effects
-> allowlisted base static modifiers
```

Reactive policy-pack backend:

```text
base handwritten on_actions for known event families
-> base handwritten reactive dispatcher event
-> generated entry-point/campaign/empire triggers
-> generated policy-branch effects
-> allowlisted base modifiers or flags
```

Generated overlay updates are full rewrites, not append-only updates.

Generated files may include:

```text
events/strategic_nexus_generated_events.txt
common/scripted_triggers/strategic_nexus_generated_triggers.txt
common/scripted_effects/strategic_nexus_generated_effects.txt
```

See `GENERATED_OVERLAY_LAYOUT_CONTRACT.md` for the current v0 staged file layout.

Generated modifiers should be avoided in v0 unless a later design review approves them.
Prefer stable allowlisted base modifiers selected by generated effects.

---

# Backend Selection Rules

The compiler chooses the Stellaris script mechanism.

The LLM and DSL should describe strategic intent, not whether the result is an event, scripted effect, scripted trigger, or modifier.

Use this priority order:

1. Prefer generated scripted triggers for marker checks, campaign/empire guards, and cheap boolean eligibility checks.
2. Prefer generated scripted effects for applying bounded state, flags, variables, or selecting allowlisted base behavior.
3. Prefer one or a small number of handwritten dispatcher events to call generated effects at controlled low-frequency moments.
4. Prefer one or a small number of handwritten reactive dispatcher events for allowlisted event families.
5. Prefer allowlisted handwritten modifiers for actual gameplay bias values.
6. Avoid generated modifier values in v0 unless reviewed, tested, and proven checksum/package safe.

Generated events should be dispatch surfaces, not a growing archive of unique behavior history.

Generated effects should be idempotent where practical.
Running the same generated effect twice should not create unbounded stacking, duplicated permanent state, or accumulating gameplay drift.

Generated triggers should stay cheap.
Avoid broad expensive checks inside high-frequency scopes.

---

# Activation Model

Generated campaign rules must be inert unless the loaded save exposes the expected Strategic Nexus marker.

The base mod may load all active generated campaign rules at startup.
Only the marker-matching ruleset may apply.

For MP-capable campaigns, generated rules may exist for every detected campaign empire, but gameplay-affecting rules must include an AI-control/current-human-control guard.
If an empire is currently controlled by a human player, its generated Strategic Nexus gameplay rules must be inert for that session.

If marker match fails:

```text
generated campaign rules do nothing
base mod fallback continues
companion app may repair or restage after the session
```

Do not rely on the mod knowing the campaign name, save folder, or filename.
Those are companion-app concepts.

---

# Runtime Budget Defaults

Initial defaults should favor performance and strategic inertia.

Priorities:

1. stable FPS and no lag
2. avoid slowing in-game simulation through frequent mod decisions
3. strategic coarse rules, not micromanagement
4. preserve vanilla AI as the executor

Initial conservative defaults:

* no per-tick logic
* no per-day logic
* no fleet-level micro rules
* no planet-by-planet generated rules in v0
* apply on load or first safe monthly pulse
* refresh rarely, such as yearly or session-level, unless testing proves a lower cost cadence is safe
* small rule count per empire
* small active ministry count per empire in v0

Exact numbers may be tuned during development using telemetry and tests.

Runtime budget changes must remain reviewed, versioned, and conservative.

---

# Validation Contract

Reject or skip a rule if:

* DSL syntax is invalid
* campaign id is missing
* empire id is missing
* ministry is unknown
* condition references an unsupported data source
* action references an unsupported domain
* value is outside allowed enum set
* rule attempts tactical control
* rule attempts arbitrary script generation
* rule exceeds budget
* rule cannot compile deterministically

Numeric intensity/confidence may be clamped to allowed ranges.
Semantic violations should be rejected, not silently rewritten.

---

# Fallback Rule

If one rule fails validation or compilation:

```text
skip that rule
continue with other valid rules
record concise audit reason
```

If an empire has no valid generated rules:

```text
use existing valid profile if still applicable
or use base mod fallback
```

If a campaign marker mismatches:

```text
ignore that campaign ruleset
use base mod fallback
```

---

# Telemetry And Tuning

Budgets and allowed strategic rule shapes may be adjusted during development.

Telemetry may inform:

* max rules per empire
* refresh cadence
* active ministry count
* generated overlay size
* performance thresholds
* validation rejection patterns

Telemetry must not silently expand runtime authority.
All budget changes must preserve the hard rules:

* no realtime LLM decisions
* no micro-management
* no vanilla AI replacement
* no raw LLM script
* no hidden information leakage

---

# Design Goal

Strategic Nexus DSL should feel like:

```text
a small strategic policy language used by empire personalities to shape next-session behavior
```

not:

```text
a general-purpose scripting language or a hidden backdoor into Stellaris runtime control
```
