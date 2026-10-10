# Snapping

Users enable snapping to the visible grid, shape geometry, or both from the Snap menu. An amber circle marks the winning target before a drawing press and during edits. Hovering a selected handle or shape body previews its eligible resize or move target without changing geometry.

## Sub-features

- `snap-hover` previews drawing starts and selected-shape edit targets without changing the document.
- `snap-menu` toggles each independent option and reaches all four combinations.
- `snap-draw` snaps a draw's start and end points.
- `snap-move` snaps shape corners, edge midpoints, or center.
- `snap-resize` snaps the dragged handle while retaining the opposite bounds.
- `snap-cancel` changing an option cancels an unfinished edit.

## How to get to it (user POV)

- Open `Snap`, then choose `To grid` or `To shape geometry`.
- Toggle either checkbox independently. Both start off.
- Draw, move, or resize near a grid or geometry target while an option is enabled.

## Driving it with control-fontu

Preconditions:

- Run from the repository root with a new evidence directory and require fresh-window doctor success.

Run `.agents/skills/verify-fontu/helpers/control-fontu snapping --evidence /tmp/fontu-proof-snapping-01`.

- Open the production Snap menu and click `To grid`, selected by `snapToGrid`. Only grid is enabled.
- Open it and click `To shape geometry`, selected by `snapToGeometry`. Both options are enabled.
- Open it and click `To grid` again. Only geometry is enabled.
- Open it and click `To shape geometry` again. Both options are off.
- Select Rectangle from the production toolbar and draw a target rectangle. With geometry enabled, hover near its outline and assert the amber circle before pressing. Enable grid too and assert the same nearer geometry target.
- Disable grid, draw from that hover point, and assert the new bounds start at the previewed geometry target.
- Switch to grid only and hover near a visible intersection. Assert its amber circle before pressing. Each hover checks that document count, selection, and the selected path are unchanged.
- Every opening, toggle, and hover produces a screenshot and state record. Require the final `PASS:` record, exit zero, and completed cleanup in `run.json`.

This command proves the toggle UI, drawing hover markers for grid/geometry/both, and a geometry-snapped drawing start. Move and resize hover/drag effects, each constrained resize axis, zoom/pan mapping, and toggle-change cancellation still need production-window recipes before claiming full snapping coverage.

## Gotchas

- The offscreen helper exposes the same production menu locally because no native OS menu exists there. macOS global-menu behavior remains unverified.
- Capture distance is eight logical screen pixels. Adaptive grid spacing changes with zoom.
- The edited shape is excluded from its own geometry targets.
- Bounds snap points can lie inside shapes or at ellipse bounds corners.
- Changing an option cancels an unfinished geometry edit.
