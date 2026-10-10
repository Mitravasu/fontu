# Snapping

Users enable snapping to the visible grid, shape geometry, or both from the toolbar's Snap group beside Undo and Redo. An amber circle marks the winning target before a drawing press and during edits. Hovering a selected handle or shape body previews its eligible resize or move target without changing geometry.

## Sub-features

- `snap-hover` previews drawing starts and selected-shape edit targets without changing the document.
- `snap-toolbar` toggles each independent option and reaches all four combinations.
- `snap-draw` snaps a draw's start and end points.
- `snap-move` snaps shape corners, edge midpoints, or center.
- `snap-resize` snaps the dragged handle while retaining the opposite bounds.
- `snap-cancel` changing an option cancels an unfinished edit.

## How to get to it (user POV)

- Click `To grid` or `To geometry + guides` in the toolbar's Snap group.
- Toggle either button independently. Both start off.
- Draw, move, or resize near a grid or geometry target while an option is enabled.

## Driving it with control-fontu

Preconditions:

- Run from the repository root with a new evidence directory and require fresh-window doctor success.

Run `.agents/skills/verify-fontu/helpers/control-fontu snapping --evidence /tmp/fontu-proof-snapping-01`.

- Resize the production window to 800 by 600, capture the toolbar, and toggle each Snap button on and off. Assert that both buttons are visible and inside the toolbar.
- Restore 1100 by 760 and capture the toolbar.
- Click `To grid`, selected by `snapToGrid`. Only grid is enabled.
- Click `To geometry + guides`, selected by `snapToGeometry`. Both options are enabled.
- Click `To grid` again. Only geometry is enabled.
- Click `To geometry + guides` again. Both options are off.
- Select Rectangle from the production toolbar and draw a target rectangle. With geometry enabled, hover near its outline and assert the amber circle before pressing. Enable grid too and assert the same nearer geometry target.
- Disable grid, draw from that hover point, and assert the new bounds start at the previewed geometry target.
- Switch to grid only and hover near a visible intersection. Assert its amber circle before pressing. Each hover checks that document count, selection, and the selected path are unchanged.
- Every toggle and hover produces a screenshot and state record. Require the final `PASS:` record, exit zero, and completed cleanup in `run.json`.

This command proves the toggle UI, drawing hover markers for grid/geometry/both, and a geometry-snapped drawing start. Move and resize hover/drag effects, each constrained resize axis, zoom/pan mapping, and toggle-change cancellation still need production-window recipes before claiming full snapping coverage.

## Gotchas

- The helper clicks the production toolbar buttons through Qt events. Native OS input remains unverified.
- Capture distance is eight logical screen pixels. Adaptive grid spacing changes with zoom.
- The edited shape is excluded from its own geometry targets.
- Bounds snap points can lie inside shapes or at ellipse bounds corners.
- Changing an option cancels an unfinished geometry edit.
