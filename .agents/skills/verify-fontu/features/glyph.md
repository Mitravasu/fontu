# Bounded glyph workspace

Run `control-fontu glyph --evidence /private/tmp/fontu-glyph-proof-unique` from the repository root with a new evidence directory.

The doctor captures the unchanged production window with the whole document fitted. The recipe clicks the Rectangle, Ellipse, Pen, Select, Undo, geometry snapping, and Reset view controls. It rejects an outside drawing start, constrains Rectangle and Ellipse endpoints, snaps a Rectangle start to x-height, bounds movement and corner resize, rejects an outside Pen anchor, and limits a cubic tangent. It pans outside the document, zooms, checks unchanged geometry, verifies a transformed baseline hover target, and fits the whole document through Reset view.

Require the final PASS record, exit zero, screenshots, and cleanup flags in run.json. This proof uses Qt events on the offscreen production window. Native macOS mouse delivery, trackpad behavior, and focus remain unverified. Other legacy recipes first establish 100 percent through real resizing, Reset view, and middle-drag events for their existing screen-coordinate fixtures.
