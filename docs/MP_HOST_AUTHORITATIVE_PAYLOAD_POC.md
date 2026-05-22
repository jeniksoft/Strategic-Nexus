# MP Host-Authoritative Payload PoC

## Result

Status: passed as a multiplayer synchronized-state PoC.

Verified on 2026-05-20:

- one normal player empire displayed the authority payload dialog
- the other normal player empire displayed the observed payload dialog
- host and client logs both recorded payload generation and observation
- existing diagnostics detected the bridge flag, legacy doctrine flag, pressure variable, and diagnostic modifier
- no Strategic Nexus errors appeared in host or client `error.log`
- no OOS/desync files were found

This confirms the synchronized-state carrier. It does not yet solve true host-only daemon delivery.
Future architecture must solve this through safe scripted packaging, local files, and bounded validation rather than lower-level game-process modification.

## Purpose

This PoC tests whether a tiny bounded Strategic Nexus payload can enter synchronized Stellaris multiplayer game state and be read by ordinary mod events on multiple human countries.

It does not implement final daemon delivery.
It does not implement the daemon.
It does not implement LLM decisions.
It does not use custom networking.

## Important Limitation

No clean Stellaris script trigger for "current country belongs to the multiplayer host" has been verified yet.

Because of that, this scripted PoC is a host-authoritative approximation, not final host-only daemon delivery. It uses a global one-shot guard so the first eligible human country that receives the monthly pulse creates exactly one payload. Other human countries then observe the synchronized global payload.

Final architecture still requires a safe host-side workflow if daemon output must be applied only by the host.

## Payload Shape

The original MP PoC used this legacy bounded state:

```json
{
  "bridge_available": true,
  "doctrine_id": 2,
  "strategic_pressure": 0.74,
  "fallback_required": false
}
```

That shape is now considered legacy because Strategic Nexus has moved to modular `strategy_dimensions`.

The current bridge-core payload model is:

```json
{
  "schema_version": 1,
  "sequence_id": 1,
  "bridge_available": true,
  "strategy_dimensions": {
    "military_posture": "defensive",
    "research_bias": "economy"
  },
  "confidence": 0.68,
  "fallback_required": false
}
```

The in-game mapping remains intentionally tiny:

- `bridge_available = true` -> `strategic_nexus_bridge_available`
- `military_posture = defensive` -> `strategic_nexus_military_posture_defensive`
- `military_posture = aggressive` -> `strategic_nexus_military_posture_aggressive`
- `research_bias = economy` -> `strategic_nexus_research_bias_economy`
- `research_bias = military_industry` -> `strategic_nexus_research_bias_military_industry`
- legacy `doctrine_id = 2` -> `strategic_nexus_doctrine_balance_against_hegemon` for old PoC diagnostics only
- `strategic_pressure = 0.74` -> country variable `strategic_nexus_pressure` on the authority country
- `fallback_required = false` -> no fallback flag

No raw text, script string, fleet target, build order, or save fragment enters the game.

## Implemented Events

`strategic_nexus.1300`

- fires on `on_monthly_pulse_country`
- multiplayer only
- human default country only
- requires no existing global payload flag
- writes the bounded global payload once
- marks the current country as the payload authority
- sets the authority country pressure variable to `0.74`
- runs existing diagnostic event `strategic_nexus.1002`

`strategic_nexus.1302`

- fires on `on_monthly_pulse_country`
- multiplayer only
- human default country only
- requires the global payload to exist
- excludes the authority country
- runs existing diagnostic event `strategic_nexus.1002`

## Manual Test

1. Host and client install the same Strategic Nexus PoC mod.
2. Confirm both players have the same Stellaris version.
3. Confirm both players have the same gameplay checksum.
4. Start or load a non-Ironman multiplayer game with two normal player empires.
5. Use a normal non-debug setup.
6. Do not use manual debug transport.
7. Let the game pass at least one monthly pulse.
8. On one player, expect `Strategic Nexus PoC: MP payload autorita`.
9. On the other player, expect `Strategic Nexus PoC: MP payload pozorován`.
10. The authority player should also see existing diagnostics for:
    - global bridge flag
    - country bridge flag
    - legacy doctrine flag
    - modular military posture flag
    - modular research bias flag
    - pressure variable
    - diagnostic modifier
11. The non-authority player should at minimum see existing diagnostics for:
    - global bridge flag
    - legacy doctrine flag
    - modular strategy dimension flags
    - diagnostic modifier
12. Let the game run for at least five in-game years.
13. Check host and client `error.log`.
14. Check host and client `game.log`.
15. Check whether any OOS/desync files were created.

## Expected Logs

Authority country:

```text
Strategic Nexus: MP bounded payload generated once by script authority
```

Non-authority country:

```text
Strategic Nexus: MP bounded payload observed by non-authority country
```

## Pass Criteria

- exactly one payload generation path is observed per fresh test state
- at least one non-authority human observes the payload
- diagnostics confirm global bridge flag and modular strategy dimension detection
- authority diagnostics confirm pressure variable detection
- no Strategic Nexus errors in host or client `error.log`
- no OOS/desync during a five-year MP soak

## Failure Meaning

If the authority event fires but other clients cannot observe the global payload, the pure scripted synchronized-state path is not enough for multiplayer runtime state.

If both sides observe the payload but OOS occurs, the payload carrier or event timing is unsafe and must be redesigned before production daemon delivery work.

If the test passes, the next blocker is true host-only automation through safe scripted packaging, local files, and predefined actions only.
