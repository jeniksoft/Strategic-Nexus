# Drop-In / Drop-Out Compatibility

## Goal

Strategic Nexus should be safe to enable on an existing save if possible, and safe to disable later if possible.

This is especially important for v0.x because the project is still proving runtime integration paths. The mod must not become a hard dependency for basic save survival unless a future reviewed feature explicitly accepts that tradeoff.

## Rules

1. Strategic Nexus must be safe to enable on an existing save if possible.
2. Strategic Nexus must be safe to disable later if possible.
3. Vanilla AI must remain functional without the mod.
4. The mod must not own critical game state.
5. The mod must not require start-game-only initialization.
6. Existing saves must initialize lazily.
7. Missing Strategic Nexus state must become fallback.
8. Unknown or missing daemon/bridge state must become fallback.
9. Avoid persistent custom game objects in v0.x.
10. Prefer flags, variables, events, and temporary modifiers.
11. Do not add buildings, components, traits, civics, traditions, resources, megastructures, or ship sections in v0.x unless explicitly reviewed.
12. Cleanup event may exist, but disabling the mod must not depend on cleanup.

## Design Implications

Strategic Nexus v0.x should treat all mod state as optional hints.

If a country, save, or multiplayer session has no Strategic Nexus state, the scripted mod layer must behave as if fallback is active.

Acceptable v0.x state carriers:

- country flags
- global flags for diagnostics or session-wide availability markers
- scoped variables with bounded values
- triggered events
- temporary modifiers used only for diagnostics or clearly reviewed gameplay hints

Avoid in v0.x unless explicitly reviewed:

- custom buildings
- ship components
- traits
- civics
- traditions
- resources
- megastructures
- ship sections
- other persistent game objects that can break saves when the mod is removed

## Lazy Initialization

The mod must not rely on a start-game-only setup event.

Any event or scripted behavior that needs Strategic Nexus state should handle missing state locally:

```text
state present
    -> use bounded Strategic Nexus values

state missing
    -> use fallback scripted behavior
```

This makes existing saves safer to adopt and reduces risk when the daemon or bridge is absent.

## Verified Lazy Trigger

`on_monthly_pulse_country` is currently verified as the preferred v0.x lazy initialization path.

Why:

- it runs after loading an existing save
- it has country scope
- it works without console
- it works in a normal non-debug setup
- it can be guarded by a one-shot country flag
- missing state can naturally fall back on the next pulse

Verified PoC:

```text
on_monthly_pulse_country
    -> strategic_nexus.1202
```

Observed result after loading a save:

```text
Strategic Nexus: monthly pulse country probe fired
```

No Strategic Nexus errors were present in `error.log`.

## Cleanup

A cleanup event may exist for development or graceful removal.

However, save safety must not depend on the player running cleanup before disabling the mod. If the mod is removed, vanilla Stellaris should still load and vanilla AI should continue functioning.

## Required Tests

1. Start a new game with the mod enabled.
2. Add the mod to an existing save and verify lazy initialization.
3. Remove the mod after initialization and verify the vanilla game still loads.
4. Remove the mod after Strategic Nexus flags, variables, and modifiers were used.
5. Check `error.log` after each case.

## Pass Criteria

- Existing save can load after enabling the mod.
- Existing save can continue with fallback when no Strategic Nexus state exists.
- Save can load after disabling the mod.
- Vanilla AI remains functional after disabling the mod.
- No Strategic Nexus errors appear in `error.log` after each required test.

## Failure Cases To Record

- save fails to load after enabling the mod
- save fails to load after disabling the mod
- missing Strategic Nexus state causes script errors
- removed modifiers or state cause repeated `error.log` noise
- vanilla AI behavior breaks because the mod owned critical state
