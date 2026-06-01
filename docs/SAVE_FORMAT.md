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

Live session archive harness:

```text
Strategic Nexus.exe --archive-live-saves <save_games_root> <archive_root> <session_id> [stability_delay_ms]
Strategic Nexus.exe --snc-live-autosave-monitor <save_root|auto> <archive_root> <session_id> [poll_ms] [stability_delay_ms] [max_iterations] [use_detected_stellaris_state] [stellaris_running_override] [capture_when_not_running]
Strategic Nexus.exe --run-snc-session-capture [archive_root] [status_output.json] [session_id] [poll_ms] [stability_delay_ms] [max_iterations] [capture_when_not_running]
```

`--archive-live-saves` recursively scans a whole `save games` root, captures stable `autosave*.sav` and `ironman.sav` revisions, and names archived copies by source identity plus content hash.
Repeating the command is idempotent for unchanged files, but a changed autosave with the same active filename becomes a new archived revision.
When SNC watches multiple discovered roots in one session, the aggregate live manifest must use the source-root label `multiple_stellaris_save_roots` rather than pretending all archived copies came from whichever root was scanned last.
`--snc-live-autosave-monitor` is the native SNC-owned monitor loop. It can use `auto` to discover Stellaris save roots, polls for stable autosave/ironman revisions, and should be called internally by the companion app so the user only has to run SNC.
`--run-snc-session-capture` is the first owner-testable SNC runtime mode. It auto-discovers save roots, creates or uses a session id, writes a heartbeat/status JSON, and uses `dist/snc_live_autosave_archive` plus `dist/private_reports/snc_live_autosave_monitor_status.json` by default.
`tools/watch_stellaris_live_autosaves.ps1` may exist as a manual development harness, but production startup capture must not depend on hidden PowerShell, HKCU Run, or scheduled-task persistence.

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

Observed local save layout, 31.5.2026:

Distribution note:

* Stellaris is not Steam-only. The canonical provider-neutral distribution/save-root contract is [STELLARIS_DISTRIBUTION_AND_SAVE_ROOTS.md](STELLARIS_DISTRIBUTION_AND_SAVE_ROOTS.md).
* Save-root discovery must treat store/launcher identity as diagnostic context only. The active save root is discovered from filesystem evidence and live file changes, not from assuming Steam, GOG, Microsoft Store/Xbox app, or Paradox Launcher.

* Stellaris may have more than one plausible Windows save root, for example local `Documents/Paradox Interactive/Stellaris/save games`, synced `OneDrive/Dokumenty/Paradox Interactive/Stellaris/save games`, and Steam Cloud local cache paths such as `Steam/userdata/<steam_user_id>/281990/remote/save games`.
* Steam itself may be installed on any drive or folder. Save-root discovery should prefer the Windows registry Steam install path (`HKCU/HKLM Software/Valve/Steam`, including `SteamPath`, `InstallPath`, and `SteamExe`) before falling back to common `ProgramFiles*` locations.
* The in-game/cloud-save checkbox can change where the active campaign writes saves. When cloud saving is enabled, the active files may appear in Steam Cloud's local synchronization cache instead of the ordinary local `save games` directory.
* The Steam Cloud path is still a local filesystem cache that SNC can monitor read-only; it must not be treated as durable archive history because Steam/Stellaris can still rotate or synchronize it.
* For GOG, Microsoft Store/Xbox app, and Paradox Launcher/direct installs, v0 must still monitor the provider-neutral Documents roots unless real filesystem evidence proves an additional provider-specific root.
* Campaigns are stored as subdirectories below `save games`, commonly with a readable slug plus an id suffix.
* A campaign folder is the storage boundary for that campaign's saves. SNC must not assume it can know the active campaign folder before observing file changes.
* Campaign folder names appear stable during normal use. Only one campaign is actively played at a time, but the user can switch campaigns or start a new campaign within the same `stellaris.exe` process/play window.
* Observed save file names include date-named saves (`YYYY.MM.DD.sav`), `ironman.sav`, and autosaves named `autosave_YYYY.MM.DD.sav`.
* Observed settings include `autosave_tocloud=yes`, which corresponds to cloud saving being enabled. A current observed autosave setting is `autosave=4`; an observed autosave campaign retained five recent autosaves.
* `continue_game.json` stores a relative save stem such as `save games/<campaign>/<save-name-without-extension>`, but it is only a hint. It can be stale or point to a save that no longer exists.
* Autosaves are a retention window, not a full history. With a monthly cadence and only several retained autosaves, the player can roll back only a few months unless they made manual saves.
* Stellaris prunes or overwrites older autosaves during the active session to avoid save-folder growth, so missing older autosaves are normal and should not be interpreted as data corruption.
* A long play session may create hundreds of autosave states while only a fixed small number remain visible on disk. SNC must preserve the full observed timeline in its own archive if those states matter for later campaign analysis.

Implementation consequence:

* When `stellaris.exe` is running, SNC must monitor all discovered save roots, not only one manually chosen campaign folder.
* Each monitor pass must rescan campaign subdirectories below `save games`; new campaigns created by the game or copied in by the user during a session must be picked up without restarting SNC.
* SNC is expected to be a long-running tray app. Its app lifetime is not the same as a capture session; autosave capture sessions are tied to observed `stellaris.exe` play activity because Stellaris is what creates autosaves.
* A capture session is not necessarily a single campaign. It may contain sequential segments from different campaign folders within one `stellaris.exe` play window, so offline analysis must partition archived saves by source campaign folder before assigning or using a `campaign_id`.
* While `stellaris.exe` is running, SNC should only capture stable saves and update status. It must not analyze, promote, or publish new generated mod rules into the active overlay during active play.
* After `stellaris.exe` exits, SNC can close the capture window, verify the archive, analyze archived autosaves per source campaign folder, and generate/stage new mod rules for the next launch.
* Campaign identity and downstream decision/DSL inputs are owned by SNC after archive verification. Manual identifiers or DSL files are development harness controls, not ordinary owner workflow.
* The native tray slice is built with `tools/build_snc_tray.ps1` and started with `tools/run_snc_tray.ps1`. It writes its tray heartbeat to `dist/private_reports/snc_tray_status.json`, mirrors live capture status to `dist/private_reports/snc_live_autosave_monitor_status.json`, and archives captured saves under `dist/snc_live_autosave_archive`.
* The live monitor must treat autosaves as a bounded rotating retention window and copy stable changed saves immediately into the SNC archive before Stellaris can prune or overwrite them.
* Realtime capture is the acceptance criterion: if a session produces hundreds of autosave revisions, the SNC archive should retain every observed stable revision, not just the last files remaining after the game exits.
* The primary live patterns are `autosave*.sav` and `ironman.sav`; date-named `.sav` files may still be archived by one-shot/manual session tools.
* Discovery should prefer real filesystem evidence over `continue_game.json`; `continue_game.json` can help choose an active campaign only after the referenced file exists.

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

Current narrow harness:

```text
Strategic Nexus.exe --parse-stellaris-save <save.sav-or-extracted-save-dir> <summary_output.json>
```

The harness accepts either a Stellaris `.sav` ZIP container or an already extracted directory containing `meta` and `gamestate`.
It emits a bounded headline summary with parse status, save version/date/name, player country id, player empire identity, founder species, capital planet, home system, owned fleet headlines, active war count, explicit missing fields, and uncertainties.
This is a first parser slice, not a full Paradox syntax model.

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
