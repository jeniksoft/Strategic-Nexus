# Strategic Nexus - Multiplayer Season Orchestrator

## Purpose

This document defines a low-friction multiplayer season architecture for Strategic Nexus.

Goal:

```text
the host prepares the Strategic Nexus state
players join through normal Stellaris/Steam multiplayer
checksum compatibility proves they have the same gameplay-affecting mod content
the session can run with minimal manual coordination
```

This design extends `MULTIPLAYER_REQUIREMENTS.md` and `CAMPAIGN_ORCHESTRATOR_ARCHITECTURE.md`.

---

# Core Rule

Multiplayer Strategic Nexus is host-coordinated.

The host owns:

* archived autosaves
* offline season analysis
* generated campaign overlay package
* package manifest
* multiplayer season handoff package
* version/checksum readiness checks

Clients do not:

* run LLM analysis for the MP campaign
* generate their own gameplay-affecting overlay
* receive per-client divergent generated gameplay files
* need to be known by Strategic Nexus before the season starts

The good news for this architecture is that Stellaris multiplayer already requires compatible game/mod state.

If a player can join the modded session successfully, they necessarily passed the normal game/version/checksum compatibility gate.

Strategic Nexus should use that gate instead of inventing a hidden network protocol.

---

# SNC Friend Mesh And Automatic MP Preparation

Release multiplayer preparation must become automatic after the first explicit user-approved pairing.

Manual ZIP sharing, copyable commands, and open-directory buttons are useful development and fallback surfaces, but they are not the target product experience.

Target owner-facing flow:

```text
Player A opens SNC
-> SNC creates a friend request package/code
-> Player A sends it to Player B through any ordinary channel
-> Player B imports it in SNC and confirms the displayed identity/fingerprint
-> SNC creates/returns an acceptance package/code
-> both SNC installs store each other as trusted friends
-> future MP packages and handoff updates synchronize automatically when allowed
```

A user may have any number of Strategic Nexus friends.

Friendship is local companion-app trust metadata. It is not a Steam identity, legal identity, proof of real-world identity, or Stellaris lobby authority.

It only authorizes encrypted out-of-game Strategic Nexus package exchange.

## Pairing Identity

Each SNC installation should create a local node identity:

```json
{
  "node_id": "stable-random-install-id",
  "display_name": "owner-chosen-name",
  "signing_public_key": "ed25519-or-equivalent",
  "encryption_public_key": "x25519-or-equivalent",
  "capabilities": ["mp_package_sync", "handoff_sync"],
  "created_at": "...",
  "expires_at": "optional-invite-expiry"
}
```

The private keys stay local.

The friend request should be portable as:

* a small `.snc-friend-request` file
* copyable text
* QR code when useful

The confirmation UI should show a short human-verifiable fingerprint.

The recipient must explicitly accept the request before the contact becomes trusted.

The originating user must receive/import the response or the transport layer must deliver it automatically before the trust relationship is complete.

SNC should support:

* rename friend locally
* disable auto-sync per friend
* revoke/block friend
* rotate local identity keys with clear warnings
* export/import friend trust backup only with explicit user action

Revocation is not retroactive. A revoked friend may still possess old packages already sent before revocation.

## Transport Adapters

The friendship layer should use pluggable transport adapters.

Preferred release order:

1. Manual import/export fallback for friend requests, acceptance packages, and MP handoff packages.
2. User-selected shared-folder/cloud-folder adapter, such as a folder synchronized by an external provider.
3. Optional HTTPS relay mailbox owned by the project or user, storing only encrypted blobs.
4. Optional LAN/direct adapter, only after explicit user consent and without requiring public IP sharing.

The architecture must not require IP address sharing.

The architecture must not require a custom real-time network path into the active Stellaris session.

If a relay exists, it is only a mailbox for encrypted package blobs and delivery metadata. The relay must not be trusted with raw saves, generated package contents in plaintext, model outputs, or friend private keys.

All synced payloads should be:

* signed by the sending SNC node
* encrypted for the recipient node or MP season group
* schema-versioned
* content-addressed by SHA-256 or stronger hash
* replay-protected with nonce/timestamp/season id
* bounded by size limits
* safe to ignore if unknown, stale, mismatched, or malformed

