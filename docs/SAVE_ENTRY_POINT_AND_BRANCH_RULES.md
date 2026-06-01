# Save Entry Point And Branch Rules

## Purpose

Strategic Nexus generated rules are valid for a specific campaign and for a specific known game-state entry point.

The user can load an older manual save, autosave, or ironman save and continue from there.
If the active generated overlay only reflects the latest observed end-of-session state, it may be wrong for an older load point.

SNC must therefore separate two different concepts:

* archived autosaves as historical evidence
* currently loadable saves as possible next-session entry points

## Entry Point Rule

After `stellaris.exe` exits, SNC must discover every currently loadable save for each known local campaign.

Potential entry points include:

* manual date-named saves
* `autosave*.sav` files still present on disk
* `ironman.sav`
* provider-specific local cache saves when they are actual loadable files

For each entry point, SNC should prepare or select generated rules that are safe for that entry point's campaign identity, empire identity, save fingerprint, and save date/state.

The active generated overlay may contain multiple rulesets for the same campaign if they correspond to different entry points or branch states.

Those rulesets must be gated so that loading the wrong save does not activate rules for a different state.

## Archive History Rule

Live autosave capture exists to preserve the campaign timeline while Stellaris rotates or deletes autosaves.

Captured autosaves help the LLM understand what happened during play and why the current state exists.
They are evidence for analysis, not automatically future entry points.

If the player plays for a while and exits without leaving a save that reaches the final observed state, the captured later autosaves may still be useful historical evidence, but they must not be treated as a loadable next-session target unless that save is still available through an active save root or a deliberate Strategic Nexus restore/export feature.

The active save folders decide what the player can load next.
The Strategic Nexus archive decides what the system can remember and analyze.

## Branch And Reload Rule

A single `stellaris.exe` run can contain branches.

Example:

```text
player loads campaign A at 2269.01
-> plays to 2270.06
-> notices a mistake
-> loads older 2269.03 save
-> plays a different line to 2270.02
-> exits
```

The archive may contain autosaves from both branches.
Some branch autosaves may no longer exist in the active game save folder because Stellaris rotated them away.

SNC must not assume a campaign's captured autosaves form one simple linear timeline.

Branch indicators include:

* in-game save dates moving backwards within one campaign during one capture session
* two saves for the same campaign/date with different content hashes
* active save roots retaining only one branch while the archive contains another
* manual saves or autosaves whose modification order does not match their in-game date order

When branch ancestry cannot be proven, SNC should fail conservatively:

* build rules for each loadable entry point from that entry point's parsed facts
* use only compatible historical evidence before or at that entry point when confidence is high
* downgrade or omit evidence from later, abandoned, or ambiguous branch saves
* emit explicit uncertainty instead of blending incompatible branches

## Generated Overlay Consequence

Generated overlay rules must be entry-point scoped.

Required identity anchors should include, as implementation matures:

* campaign identity
* empire identity
* save fingerprint/hash
* in-game date
* source campaign folder key
* Strategic Nexus marker values available inside the save
* branch or timeline segment id when derivable

The mod should activate only the ruleset whose marker and entry-point checks match the loaded save.

If the loaded save cannot be matched confidently, the generated overlay must stay inert for that ruleset and the base mod fallback should run.

## Post-Play SNC Pipeline

Correct production shape:

```text
Stellaris exits
-> verify capture archive
-> scan all current save roots
-> identify loadable campaign entry points
-> partition archived autosaves by campaign and branch/segment where possible
-> for each loadable entry point:
   -> choose compatible historical evidence
   -> parse the entry-point save
   -> build bounded entry-point brief
   -> ask/derive validated DSL candidate
   -> compile marker-gated generated rules
-> publish complete generated overlay snapshot while Stellaris is closed
```

Incorrect shape:

```text
latest captured autosave
-> one campaign rule
-> apply to every future load
```

That would be wrong after rollback, manual save loading, cloud/local root switching, or same-session branch replay.

## Implementation Requirements

SNC should record enough manifest metadata to support later partitioning without exposing unnecessary local private paths.

Useful safe metadata:

* `source_root_kind`
* `source_campaign_key`
* `source_save_name`
* `captured_at_local`
* `observed_sequence`
* `save_game_date` after parse
* `content_hash`

Avoid storing full local usernames or absolute paths in durable shared artifacts unless the artifact is explicitly local/private diagnostic output.

Current v0 gap:

The live archive can already preserve multiple campaign folders and repeated autosave revisions, but post-play analysis does not yet build entry-point-scoped rule bundles or explicit branch timelines.
This is now a required architecture slice before generated rules can be considered safe for arbitrary user load choices.
