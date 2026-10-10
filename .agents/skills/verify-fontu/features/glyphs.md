# Digit glyph selection

Run `control-fontu glyphs --evidence /private/tmp/fontu-digits-proof-unique` with a new evidence directory.

The doctor checks the unchanged production main window. The recipe clicks actual glyph rows and toolbar actions. It checks digits 0 through 9, initial 0 selection, empty incoming glyphs, distinct Rectangle and Ellipse drawings, independent Undo and Redo, preserved completed selection, canceled Pen drafts, rollback of an unfinished move, focus restoration, and scrolling at 800 by 500. Returning to a glyph preserves its completed paths.

Require final PASS, exit zero, PNGs, and all cleanup flags in run.json. This proof injects Qt events through the production window using the offscreen backend. Native macOS input, trackpad, and desktop focus remain unverified. Arbitrary 80-entry collection scrolling and non-row IDs have standalone widget regression coverage.