## Automatic MP Package Sync

After friends are paired, the host/coordinator should be able to create an MP season group from accepted friends.

The host SNC then prepares a canonical MP package for the campaign and publishes a signed package announcement:

```json
{
  "message_type": "mp_package_announcement",
  "season_id": "mp-season-0007",
  "campaign_marker": "sn-marker-...",
  "game_version": "...",
  "mod_version": "...",
  "generated_overlay_manifest_hash": "...",
  "package_zip_sha256": "...",
  "package_bytes": 123456,
  "host_node_id": "...",
  "created_at": "..."
}
```

Friend SNC instances should automatically:

1. receive the announcement through the configured transport
2. download/import the encrypted package
3. verify sender trust, campaign identity, schema version, package hash, manifest hash, file list, byte counts, game version, mod version, and load-order assumptions
4. stage the package locally for next launch
5. show a clear Status Center state such as `MP package ready`, `MP package waiting for approval`, or `MP package mismatch`

Automatic sync may download and verify packages without another manual step once the friend/season trust policy allows it.

Publishing gameplay-affecting files into the active generated overlay remains fail-closed:

* never publish while `stellaris.exe` is running
* never publish when campaign identity or package identity mismatches
* never publish malformed or unverified packages
* never run recipient-side LLM generation to "fix" a host package
* default to staging plus clear approval unless the user explicitly enables a trusted auto-apply policy for that friend/season

The host remains package authority for the MP season.

Clients may have local models installed, but they must not generate divergent gameplay-affecting rules for that MP campaign.

## Host Rotation And Automatic Handoff Sync

The same friend mesh should carry host-rotation handoff packages.

After an MP session ends, the current host SNC should automatically prepare and sync:

* generated overlay package
* package manifest
* previous-host continuity metadata
* latest-session delta ledger
* bounded campaign memory snapshot
* empire memory/personality snapshot
* analysis audit summary

The next host SNC should import and verify the latest trusted handoff before hosting.

If no trusted handoff arrives, Status Center should explicitly show degraded continuity and the recovery options.

This turns host rotation from a manual "find and send the right ZIP" task into an automatic verified sync state.

## Privacy And Safety Boundaries

The friend mesh must obey the same multiplayer safety boundaries as the rest of Strategic Nexus:

* no save editing
* no modification of the running game program
* no custom game-state network channel during active Stellaris play
* no per-client gameplay LLM decisions
* no plaintext raw saves sent by default
* no silent upload of personal paths, account names, IP addresses, or player identities
* no package trust solely because a sender is a friend; manifest verification is still mandatory

Raw saves or detailed campaign archives may be shared only through explicit user action.

The normal Stellaris multiplayer lobby and checksum gate remain authoritative for joining a game.

Strategic Nexus only prepares, verifies, and synchronizes the files needed before that normal join flow.

---

# Stellaris Campaign Multiplayer Eligibility

Stellaris multiplayer eligibility is a campaign property, not just a current player-count property.

Observed owner rule:

* a campaign created as single-player cannot later be played as multiplayer
* a campaign created as multiplayer can be played by one player
* a running multiplayer-created campaign is exposed through the normal Stellaris lobby/discovery path while hosted
* the host remains the authority for approving or rejecting joining players
* Stellaris can support up to 32 players in one multiplayer campaign
* when a multiplayer campaign is loaded, joining players may take control of available empires according to normal Stellaris host/lobby rules
* the full empire roster in a campaign can be larger than the current human player count and larger than the 32-player lobby limit
* some empires present at campaign start are not human-playable choices even in multiplayer
* additional empires may appear or disappear during campaign history
* a destroyed empire or a special non-playable empire must not be treated as player-joinable merely because it exists in save history or campaign memory

Architecture consequences:

