# Shape Builder

Select filled closed shapes with Shift-click in Select or the platform Select All shortcut. Activate Shape Builder with its toolbar button or B. Click a highlighted region to extract it. Drag across regions to combine them. Hold Alt when pressing to remove the regions. Open paths and Stroke paths do not participate.

Run `.agents/skills/verify-fontu/helpers/control-fontu shape-builder --evidence /private/tmp/fontu-builder-proof-unique` from the repository root.

The recipe drives the unchanged production window with Qt events. It verifies Shift selection, both tool entry points, hover without editing, overlap extraction, whole-segment drag previews, one-step Undo and Redo, ordinary result movement, Alt subtraction, Escape cancellation, Save As, Open, and exact generated geometry after reopening. A curved merge retains an unrelated source path exactly. Screenshots include the normal 1100-pixel window and both Snap controls.

Require the doctor record, final PASS, exit zero, retained screenshots, and all cleanup flags in run.json. Inspect the hover, held preview, merge, subtraction, and curved outline screenshots.

Native OS mouse input, accessibility, global menus, trackpad delivery, and desktop focus are unverified. Holes, winding, coincident sources, disconnected interiors, pinched contours, tangent slivers, re-merging Builder fragments that share flattened curves, glyph switching, and group movement cancellation have core or widget regression coverage. The production recipe does not claim those cases. Qt Boolean results can flatten curves into editable line segments. Unconsumed original sources retain their exact path elements.
