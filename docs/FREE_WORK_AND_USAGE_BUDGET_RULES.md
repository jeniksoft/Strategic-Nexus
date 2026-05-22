# Strategic Nexus - Free Work And Usage Budget Rules

## Core Rule

Codex may perform autonomous Free Work only when the declared usage budget is high enough and the next work item is clear, local, testable, and valuable.

Free Work must never mean burning compute just because budget exists.

Free Work must produce project value.

---

# Budget Visibility Rule

Codex cannot reliably detect the user's remaining account or tariff limit internally.

Codex must treat usage budget as externally declared user context.

Allowed budget sources:

* direct user statement in chat
* user-maintained local project note
* future explicit automation or exported account status if safely available

Rejected assumptions:

* guessing remaining limit from time alone
* assuming tariff reset time
* assuming billing state from model availability
* pretending internal account visibility exists

If no current budget state is known, Codex should assume a conservative budget mode.

---

# Declared Budget Rule

When the user declares a remaining weekly limit percentage, Codex may use that value as the current working budget estimate.

Example:

```text
weekly limit remaining: 95%
```

This value is not independently verified by Codex.

It remains valid until:

* the user updates it
* the conversation context becomes stale
* the work intensity changes significantly
* Codex has reason to believe the estimate is no longer useful

---

# Budget Mode Thresholds

## Free Work Mode

Use when declared remaining weekly limit is above 80%.

Allowed behavior:

* continue autonomously on roadmap items
* implement small and medium scoped tasks
* run local tests
* improve regression coverage
* update architecture docs
* commit and push completed work

Constraints:

* avoid speculative rabbit holes
* avoid unbounded research loops
* stop when strategic direction becomes unclear

---

## Productive Work Mode

Use when declared remaining weekly limit is 60-80%.

Allowed behavior:

* continue high-value implementation
* prioritize roadmap tasks
* prefer testable commits
* avoid low-value polish

---

## Focused Work Mode

Use when declared remaining weekly limit is 40-60%.

Allowed behavior:

* important implementation only
* bug fixes
* validation fixes
* architecture blockers

Avoid:

* broad audits without clear output
* optional refactors
* speculative documentation expansion

---

## Conservation Mode

Use when declared remaining weekly limit is 20-40%.

Allowed behavior:

* critical blockers
* small exact edits
* important review
* short planning

Avoid:

* long autonomous loops
* large exploratory work
* noncritical polish

---

## Critical Reserve Mode

Use when declared remaining weekly limit is below 20%.

Allowed behavior:

* critical fixes only
* user-requested exact actions
* short explanations
* emergency debugging

Codex should preserve remaining budget for high-value decisions and blockers.

---

# Free Work Eligibility Rule

Free Work is allowed only if the task is:

* aligned with the roadmap
* bounded
* locally testable or document-verifiable
* safe to commit
* not dependent on missing user decisions
* not dependent on live Stellaris interaction unless explicitly requested

Good Free Work examples:

* add regression tests
* harden validators
* improve fail-safe behavior
* document confirmed runtime findings
* update architecture index
* clean small CI issues
* implement deterministic pipeline scaffolding

Bad Free Work examples:

* invent large new systems without review
* rewrite architecture direction
* attempt lower-level game-process integration
* generate speculative gameplay content
* expand scope without tests
* research indefinitely without concrete output

---

# User Decision Boundary Rule

Codex should stop and ask the user when work requires:

* architecture direction choice
* gameplay philosophy choice
* risk acceptance
* native runtime interaction
* manual Stellaris testing
* dependency installation
* account authentication
* ambiguous design tradeoff

Codex should not spend Free Work budget deciding major project philosophy alone.

---

# Work Chunk Rule

Even in Free Work Mode, Codex should work in completed chunks.

Each chunk should preferably end with:

* tests run
* docs updated if relevant
* commit created
* push completed if appropriate
* CI checked when available

This prevents long invisible work sessions from becoming hard to review.

---

# Budget Efficiency Rule

Codex should use the smallest adequate reasoning effort for the task.

Examples:

* low effort for simple docs, git, instructions, and small deterministic edits
* medium effort for normal implementation and tests
* high effort for architecture, security, synchronization, and complex debugging

The goal is not to minimize capability.

The goal is to match compute intensity to task value.

---

# Context Preservation Rule

Free Work should produce project memory where useful.

When work discovers durable project knowledge, Codex should record it in docs rather than relying on chat memory.

This is especially important for:

* Stellaris version behavior
* architecture decisions
* test results
* CI constraints
* failed approaches

---

# Design Goal

Strategic Nexus should use available Codex capacity like an engineering budget:

```text
high budget -> autonomous valuable progress
medium budget -> focused implementation
low budget -> preserve capacity for blockers
```

not:

```text
high budget -> uncontrolled compute spending
low budget -> accidental exhaustion before critical work
```

Free Work exists to accelerate development while preserving judgment, reviewability, and architectural discipline.