* Strategic Nexus must distinguish `singleplayer-founded` from `multiplayer-founded` campaign capability when preparing MP status, packages, or handoff guidance.
* Current player count is not enough. A multiplayer-founded campaign with one current player is still MP-capable.
* Strategic Nexus must distinguish `all detected campaign empires`, `currently/player-selectable empires when known`, and `currently human-controlled empires`.
* The 32-player ceiling is a lobby/human-slot limit, not a hard upper bound on total empire count inside a campaign.
* A singleplayer-founded campaign must not be presented as MP-ready merely because the generated overlay package verifies.
* If SNC cannot prove whether the campaign was multiplayer-founded from save metadata, markers, durable memory, or prior owner confirmation, MP package/export status must be `needs_confirmation` or equivalent, not silently ready.
* If SNC cannot prove that a detected empire is currently a valid human-selectable choice, release guidance should treat it as `unknown` or `not_assumed_playable`, not silently present it as available.
* Release UX should default to hiding destroyed, fallen, and special non-playable empires from selectable guidance unless there is explicit proof that the game currently allows a human to join them.
* If the UI still needs to mention such empires for analysis context, it should label them as historical or non-assumed-playable rather than presenting them as normal join targets.
* Empire memory still belongs to campaign empires, not human users. A later player may choose a different available empire, so player identity is session metadata.

---

# Host Rotation And Memory Handoff

The active MP host may change between seasons.

This is a critical architecture case.

The previous host may be completely absent from the next season.

Strategic Nexus must not assume the previous host will be present as host, client, or available support contact.

The previous host has the best autosave archive because clients do not generate the normal multiplayer autosave sequence.

A client player may manually create a save during the session or may later become the next host, but that client will not have the same autosave sequence that the previous host archived.

Therefore, Strategic Nexus MP continuity must not depend only on the next host's local autosave archive.

After each MP season, the current host orchestrator should produce a portable handoff package:

```text
StrategicNexus_MP_Handoff_<campaign_marker>_<season_id>.zip
```

The handoff package should contain:

* generated overlay package
* generated overlay manifest
* campaign marker and campaign identity confidence
* last verified save fingerprint
* latest-session delta ledger
* durable Strategic Nexus campaign memory snapshot
* empire memory/personality snapshot
* analysis audit summary
* package version and required mod/game version

The next host should import the latest handoff package before the next planned MP season.

If this succeeds:

```text
previous host season learning
-> portable handoff package
-> next host imports memory and overlay state
-> next host stages byte-identical generated MP overlay
-> players join normally
```

This keeps the AI best learned even when the original host is not the next host.

The orchestrator must also record who produced the previous season handoff.

This is not a Stellaris script responsibility.

Recommended external handoff fields:

```json
{
  "previous_host_node_id": "local-install-or-user-approved-host-id",
  "previous_host_display_name": "optional-user-visible-name",
  "season_id": "mp-season-0007",
  "campaign_marker": "sn-marker-...",
  "last_verified_save_fingerprint": "...",
  "generated_overlay_manifest_hash": "..."
}
```

This identifies the machine/app instance that exported the handoff package.

It must not be treated as proof of a Steam identity, legal identity, or game-authoritative host identity.

If the next host imports a handoff from a different previous host, this is expected and should be shown as normal continuity:

```text
Previous host package imported.
Learning state is current up to season <season_id>.
```

If no previous host handoff is available, the new host can still continue with older memory or client-save recovery, but learning confidence must drop.

The project should treat "previous host unavailable" as a normal degraded continuity path, not an exceptional manual emergency.

---

# Mod-Persisted MP Markers

Some MP identity facts must survive inside the Stellaris save so that the orchestrator can read them from archived saves later.

Stellaris script can persist simple state through normal game mechanics:

* flags can be attached to scopes such as country or global scope
* variables can be set on scopes such as country or global scope
* `is_ai = no` can identify human-controlled countries at the moment a scripted event runs

Strategic Nexus should use this for small bounded markers only.

The base mod should write:

* campaign marker
* Strategic Nexus save initialized flag
* per-country Strategic Nexus empire marker
* per-country "was human controlled during this season" marker or counter
* per-country current human-control guard where a safe scripted check is available
* optional season counter / last observed generated overlay version

These markers should be written through ordinary scripted events/on_actions and then preserved by the normal save.

They must not contain:

* Steam IDs
* IP addresses
* personal identifiers
* arbitrary player names as logic
* large histories
* generated strategy text

