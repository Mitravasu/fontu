# fontu

A native Qt 6 / C++ vector canvas for font creation. Milestone 1 supports rectangles, ellipses, open pen paths, filled closed pen shapes, selection, movement, eight resize handles, deletion, undo and redo, a grid, a bounded glyph workspace, labeled typography guides, optional snapping to the grid and shape geometry or guides, zoom, and pan. Geometry remains editable paths in document space. The right Glyphs panel starts with digits 0 through 9. Each glyph retains its completed drawing, selection, and Undo/Redo history when you switch.

The fixed workspace is 1000 by 1000 font units at 1000 UPM. Startup and Reset view fit the document. Drawing and editing stop at its bounds while pan and zoom remain available outside it. [Glyph workflow milestones](docs/glyph-workflow-milestones.md) records the planned new-document screen, configurable presets and guides, and character sets.

## Build and launch

Requires CMake 3.21+, a C++17 compiler, and Qt 6.4+ with Widgets and Test. Tested on macOS with Qt 6.11.2 and AppleClang 21.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On macOS with Homebrew Qt, use `-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"`. Launch the development application:

```sh
open build/fontu.app
```

On Linux, run `./build/fontu`. On Windows, run `build/Debug/fontu.exe` or the executable in your chosen configuration. Qt's libraries and platform plugins must be discoverable. Windows and Linux have not been verified yet. This build is for development, not a standalone distribution package.

## Controls

| Action | Control |
| --- | --- |
| Select tool | Toolbar or V |
| Rectangle tool | Toolbar or R |
| Ellipse tool | Toolbar or E |
| Pen tool | Toolbar or P |
| Add a pen anchor | Click with the left mouse button |
| Curve at a pen anchor | Drag a second or later anchor to set symmetric handles |
| Finish an open pen path | Enter or Return after releasing the mouse |
| Close a filled pen shape | Click the first anchor after adding at least three anchors |
| Create a rectangle or ellipse | Drag with the left mouse button |
| Select / move | Select tool, click a shape / drag its fill or outline |
| Resize | Select tool, drag a corner or edge handle |
| Snap to grid or geometry and guides | Independent `To grid` and `To geometry + guides` toggles beside Undo and Redo in the toolbar |
| Delete selection | Delete or Backspace |
| Undo | Toolbar or the platform Undo shortcut, Command+Z on macOS and Ctrl+Z on Windows/Linux |
| Redo | Toolbar or the platform Redo shortcut, Command+Shift+Z on macOS and Ctrl+Y on Windows/Linux |
| Zoom at pointer | Mouse wheel or vertical trackpad scroll |
| Pan | Middle-button drag or hold Space and left-drag |
| Fit the complete document | Reset view toolbar button or 0 |
| Cancel an edit | Escape during a draw, move, resize, or pen draft |
| Clear selection | Escape when idle or click empty canvas with Select |

Draw several shapes with R and E, switch to V, select and drag a shape, resize it with a handle, zoom and pan, then delete it. The bottom status bar shows tool controls and Pen closure instructions as the draft changes, including Undo and Redo. Usage tips stay outside the drawing area. New shapes are selected automatically. Overlapping shapes select in reverse drawing order. Rectangles and ellipses smaller than three screen pixels on either axis are discarded when created.

With P, click anchors for straight segments. Drag a second or later anchor to curve the incoming and following segments with symmetric handles. The first anchor has no handles. After adding at least three anchors, click within eight logical screen pixels of the first anchor to close and fill the shape immediately. A ring marks the first anchor. Hovering over it previews the filled shape and shows a hand cursor. The closing segment preserves the last anchor's curve handle. Select a closed shape by its interior or outline.

Enter or Return finishes an open stroke with at least two anchors after mouse release. Select open strokes near their outlines. Both finish controls keep the Pen tool active. Hover previews the next segment; it does not add an anchor. Escape discards the entire unfinished path, including between clicks. Horizontal paths have left and right resize handles; vertical paths have top and bottom handles. Resizing preserves their collapsed axis.

Switching glyphs cancels unfinished Pen drafts and geometry gestures. The canvas view, tools, and snapping stay shared. Clicking a glyph row returns keyboard focus to the canvas.

Losing canvas focus, changing tools, resizing the canvas, or resetting the view cancels an unfinished geometry edit. Pen drafts survive zoom and pan. Escape during panning ends the gesture and keeps the current view. Resizing across the opposite edge normalizes the bounds without mirroring the path.

