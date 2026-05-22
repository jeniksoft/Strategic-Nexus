# Multiplayer Requirements

## Status

Multiplayer support is mandatory for Strategic Nexus.

The current scripted PoC path is verified through mod events, flags, modifiers, bounded variables, fallback behavior, and visible diagnostic events `strategic_nexus.1100` through `strategic_nexus.1105`.

This does not prove that host-delivered daemon state synchronizes to connected clients.

## Authority Model

The host is authoritative.

The LLM daemon runs only on the host.

Clients do not run the LLM daemon.
Clients do not generate strategic decisions.
Clients do not independently choose doctrines, weights, or strategic pressure values.

Clients only need:

- the same Stellaris version
- the same Strategic Nexus mod version
- the same gameplay checksum before joining
- the same generated config before game launch if generated files affect gameplay or checksum

## Runtime State Rule

Runtime decisions must enter synchronized Stellaris game state.

Allowed synchronized carriers to test first:

- predefined events
- predefined flags
- bounded variables
- harmless diagnostic modifiers

If host-triggered state does not synchronize to clients, the current runtime bridge path is singleplayer-only until redesigned.

## Generated Files Rule

If generated files affect gameplay, all multiplayer participants must have the same generated files before launch.

Do not rely on clients generating local strategic state during a multiplayer session. Any generated gameplay-affecting config must be distributed before the game starts and must preserve checksum compatibility.

## Networking Rule

Custom host-to-client networking is a fallback only, not the first design.

Preferred flow:

```text
host-only daemon
    -> host-only bridge or automation
    -> predefined event/effect
    -> synchronized game state
    -> clients receive normal Stellaris MP state
```

Rejected first design:

```text
host daemon
    -> custom Strategic Nexus network relay
    -> client daemons
    -> client-local decisions
```

## Development Harness Rule

Manual debug workflows are not part of the supported architecture.

## Verified MP Script Path

`on_monthly_pulse_country` is verified as the current no-console/no-debug active multiplayer scripted path.

Verified on 2026-05-20:

- two normal player empires
- Czech `Monthly probe` dialog appeared
- `strategic_nexus.1202` fired for both humans
- more than five in-game years elapsed
- no Strategic Nexus errors
- no OOS/desync files

This means Strategic Nexus can use synchronized country-scoped scripted initialization in MP. The unresolved runtime dependency is how the host daemon/bridge will feed bounded values into that scripted layer.

The final system must automate the host-side trigger.

## MP Payload PoC

The next prepared test is documented in [MP_HOST_AUTHORITATIVE_PAYLOAD_POC.md](MP_HOST_AUTHORITATIVE_PAYLOAD_POC.md).

Because no clean pure-script `is_host` trigger has been verified, the current mod-only payload test is a single-authority approximation:

- one eligible human country writes a tiny predefined payload once
- the payload enters synchronized game state through global flags
- the authority country also receives a bounded pressure variable
- other human countries observe the global payload through normal scripted events

This is not the final host-only daemon bridge. It is a multiplayer synchronization gate before any production daemon delivery workflow.

Pass requirement:

- exactly one payload generation log
- at least one non-authority payload observation log
- no Strategic Nexus `error.log` issues
- no OOS/desync during a five-year MP soak

## Safety Rules

The multiplayer design must preserve the project safety boundaries:

- no save editing
- no raw LLM command execution
- no arbitrary command strings from daemon
- no direct tactical control
- no fleet control
- no economy internals patching
- no AI replacement
- no blocking wait on daemon output
- vanilla AI remains the execution layer
- mod fallback must work without daemon or bridge

## Verdict Gate

Before production daemon delivery work proceeds, multiplayer PoC A must answer:

- Does the host event fire?
- Does host-set country/global state synchronize?
- Does the diagnostic modifier/state become visible to the client?
- Does bounded variable-driven branch behavior remain synchronized?
- Does time continue without desync/OOS?
