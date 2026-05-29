# Save Data Philosophy

## Core Principle

The LLM should NEVER receive full raw save data.

Stellaris autosaves are active game-owned files.
Strategic Nexus may archive copies for later analysis, but it must never edit, rename, delete, or overwrite active autosaves.

Instead:
- strategic abstraction
- compressed summaries
- historical significance
- geopolitical context

must be extracted.

---

## Autosave Archive Rule

Autosaves may be overwritten by Stellaris during normal play.

The companion app may preserve them by copying stable files into a separate archive directory.

Required behavior:

- read-only access to active autosaves
- copy only after file size and modification time are stable
- skip locked or changing files
- preserve timestamp and source metadata in an archive manifest
- never modify the active save directory except through normal user configuration outside game-owned save files

Archived copies become Strategic Nexus analysis inputs.
Active saves remain owned by Stellaris.

Current local harness:

```text
Strategic Nexus.exe --archive-stable-saves <save_root> <archive_root> <session_id> [stability_delay_ms]
```

This copies only stable `.sav` files into a separate session archive and writes a manifest with source path, archived path, byte count, content hash, and copy/skip status.
It must remain read-only toward the active Stellaris save directory.

Archive verification harness:

```text
Strategic Nexus.exe --verify-autosave-archive <session_archive_dir>
```

Verification checks copied saves against the manifest before they are used as analysis input.

Archived-session summary harness:

```text
Strategic Nexus.exe --summarize-autosave-archive <session_archive_dir> <summary_output.json>
```

The summary contains bounded metadata only.
It is safe as a pipeline handoff object, but it is not a substitute for later save parsing.

Autosave cadence affects analysis precision.
More frequent autosaves provide a finer history step for reconstructing campaign development.
Less frequent autosaves are still valid, but the offline analysis will have a coarser view of history.

The companion app should explain this tradeoff to the user instead of changing game settings silently.

---

## Stellaris `.sav` Container Contract

Stellaris `.sav` files are ZIP containers for Strategic Nexus purposes.

Observed Stellaris save shape:

```text
*.sav
  gamestate
  meta
```

The file may look binary in a text editor because it is compressed, but the extracted `meta` and `gamestate` entries are text in Paradox-style syntax.

Example observed metadata fields:

```text
version="Cetus v4.3.7"
name="Aeelcorp"
date="2200.01.13"
```

Implementation rule:

* open `.sav` read-only as a ZIP archive
* require `meta` and `gamestate` entries before treating the save as parseable
* extract only from an archived verified copy, not directly from an active changing save
* parse `meta` first for cheap campaign identity and version hints
* parse `gamestate` with bounded, whitelist-based extractors
* tolerate missing or renamed fields by emitting explicit uncertainties
* fail closed on corrupt ZIPs, missing entries, oversized fields, unsupported syntax, or parser confidence failure
* never write back into the `.sav`

The first parser should be narrow.
It should extract only fields needed for the next architecture slice instead of trying to understand the whole save.

Recommended first extracted facts:

* game version
* save date
* campaign name
* player country id
* required DLC names
* empire/country identity hints
* coarse planet, fleet, war, diplomacy, economy, and technology availability markers when safely parseable

The parser output must be structured Strategic Nexus data.
Raw `gamestate` text must not be passed to the LLM.

---

## Correct Approach

Good:

{
  "player": {
    "fleet_power": 920000,
    "technology_rank": 1,
    "alliance_members": 0
  }
}

Bad:
- entire save dumps
- excessive tactical details
- planet-by-planet spam
- unnecessary numeric noise
- editing active autosaves
- moving autosaves out of the Stellaris save directory

---

## Important Data Types

### Strategic Power
- fleet strength
- economic ranking
- technology ranking
- diplomatic influence

### Personality Data
- civilization archetype
- historical behavior
- adaptive state

### Historical Events
- betrayals
- major wars
- crisis participation
- federations
- genocides
- megastructures

### Galactic Context
- crisis progression
- hegemonic threats
- alliance blocs
- instability level

---

## LLM Responsibility

The LLM performs:
- interpretation
- strategic abstraction
- doctrine selection
- geopolitical reasoning

The LLM does NOT:
- micro-manage fleets
- optimize build orders
- control tactics
- manipulate game memory
