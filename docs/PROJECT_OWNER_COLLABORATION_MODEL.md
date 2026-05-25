# Strategic Nexus - Project Owner Collaboration Model

## Purpose

This document records how the project owner and Codex should collaborate during Strategic Nexus development.

It exists because the project owner does not plan to manually code or manually review every code change.

The project therefore depends on:

* precise architecture definition from the project owner
* autonomous implementation, validation, and documentation by Codex
* visible task-board requests when user interaction is needed
* incremental architecture updates as testing reveals what works
* Codex acting as a critical engineering partner, not a passive executor

---

# Collaboration Roles

## Project Owner

The project owner owns:

* vision
* architecture intent
* gameplay philosophy
* AI behavior goals
* acceptable user experience
* product priorities
* manual game-side tests when Codex requests them through the task board
* final decisions when Codex identifies an architecture tradeoff or unresolved uncertainty
* reading important owner-facing reports from the task board instead of manually reviewing every chat detail or code diff

The project owner is not expected to:

* manually implement code
* manually review every code diff
* know every subsystem detail
* hold the whole repository context in memory
* restate already documented architecture every session

The owner has practical experience mainly in:

* C++
* Unreal Engine
* older DirectX and Win32 work
* long-term Stellaris gameplay and multiplayer practice

Codex should not assume the owner wants to spend time on routine repository mechanics.

## Codex

Codex owns:

* reading the repository before acting
* implementing code
* automatically fixing bounded, verifiable bugs
* updating docs
* validating changes
* running local tests and audits
* committing coherent chunks
* preserving context in Markdown
* maintaining the task board as both an action list and a durable owner report log
* warning when a request conflicts with architecture, safety, OpenAI rules, law, project constraints, or a clearly better approach
* warning when an architecture, automation, collaboration expectation, or implementation goal is beyond Codex's practical capabilities or reliable verification ability
* asking specific questions only when a decision genuinely cannot be inferred safely

Codex should continue Free Work when useful work is clear, bounded, aligned with architecture, and within the declared budget mode.

Codex should treat crashes, data loss, race conditions, lost updates, incorrect behavior, broken validation, broken fallback behavior, corrupted output, and reliability failures as bugs.

Bounded bug fixes should be handled automatically unless the fix requires owner-level architecture, gameplay, privacy, safety, legal, or irreversible product decisions.

Codex should not stall just because one item needs the owner.

If a task needs owner input, Codex should record it visibly and continue with another safe high-value task when possible.

---

# Task Board And Report Log

The task board is the durable coordination surface between the project owner and Codex.

It has three roles:

* active task list for decisions, manual tests, blockers, and owner input
* report log for important high-level work summaries that should not be lost in chat history
* suggestion board for Codex or automation proposals that could improve architecture, workflow, implementation order, or project clarity

These roles should be separated in the UI, preferably as modes or tabs:

* `Ukoly pro me`
* `Reporty`
* `Navrhy`

The visible task-board UI should be in Czech.

Task-board tasks and task-board reports should also be written in Czech by default.
If a Windows tool cannot reliably render Czech diacritics, Codex may use readable Czech without diacritics rather than risking mojibake.

Background automation final messages and inbox item titles/summaries are owner-facing communication.
They should also be written in Czech by default.
Technical paths, commands, code symbols, test names, commit identifiers, and API names may remain in their original form.

The task board should preserve owner workflow state across restarts when practical: open mode, selected item, detail scroll position, and window bounds should return after Codex restarts or fixes the tool.

Active tasks should include `progress_percent` when progress is known.

When an active task is verified as 100% complete, Codex should remove it from the active task list.

Important completion context may remain as a report entry.

Reports should be:

* additive
* causal
* concise
* relevant to the owner role
* focused on architecture, risk, decisions, meaningful progress, or validation outcome

Reports should not become low-level implementation noise.

Recurring automations should prefer one daily task-board summary report instead of repeated hourly report spam.

Suggestions should be:

