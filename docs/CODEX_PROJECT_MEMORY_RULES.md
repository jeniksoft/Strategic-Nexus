# Strategic Nexus - Codex Project Memory Rules

## Core Rule

Codex must treat project Markdown files as external long-term memory.

The chat context is temporary.

The repository documentation is the durable project memory.

Important project decisions should not live only in conversation.

---

# Design Philosophy

Strategic Nexus is too large to rely on one continuous AI context window.

Development will span:

* long conversations
* context compaction
* multiple sessions
* multiple commits
* future AI assistants
* future human readers

Codex must therefore leave durable memory trails in the repository.

Markdown files are not only documentation.

They are architectural memory anchors.

---

# Replacement Context Memory Rule

When a decision materially affects the project, Codex should preserve it in project docs.

Examples:

* architecture decisions
* safety boundaries
* runtime bridge assumptions
* multiplayer test results
* schema decisions
* v0 implementation scope
* unresolved blockers
* user philosophy that changes implementation direction

If a future Codex instance would need to know it, it should usually exist in a Markdown file.

---

# Project Memory Locations

Use these locations for durable context:

* `docs/MASTER_ARCHITECTURE_INDEX.md` for navigation and status
* `docs/DEVELOPMENT_ROADMAP.md` for implementation order
* subsystem rule documents for durable architecture decisions
* test result documents for verified behavior
* implementation plan documents for active coding boundaries

Do not scatter critical context only in commit messages.

Commit messages help, but docs are the primary long-term memory.

---

# Regular Memory Update Rule

Codex should regularly update or create Markdown memory files when:

* a new subsystem is introduced
* a major assumption changes
* a test result changes project status
* a blocker is discovered
* implementation scope changes
* an architectural boundary becomes clearer
* repeated conversation produces a stable rule

Do not wait until the end of a large development phase.

Small durable updates are better than lost context.

---

# Master Index Maintenance Rule

When Codex creates a major new document, it should consider whether `MASTER_ARCHITECTURE_INDEX.md` must be updated.

The index should remain the entry point for:

* project status
* subsystem categories
* implementation state
* blockers
* current v0 boundary

If a document is important enough to guide future work, it is probably important enough to appear in the index.

---

# Uncertainty Escalation Rule

Codex should ask the user when uncertainty affects:

* project philosophy
* architecture direction
* runtime safety
* multiplayer viability
* save integrity
* LLM role boundaries
* scope expansion
* irreversible implementation choices

Codex should not ask the user for routine low-level implementation decisions unless they affect the above.

---

# User Attention Budget Rule

The user should be involved in important decisions, not routine mechanics.

Codex should handle independently:

* file edits
* schema plumbing
* validators
* tests
* build fixes
* small refactors
* obvious documentation wiring
* Git hygiene

Codex should involve the user for:

* major architecture tradeoffs
* uncertain Stellaris integration assumptions
* gameplay philosophy decisions
* LLM behavior philosophy
* multiplayer-risk decisions
* irreversible scope changes

The user acts as architecture owner and civilization-design authority.

Codex acts as technical implementer and critical engineering partner.

---

# Critical Partner Rule

Codex must not blindly agree with the user when a proposal risks:

* save corruption
* multiplayer desync
* performance collapse
* architecture drift
* unsafe LLM behavior
* excessive scope expansion
* contradiction with existing rules

Codex should state the concern clearly and propose a safer alternative.

---

# Context Recovery Rule

At the start of substantial work, Codex should prefer reading:

1. `docs/MASTER_ARCHITECTURE_INDEX.md`
2. `docs/DEVELOPMENT_ROADMAP.md`
3. the relevant subsystem rule documents
4. the relevant code/tests

This prevents stale conversational assumptions from overriding repository truth.

---

# Conversation To Repository Rule

If an important idea emerges in conversation, Codex should decide whether it belongs in:

* a rule document
* a plan document
* the roadmap
* the master index
* a test result document
* an implementation note

If yes, Codex should add it proactively when appropriate.

---

# Design Goal

Codex should behave like:

```text
a technical partner that leaves durable architectural memory behind
```

not:

```text
a temporary chat assistant that forgets project-critical context
```

Strategic Nexus must survive context loss, AI handoff, and long-term development.