Snapping starts with both options off. Enable either option or both in the toolbar's Snap group. Drawing snaps the start and end points; pen anchors and dragged handle endpoints use the same targets; moving snaps the shape's bounds corners, edge midpoints, or center; resizing snaps the dragged handle while keeping the opposite bounds fixed. Targets must be within eight logical screen pixels, so zoom does not change the capture distance. Grid targets use the visible adaptive grid intersections. Edge handles snap to grid lines on their adjustable axis. Shape geometry targets include actual path outlines and bounds snap points. An amber circle marks the winning target during a gesture. Changing a snap option cancels an unfinished edit.

## Architecture

- `src/document.*` owns ordered shapes, stable IDs, and single selection. Each shape records whether it is filled or stroked. Shape geometry is a `QPainterPath` containing lines and cubic curves, with no widget or screen coordinates. Geometry functions construct and transform paths.
- `src/viewport.*` maps an unbounded y-up document plane to y-down logical screen pixels, with cursor-anchored zoom. Rendering and input share this mapping.
- `src/canvas.*` paints paths, an adaptive grid, and fixed-screen-size handles, and handles gestures. Move/resize updates derive from the original path snapshot. Pen drafts store document-space anchors and tangents until committed. Canvas owns stable glyph records, each with its own Document, before/after snapshots, and history cursor. A single active record drives rendering and editing.
- `src/snapping.*` queries grid and path targets in screen space, excludes the edited shape by ID, and returns the nearest eligible target.
- `src/glyph_panel.*` derives a scrolling glyph picker from Canvas metadata and keeps its selected row synchronized.
- `src/snap_menu.*` adds the two independent toolbar snap actions; `src/main.cpp` creates the native window and tool actions.

The [typography domain reference](docs/typography_domain_reference.md) defines font terminology. The [terminology audit](docs/terminology-audit.md) records how it applies to the current canvas.

See [the milestone design](docs/milestone-1-design.md) for decisions and the comparison with Graphics View. Pen paths add lines and cubic curves to the document. Path validation accepts extent on either axis. Rectangle and ellipse creation still requires extent on both axes. QPainterPath is the in-memory geometry representation, not a proposed persistent font format.

## Verification

`core_tests` checks normalized bounds, ellipse containment, curve preservation, movement and resizing, document selection/deletion, stacking-order hit testing, inverse coordinate mapping, and zoom limits and anchors. Snapping checks cover adaptive grid spacing, the fixed screen tolerance, nearest-target selection, exact ties, shape exclusion, curve targets, and constrained outline intersections.

`canvas_tests` sends mouse, wheel, and keyboard events to the actual widget. It checks drawing, tiny/cancelled drags, overlapping selection, moving, deleting, every resize handle, crossing the resize anchor, off-center handle presses, pan/zoom followed by editing, view reset, Escape/tool-change cancellation, and focus-loss rollback. Snapping cases cover toolbar keyboard/action activation and all toggle states, snapped drawing/moving/resizing, off-center handles, curve outlines, side-handle constraints, self-exclusion, competing grid/geometry targets, zoom/pan mapping, and toggle-change rollback. Pen cases cover straight and cubic segments, exact controls, commit timing, cancellation, snapped anchors and handles, draft navigation, stroke hit testing, and horizontal/vertical movement and resizing. Closing cases check filled triangles, curved closing segments, interior selection and movement, the minimum anchor count, screen capture distance, closure after zoom and pan, and draft cleanup. CTest runs this suite with Qt's offscreen backend.

The canvas suite also passed with the native macOS Qt backend, and a rendered canvas image was inspected. Native test windows do not take desktop focus because the tests inject Qt events directly; focus loss is tested explicitly. Desktop UI automation was unavailable because Computer Use permissions were not granted, so an OS-driven mouse/keyboard walkthrough remains unverified.

## Current limitations

The document is session-only. There is no saving or autosave. Closing the window loses its shapes and edit history. Undo and Redo restore completed drawings, Pen paths, moves, resizes and deletions. A new edit clears the redo branch. Selection, tools, snapping and navigation do not add history. During an unfinished Pen path, Undo removes one vertex and Redo restores it with its tangent handles. A fully undone draft keeps Redo available and blocks document Undo until Escape or a tool switch cancels it. A new vertex replaces draft redo. Enter or click closure commits the visible draft as one edit. Undo or Redo cancels other unfinished gestures before restoring a completed edit. History stores document snapshots in memory without a size limit. Selection is limited to one shape. Resizing uses axis-aligned bounds. There is no rotation, node editing, independent pen handles, Shape Builder, or font export in this milestone. Pen handles cannot be edited after committing a path.

Geometry snap queries use Qt's screen-space polygon approximation of curves. Stored paths retain their cubic segments. This is suitable for interactive canvas snapping, not exact curve intersections or font construction. Bounds snap points include points inside shapes and ellipse bounds corners, which can also be snap targets.
