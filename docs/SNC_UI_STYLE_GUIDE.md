# SNC UI Style Guide

## Purpose

This document is the canonical visual and control contract for the Strategic Nexus Companion (SNC) UI.

SNC is the companion / orchestrator surface, not the development Task Board. It should feel like the same product family, but it serves a different job and should remain valid whether the companion is distributed privately or publicly:

* monitor live game and archive state
* expose readiness and warning states
* prepare next-session mod work
* present companion actions without dumping raw log text

Implementation references:

* `src/SncTrayApp.cpp`
* `resources/StrategicNexusCompanion.rc`
* `resources/snc_tray_icon.ico`
* `dist/private_reports/snc_tray_status.json`
* `dist/private_reports/snc_local_model_state.json`

## Product Boundary

SNC should look like a polished companion surface, not:

* a debug console
* a white Windows dialog
* a plain text viewer
* a generic tray utility with no identity

The main SNC UI should be calm, informative, and obviously branded.

## Visual Language

SNC should share the Task Board palette and tone:

* dark shell
* teal body tones
* gold chrome accents
* muted but legible disabled states
* structured content blocks instead of naked text dumps

The tray icon should be the branded 3D SNC icon, not a generic Windows symbol.

## WDUi Catalog And Skins

SNC UI styling should be routed through the WDUi-style catalog layer instead of hard-coded one-off colors inside the window procedure.

Current skins:

* `Starlance` is the default gaming / 3D skin: deep navy shell, sapphire/cyan edges, amber action accents, gradient surfaces, glow, shadow, and subtle noise.
* `Classic` is the conservative fallback based on the original teal/gold SNC family.

New controls, dialog surfaces, scrollbars, and action buttons should receive a catalog style id or be mapped to the active SNC catalog. Do not add new fixed SNC-only color constants unless they are only a temporary bridge into the catalog.

Skin choice is user UI state and should persist locally with the rest of the SNC window state.

## Canonical Palette

Use the same family as the Task Board unless a document says otherwise.

| Role | Color |
| --- | --- |
| Frame | `#A47A45` |
| Shell / outer background | `#050A0F` |
| Main panel | `#07161D` |
| Section header | `#0C3A36` |
| Control fill | `#08161C` |
| Border | `#224244` |
| Accent / chrome | `#C49652` |
| Primary text | `#D3E4DD` |
| Secondary text | `#B0CCC4` |
| Disabled text | `#7A928C` |

## Typography

Preferred fonts:

* `Bahnschrift` for labels, buttons, summary headings, and compact metadata
* `Cascadia Mono` for paths, hashes, command hints, JSON-like values, and technical details

Guidelines:

* keep the main status view compact and readable
* do not use oversized, decorative copy in the companion surface
* prefer Czech owner-facing labels
* show timestamps in compact Czech format when they are shown to the owner: `d.M.yyyy  H:mm:ss`

## Layout Contract

The SNC main surface should evolve toward a structured dashboard:

1. title / header band
2. state summary band
3. structured status cards
4. command / action strip
5. detail or diagnostics area

The dashboard should present:

* archive readiness
* live capture state
* generated overlay readiness
* local LLM readiness
* next action / next step
* warnings and degraded states

The default view should prioritize at-a-glance understanding over raw detail volume.

## Control Language

SNC should reuse the same control family language as the Task Board:

* dark buttons with gold borders
* custom combobox styling
* clear selected/hover states
* structured read-only fields where editing is not expected
* no generic white MessageBox as the primary presentation layer

Visible controls should be custom-skinned or owner-drawn whenever standard WinForms styling would look off-theme.

## Data Presentation Rules

* Primary data source is the SNC status JSON, not free-form concatenated text.
* Raw status text may exist for debugging, but it should not be the default main surface.
* Group related fields into labeled sections, such as:
  * archive
  * capture
  * overlay
  * model
  * next action
  * warnings
* Make reduced mode obvious when no local model is installed.
* Make degraded or unavailable states explicit and calm, not alarming.
* Never imply that model output is trusted. SNC is a validator and orchestrator, not an unquestioned source of truth.

## Do Nots

Do not:

* use a log dump as the main UI
* fall back to a generic system dialog for the primary surface
* expose empty panels with no meaning
* make the owner hunt for the current state
* use bright default Windows chrome as the dominant visual element
* blur the difference between companion state and validated mod state

## Acceptance Criteria

SNC matches this guide when:

* the tray icon is branded and instantly recognizable
* the main window visually belongs beside the Task Board
* the owner can read readiness and next action in seconds
* the UI uses structured status blocks instead of a long text dump
* no primary surface looks like an unstyled Windows default
* the UI still behaves well when the model is missing, the archive is stale, or the game is not running
