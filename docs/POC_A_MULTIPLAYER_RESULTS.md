# PoC A Multiplayer Results

## Verified MP Bounded Payload Result

Status: passed.

On 2026-05-20, an active multiplayer payload test was run with two normal player empires.

Confirmed by player-visible dialogs:

- one player saw `Strategic Nexus PoC: MP payload autorita`
- the other player saw `Strategic Nexus PoC: MP payload pozorovan`
- the authority path created the bounded payload once
- the non-authority path observed the synchronized payload

Confirmed by host and client logs:

- `strategic_nexus.1300` fired for `human 1`
- `strategic_nexus.1302` fired for `human 3`
- `strategic_nexus.1002` fired for both relevant human countries
- global bridge flag was detected
- country bridge flag was detected on the authority country
- doctrine flag was detected
- pressure variable was detected on the authority country
- diagnostic modifier was applied
- no Strategic Nexus errors appeared in host `error.log`
- no Strategic Nexus errors appeared in client `error.log`
- no OOS/desync files were found

Host `game.log` evidence:

```text
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 86. Strategic Nexus: MP bounded payload generated once by script authority
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 164. Strategic Nexus: global bridge flag detected
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 173. Strategic Nexus: country bridge flag detected
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 185. Strategic Nexus: doctrine balance against hegemon flag detected
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 197. Strategic Nexus: strategic pressure variable detected above threshold
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 212. Strategic Nexus: diagnostic bridge modifier applied
[13:03:06][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 117. Strategic Nexus: MP bounded payload observed by non-authority country
[13:03:07][eventcommands.cpp:88]: Event strategic_nexus.1300 added info about event selection. selectedOption 0, human 1, playerEventId 69
[13:03:08][eventcommands.cpp:88]: Event strategic_nexus.1002 added info about event selection. selectedOption 0, human 1, playerEventId 68
[13:03:17][eventcommands.cpp:88]: Event strategic_nexus.1302 added info about event selection. selectedOption 0, human 3, playerEventId 74
[13:03:17][eventcommands.cpp:88]: Event strategic_nexus.1002 added info about event selection. selectedOption 0, human 3, playerEventId 73
```

Client `game.log` evidence:

```text
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 86. Strategic Nexus: MP bounded payload generated once by script authority
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 164. Strategic Nexus: global bridge flag detected
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 173. Strategic Nexus: country bridge flag detected
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 185. Strategic Nexus: doctrine balance against hegemon flag detected
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 197. Strategic Nexus: strategic pressure variable detected above threshold
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 212. Strategic Nexus: diagnostic bridge modifier applied
[13:03:07][effect_impl.cpp:22189]: [2207.2.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 117. Strategic Nexus: MP bounded payload observed by non-authority country
[13:03:08][eventcommands.cpp:88]: Event strategic_nexus.1300 added info about event selection. selectedOption 0, human 1, playerEventId 69
[13:03:09][eventcommands.cpp:88]: Event strategic_nexus.1002 added info about event selection. selectedOption 0, human 1, playerEventId 68
[13:03:18][eventcommands.cpp:88]: Event strategic_nexus.1302 added info about event selection. selectedOption 0, human 3, playerEventId 74
[13:03:19][eventcommands.cpp:88]: Event strategic_nexus.1002 added info about event selection. selectedOption 0, human 3, playerEventId 73
```

Conclusion:

The MP bounded payload synchronization path is verified at the mod-script level. A tiny predefined payload can be written once into synchronized game state and observed by another human country in a normal non-debug setup and without Strategic Nexus errors.

Important limitation:

This is still a single-authority scripted approximation, not the final host-only daemon bridge. The next technical blocker is automatic host-side delivery of the same predefined payload without arbitrary command execution.

## Verified Active MP Monthly Probe Result

Status: passed.

On 2026-05-20, an active multiplayer test was run for more than five in-game years with both players controlling normal empires, not observer mode.

Confirmed:

- host and client both loaded Strategic Nexus PoC
- visible Czech `Strategic Nexus PoC: Monthly probe` dialog appeared
- `strategic_nexus.1202` fired for `human 1`
- `strategic_nexus.1202` fired for `human 2`
- no Strategic Nexus errors appeared in host `error.log`
- no Strategic Nexus errors appeared in client `error.log`
- no OOS/desync files were found on host
- no OOS/desync files were found on client

