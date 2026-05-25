# Strategic Nexus - Codex Model Usage Rules

## Core Rule

Codex should automatically adapt model intensity and reasoning effort to the task type.

The goal is to balance:

* cost
* speed
* accuracy
* architectural quality
* project safety

Do not use maximum intelligence for routine mechanical work.

Do not use weak reasoning for architectural blockers.

---

# Practical Interpretation

"Model intelligence" means:

* stronger model capability when available
* higher reasoning effort when available
* deeper analysis before acting
* more careful verification

Higher intelligence usually increases resource usage.

Strategic Nexus should spend intelligence where mistakes would be expensive.

---

# Low Consumption Mode

Use low-cost reasoning for:

* creating simple documentation files from user-provided text
* appending exact rule text
* small copy edits
* git status checks
* commits
* pushes
* simple file existence checks
* formatting-only documentation changes

Behavior:

* act quickly
* avoid over-analysis
* preserve user text faithfully
* verify basic file status
* commit only intended files

---

# Medium Mode

Use medium reasoning for:

* normal code changes
* small C++ implementation tasks
* test harness updates
* JSON validation logic
* build fixes
* script/mod file changes
* local debugging with logs
* moderate documentation synthesis

Behavior:

* inspect relevant files first
* follow existing project patterns
* run focused verification
* avoid unnecessary redesign

---

# High Reasoning Mode

Use higher reasoning for:

* runtime bridge architecture
* multiplayer synchronization decisions
* payload schema design
* safety boundaries
* daemon/mod/bridge responsibility splits
* save compatibility strategy
* anti-omniscience architecture
* memory model design
* personality and ethics system architecture
* major refactors
* decisions that may constrain the project long-term

Behavior:

* reason carefully
* challenge weak assumptions
* compare alternatives
* preserve safety constraints
* document consequences
* avoid premature implementation

---

# Maximum Reasoning Mode

Use maximum available reasoning only for:

* hard technical blockers
* architecture decisions with high irreversibility
* safety/security-sensitive integration choices
* multiplayer desync risk analysis
* decisions where a wrong path could waste significant development time

Behavior:

* be skeptical
* verify assumptions
* prefer primary sources where possible
* state uncertainty explicitly
* do not rush code

---

# Automatic Task Classification Rule

Codex should classify each task before acting:

* mechanical
* documentation
* normal implementation
* debugging
* architecture
* safety-critical
* blocker

Then choose reasoning intensity accordingly.

If the task is ambiguous, default to medium.

If the task touches Strategic Nexus core architecture, default to high.

---

# Consumption Optimization Rule

Strategic Nexus should not waste expensive reasoning on:

* exact text insertion
* routine commits
* already-decided documentation
* repetitive file creation

Strategic Nexus should spend reasoning on:

* project direction
* bridge architecture
* multiplayer viability
* schema evolution
* save compatibility
* bounded LLM behavior
* safety and fallback design

---

# User Override Rule

If the user explicitly asks for:

* quick
* cheap
* simple
* just commit it

Codex should bias toward lower consumption.

If the user explicitly asks for:

* deep review
* critical analysis
* architecture decision
* verify from sources
* think hard

Codex should bias toward higher reasoning.

---

# Critical Partner Rule

Lower consumption does not mean blind agreement.

Codex must still flag obvious contradictions, unsafe assumptions, or project rule violations.

Higher reasoning is required when disagreement may materially affect:

* architecture
* safety
* multiplayer support
* long-term maintainability
* feasibility

---

# Current Strategic Nexus Preference

For this project:

* routine documentation tasks should be cheap
* normal implementation should be medium
* architecture and bridge work should be high
* safety, synchronization, and complex debugging should use higher effort when needed

The project should conserve resources without sacrificing architectural judgment.
