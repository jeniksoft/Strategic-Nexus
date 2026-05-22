# Strategic Nexus — Privacy And Learning Rules

## Core Rule

Strategic Nexus may learn from played campaigns only under strict privacy and consent rules.

Default behavior must be privacy-preserving.

No personal data, save content, multiplayer data, custom names, or campaign data may be uploaded or shared without explicit user consent.

Local learning is allowed.
Remote learning is opt-in only.

---

# Design Philosophy

Strategic Nexus is intended to improve over time.

However, campaign data may contain sensitive or personal information.

Possible sensitive data:

* player names
* Steam names
* custom empire names
* custom species names
* custom ruler names
* custom planet names
* multiplayer participant data
* chat-like user input if ever added
* local file paths
* machine identifiers
* campaign metadata
* logs that reveal user behavior

The project must treat campaign data as potentially private.

---

# Default Privacy Rule

The default mode must be:

```text
local only
```

By default, Strategic Nexus must not:

* upload saves
* upload raw prompts
* upload raw LLM outputs
* upload player identifiers
* upload multiplayer participant information
* upload campaign history
* upload custom names
* upload logs containing personal data

No network learning by default.

---

# Local Learning Rule

Local learning is allowed.

Local learning may use:

* campaign memory
* strategic audit logs
* doctrine outcomes
* faction outcomes
* war outcomes
* local performance telemetry
* local fallback history

Only on the user's machine.

Local learning must remain:

* campaign-scoped
* recoverable
* deletable
* portable where appropriate

---

# Remote Learning Opt-In Rule

Any remote learning, telemetry, analytics, model improvement, or data sharing must be explicit opt-in.

Opt-in must be:

* clear
* informed
* specific
* reversible
* separate from basic mod functionality

The mod must remain usable if the player refuses data sharing.

---

# No Raw Save Upload Rule

Strategic Nexus must never upload raw save files by default.

Raw saves may contain:

* player-created names
* multiplayer information
* local metadata
* campaign history
* hidden private user choices

Raw save upload is forbidden unless:

* user explicitly exports it
* user understands what is included
* it is for manual debugging
* it is not automatic

---

# Data Minimization Rule

Collect the minimum data necessary.

Prefer:

* aggregate counters
* anonymized metrics
* local summaries
* bounded diagnostic events

Avoid:

* raw logs
* raw prompts
* raw save sections
* full campaign histories
* personal identifiers

---

# Anonymization And Pseudonymization Rule

If data is ever exported:

* remove direct identifiers
* remove custom names where possible
* replace empire/player ids with random local ids
* remove file paths
* remove machine names
* remove account identifiers

Important:
pseudonymization reduces risk but does not automatically make data non-personal.

Treat pseudonymized data cautiously.

---

# Multiplayer Consent Rule

Multiplayer data is especially sensitive.

The host must not upload data that includes other players' information without clear consent.

If multiplayer telemetry ever exists:

* disclose it
* allow disabling it
* avoid player identifiers
* aggregate heavily
* prefer local-only processing

---

# Learning Separation Rule

Separate learning types:

## Local Campaign Learning

Allowed by default.

Used for:

* memory
* doctrine adaptation
* empire personality evolution
* relationship history

## Local Cross-Campaign Learning

Optional.

May use local summaries to improve future local behavior.

Should remain user-controllable.

## Remote Aggregate Learning

Opt-in only.

May include:

* performance metrics
* fallback counts
* crash counts
* anonymized feature usage

## Remote Strategy Learning

High risk.

Do not implement until privacy, consent, anonymization, and governance are fully designed.

---

# User Control Rule

Users must be able to:

* disable telemetry
* delete local memory
* clear campaign learning
* export diagnostic data manually
* inspect what is being stored
* continue playing without cloud learning

Privacy controls must not be hidden.

---

# Privacy By Design Rule

Privacy must be designed into the architecture.

Preferred:

* local-first
* opt-in sharing
* minimal data
* explicit retention
* easy deletion
* no hidden uploads
* no automatic cloud training

Avoid:

* silent telemetry
* unclear analytics
* raw save upload
* bundled consent
* mandatory tracking

---

# Retention Rule

Stored data should have clear retention behavior.

Examples:

* temporary logs rotate
* old debug logs expire
* local campaign memory remains user-controlled
* exported diagnostics are explicit user action

Avoid indefinite uncontrolled log accumulation.

---

# Security Rule

Stored memory and logs should be treated as sensitive.

The system should avoid:

* writing secrets to logs
* storing account tokens
* storing unnecessary file paths
* exposing multiplayer identifiers

---

# LLM Provider Rule

If external LLM APIs are ever used:

* disclose provider
* disclose what data is sent
* avoid raw save data
* minimize prompt content
* allow local-only mode
* require explicit user consent

Local models are preferred for privacy.

---

# Legal Awareness Rule

The project must be designed with privacy regulations in mind, especially for EU users.

Important principles:

* informed consent
* purpose limitation
* data minimization
* right to deletion
* privacy by design
* privacy by default

This document is not legal advice.
If remote telemetry or cloud learning is implemented, legal review may be required.

---

# Anti-Exploitation Rule

Player data must not be used to:

* profile players commercially
* sell behavioral data
* train unrelated systems without consent
* identify individuals
* expose multiplayer behavior

Strategic Nexus exists to improve gameplay, not to harvest player data.

---

# Design Goal

Strategic Nexus should behave like:

```text
a local-first strategic intelligence system that respects player privacy
```

not:

```text
a data-harvesting telemetry platform
```

Learning is valuable only if it preserves trust, privacy, and player control.