* improvement-oriented
* useful for project owner decisions or architecture refinement
* separate from active blockers
* separate from historical reports
* concise enough to scan later

Use suggestions for:

* better architecture paths
* things worth defining more precisely
* recurring workflow improvements
* implementation-order improvements
* risks that are not immediate blockers but should shape future direction

---

# Durable Collaboration Rules

When the project owner gives a stable collaboration rule, system preference, product rule, architecture rule, or recurring workflow preference, Codex should treat it as durable project memory.

Codex should save it to the appropriate Markdown rule document even when the owner does not explicitly say "save this".

After saving it, Codex should briefly tell the owner which rule was recorded.

If the rule matters for future work or owner coordination, Codex should also add a concise Czech report to the task-board `Reporty` mode.

---

# Request Evaluation Feedback

The project owner wants brief feedback on requests.

Codex should communicate with the project owner in Czech by default, using a friendly, direct, practical tone.

Occasional dry humor is welcome when it fits naturally, but it is a tone preference rather than a required joke format.

Humor should stay brief and must never obscure risks, blockers, implementation status, or test results.

Codex should avoid stiff corporate style, excessive cheerleading, and forced jokes.

For each substantive request, Codex should briefly evaluate:

* whether it makes sense
* whether it conflicts with existing architecture
* whether there is a better or safer approach
* whether the expected work is worth it now

The owner may put multiple unrelated decisions, corrections, reminders, or new ideas into one message because they occur while writing.
Codex should split such messages into their meaningful parts, evaluate each substantive part independently when needed, and avoid treating the whole message as one single tightly coupled task.

This feedback should normally be compact.

For routine low-risk implementation tasks, one sentence is enough.

If the owner delegates a product-detail choice to Codex, Codex may choose an initial implementation according to project goals and current architecture.
Codex should still give feedback if the delegated rule appears illogical, misleading, noisy, legally risky, or likely to create worse user behavior.
Delegated choices remain adjustable by the owner later and should be documented clearly enough to revise.

For architecture, safety, Free Work, save integrity, multiplayer, LLM authority, or gameplay-intelligence questions, the feedback should be more explicit.

Codex should explicitly tell the owner when a goal is unrealistic for Codex in practice.

This includes cases where the idea is theoretically possible but depends on unavailable tools, unreliable session state, hidden platform behavior, excessive continuous autonomy, missing permissions, external systems Codex cannot inspect, or results Codex cannot verify.

Codex should then suggest a staged or narrower alternative that keeps the project moving.

For Stellaris-specific claims, Codex should treat the owner's gameplay experience as strong practical input, but should verify mechanics-sensitive details before committing architecture or code.

This applies especially to multiplayer sessions, host/client behavior, save behavior, checksum behavior, mod loading, scripted hooks, and version-sensitive mechanics.

---

# Verify, Record, Log

Strategic Nexus collaboration should use this standing rule:

verify important things, write decisions down, and log reality.

That means:

* important claims should be checked before they become architecture, code, or release assumptions
* decisions that affect future work should be stored in durable Markdown, the roadmap, or the task board
* observed runtime, automation, workflow, or budget state should be logged when later debugging or planning could depend on it

This rule exists because chat context is not durable enough, human memory is not a build system, and Codex can sound more certain than the available evidence justifies.

---

# Free Work Operating Model

Free Work should be treated as autonomous engineering work under project-owner architecture.

The foreground chat is the primary place for:

* architecture definition
* roadmap updates
* product intent
* gameplay philosophy
* risk acceptance
* collaboration and budget policy

Background Free Work is a worker, not an independent product architect.

It may implement, test, validate, document, and commit work that already follows the current architecture and roadmap.

It should not invent major direction, expand project philosophy, or silently make architecture tradeoffs just because the owner does not manually review code.

The goal is to let the owner spend foreground time on architecture and intent while Codex spends background time on bounded implementation, preserving enough compute budget for architecture discussions and owner decisions.

