# Strategic Nexus - Codex Tool Installation Autonomy Rules

## Core Rule

Codex may install tools needed for Strategic Nexus work without asking for separate approval every time.

The user has explicitly authorized Codex to install useful development tools when they improve:

* project progress
* verification quality
* development speed
* debugging comfort
* hardware monitoring
* build reliability
* test coverage
* local research capability

The user's preference is:

```text
use the tools that make the work faster and better
```

not:

```text
avoid installation friction at the cost of slow work
```

---

# Allowed Installations

Codex may install or configure:

* compilers
* SDKs
* package managers
* Python packages
* Node packages
* debuggers
* profilers
* binary analysis tools
* hardware monitoring tools
* test runners
* build tools
* Git/GitHub utilities
* local developer support tools

when they are materially useful for the project.

---

# Engineering Judgment Rule

Installation autonomy is not permission to install random software.

Codex should prefer:

* official sources
* reputable package managers
* well-known open-source tools
* portable tools when reasonable
* silent/non-interactive installers
* command-line configuration
* minimal required components
* tools that can be documented and reproduced

Avoid:

* suspicious binaries
* unrelated convenience software
* unnecessary background services
* UI-only installers when a silent option exists
* invasive always-on tools
* unclear licensing
* avoidable privacy risk

---

# Ask First Rule

Codex must ask before installing or changing anything that could materially affect:

* system security
* kernel drivers
* antivirus configuration
* credential stores
* paid services
* cloud uploads
* personal data handling
* game integrity
* multiplayer fairness
* irreversible OS state

If the risk is unclear, Codex should state the uncertainty before proceeding.

---

# Documentation Rule

Installed tools that matter for future work should be documented in the relevant project notes.

Examples:

* build dependencies in bridge-core docs
* monitoring tools in local safety notes
* test tools in validation docs

The project should not depend on hidden local setup when it can be documented.

---

# Comfort Principle

The user's stated priority is that Codex should have a comfortable and capable development environment.

If installing a tool saves significant time or improves quality, Codex should usually install it.

Development comfort is treated as a legitimate engineering benefit.
