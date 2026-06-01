# Stellaris Distribution And Save Roots

## Purpose

Strategic Nexus must not assume that Stellaris is a Steam-only game.

SNC save capture should be provider-neutral first and provider-specific only where a store adds a distinct active save/cache root.

Verified on 2026-06-01 from official Paradox/store sources:

* Paradox's Stellaris page lists PC availability through Steam, Microsoft, and GOG, plus Xbox and PlayStation console editions:
  `https://www.paradoxinteractive.com/games/stellaris/about`
* Steam lists the Windows/Mac/Linux PC release and Steam-specific features such as Steam Cloud and Steam Workshop:
  `https://store.steampowered.com/app/281990/Stellaris/`
* GOG lists Stellaris as a GOG-distributed PC title:
  `https://www.gog.com/en/game/stellaris`
* Xbox/Microsoft lists Stellaris for PC through the Xbox/Microsoft Store surface:
  `https://www.xbox.com/en-US/games/store/stellaris/9NXQPGMR917R`
* Paradox Launcher documentation says the Paradox client can install and patch games bought from the Paradox Store for users who do not want Steam, and lists Stellaris as playable through that client:
  `https://support.paradoxplaza.com/hc/en-us/articles/115003166953-Paradox-Launcher-v1-aka-Paradox-Client-FAQ`
* The Paradox Launcher page says users who bought Stellaris in the Paradox Store can download the launcher and play:
  `https://www.paradoxinteractive.com/our-games/launcher`

## Provider Categories

Confirmed PC provider surfaces:

* Steam
* GOG / GOG Galaxy
* Microsoft Store / Xbox app / PC Game Pass
* Paradox Store / Paradox Launcher direct-download path

Confirmed non-PC/console provider surfaces:

* Xbox One / Xbox Series X|S / Xbox Cloud Gaming
* PlayStation 4 / PlayStation 5

Console editions are distribution facts, but they are outside the Windows SNC mod/save-capture path unless a future platform explicitly exposes compatible local save files and mod support.

Epic is not listed as a current Stellaris buy target on the checked Paradox Stellaris product page. Paradox support documentation names Epic as one possible source from which Paradox Launcher v2 can be run, so SNC should not hard-code assumptions that reject Epic-like launch providers. Treat Epic as a launcher/provider surface to tolerate, not as a confirmed current Stellaris retail listing from this verification pass.

Third-party key sellers can exist, but they usually map to one of the above runtime providers. They should not create new save-root logic unless they install/run a distinct build with distinct save storage.

## Save Root Rule

The active save root is a gameplay/runtime fact, not a store branding fact.

Provider-neutral roots:

* local user Documents:
  `Documents/Paradox Interactive/Stellaris/save games`
* synced/localized Documents variants such as OneDrive `Documents` and `Dokumenty`

Known provider-specific roots:

* Steam Cloud local cache:
  `Steam/userdata/<steam_user_id>/281990/remote/save games`

For Steam Cloud cache discovery, SNC should find Steam from the Windows registry first, because Steam can be installed on any drive or folder. Common `ProgramFiles*` probing is only fallback.

For GOG, Microsoft Store/Xbox app, and Paradox Launcher/direct-download installs, the currently supported v0 assumption is that SNC still captures the provider-neutral Documents save roots unless real filesystem evidence proves a provider-specific cache exists.

## SNC Implementation Contract

SNC must:

* detect `stellaris.exe` by process activity, not by Steam launcher state
* monitor all discovered save roots while Stellaris is running
* preserve provider-neutral Documents roots even when Steam roots exist
* add provider-specific roots only as additive candidates
* never require the owner to choose Steam, GOG, Microsoft, or Paradox Launcher before capture can start
* offer a future manual additional-save-root override for uncommon installs, portable installs, or changed platform behavior
* store provider/source metadata as diagnostic context only; campaign identity must come from save/campaign evidence, not from store identity

Store/provider detection must not become a safety boundary. The archive verifier, save parser, DSL validation, generated overlay verification, and multiplayer package verification remain the actual gates.

## Current V0 Implementation Mapping

Current v0 save-root discovery already satisfies the provider-neutral baseline by always considering local Documents and OneDrive Documents/Dokumenty roots, then adding Steam Cloud userdata roots when they can be discovered.

This is enough for Steam-disabled, cloud-disabled, GOG, Microsoft Store/Xbox app, and Paradox Launcher/direct installs as long as those builds write to the standard Paradox Documents save tree.

Remaining future work:

* add a user-visible additional-save-root override in SNC
* record provider/source hints in Status Center diagnostics without requiring the user to pick a store
* add provider-specific cache discovery only after a real install or official documentation proves a distinct active save/cache root
* keep regression tests proving that provider-neutral Documents roots are never dropped just because a Steam root exists