The app reads these markers from archived saves to improve continuity.

The markers do not replace external handoff packages.

They are the save-side anchor that tells the orchestrator:

```text
this save belongs to Strategic Nexus campaign marker X
country Y existed and had Strategic Nexus marker Z
country Y was human-controlled at least during observed season N
generated overlay version V was loaded
```

The runtime rule is stricter than the memory rule:

```text
generated rules may exist for every empire in an MP campaign
-> if that empire is currently human-controlled, gameplay-affecting Strategic Nexus rules for that empire must not activate
-> if that empire is AI-controlled, its validated campaign/empire rules may activate through normal marker guards
```

This lets the package remain complete for a campaign where players can choose different empires, while avoiding Strategic Nexus gameplay nudges overriding a human player's empire.

---

# Player Count And Empire Control Changes

MP player count may change between seasons.

An MP-capable campaign may also have only one active human player in a given session.

The set of existing campaign empires may be larger than the set of human-playable empires.

The set of human-playable empires may also be larger or smaller than the current player count.

The same human user may play a different empire.

Different users may play the same empire across different sessions.

Therefore Strategic Nexus must not bind empire personality to a human player.

Correct identity layers:

```text
campaign identity
-> empire identity
-> empire memory/personality
-> observed human-control state per season
-> optional external host/app instance identity for handoff
```

Empire personality belongs to the campaign empire.

Player participation is session metadata.

For a newly discovered or zero-history MP-capable campaign, the host may generate bootstrap personalities and conservative rules for every detected empire.
Those bootstrap defaults must be deterministic, package-manifest visible, and byte-identical for all clients.
Clients must not generate their own local bootstrap personalities or policy-pack variations.

Bootstrap variation may make a new campaign feel different from the previous one, but it must not profile the human players.
It belongs to campaign empires only and remains subject to the human-control activation guard.

MP generated overlays should be prepared for all known campaign empires, because any AI empire may later be selected by a joining player or become AI-controlled again.
Activation must be conditional on current human-control state.

This does not mean every detected empire is a valid player-assignment target.
Destroyed empires, fallen empires, or other special non-playable empires may still exist in campaign memory/history and may still matter for analysis, but release-companion joinability guidance must not advertise them as normal player-selectable choices.

If a human temporarily controls an empire, the orchestrator may record that fact for analysis context, but it should not erase or replace the empire's personality.

If control changes:

```text
same empire marker
-> continue same empire memory
-> add session note that control changed or human-control status changed
```

If an empire disappears, splits, rebels, is integrated, or changes name:

```text
use empire identity resolver
-> preserve memory only when confidence is high
-> otherwise create/branch profiles conservatively
```

For single-player-founded campaigns, the owner player empire is expected to remain fixed after campaign creation.
Strategic Nexus can treat the player empire as a stable do-not-apply target unless the save parser or markers prove a different safe rule.

---

# Client Manual Save Recovery

A client-side manual save can be useful, but it should be treated as a recovery anchor.

Clients do not generate the normal MP autosave timeline.

Therefore, client manual saves cannot replace the previous host's autosave archive for learning quality.

Client save use cases:

* original host is unavailable
* no handoff package was exported
* a new host must continue from a save they possess
* the client manual save is newer than the last known handoff package

Strategic Nexus may import a client-provided MP save as:

```text
client manual save snapshot
-> campaign marker/fingerprint check
-> recovery season anchor
-> lower-confidence memory update or rebuild request
```

But a single client manual save is not equivalent to the host's full autosave archive.

It may not contain enough intermediate history to reconstruct the same season delta.

If only a client manual save is available, the orchestrator should:

* preserve existing durable memory if available
* extract what can be safely verified from the save
* mark the season delta as incomplete
* lower confidence for inferred personality/memory changes
* prefer conservative generated rules
* show Status Center warning that learning quality is reduced

This is still better than losing continuity completely.

---

# Pre-Season Sync

Before a planned MP season, the intended host should run a pre-season sync:

```text
import latest handoff package if available
verify local save/marker matches handoff
verify generated overlay package
stage host local overlay while Stellaris is closed
produce copyable invite/package status text
```

