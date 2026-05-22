# on_game_start_country SP Test

## Purpose

Verify whether Stellaris `on_game_start_country` fires for the Strategic Nexus PoC mod in a simple singleplayer game.

This isolates the on_action hook from multiplayer, observer, hotjoin, and synchronization behavior.

Note: `on_game_start_country` is a new-game hook. It should not be expected to fire when loading an existing save.

## Test Asset

The PoC mod registers:

```text
on_game_start_country
    -> strategic_nexus.1201
```

Event `strategic_nexus.1201` is singleplayer-only and visible.

Current probe trigger is intentionally minimal:

```text
is_ai = no
NOT = { has_country_flag = strategic_nexus_poc_sp_on_game_start_seen }
```

Earlier stricter trigger conditions were removed after a first SP run showed no `1201` event/log output. The goal is to isolate the on_action itself before adding scope filters back.

Expected visible event:

```text
Strategic Nexus PoC: SP Start Probe
```

Expected `game.log` line:

```text
Strategic Nexus: SP on_game_start_country probe fired
```

## Manual Steps

1. Update/install the latest `strategic_nexus_poc` mod.
2. Start Stellaris in a normal non-debug setup.
3. Enable the Strategic Nexus PoC mod in the launcher.
4. Start a fresh non-Ironman singleplayer game as a normal empire.
5. Watch for the visible event `Strategic Nexus PoC: SP Start Probe`.
6. Let the game run briefly if the event does not appear immediately.
7. Close Stellaris.
8. Check `logs/game.log` for:

   ```text
   Strategic Nexus: SP on_game_start_country probe fired
   ```

9. Check `logs/error.log` for Strategic Nexus errors.

## Pass Criteria

- visible event `strategic_nexus.1201` appears, or
- `game.log` contains the SP probe line

## Fail Criteria

- no visible event
- no SP probe log line
- Strategic Nexus on_action/event errors appear in `error.log`

## Interpretation

If this passes, `on_game_start_country` works in SP and the MP failure is probably caused by multiplayer/observer/hotjoin timing or scope.

If this fails, `on_game_start_country` is not a reliable harness for this PoC and the project should switch to another deterministic scripted trigger path.

## Load-Safe Monthly Probe

Because save loading is important for drop-in/drop-out compatibility, the PoC also registers:

```text
on_monthly_pulse_country
    -> strategic_nexus.1202
```

Event `strategic_nexus.1202` is visible and should fire once for a human country on the first monthly pulse after starting or loading a game.

Expected visible event:

```text
Strategic Nexus PoC: Monthly Probe
```

Expected `game.log` line:

```text
Strategic Nexus: monthly pulse country probe fired
```

This probe is more useful for existing saves than `on_game_start_country`, because it can initialize lazily after load.

## Verified Result

Status: passed.

On 2026-05-20, the monthly probe was tested after loading a save and letting the game advance to the next monthly pulse.

Observed visible result:

```text
Strategic Nexus PoC: Monthly Probe
```

Observed `game.log` lines:

```text
[12:15:08][effect_impl.cpp:22189]: [2205.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 78. Strategic Nexus: monthly pulse country probe fired
[12:15:12][eventcommands.cpp:88]: Event strategic_nexus.1202 added info about event selection. selectedOption 0, human 1, playerEventId 37
```

Observed `error.log` result:

```text
No Strategic Nexus errors.
```

Conclusion:

`on_monthly_pulse_country` is currently the best verified load-safe scripted trigger for Strategic Nexus v0.x lazy initialization.

## INI Note

No usable Stellaris script/mod path has been identified where a normal mod can read arbitrary `.ini` files at runtime through supported script alone.

Configuration-style files may be read by the game or launcher at startup, but they do not provide a proven daemon-to-runtime channel for small strategic values. For the current PoC, prefer scripted on_actions and bounded flags/variables.
