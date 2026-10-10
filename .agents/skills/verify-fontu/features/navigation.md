# Navigating the canvas

Users zoom around the pointer and pan around a bounded glyph document. Reset view fits the complete document while preserving document geometry.

## Sub-features

- `view-zoom` zooms through mouse wheel or vertical trackpad scroll.
- `view-pan-middle` pans through a middle-button drag.
- `view-pan-space` pans through Space plus a left drag.
- `view-reset` restores the view through the toolbar or 0.

## How to get to it (user POV)

- Scroll vertically with the pointer over the canvas.
- Middle-drag, or hold Space and left-drag.
- Click `Reset view (0)` or press 0 with canvas focus.
- Press Escape during panning to end the gesture while retaining the current view.

## Driving it with control-fontu

Preconditions:

- Run from the repository root with a new evidence directory and require the helper's fresh-window doctor.

Run `.agents/skills/verify-fontu/helpers/control-fontu navigation --evidence /tmp/fontu-proof-navigation-01`.

- Draw the rectangle `(100,100)` to `(220,200)` through the toolbar.
- Send a wheel event with angle delta `+120` at `(200,150)`. The displayed zoom becomes 120 percent and the assertion reads zoom 1.2.
- Middle-drag `(500,350)` to `(530,370)`. Rendered shape bounds shift by `(30,20)`.
- Hold Space, left-drag `(500,350)` to `(520,360)`, then release Space. Rendered bounds shift by another `(20,10)`.
- Click `Reset view (0)`. The complete document fits. Zoom again and press 0. The fitted zoom returns and the stored shape path equals its original path.
- Require the final `PASS:` record, exit zero, rendered evidence, and completed cleanup in `run.json`.

For full navigation coverage, add production-window assertions for pointer anchoring, zoom limits, reset centering, editing after zoom/pan, and Escape during a held pan. Trackpad and native OS scroll delivery require a separate owned native instance.

## Gotchas

- Qt logical pixels differ from physical display pixels.
- The synthetic wheel event proves the production Qt handler; it does not prove trackpad delivery.
- Escape during a pan retains the view. Geometry edit cancellation has different behavior.