The current SNC companion and tray status surfaces also expose `friend_mp_sync_transport_state`, `friend_mp_sync_transport_reason`, and `friend_mp_sync_transport_next_step` so the signed/encrypted transport gap stays explicit while manual export/import remains the active fallback.

If the intended host lacks the latest handoff:

```text
Status Center warning:
Latest host learning package missing.
You can still host, but Strategic Nexus may use older memory or conservative fallback.
Ask previous host for the latest handoff package if available.
```

This is the main MP workflow for making the mod "best learned" before the next agreed season without requiring all clients to run analysis.

---

# Target No-Maintenance Flow

```text
host finishes a play session
-> host orchestrator archives autosaves
-> host orchestrator analyzes latest session
-> host orchestrator builds generated MP overlay package
-> host orchestrator builds MP handoff package
-> host orchestrator verifies manifest and local install
-> host shares package/invite info if needed
-> next host imports handoff package if host changes
-> players install or update identical generated overlay package
-> players join through normal Stellaris/Steam lobby/friends flow
-> checksum mismatch blocks or warns through normal game behavior
-> session runs with already-loaded overlay
```

The player experience should be:

```text
install/update package or import host handoff if becoming host
join host through ordinary multiplayer
play
```

No IP address workflow should be required by Strategic Nexus.

Steam friends/lobby invites should be the preferred human communication path when available.

Strategic Nexus may provide copyable status text, package names, and links, but it should not replace the game's multiplayer lobby system.

---

# Hot Join And Unknown Players

Players may join a live or continuing MP season even if the host did not know them in advance.

Strategic Nexus should not require a predeclared participant list.

Safe model:

* host prepares one canonical generated overlay package for the campaign
* host exports a handoff package after the session
* every participant uses that same package
* the game checksum/load-order gate decides compatibility
* the host may approve or reject joiners through normal Stellaris multiplayer authority
* joining players may appear without being known to Strategic Nexus in advance, because the host/lobby flow decides whether they can enter and which empire they may control

Strategic Nexus does not need to identify every human player before launch.

It only needs to ensure that:

* the generated gameplay-affecting files are byte-identical
* the mod version is compatible
* the generated overlay manifest matches the host package
* no client-specific generated gameplay logic exists

If a joining player lacks the package or has a different generated overlay, the normal Stellaris checksum/load-order gate should prevent a valid shared modded session.

Strategic Nexus should still provide pre-join warnings and easy package sharing where detectable, but it must not pretend it can hot-patch a player after join.

CLI support for explicit pre-join mismatch warnings:

```text
Strategic Nexus.exe --verify-mp-overlay-package <package_dir> [expected_campaign_id] [expected_overlay_version] [expected_game_version] [expected_mod_version] [expected_manifest_hash]
```

If optional expected values are supplied, the verifier keeps normal structural/hash validation and also emits explicit mismatch warnings when package identity or manifest hash differs from the expected host/session values.

---

# Generated MP Package

The host orchestrator should produce a package containing:

* Strategic Nexus base mod version requirement
* generated overlay files
* generated overlay manifest
* campaign package id
* package version
* Stellaris version expectation
* DLC/mod compatibility notes if known
* checksum-relevant file hashes
* installation instructions for the companion app
* pointer to the related MP handoff package when present

Package identity should be stable and human-readable.

Example:

```text
StrategicNexus_MP_<campaign_marker>_<overlay_version>.zip
```

The manifest should clearly separate:

* gameplay-affecting files that must be byte-identical
* diagnostics that are useful but not authoritative
* local-only Status Center state that must not affect checksum

The generated overlay package is for all players.

The handoff package is mainly for the next host/coordinator.

Clients may keep it as backup, but they should not generate independent gameplay-affecting overlays from it during the active session.

---

# Communication And Sharing

Strategic Nexus should not require users to exchange IP addresses.

Preferred communication options:

1. Steam friends / Steam lobby invite
2. normal Stellaris multiplayer lobby discovery
3. copyable host message from Status Center
4. manual package file sharing as fallback

The release Status Center may provide:

* a "Copy invite/status text" action
* package filename and version
* handoff package filename and version when relevant
* host package hash
* short compatibility checklist
* "ready for MP" / "package mismatch" status

