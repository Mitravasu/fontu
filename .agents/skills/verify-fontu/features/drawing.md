# Drawing shapes

Users create editable rectangles and ellipses by choosing a tool and dragging the canvas. New shapes are selected and have resize handles.

## Sub-features

- `draw-tools` chooses each shape tool through its toolbar button and its key.
- `draw-rectangle` commits a rectangle after a left drag.
- `draw-ellipse` commits a curved ellipse after a left drag.
- `draw-tiny` discards a drag smaller than three screen pixels on an axis.
- `draw-cancel` discards an unfinished draw with Escape.

## How to get to it (user POV)

- Click `Rectangle (R)` or press R with canvas focus.
- Click `Ellipse (E)` or press E with canvas focus.
- Left-drag the canvas, then release to commit.
- Press Escape while the left button is held to cancel.

## Driving it with control-fontu

Preconditions:

- Run from the repository root with a working Qt build and a new evidence directory.
- The helper's doctor must show a fresh empty canvas with both Snap options off.

Run `.agents/skills/verify-fontu/helpers/control-fontu drawing --evidence /tmp/fontu-proof-drawing-01`.

- Click `Rectangle (R)`, then drag `(100,100)` to `(220,190)`. The action log and screenshots show the toolbar selection, drag preview, and release. One selected rectangle has bounds `[100,100,120,90]`.
- Press E, then drag `(300,100)` to `(430,200)`. A second selected shape has bounds `[300,100,130,100]` and a curved path.
- Click `Rectangle (R)`, then `Ellipse (E)`, then press R. Checked toolbar state follows each change. Both toolbar and keyboard entry points have action records.
- Drag `(500,100)` to `(501,101)`. The shape count remains two.
- Press at `(500,250)`, move to `(600,350)`, press Escape, then release. The preview disappears and the shape count remains two.
- Require the final `PASS:` record, exit zero, screenshots for these actions, and completed cleanup in `run.json`.

## Gotchas

- Coordinates are canvas logical pixels, not desktop coordinates.
- Snap options start off in every new instance.
- The helper captures Qt-rendered screenshots. OS pointer delivery and desktop focus are unverified.
- Tool-change and focus-loss cancellation have existing regression tests, but this recipe proves Escape cancellation only.
