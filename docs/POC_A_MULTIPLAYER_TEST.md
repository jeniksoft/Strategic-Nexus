# PoC A Multiplayer Test

## Purpose

Verify whether Strategic Nexus scripted event, flag, variable, and diagnostic modifier state can run in multiplayer without desync.

The preferred current test is an automatic scripted smoke test that runs from normal mod script when a multiplayer game starts.

This test does not prove final daemon/bridge automation. It only tests whether the fully verified singleplayer runtime state path can exist in multiplayer without special debug setup.

## Scope

The primary test uses development-only mod-side automation through `on_game_start_country`.

Do not implement or test:

- lower-level game-process modification
- custom networking
- LLM
- daemon
- save editing
- fleet control
- tactical commands

## Required Setup

1. Host and client install identical `strategic_nexus_poc` mod files.
2. Host and client use the same Stellaris version.
3. Host and client use matching DLC/playset configuration.
4. Host and client confirm the same checksum before starting the test.
5. Host and client use a normal non-debug setup.
6. Host and client do not need any manual debug workflow for the primary test.

## Automatic Scripted MP Smoke Test

1. Host and client install the identical updated `strategic_nexus_poc` mod.
2. Host and client both launch Stellaris in a normal non-debug setup.
3. Host starts a non-Ironman multiplayer test game.
4. Client joins.
5. Start or unpause the session.
6. The mod should automatically run `strategic_nexus.1200` for each human default country at game start.
7. Verify the host sees visible diagnostics:

   - `strategic_nexus.1002` main PoC event
   - `strategic_nexus.1100` global bridge flag
   - `strategic_nexus.1101` country bridge flag
   - `strategic_nexus.1102` doctrine flag
   - `strategic_nexus.1103` pressure variable
   - `strategic_nexus.1105` diagnostic modifier

8. Verify the client sees equivalent diagnostics for their country if possible.
9. Continue with either active gameplay validation or observer OOS soak validation.
10. Observe whether a desync/OOS occurs.
11. Check host logs.
12. Check client logs.
13. Record results in `docs/POC_A_MULTIPLAYER_RESULTS.md`.

Expected `game.log` line on each machine/country where the smoke test ran:

```text
Strategic Nexus: automatic MP smoke test started
```

Important: this automatic smoke test may run once for each human default country. That is acceptable for this test. It does not prove final host-only daemon delivery; it only proves a no-console, no-debug scripted MP path.

Use active gameplay for the strongest validation: host and client keep controlling their empires and play normally after Strategic Nexus state enters the synchronized game.

Use observer OOS soak validation when the goal is only to stress multiplayer synchronization over time without manually playing two empires. This is better than leaving time to pass with no interaction, because AI-controlled empires continue creating normal galaxy activity. Record clearly that `observe` was used.

Do not treat observer soak as proof of final gameplay UX. It validates stability/OOS behavior under simulation, not normal human player control.

Later, observer mode should also be used for strategic evaluation once daemon/LLM doctrine output exists. In that mode the tester watches whether the whole mod produces believable galaxy-level behavior while vanilla AI still handles normal execution.

Prefer the built-in multiplayer lobby observer role when selecting an empire at game start or load.

## Observer OOS Soak Test

Use this as an optional follow-up after the automatic no-debug smoke test has fired and both machines are in sync.

1. Confirm host and client both saw the expected automatic diagnostics or logs.
2. Select the built-in observer role from the multiplayer lobby when starting or loading the MP game.
3. Let the game run for several in-game months or years.
4. Watch for OOS/desync.
5. Check host and client logs.
6. Record that observer mode was used in `docs/POC_A_MULTIPLAYER_RESULTS.md`.

Important limits:

- observer mode is only a test convenience.
- observer mode is not a Strategic Nexus bridge.
- observer mode is not release automation.
- active gameplay validation is still needed before claiming the normal MP experience is verified.

## Future Observer Strategic Evaluation

Use this later, after runtime doctrine decisions exist.

1. Start a test game with Strategic Nexus runtime decisions enabled.
2. Let the daemon/bridge or scripted fallback produce bounded doctrine state.
3. Select the MP lobby observer role if available in the test environment.
4. Watch the galaxy over years or decades.
5. Record whether Strategic Nexus decisions create visible strategic pressure.
6. Verify vanilla AI still performs tactical and economic execution.
7. Watch for OOS/desync, runaway behavior, or decisions that have no meaningful effect.

This is a design/balance validation mode, not the bridge proof itself.
14. Record results in `docs/POC_A_MULTIPLAYER_RESULTS.md`.

## Explicit Checks

Record each item independently:

- event `strategic_nexus.1002` fired on host
- event `strategic_nexus.1100` global flag diagnostic fired on host
- event `strategic_nexus.1101` country flag diagnostic fired on host
- event `strategic_nexus.1102` doctrine flag diagnostic fired on host
- event `strategic_nexus.1103` variable diagnostic fired on host
- event `strategic_nexus.1105` modifier diagnostic fired on host
- country flag result visible on host
- diagnostic modifier visible on host
- diagnostic modifier or equivalent state visible to client
- variable branch worked
- no desync/OOS after time passes

## Expected Host `game.log`

The main PoC should produce host log lines similar to:

```text
Strategic Nexus: global bridge flag detected
Strategic Nexus: country bridge flag detected
Strategic Nexus: doctrine balance against hegemon flag detected
Strategic Nexus: strategic pressure variable detected above threshold
Strategic Nexus: diagnostic bridge modifier applied
```

## Expected Client Observation

Best result:

- client sees the same visible event/modifier state through normal multiplayer synchronization
- no OOS occurs after several in-game months

Acceptable but incomplete result:

- host sees all diagnostics
- client cannot directly see the event windows, but synchronized country state/modifier is visible
- no OOS occurs

Failure result:

- host state does not appear on client
- OOS/desync occurs
- client logs show script or sync errors tied to Strategic Nexus

## Fallback Test

Fallback should be tested in a fresh multiplayer state where the bridge flags have not already been set.

Expected host result:

- event `strategic_nexus.1002` fires
- event `strategic_nexus.1104` fallback diagnostic fires
- `game.log` contains:

```text
Strategic Nexus: fallback path selected, no bridge availability flag detected
```

Expected client result:

- no desync/OOS
- any synchronized fallback marker or visible state behaves consistently

If stale flags affect fallback testing, record that explicitly in `docs/POC_A_MULTIPLAYER_RESULTS.md`. Do not hide stale-state behavior.

## Logs To Collect

Host:

- `logs/error.log`
- `logs/game.log`
- OOS/desync files if generated

Client:

- `logs/error.log`
- `logs/game.log`
- OOS/desync files if generated

## Interpretation

If host-triggered state synchronizes to clients without desync, then the project can continue researching a host-only bridge/automation path.

If host-triggered state does not synchronize, the runtime bridge remains singleplayer-only until the multiplayer design is revised.