Example copyable text:

```text
Strategic Nexus MP package:
Campaign: <campaign_name_or_marker>
Package: StrategicNexus_MP_<id>_<version>.zip
Handoff: StrategicNexus_MP_Handoff_<id>_<season>.zip
Stellaris: <version>
Strategic Nexus: <mod_version>
Generated overlay: <manifest_hash>
Join: use Steam invite / Stellaris lobby from host
```

This is communication support, not a custom network layer.

---

# Host Approval Model

The host remains the authority for who may join.

Strategic Nexus may show the host:

* local package readiness
* expected package id
* last analysis time
* whether the active overlay is MP-ready
* whether generated files match the manifest

If future APIs or local signals can detect connected players safely, the Status Center may show them.

But the base architecture does not require it.

The host can approve players through normal Stellaris multiplayer UX.

---

# Automation Policy

The host orchestrator may automatically:

* build the latest generated MP package after session analysis
* build the latest MP handoff package after session analysis
* import a previous handoff package before hosting
* validate package files against the manifest
* stage the host local overlay while Stellaris is closed
* mark the Status Center as MP-ready
* produce copyable invite/status text

It should ask or warn when:

* package generation fails
* manifest verification fails
* Stellaris is running
* local generated files do not match the package
* mod version or game version looks stale
* a player-visible package needs manual sharing
* the intended host is missing the latest handoff package
* only a client save is available and learning quality is reduced
* low-confidence campaign identity would affect generated rules

No routine pre-session approval should be required when all checks pass.

---

# Real MP Season Validation

When the owner runs the first real multiplayer season test, the useful check is not "does the UI exist?" but "does the handoff guidance stay readable and point at the right artifacts?"

Owner-facing minimum test:

* open `dist/private_reports/snc_next_steps_brief.txt`
* confirm the MP package lines show the current package directory, ZIP path, and strict verify/import commands
* run one ordinary MP handoff or join flow using the current package guidance
* expect `friend_mp_sync_transport_state` to stay disabled for now; manual MP export/import is still the active fallback
* keep any observed warning text or blocked state unchanged until Codex inspects the resulting evidence

What Codex will inspect after the run:

* `dist/real_session_v0_loop/<session_id>/real_session_v0_loop_evidence.json`
* `dist/real_session_v0_loop/<session_id>/real_session_v0_next_steps.md`
* `dist/private_reports/snc_next_steps_brief.txt`
* current SNC status or tray snapshot if the run was degraded, blocked, or missing the previous host

Known limitation:

* this validates the release-companion and handoff contract around the real season test, not the underlying gameplay effect of the campaign

---

# Live Session Boundary

During a live multiplayer session, Strategic Nexus must not:

* rewrite gameplay-affecting mod files
* push new generated rules into the running game
* ask clients to run LLM analysis
* create per-client divergent state
* rely on debug console transport

Already-loaded overlay behavior may run through ordinary scripted mod logic.

Next strategic updates wait for the next season/session package.

---

# Drop-In / Drop-Out Rule

Because Strategic Nexus generated gameplay files must be identical for all MP participants, drop-in/drop-out support is mostly a package readiness problem.

Drop-in player:

```text
has identical package and load order
-> can join normally if host/game accepts

missing or different package
-> normal checksum/load-order mismatch blocks shared play, or pre-join warning appears when detectable
-> install/update package
-> retry join
```

Drop-out player:

```text
leaves session
-> no Strategic Nexus state change required
```

The host archive and analysis remain authoritative for the campaign.

If the host changes between seasons, the latest imported handoff package becomes the continuity authority for the new host.

---

# Non-Goals

Do not build first:

* custom peer-to-peer networking
* client-side LLM workers
* per-client generated overlays
* IP address exchange system
* hidden Steam automation
* runtime generated rule streaming
* live patching after a player joins

If official APIs later make package sharing easier, they can be added as optional convenience, not as the core MP safety model.

---

# Design Goal

Strategic Nexus MP should feel like:

```text
the host shares the right mod package, players join normally, checksum keeps everyone honest
```

not:

```text
a second multiplayer system bolted next to Stellaris
```
