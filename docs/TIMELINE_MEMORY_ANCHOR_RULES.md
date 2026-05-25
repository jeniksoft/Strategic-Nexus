# Strategic Nexus - Timeline Memory Anchor Rules

## Core Rule

The Stellaris save timeline is the canonical portable historical memory anchor for a campaign.

Strategic Nexus must use timeline/history information stored inside the save as the foundation for campaign continuity.

The daemon may extend memory.
The save must preserve core historical identity.

---

# Design Philosophy

A campaign must remain portable.

The player may:

* rename saves
* move saves
* transfer saves to another machine
* continue campaign with another daemon instance
* temporarily lose daemon memory

Strategic Nexus must survive these situations safely.

Campaign continuity must not depend entirely on external daemon storage.

---

# Save-As-Portable-State Principle

The save must contain enough information to preserve:

* campaign identity
* empire identity
* strategic continuity
* historical context
* civilization personality continuity

The daemon may enrich memory, but must not become the sole source of campaign history.

---

# Timeline As Historical Anchor

Stellaris already stores historical events inside the save.

Examples may include:

* wars
* diplomatic changes
* federation events
* ruler changes
* crises
* conquests
* defeats
* first contact events
* ascension milestones
* major galactic developments

Strategic Nexus should use this information as historical memory anchors.

---

# Bootstrap Memory Reconstruction

If daemon-side memory is missing:

load save
-> read campaign id
-> parse timeline/history
-> rebuild minimal strategic memory
-> continue campaign

This allows:

* portable campaigns
* safe recovery
* daemon migration
* save sharing
* graceful degradation

---

# Save Identity Rule

Campaign identity must NOT depend on:

* save filename
* folder path
* machine path
* local daemon state

Campaign identity must come from:

* embedded campaign id
* stable campaign metadata
* save-contained identifiers

The daemon uses this identity to locate optional external memory.

---

# Save Responsibility

The save should contain gameplay-facing persistent state.

Examples:

* campaign id
* empire ids
* current doctrine ids
* personality ids
* doctrine inertia state
* strategic flags
* important historical markers
* timeline/history data

The save should NOT contain:

* full LLM prompts
* raw reasoning logs
* large parser snapshots
* verbose daemon internals
* unrestricted memory dumps

---

# Daemon Responsibility

The daemon stores reasoning-facing state.

Examples:

* memory summaries
* strategic interpretations
* historical analysis
* long-form strategic memory
* parser caches
* decision history
* debug logs

These are optional enrichments.

The campaign must still function if daemon memory is lost.

---

# Graceful Recovery Rule

Loss of daemon memory must not corrupt or invalidate the campaign.

Correct behavior:

daemon memory missing
-> rebuild minimal memory from save timeline
-> continue with reduced historical richness
-> preserve current strategic doctrine if possible

Rejected behavior:

daemon memory missing
-> campaign unusable
-> strategic state destroyed

---

# Timeline Interpretation

Timeline events should influence:

* paranoia
* trust
* rivalry
* trauma
* expansionism
* federation preference
* crisis fear
* hegemon fear
* military posture
* diplomacy

Examples:

* repeated invasions increase border paranoia
* betrayal increases distrust
* crisis devastation increases crisis preparation
* successful conquest increases strategic confidence

---

# Memory Persistence Philosophy

Strategic Nexus memory should behave like civilization memory.

Civilizations:

* remember wars
* remember betrayals
* remember humiliation
* remember existential threats
* reinterpret history over time

The save timeline is the durable backbone of this memory.

The daemon is the higher-level interpretation layer.

---

# Multiplayer Rule

In multiplayer:

* timeline exists inside synchronized save state
* host/coordinator offline analysis may reconstruct memory from archived saves
* clients do not maintain separate Strategic Nexus reasoning memory
* strategic continuity remains host-authoritative

This prevents divergent historical interpretation between clients.

---

# Design Goal

The player should feel:

This civilization remembers what happened in this galaxy.

Even if:

* the save was moved
* the save was renamed
* the daemon cache was lost
* the campaign changed machines

Strategic Nexus must preserve historical continuity through the save itself whenever possible.
