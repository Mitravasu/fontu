# Project persistence

Run `.agents/skills/verify-fontu/helpers/control-fontu persistence --evidence <new-directory>`.

The recipe drives the unchanged production main window through Qt events with the offscreen backend. It opens a temporary blank project, draws a rectangle in digit 0 and a cubic Pen stroke in digit 1, and saves the whole collection. It checks exact saved geometry and metadata, the active glyph, clean and dirty indicators, and Undo and Redo back to saved content.

It drives Open, Save, and Save As through Qt standard shortcuts. It clicks each File menu action and selects files through QFileDialog. Open with Cancel preserves current drawings and filename. Discard permits reopening the saved collection, clears selection and history, and restores its active glyph. Invalid JSON shows an error and preserves completed work. Close with Cancel keeps the production window open.

The initial Open confines writes to the evidence directory. It does not verify first Save's default folder in the production main window. `project_window_tests` verifies first Save, automatic suffix, exact dialog directory, default folder creation, canceled saving, failed saving, and Save on Close with a temporary preferred folder.

Require the first doctor record, final PASS, screenshots, and all cleanup flags in run.json. Native OS input, macOS global menu delivery, and native dialog appearance remain unverified. File actions deliberately cancel unfinished input and preserve completed drawings.
