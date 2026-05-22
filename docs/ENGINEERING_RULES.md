1. Prefer explicit systems over generic frameworks.
2. Prefer bounded enums over free-form text.
3. Prefer fail-safe fallback over correctness.
4. Never trust daemon output directly.
5. Never execute arbitrary runtime commands.
6. Keep integration in documented mod scripting and local files.
7. Keep vanilla mod functional without bridge.
8. All gameplay behavior lives in scripted mod layer.
9. The bridge core is validation and artifact generation only.
10. Avoid hidden magic and implicit behavior.
11. Simplicity is more important than elegance.
12. Runtime safety is more important than features.
13. Do not accept any architecture change that violates, weakens, bypasses, disables, or narrows the safety audit.
14. If an architecture proposal conflicts with `tools/run_safety_audit.ps1`, redesign the proposal until the audit passes with no findings.
15. Multiplayer test packages must match the host mod byte-for-byte except for the top-level `.mod` path when a different user profile requires it.
16. Before giving a multiplayer client package to another player, compare host-installed mod files against the packaged files and record any intentional differences.
17. Version/checksum mismatches must be treated as environment/package problems first, not gameplay logic problems.
18. Commit messages and committed docs must include enough project context for a future AI assistant or GitHub reader to understand what changed, why it changed, and what is already verified.
19. Keep GitHub history useful: commit the docs, tests, and source needed to reconstruct project state; do not commit local build artifacts such as `dist/` zips unless the package itself is intentionally part of the deliverable.
20. Stellaris multiplayer checksum must be treated as sensitive to gameplay-affecting mod/script content. Host and clients must use the same Stellaris version, same enabled mod list, same load order, and byte-identical gameplay-affecting mod files before MP tests.
21. Changes under gameplay script folders such as `common/`, `events/`, `localisation_synced/`, and other non-visual game data must be assumed checksum-relevant unless proven otherwise. Package them identically for host and clients.
22. Plain localisation under `localisation/` may not be gameplay logic, but keep it identical across MP test packages anyway to avoid hidden packaging drift.
23. If MP checksum differs, verify vanilla checksum, mod list, load order, launcher state, debug mode, and packaged file hashes before changing Strategic Nexus behavior.
24. Release runtime integration must not require debug interfaces or manual transport steps.
25. Debug-only workflows must be treated as development environment constraints, not Strategic Nexus release dependencies.
26. When debug workflows conflict with multiplayer checksum testing, prefer deterministic mod-side smoke harnesses.
27. Development smoke tests must be clearly marked as PoC harnesses and must not be mistaken for final daemon/bridge architecture.
28. Do not use observer mode as a runtime bridge.
29. Prefer Stellaris' built-in multiplayer lobby observer role for OOS soak tests and strategic evaluation.
30. Observer workflows are test conveniences, not release automation.
31. Observer mode may be used as an OOS soak-test convenience after Strategic Nexus state has entered the game, but the result must be recorded separately from active player-control validation.
32. Once LLM/daemon doctrine output exists, observer mode may also be used for strategic evaluation of galaxy-level behavior, but never as proof of bridge correctness or release automation.
33. During private development, visible debug/diagnostic dialog text should be Czech even when stored under `localisation/english`; final player-facing release text can be converted back to English later.
34. Codex must treat project Markdown files as durable replacement context memory. Important architecture decisions, verified results, blockers, and stable user philosophy should be preserved in docs instead of relying on chat memory.
35. Codex should update `MASTER_ARCHITECTURE_INDEX.md`, roadmap documents, or subsystem rule docs when new decisions materially affect future work.
36. Codex should ask the user only for decisions that affect architecture, safety, multiplayer viability, save integrity, LLM role boundaries, gameplay philosophy, or irreversible scope. Routine low-level implementation should be handled independently.
37. When Codex is uncertain about something that could materially change the project direction or risk profile, it must state the uncertainty and ask before committing to that path.
38. Stellaris mechanics knowledge must be treated as versioned and unstable. Before implementing features that depend on specific mechanics, verify against the current game version, local game files, official sources, or targeted in-game tests.
39. Do not guess current Stellaris identifiers, scripted hooks, AI weights, economy behavior, or DLC/base-game status from memory when the implementation depends on them.
40. Codex may install development tools, dependencies, runtimes, debuggers, profilers, monitoring tools, SDKs, and support utilities without asking for separate approval when they materially improve project progress, verification, safety, or development comfort.
41. Tool installation autonomy does not remove engineering judgment: prefer reputable sources, package managers, official releases, portable tools when reasonable, and minimal necessary scope. Avoid suspicious binaries, unrelated software, invasive background services, or tools that create avoidable security/privacy risk.
42. If an installation could materially affect system security, drivers, antivirus posture, credentials, paid services, cloud uploads, game integrity, multiplayer fairness, or irreversible OS state, Codex must call out the risk and ask before proceeding.
43. Installed tools that matter for future development should be documented in the relevant architecture/tooling notes so later work can reproduce the environment.
44. Prefer silent/non-interactive installations and command-line configuration. UI-driven installers are fragile for Codex and should be avoided unless there is no practical silent path.
45. UI automation with mouse, keyboard, screenshot recognition, or OCR is allowed only as a last-resort emergency/support tool with bounded actions, logging, and visual confirmation. It must not become a runtime bridge, gameplay automation path, credential entry path, or normal replacement for CLI/API/file interfaces.
46. UI automation is mainly for development and testing support. Screenshot/OCR/vision recognition is comparatively expensive and less deterministic, so prefer structured state, logs, CLI output, and deterministic probes before using model-based visual interpretation.
47. Full-desktop screenshots, active-session snapshots, user desktop/window OCR, and vision inspection of the real interactive session are privacy-sensitive. For UI development, prefer isolated offscreen render artifacts that do not capture the user's wider desktop, other applications, notifications, or private monitor content. Codex may create isolated offscreen UI previews without asking or announcing every preview. If real desktop capture is needed during Free Work, Codex must create a task-board request, wait for approval, and continue with other work. Temporary diagnostic screenshots must be deleted after use and never committed.
48. If an installed tool proves useless, unreliable, or harmful for the project, Codex may uninstall it without asking, and should document the reason when the tool mattered to the development environment.
49. When the user describes an architectural idea, design rule, gameplay principle, or long-term project direction ambiguously, Codex should briefly state how it understood the request, what assumption would drive implementation, and what risk exists if that interpretation is wrong before making durable architecture or code changes.
50. This clarification rule is especially important because user messages may be abbreviated, contain typos, mix Czech and English terminology, or assume shared long-term context. For low-risk implementation details, Codex may make a reasonable assumption and document it.
51. Only legal, GitHub-rule-compliant, OpenAI-rule-compliant, and safe-use-compliant material may be committed or pushed to GitHub.
52. Treat every GitHub commit as potentially public even while the repository is private, because visibility can change later.
53. Do not put anything on GitHub that would be unsafe, unlawful, rights-violating, platform-term-violating, GitHub-rule-violating, or OpenAI-rule-violating if made public.
54. Maintain a local working copy of the GitHub repository as a backup and continuity anchor.
55. Before risky repository operations, major architecture rewrites, or cleanup commits, verify the local clone is present and healthy enough to preserve current project state.
56. The local backup must follow the same legal, GitHub-rule, OpenAI-rule, and safe-use boundaries as the remote repository.
57. When Codex needs a manual user-side Stellaris test or other human action during Free Work, it should make that request highly visible instead of hiding it in chat history. Prefer the local user task board under `tools/dev_attention/`, with clear task, reason, severity, and exact instructions.
58. The user task board should show only current active tasks. Codex should add, update, or clear tasks as its needs change, and tasks the user completes may be removed from the active list.
59. If a requested user-side test remains unacknowledged and the work is blocked, Codex may repeat or refresh the visible task at a reasonable cadence based on urgency. Repeated prompts must remain bounded and should not interfere with ordinary computer use more than necessary.
60. The user task board may be installed as a normal Windows shortcut and Startup-folder shortcut so the user can keep it pinned and visible. Avoid a background service unless a plain shortcut cannot satisfy the workflow.
61. If a user request appears illogical, inefficient, impractical, unnecessarily risky, or inferior to another clear approach, Codex should say so before implementing. Codex should explain the concern concisely and propose the better approach, while still respecting the user's final decision.
62. Remote Windows Desktop, Fast User Switching, lock screen, and disconnected interactive sessions must be treated as unreliable for UI-dependent workflows. Prefer files, logs, CLI tools, services, scheduled tasks, or tray helpers that tolerate session changes.
63. Codex must not assume a tray app, visible dialog, mouse click, keyboard action, screenshot, or game window is available in the active user session when the machine is accessed remotely or locked. If a task requires the user's visible desktop or Stellaris window, request explicit user-side confirmation through the task board or chat.
64. Remote desktop may affect GPU behavior, game rendering, focus, hotkeys, clipboard, audio, monitor sleep, and secure desktop/UAC prompts. UI automation results collected over RDP must be marked as environment-sensitive unless independently verified in the normal local session.
65. When implementing or revising UI, icons, reports, images, charts, or any other project artifact where color matters, Codex must explicitly check color harmony, color dominance, contrast, and readability before considering the change finished.
66. UI color palettes should use coherent color relationships rather than ad hoc colors. Prefer a small primary palette, restrained accent colors, and stable semantic meaning. Large surfaces should use harmonious base colors; warning/error colors should normally be small accents unless the entire screen is intentionally an emergency state.
67. Codex should avoid pure white text, toxic saturated colors, clashing large red/green areas, and one-note palettes. Text colors should be softly tinted to match the palette while preserving sufficient contrast.
68. For UI work, Codex should verify contrast mechanically where practical, using WCAG-style contrast checks or an equivalent local calculation. If aesthetics and readability conflict, readability wins, but Codex should still search for a more harmonious readable palette.
69. Color decisions should account for area size and visual hierarchy. A color that works as a small icon, border, or badge may be wrong as a large header or background panel.
70. Czech is the primary interaction language for the project owner. Codex should use Czech in chat, private development UI, local task-board text, debug dialogs, and user-facing development notes unless a specific artifact requires English.
71. When an English technical term is useful or necessary, Codex should usually include a Czech explanation or translation in parentheses on first use, unless the term is a proper name, code symbol, filename, API name, command, or would become less clear if translated.
72. Project documentation may remain English when it is intended as durable architecture material for GitHub, future contributors, or tools, but communication with the user should prefer Czech and avoid unnecessary English cognitive load.

## External Verification Notes

- Paradox support documents multiplayer failures caused by differing checksums.
- Stellaris wiki troubleshooting states that multiplayer requires the same mod list and load order for all players.
- Stellaris modding tutorial states that gameplay-affecting mods change checksum.
- Debug interfaces are not stable Strategic Nexus release transport paths.
- Prefer the built-in MP lobby observer role for observer-style tests when available.

Sources:

- https://support.paradoxplaza.com/hc/en-us/articles/360016500800-Checksum-differs-in-multiplayer
- https://stellaris.fandom.com/wiki/Modding#Troubleshooting_mod_installation
- https://stellaris.fandom.com/wiki/Modding_tutorial