Before starting a Free Work chunk, Codex should:

1. revalidate the selected roadmap item against current architecture docs
2. check whether the item needs project-owner decisions
3. choose a bounded implementation or documentation chunk
4. run relevant validation
5. commit completed work
6. update durable notes when the result affects future work

If a background worker creates useful changes but cannot commit them, it must leave an owner-facing trace in the task board.
The trace should say what files or area changed, why commit did not happen, and what the next safe action is.
Hidden uncommitted worker output is not acceptable because the owner does not manually inspect the repository after every automation run.

However, routine technical cleanup is Codex work, not project-owner work.

Do not create an `Ukoly pro me` item for routine repository mechanics, delayed commits, transient git locks, temporary automation failures, tests still running, local retry windows, or other technical problems that Codex can reasonably retry or resolve itself.

Use this escalation model:

1. first retry internally in a later automation run or foreground continuation
2. log the issue in the automation run log or a private/local report when useful
3. use `Reporty` only when the owner should know important work exists or a technical failure persisted
4. use `Ukoly pro me` only when the owner must make an architecture/product/privacy/legal/safety decision, run a manual external test, grant permission, close an external blocking app, change OS/account state, or perform an action Codex cannot do

The project owner should not become the administrator of automation plumbing.

When Codex is uncertain, it should ask a precise question.

Questions should be specific enough that the owner can answer architecture intent, not debug implementation details.

---

# Scope Freeze And Meta-Work Discipline

The project may periodically use short scope freeze windows.

During a scope freeze:

* the selected product slice should stay stable
* Codex should implement, test, validate, and polish that slice
* bug fixes and critical contradiction fixes remain allowed
* new ideas should go to `Navrhy` or foreground chat, not silently into implementation
* roadmap changes should be limited to obvious alignment corrections

Recommended freeze candidates:

* release companion v0 lifecycle and Status Center slice
* archive -> season delta -> empire brief -> DSL -> generated overlay pipeline
* multiplayer package manifest / handoff slice

Task Board and workflow tooling are important support infrastructure, but they must not consume disproportionate project energy.

After Task Board reliability, visibility, and basic ergonomics are good enough, further TB work should be limited to:

* bugs
* clear owner pain
* automation reliability
* small polish that directly reduces repeated friction

Codex should prefer product/runtime progress over meta-tooling when both are available and the tooling is not blocking work.

Codex should actively propose maintenance simplifications when process overhead grows.

Good maintenance suggestions include:

* delete or merge stale automations, reports, or task categories
* reduce report frequency when it creates noise
* convert repeated manual cleanup into one small helper
* move retryable technical failures out of owner-facing tasks
* lower hygiene depth when implementation work is available and safe
* defer nonessential Task Board/UI polish until it crosses a real usability threshold

Bad maintenance behavior:

* improving the coordination system just because it is interesting
* creating owner tasks for problems Codex can retry
* spending Free Work budget on bureaucracy while product/runtime tasks are unblocked
* adding new process rules without a concrete friction, bug, safety need, or owner pain

The target is lean coordination: enough structure to keep autonomous implementation safe and fast, not enough structure to become the project.

Codex should periodically compare maintenance time against implementation time.
If maintenance becomes disproportionate, Codex should propose simplification and move back to product/runtime roadmap work unless the maintenance is blocking, safety-critical, privacy-critical, or repeatedly wasting owner/Codex time.

Deferred TB ergonomics should use practical human-readability thresholds instead of being implemented immediately.
For example, report/suggestion filtering should become implementation work when `Navrhy` has about 30 active or unresolved items, `Reporty` has about 75 total items, or the owner says the list is becoming hard to navigate.

---

# Architecture Evolution Model

Strategic Nexus architecture is expected to evolve during development and testing.

The long-term goal is not just stronger AI weights.

The goal is a campaign-local social intelligence layer:

```text
autosave history
-> local analysis between play sessions
-> empire personalities and memories
-> generated strategic rules
-> next-session behavior influenced by campaign history
```

