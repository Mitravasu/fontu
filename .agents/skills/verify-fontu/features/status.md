# Verify bottom status hints

Run `.agents/skills/verify-fontu/helpers/control-fontu status --evidence /private/tmp/fontu-proof-status-01` from the repository root with a new evidence directory.

The recipe checks the initial Select instructions and selects Rectangle, Ellipse, and Pen through toolbar clicks and keyboard input. After the third Pen anchor, the bottom status bar offers click closure. Toolbar Undo removes that instruction when two vertices remain, and Redo restores it. Space pan and closure hover preserve the draft hint. Enter and click closure restore the initial Pen instructions. Escape, tool changes, and window resize cancel the draft and reset its hint.

The recipe checks that the visible status bar sits below the canvas and that its text fits at the default 1100-pixel width, at 1000 pixels, and at 800 pixels. The 800-pixel checks cover both Select and Pen closure instructions. Every action record contains `controlsHint`. Inspect the screenshots for readable bottom text, retained canvas zoom, and a drawing area without usage-tip text.

Require the initial doctor record, final PASS, exit zero, screenshots, and all cleanup flags in `run.json`.

This recipe injects Qt events into the unchanged production window with the offscreen backend. Native OS input, desktop focus, and other platforms remain unverified. Windows narrower than 800 pixels can clip the single-line status text.
