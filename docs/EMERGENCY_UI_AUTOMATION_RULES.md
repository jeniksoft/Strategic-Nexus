# Strategic Nexus - Emergency UI Automation Rules

## Core Rule

UI automation is allowed only as a last-resort support tool.

It may be used for:

* emergency recovery
* development testing assistance
* unattended safety actions
* clicking a known blocking dialog
* collecting screenshots for diagnostics
* assisting a manual test when no API or CLI path exists

It must not become the normal development interface.

It is primarily a development and testing support tool.
It is not part of the Strategic Nexus gameplay architecture.

Desktop screenshots and visual snapshots are privacy-sensitive, but they are allowed for development when bounded correctly.

Codex must not capture the user's full desktop or active screen silently merely for convenience.

For UI development, Codex should prefer isolated render artifacts over full-desktop screenshots.

Preferred UI preview order:

```text
offscreen render artifact -> target-window screenshot -> full-desktop screenshot
```

Offscreen render artifacts are generated from the UI under test without capturing the user's wider desktop.

Offscreen render artifacts may include:

* WinForms `DrawToBitmap` previews
* browser screenshots of a local test page or isolated app viewport
* generated image assets
* rendered document/report/chart previews
* screenshots of synthetic test surfaces created by the project

Offscreen render artifacts must not include:

* the user's desktop
* other visible applications
* notifications
* personal windows
* private monitor content
* remote desktop session content outside the target artifact

Codex may create offscreen UI render previews during development without asking and without announcing every preview.

Temporary offscreen previews should be deleted after use unless they are useful build artifacts.

Allowed screenshot sources without new confirmation:

* screenshots explicitly sent by the user in chat
* screenshots generated from isolated render/test artifacts
* screenshots from a tool window that the user explicitly asked Codex to inspect in the current task

For full-desktop screenshots, active-session screenshots, user desktop/window OCR, or vision inspection of the real interactive session, Codex must ask first and wait for approval.

If Codex needs such a desktop capture during Free Work, it should create a visible task-board request and continue with other work until the user approves.

Temporary diagnostic screenshots should be deleted after use unless the user asks to keep them.

---

# Design Philosophy

Codex should prefer:

```text
API -> CLI -> file/script interface -> deterministic automation -> UI automation
```

For visual inspection specifically, prefer:

```text
structured state -> logs -> screenshots with deterministic image checks -> OCR/vision model interpretation
```

Vision model interpretation is the most expensive and least deterministic option.
Use it only when it provides information that cheaper methods cannot provide.

UI automation is fragile because it depends on:

* screen resolution
* window focus
* DPI scaling
* localization
* timing
* accidental mouse/keyboard state
* visual ambiguity
* screenshot/OCR/vision inference cost

Therefore it should be treated as a controlled emergency capability, not as a primary architecture path.

---

# Allowed Capabilities

An emergency UI automation tool may support:

* screenshot capture
* image recognition
* OCR-like visual inspection
* mouse movement
* mouse click
* keyboard input
* hotkey input
* focused-window detection
* dry-run preview
* action logging

---

# Safety Requirements

UI automation must default to conservative behavior.

Required safeguards:

* dry-run mode where practical
* explicit target window matching
* explicit user consent for full-desktop screenshots unless pre-authorized emergency safety applies
* screenshot before action
* screenshot after action
* concise action log
* bounded action count
* bounded runtime
* no credential entry
* no destructive file actions through UI
* no blind clicking without visual confirmation

If visual confidence is low, the tool should stop instead of guessing.

---

# Explicit Use Boundary

UI automation should not be used to:

* bypass normal APIs
* replace bridge architecture
* automate gameplay as a hidden player
* control Stellaris tactically
* enter passwords or secrets
* alter antivirus/security settings without user awareness
* click unknown dialogs
* make purchases or accept legal agreements
* perform irreversible OS actions without a clearly defined emergency reason

---

# Stellaris Boundary

For Strategic Nexus, UI automation must never become:

```text
runtime bridge
```

It may help with:

* development tests
* local UI test assistance
* screenshot capture
* emergency dialog handling
* manual test assistance

It must not be used as:

* daemon-to-game transport
* gameplay automation
* tactical control
* multiplayer decision injection

Screenshot recognition may be used to verify visible UI test state, but it should not be used for routine gameplay reasoning when game logs, save parsing, or structured state are available.

---

# Emergency Hardware Safety Use

UI automation may be acceptable for machine-safety tasks when no better API exists.

Examples:

* close a blocking overheating warning
* trigger sleep/hibernate through a known OS path
* capture a thermal-monitor screenshot

Preferred path remains direct system APIs such as `SetSuspendState`, not mouse clicking.

---

# Logging Rule

Every UI automation run should record:

* timestamp
* target window
* screenshots used for decision
* confidence if image recognition is used
* exact actions taken
* reason for stopping

Logs should avoid storing personal/private screen content unless necessary for debugging.

Temporary screenshots must not be committed to Git.

Temporary screenshots should normally live under ignored private diagnostics paths and be removed when the diagnostic task is finished.

---

# Implementation Preference

Prefer a small local tool with:

* explicit command-line arguments
* no background persistence by default
* no network access
* no hidden input hooks
* no self-start behavior

The tool should be easy to stop and easy to audit.

---

# Human Override Rule

If the user is present, Codex should prefer asking for manual confirmation before UI actions that could affect the wider desktop.

If the user is away and a pre-authorized emergency condition is met, the tool may perform the smallest safe action needed to protect the machine or preserve work.
