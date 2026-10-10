# fontu verification map

Read the matching recipe before running `control-fontu`. Every run starts its own fresh production window with Qt's offscreen backend. The controller owns its process and private build directory. The user's desktop app remains untouched. Each proof directory must be new.

## Driving conventions

Run commands from the repository root. Each named command builds, launches, checks readiness with doctor, exercises its scenario, captures screenshots and state, and cleans up. The helper injects Qt events into the unchanged production window. It does not send native OS input. See [the skill](../SKILL.md) for the evidence contract and cleanup checks.

Each feature entry lists all known user controls, then separates the scripted coverage from the remaining checks. A pass for a shortcut does not prove its toolbar counterpart. Native input, trackpad, and focus behavior remain separate checks. Project saving is available. Font export remains absent. Undo and Redo restore completed canvas edits and individual vertices in unfinished Pen paths.

## Features

- [Project persistence](persistence.md) covers File actions, standard shortcuts, temporary file dialogs, all-glyph vector round trips, dirty Undo and Redo, invalid Open, and canceled transitions.

- [Digit glyph selection](glyphs.md) covers the right-side picker, initial digit 0, per-glyph drawings and histories, canceled drafts and gestures, and scrolling at smaller height.

- [Bounded glyph workspace](glyph.md) covers initial fit, typography guide snapping, hard drawing and editing bounds, bounded Pen controls, pan, zoom, and fitted Reset view.

- [Drawing shapes](drawing.md) covers Rectangle and Ellipse toolbar/keyboard selection, drags, tiny-drag rejection, and Escape cancellation.
- [Pen paths](pen.md) covers Pen toolbar/keyboard selection, lines, corners, symmetric cubic handles, Enter commit, click closure, filled interior selection, cancellation, draft navigation, snapping, and stroke editing.
- [Undo and redo](history.md) covers toolbar buttons, Qt standard shortcut bindings, edit transactions, Pen vertex history, redo invalidation, no-op preservation, and cancellation.
- [Bottom status hints](status.md) covers initial tool hints, Pen closure hints through vertex Undo and Redo, completion and cancellation, draft navigation, and readability at 1100 and 800 pixels.
- [Selecting and editing](editing.md) covers Select toolbar/keyboard, move, one corner resize, deselection, Delete, and Backspace. It lists remaining handle and rollback checks.
- [Navigating the canvas](navigation.md) covers wheel events, both pan controls, and both Reset view controls. Native trackpad delivery remains unverified.
- [Snapping](snapping.md) covers the two Snap toolbar toggles, all four toggle combinations, pre-press drawing hover targets, and a geometry-snapped drawing start. Move and resize effects require additional recipes.

## Proof reporting

Name the feature, exact entry points driven, evidence directory, backend, asserted result, and cleanup result. Retain action records with before/after screenshots. Read `run.json` after teardown and confirm the proof still exists. Do not report an unimplemented map check as covered by standalone-widget tests.
