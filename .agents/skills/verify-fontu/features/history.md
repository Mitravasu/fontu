# Undo and redo

Undo and Redo restore completed rectangles, ellipses, open and closed Pen paths, movement, resizing and deletion. Buttons are disabled when their direction has no edit. Qt standard Undo and Redo bindings supply platform shortcuts. On macOS these include Command+Z and Command+Shift+Z. A real new edit clears the redo branch. Selection, navigation, tools, snapping, cancelled edits and unchanged drags do not add history.

During an unfinished Pen path, Undo removes one vertex and Redo restores it with its tangent handles. These operations use draft history until the path commits or cancels. After Undo removes every vertex, Undo stays disabled and Redo can restore the draft. Escape or a tool switch discards draft redo and resumes document history. Adding a new vertex replaces the draft redo branch. Enter or click closure commits the visible vertices as one document edit.

For other unfinished gestures, Undo or Redo cancels the gesture before restoring a completed edit. Snapshots restore shape order, IDs, paths, paint and selection together. View and tool settings remain current. History is session-only and has no size limit.

## Driving it with control-fontu

Run `.agents/skills/verify-fontu/helpers/control-fontu history --evidence /private/tmp/fontu-proof-history-01` from the repository root with a new evidence directory.

The recipe clicks disabled buttons, draws a rectangle, exercises every Qt Undo and Redo binding for the current platform, and checks exact shape identity and path restoration. It then moves, resizes and deletes the shape, reverses those edits through the toolbar, preserves redo after an unchanged drag and Reset view, and replays the edits. A new ellipse invalidates the old redo branch.

For Pen drafts, the recipe uses toolbar and standard shortcuts to remove and restore vertices, including a dragged tangent. It checks the zero-vertex boundary, preserves redo through pointer movement, and commits an exact curved prefix. It removes a held anchor, replaces the redo branch with a new vertex, and commits that exact open path. A restored draft closes to an exact filled curve, which completed-path Undo removes as one edit. Escape and a toolbar tool switch each discard zero-vertex draft redo and restore document button availability.

Require the doctor record, final PASS, exit zero, screenshots, and all cleanup flags in run.json. Each state record includes canUndo and canRedo. Standalone canvas tests additionally cover a held tangent during vertex Undo, external focus-loss cancellation, held draw and move history, tiny draws, empty deletion, pan, zoom and snap settings.

## Verification boundary

This recipe injects Qt events into the unchanged production window with the offscreen backend. Native OS keyboard delivery, desktop focus and other platforms remain unverified. There are no text-entry controls in the current window. The recipe covers the current platform bindings only.
