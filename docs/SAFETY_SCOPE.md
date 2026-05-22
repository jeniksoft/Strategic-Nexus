# Strategic Nexus - Safety Scope

## Purpose

This document defines the safe development boundary for Strategic Nexus.

The project is a local gameplay modding experiment for improving a private Stellaris play experience through ordinary mod scripting, external reasoning, and bounded local data exchange.

## Law And OpenAI Policy Rule

Strategic Nexus must not do anything illegal or against OpenAI rules.

This is a hard no-go rule. If a proposed feature, tool, research direction, or implementation path could violate law, third-party rights, platform terms, OpenAI policy, or safe-use boundaries, the project must stop that path and choose a safer architecture.

When uncertain, default to the safer interpretation and document the decision before continuing.

## Allowed Work

Strategic Nexus may implement:

* Stellaris mod files using documented script, event, modifier, localization, and descriptor formats
* an external daemon that reads and writes local JSON files
* deterministic strategic pipeline logic
* local LLM calls that output only validated structured recommendations
* schema validation, replay protection, fallback handling, and audit records
* save or snapshot parsing for local strategic summaries when legally available to the user
* tests, fixtures, and CI for the local daemon and payload validators

## Disallowed Work

Strategic Nexus must not implement or publish:

* lower-level modification of the running game process
* proprietary executable internals or analysis notes
* tooling that attaches to, traces, patches, or inspects the running game
* anti-cheat, DRM, checksum, or multiplayer bypasses
* arbitrary debug-interface command execution from daemon or LLM output
* tactical orders, fleet control, save editing, or hidden gameplay mutation

## Integration Model

The safe model is:

```text
Stellaris mod scripts
    -> explicit local fixture or request data
    -> Strategic Nexus daemon
    -> bounded JSON recommendation
    -> validator and allowlisted effect batch artifact
    -> normal scripted mod receiver or manual development harness
```

The daemon is advisory. The game remains authoritative. Invalid, missing, stale, or low-confidence payloads fall back to normal scripted behavior.

## LLM Boundary

LLM output is untrusted until validated.

The LLM may recommend bounded values such as:

```json
{
  "strategy_dimensions": {
    "military_posture": "defensive",
    "research_bias": "economy"
  }
}
```

The LLM must not output script, debug-interface text, executable commands, memory addresses, or tactical orders.

## Repository Rule

Only legal, GitHub-rule-compliant, OpenAI-rule-compliant, and safe-use-compliant material may be committed or pushed to GitHub.

Treat every GitHub commit as potentially public, even while the repository is private, because repository visibility can change later by mistake or by decision.

Do not put anything on GitHub that would be unlawful, violate GitHub rules, violate third-party rights, violate platform terms, violate OpenAI policy, expose sensitive proprietary internals, or create an unsafe-use risk if the repository became public.

Keep this repository private unless the public branch has been reviewed to contain only safe mod scripting, daemon code, fixtures, tests, and non-sensitive design documentation.

## Local Backup Rule

Codex must maintain a local working copy of the GitHub repository as a backup and source of continuity.

Before risky repository operations, major architecture rewrites, or cleanup commits, verify the local clone is present and healthy enough to preserve the current project state.

The local copy is not a place to hide unsafe material. It must follow the same legal, GitHub-rule, OpenAI-rule, and safe-use boundaries as the remote repository.