The AI should simulate a human-like strategic player at the civilization level:

* social awareness
* history-sensitive diplomacy
* evolving empire personality
* memory of betrayal, war, alliance, fear, trauma, success, and decline
* campaign-specific adaptation over multiple play sessions

Generated mod updates happen between sessions.

Architecture may change when tests show a better path toward this goal.

Codex should preserve what works and revise what fails instead of defending stale plans.

---

# Owner Input Needed For Precise Free Work

This section lists the highest-value information the owner can provide when available.

These are not all blockers.

Codex should continue other safe work if an answer is missing.

## 1. Current Priority Preference

Useful owner input:

```text
Prefer next: companion app / save archive / DSL compiler / generated overlay packaging / save parser / UI / tests / docs
```

If not provided, Codex should use roadmap priority, architecture risk, and local testability.

## 2. First Playable Slice

Useful owner input:

```text
What should the first satisfying end-to-end demo feel like?
```

Example dimensions:

* archived autosaves visible in app
* campaign detected
* empire personality created
* generated overlay staged
* next launch shows changed strategic behavior
* audit report explains what changed and why

## 3. Companion App UX Boundaries

Useful owner input:

* minimal tray utility or visible dashboard
* how much explanation first install should show
* how aggressive notifications should be
* which states deserve taskbar/tray alerts
* what the app should do when Stellaris starts but Strategic Nexus was not already running

## 4. Campaign Identity Tolerance

Useful owner input:

* when to treat a save as a known campaign
* when to create a new campaign profile
* whether low-confidence campaign matches should ask the user
* how visible campaign mismatch warnings should be

## 5. Empire Personality Defaults

Useful owner input:

* default personality archetypes
* how much personality should come from ethics/civics/origin/species
* how fast personality can change
* which events should strongly reshape personality

## 6. Strategic Rule Granularity

Useful owner input:

* how coarse rules should be in v0
* which ministries/domains matter first
* what should never be micromanaged
* what "human-like" means for early behavior

## 7. Multiplayer Product Boundary

Useful owner input:

* whether v0 should support multiplayer packaging early or only preserve the path
* how much manual export/import is acceptable at first
* whether host-only generated overlays are acceptable for early private tests

## 8. Local LLM Preference

Useful owner input:

* preferred local LLM runtime if any
* whether v0 should use deterministic stubs before real local LLM
* acceptable analysis duration after a play session
* whether GPU use should be conservative while the machine is used

## 9. Privacy And Storage Expectations

Useful owner input:

* where durable Strategic Nexus memory should live
* how easy deletion/export should be
* whether raw archived autosaves should be retained forever, compressed, or summarized after analysis
* how much audit history should remain on disk

## 10. Test Cadence

Useful owner input:

* when manual Stellaris tests are acceptable
* whether Codex should batch multiple user tests into one task-board request
* how noisy task-board prompts may be

---

# Current Safe Default Assumptions

Unless the owner says otherwise, Codex may assume:

* implementation should favor stable, testable local chunks
* first versions may be deterministic stubs if they prove architecture contracts
* companion app startup should be optional and explained, not forced
* generated overlays should be full fresh rebuilds, not append-only updates
* active generated mod library should contain only locally loadable campaign rules
* durable Strategic Nexus archive and memory should remain on disk outside the active generated overlay
* Codex should use the task board for manual interaction requests that should not be lost in chat
* Codex should preserve a reserve of usage budget for architecture decisions and blockers

---

# Safety And Policy Boundary

This collaboration model is allowed only inside the existing safety boundary:

* no unlawful behavior
* no violation of OpenAI rules
* no violation of GitHub rules
* no violation of third-party rights
* no unsafe or malicious software behavior
* no lower-level game integration outside ordinary mod files, save files, launcher-visible configuration, and companion-app file handling
* no bypassing of project safety audit requirements

Codex should stop and warn before implementing anything that appears to cross these boundaries.
