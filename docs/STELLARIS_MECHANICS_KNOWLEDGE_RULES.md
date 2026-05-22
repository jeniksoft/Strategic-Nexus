# Strategic Nexus - Stellaris Mechanics Knowledge Rules

## Core Rule

Strategic Nexus must not assume Stellaris mechanics are permanently known.

Stellaris is actively updated.

Game mechanics, AI behavior, economy balance, naval balance, DLC ownership, scripting behavior, and checksum rules may change between versions.

Codex and the daemon must treat current Stellaris mechanics as versioned knowledge.

---

# Design Philosophy

Strategic Nexus depends on correct understanding of Stellaris.

If the project misunderstands current mechanics, then even a good LLM may produce bad strategic advice.

The project must therefore maintain a living mechanics knowledge base.

This is not optional documentation.

It is part of the technical foundation.

---

# Latest-Version Rule

Strategic Nexus development should target the latest current public Stellaris version unless explicitly testing rollback compatibility.

Do not rely on old patch knowledge without verifying it against the current version.

When discussing mechanics, record:

* Stellaris version
* checksum if known
* DLC/base-game status
* source date
* whether the knowledge was verified locally

---

# Source Hierarchy

Use this source priority order.

## 1. Local Installed Game Files

Highest authority for the user's active install.

Useful for:

* scripted triggers
* scripted effects
* defines
* common files
* events
* AI weights
* modding syntax
* actual identifiers

Local files reflect what the installed game can actually load.

## 2. Local In-Game Tests

High authority for behavior.

Useful for:

* console/effect behavior
* scripted event behavior
* on_action behavior
* multiplayer synchronization
* error.log validation
* game.log validation

If documentation and local behavior differ, local behavior must be investigated and recorded.

## 3. Official Paradox Sources

Use for:

* patch notes
* dev diaries
* release announcements
* known balance changes
* DLC/base-game changes
* official intent

Preferred sources:

* Paradox Interactive Stellaris news
* Paradox forums
* Steam official news for Stellaris

## 4. Stellaris Wiki

Use for:

* mechanics summaries
* modding pages
* scripting references
* version-history navigation

The wiki is useful but must be checked against:

* page version
* latest patch status
* local installed files

If the wiki is blocked by browser challenges, use direct browser access, local mirrored notes, or other official sources and record the limitation.

## 5. Community Sources

Use only as supporting evidence.

Examples:

* Reddit
* community guides
* patch mirrors
* third-party patch trackers

Community sources may help discover issues, but they should not be treated as primary truth without verification.

---

# Current Baseline Snapshot

As of 2026-05-21, web research indicates:

* Stellaris 4.3 "Cetus" released publicly on 2026-03-18.
* Stellaris 4.3.6 patch released on 2026-05-11 with checksum `6ccb`.
* 4.3 "Cetus" significantly changed naval capacity, economy balance, ascension/origin balance, performance, and stability.
* 4.3.6 rolled Utopia, Synthetic Dawn, and Humanoids into the base game according to public patch/news mirrors.

This baseline must be rechecked before using it for implementation decisions.

Sources recorded at creation:

* Paradox news: https://www.paradoxinteractive.com/games/stellaris/news/cetus-update-is-now-available
* Steam official news: https://store.steampowered.com/news/app/281990/view/670616610165229048
* Patch mirror for quick discovery: https://patchtracker.gg/stellaris/stellaris-4-3-6-patch-released-checksum-6ccb

---

# Mechanics Knowledge Base Rule

The project should maintain mechanics notes in docs.

Recommended future file:

```text
docs/STELLARIS_MECHANICS_BASELINE.md
```

This file should summarize current verified mechanics relevant to Strategic Nexus.

Possible sections:

* version and checksum
* economy model
* naval capacity and fleet scaling
* AI strategic weights
* diplomacy and federations
* espionage and intel
* factions and ethics
* traditions and ascension
* war exhaustion and war goals
* crisis behavior
* modding/script behavior
* known parser-relevant save structures

Do not attempt to document the whole game at once.

Document only mechanics that matter for current implementation.

---

# Update Monitoring Rule

Strategic Nexus should periodically check for Stellaris updates.

Check:

* Paradox Stellaris news
* Paradox forum patch notes
* Steam Stellaris news
* Stellaris wiki version history
* local installed game version/checksum

When a new patch is detected:

1. Record version/checksum/date.
2. Identify mechanics relevant to Strategic Nexus.
3. Check whether bridge/mod/script assumptions changed.
4. Check whether payload domains need adjustment.
5. Check whether parser assumptions changed.
6. Update mechanics notes.
7. Add tests or retest instructions if needed.

---

# Research Task Rule

Mechanics research tasks should be explicit and scoped.

Good research task:

```text
Verify how current Stellaris version handles research alternatives, research weights, and script-accessible research bias hooks.
```

Bad research task:

```text
Learn all Stellaris mechanics.
```

The project should research mechanics just before implementing a feature that depends on them.

---

# Implementation Gate Rule

Before implementing a Strategic Nexus feature that depends on a Stellaris mechanic, Codex should verify that mechanic.

Examples:

* research bias implementation requires current research/technology mechanics knowledge
* military posture implementation requires current fleet/naval capacity/AI weight knowledge
* ethics governance requires current factions/ethics mechanics knowledge
* espionage policy requires current intel/espionage mechanics knowledge
* tradition strategy requires current traditions/ascension mechanics knowledge

If the mechanic is uncertain, ask the user or perform focused research before coding.

---

# Local File Extraction Rule

When possible, prefer reading local Stellaris game files for exact identifiers.

Examples:

* `common/`
* `events/`
* `scripted_effects/`
* `scripted_triggers/`
* `defines/`
* generated script docs if available

Do not guess identifiers from memory.

---

# Version Drift Rule

When a new Stellaris update lands, assume Strategic Nexus may need review.

Review at minimum:

* bridge PoC assumptions
* mod script compatibility
* checksum-sensitive files
* effect/trigger syntax
* strategy dimension meaning
* save parser assumptions
* balance-sensitive reasoning prompts

Minor patches may still affect implementation.

---

# Design Goal

Strategic Nexus should behave like:

```text
a version-aware Stellaris strategic overlay grounded in verified current mechanics
```

not:

```text
an AI project relying on stale remembered game knowledge
```

Correct mechanics knowledge is required for believable strategic intelligence.
