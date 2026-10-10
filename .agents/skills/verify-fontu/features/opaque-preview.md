# Opaque preview

Click Opaque preview in the toolbar or View menu to fill completed shapes solid black. Turn it off to return to the default X-ray view. Selected shapes retain their bounds and resize handles. Shape Builder hides its idle partition outlines in opaque mode, while hover and held gestures remain visible.

Run `.agents/skills/verify-fontu/helpers/control-fontu opaque-preview --evidence /private/tmp/fontu-opaque-proof-unique` from the repository root.

The recipe drives both toolbar toggle directions at 1100 pixels and both View menu toggle directions at 800 pixels. Pixel assertions check selected and overlapping black fills, absent internal source edges, a Shape Builder hole, and an unfilled open Pen path. Every toggle preserves project bytes, selection, tool, Undo and Redo availability, and dirty state. Reset view and glyph switching retain the preview mode. Standalone Canvas tests additionally exercise odd-even and winding holes.

Require the doctor record, final PASS, exit zero, screenshots, and cleanup flags in run.json. Inspect the opaque overlap, Builder hole, Pen stroke, and View menu screenshots. Native OS input, accessibility, macOS global menus, trackpad delivery, and desktop focus remain unverified.
