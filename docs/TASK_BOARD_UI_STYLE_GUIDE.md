# Task Board UI Style Guide

## Purpose

This document is the canonical visual and interaction contract for the owner-facing Task Board in the private Strategic Nexus repository.

The Task Board is development-only coordination tooling. Its design is not a generic application shell and it is not release companion UI.

Use this document as the source of truth for:

* colors
* typography
* layout density
* control styling
* disabled-state behavior
* list/detail presentation
* Czech owner-facing wording

Implementation references:

* `.codex_local/decompiled_native_tb/TaskBoardForm.cs`
* `.codex_local/decompiled_native_tb/DarkListView.cs`
* `.codex_local/decompiled_native_tb/DarkComboBox.cs`
* `.codex_local/decompiled_native_tb/NativeUi.cs`

## Design Goals

The Task Board should feel:

* dark, compact, and work-focused
* dense enough for scanning, but not cramped
* visually consistent across tabs and record types
* explicit about state, especially disabled and read-only controls
* pleasant enough that repeated daily use does not feel like fighting the UI

The Task Board should not feel like:

* a default Windows form
* a white admin panel
* a text dump with controls around it
* a preview-only mockup with fake product language

## Canonical Palette

The decompiled native Task Board uses the following core colors.

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

If a visual surface is obviously part of the Task Board family, it should stay inside this palette unless a deliberate exception is documented.

## Typography

Preferred fonts:

* `Bahnschrift` for general UI labels, buttons, filters, tabs, and rows
* `Cascadia Mono` for structured text, JSON-like data, paths, hashes, or technical values

Guidelines:

* keep letter spacing normal
* avoid oversized headings in compact panels
* use compact Czech timestamps when showing owner-facing dates: `d.M.yyyy  H:mm:ss`
* keep labels short and predictable

## Layout Contract

The default Task Board layout should remain:

1. top command bar
2. filter row
3. mode tabs
4. split main body
   * left: list / table
   * right: detail / inspector
5. bottom status area

The left list is the primary working surface.
The right detail panel is secondary and should not dominate the window.

The layout should support:

* task, report, and suggestion modes
* read-only record inspection where editing is not the goal
* free vertical scrolling
* horizontal scrolling when the content demands it

## Control Language

### Buttons

Buttons should be:

* rectangular
* dark-filled
* gold-bordered
* visually consistent with the rest of the shell
* legible when disabled

Disabled buttons must still be readable. A disabled state is not the same as a hidden state.

### Combo boxes

Combo boxes should be owner-drawn or otherwise custom-skinned.
Avoid the default Windows combobox appearance in visible owner-facing areas.

### List view / table

The record list should:

* be dark themed
* have strong but not harsh column separators
* show a clear selected row state
* keep headers readable
* preserve a consistent header remainder fill when columns do not occupy the full width
* use a custom horizontal scrollbar appearance instead of a bright native bar leaking through the theme

### Detail panel

The detail panel should:

* prefer structured fields over free-form text dumps
* use read-only fields where the owner is not expected to edit
* allocate enough vertical space for long text fields
* avoid large empty white/bright areas

## Data Presentation Rules

* Czech labels are the default owner-facing language.
* Preserve stable, compact identifiers for IDs, categories, and technical paths.
* Show human-readable states in a way that stays legible at a glance.
* Keep timestamps compact and consistent across tabs.
* Preserve per-mode filter state when switching between Tasks, Reports, and Suggestions.

## Do Nots

Do not:

* switch the Task Board to a white/light default form
* use a generic message box for primary workflow
* hide disabled controls if the state itself matters
* let the UI depend on raw free-form chat memory
* mix release-companion language into the development Task Board
* use visual whitespace as a substitute for structure

## Acceptance Criteria

A Task Board implementation matches this guide when:

* it still feels like the same product family as the decompiled native board
* its visible controls use the same palette and density
* list/detail work feels fast to scan
* Czech labels are clear and consistent
* disabled states remain readable
* the UI does not regress into default Windows styling

