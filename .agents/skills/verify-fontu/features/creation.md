# Project creation

Run `.agents/skills/verify-fontu/helpers/control-fontu creation --evidence <new-directory>`.

The recipe drives the unchanged production window with Qt events and the offscreen backend. It checks Create and Open at 800 by 600, rejects empty Custom, and opens an older version-1 file without guide flags. With Keep proportions on, UPM rescales the workspace and Height rescales the guides; an Ascender typed above the workspace is clamped to the top with a guide note (no error), and only an out-of-order Ascender blocks Create. Hidden editor Undo must preserve retained content.

It pastes a Custom set through the standard Paste shortcut. Supplementary-plane emoji, repeated scalars, space, LF, nonbreaking space, and U+2028 retain exact code points and first-appearance order. Nondefault 1200 by 1400 bounds (entered after 2048 UPM), edited metrics, and an unchecked x-height must reach the editor. Pixel checks in each guide's shared `guideColor()` prove hidden x-height absence and enabled cap-height rendering. Guide-preview dragging is covered by `project_window_tests`, not this recipe. Rectangle events ignore the disabled guide and snap to the enabled guide.

The glyph picker switches to the emoji, where drawing, Undo, and Redo are independent. Save As and Open preserve exact settings, characters, and drawings. Dirty Create Cancel and Back preserve filename, drawings, viewport, and history. Dirty Create Save writes the retained project before replacement. Discard creates a clean Untitled project with empty histories.

The recipe creates workspaces at both 16 and 8192 unit endpoints, fits them at 800 by 600, draws a rectangle, and undoes to clean state. The standalone project suite also checks presets and exact built-in character lists. Window tests cover canceled and failed saves, Close on the creation page, and first Save paths.

Require startup doctor, every claimed action record, final PASS, inspected screenshots, matching source hashes, and cleanup flags in run.json. Native macOS input, accessibility, global menus, trackpad delivery, and desktop focus remain unverified.