Host `game.log` evidence:

```text
[12:41:11][effect_impl.cpp:22189]: [2206.8.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 78. Strategic Nexus: monthly pulse country probe fired
[12:41:15][eventcommands.cpp:88]: Event strategic_nexus.1202 added info about event selection. selectedOption 0, human 2, playerEventId 53
[12:41:21][eventcommands.cpp:88]: Event strategic_nexus.1202 added info about event selection. selectedOption 0, human 1, playerEventId 52
```

Client `game.log` evidence:

```text
[12:41:12][effect_impl.cpp:22189]: [2206.8.1] Log effect, file: events/strategic_nexus_poc_events.txt line: 78. Strategic Nexus: monthly pulse country probe fired
[12:41:17][eventcommands.cpp:88]: Event strategic_nexus.1202 added info about event selection. selectedOption 0, human 2, playerEventId 53
[12:41:22][eventcommands.cpp:88]: Event strategic_nexus.1202 added info about event selection. selectedOption 0, human 1, playerEventId 52
```

Conclusion:

`on_monthly_pulse_country` is verified as a no-console, no-debug, load-safe active multiplayer trigger for normal player empires.

This does not prove daemon-to-game transport yet, but it proves that bounded scripted Strategic Nexus state can be initialized in MP through a normal synchronized country-scoped on_action without OOS in a five-year smoke run.

## Environment

- Host Stellaris version:
- Client Stellaris version:
- Host DLC set:
- Client DLC set:
- Host OS:
- Client OS:
- Host checksum:
- Client checksum:
- Same checksum before test: yes/no
- Same mod version: yes/no

## Session

- Host account:
- Client account:
- Host started MP game: yes/no
- Client joined successfully: yes/no
- Non-Ironman: yes/no

## Main PoC

- Automatic scripted smoke test executed: yes/no
- Host used normal non-debug setup: yes/no
- Client used normal non-debug setup: yes/no
- Host automatic event `strategic_nexus.1200` ran/logged: yes/no
- Client automatic event `strategic_nexus.1200` ran/logged: yes/no/unknown
- Host saw automatic `strategic_nexus.1002` main event: yes/no
- Client saw automatic `strategic_nexus.1002` main event: yes/no/unknown
- Notes on automatic test:

- Host saw `strategic_nexus.1102` doctrine flag diagnostic: yes/no
- Host saw `strategic_nexus.1103` pressure variable diagnostic: yes/no
- Host saw `strategic_nexus.1105` modifier diagnostic: yes/no

## Client Synchronization

- Client saw any diagnostic window: yes/no/unknown
- Client saw modifier/state effect: yes/no/unknown
- Client state matched host: yes/no/unknown
- Client logs contain Strategic Nexus events: yes/no/unknown

## Fallback

- Fallback test executed: yes/no
- Host saw `strategic_nexus.1104` fallback diagnostic: yes/no
- Client saw fallback result: yes/no/unknown
- Stale flags affected fallback testing: yes/no
- Stale flag notes:

## Stability

- Desync/OOS occurred: yes/no
- Time played after payload delivery:
- Host continued active play: yes/no
- Client continued active play: yes/no
- `observe` used during test: yes/no
- Observer OOS soak duration:
- Observer mode used on host/client/both:
- Observer source: MP lobby observer / unknown
- Result type: active gameplay / observer OOS soak / mixed
- Game remained playable: yes/no

## Logs

Host `error.log` Strategic Nexus errors:

```text

```

Client `error.log` Strategic Nexus errors:

```text

```

Host `game.log` relevant lines:

```text

```

Client `game.log` relevant lines:

```text

```

OOS/desync files:

```text

```

## Conclusion

- MP synchronization passed: yes/no/partial
- Host-authoritative model viable: yes/no/unknown
- No-console/no-debug scripted MP smoke path viable: yes/no/unknown
- Active gameplay validation passed: yes/no/not tested
- Observer OOS soak passed: yes/no/not tested
- Note: automatic smoke test does not prove final host-only daemon delivery.
- Recommended next step:
