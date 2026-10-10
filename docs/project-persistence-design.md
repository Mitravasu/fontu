# Project saving and reopening

The File menu saves the whole glyph collection and reopens it as one editable project. The `.fontu` format stores vector drawings and project settings. It is separate from font export.

## Content and ownership

`ProjectSnapshot` contains a shared `GlyphDocumentDefinition`, ordered `ProjectGlyph` records, and an active glyph ID. Each record contains a stable ID, label, and ordered shapes. Shapes retain their identity, paint mode, fill rule, and Move, Line, or Cubic commands.

Canvas continues to own completed drawings and per-glyph edit histories. Its snapshot reads the completed document before an unfinished Move or Resize. Restoring a validated snapshot replaces the collection, clears selections and histories, restores the active glyph, rebuilds the picker, and fits the document.

`project.cpp` owns JSON validation and atomic file I/O. `ProjectWindow` owns the filename, saved content baseline, dialogs, File actions, dirty indicator, and close gate.

## Design comparison

Two independent read-only candidates traced the current ownership before implementation. Candidate A retained Canvas ownership and added a typed snapshot plus file window. Candidate B moved glyphs and histories into an external Project session injected into Canvas.

Candidate A was selected. Candidate B would require a new lifetime contract and move the existing editing owner without changing the requested saving behavior. Its distinction between completed content and transient input was retained.

Model the Domain produced one typed snapshot rather than exposing Canvas records. Boundary Discipline placed untrusted-file checks in the codec. Laziness Protocol kept histories with their existing owner. Prove It Works required actual File actions and dialogs in the production window. Test Behavior, Not Implementation produced concrete geometry and failure-preservation assertions.

## Format and validation

The readable JSON envelope has `format` equal to `fontu` and `version` equal to 1. IDs are decimal strings, so uint64 identities retain their precision. The maximum uint64 value is reserved. Shape IDs are positive. Existing zero glyph IDs remain supported.

The parser rejects malformed JSON, unsupported versions, invalid or duplicate IDs, absent or invalid fields, empty collections, malformed commands, unknown paint or fill rules, invalid metrics, and outlines outside the document. Bounds and coordinates must be finite and within one billion units. Bounds have positive extent. Typography guides are ordered inside the document, and units per em is positive. Files are limited to 64 MiB. Unknown fields are accepted. Missing active glyph metadata selects the first glyph.

The complete file is decoded before any project replacement. `QSaveFile` writes and atomically commits the selected path. A failed write changes neither the current filename nor its saved baseline. Restored shape allocation skips occupied IDs, including after an imported ID approaches the reserved maximum.

## File flow

Open, Save, and Save As use Qt's standard platform shortcuts and native-capable QFileDialog. First Save and Save As start in the platform Documents directory's `fontu` subfolder. The first Save action creates that folder before showing the dialog. Canceling can leave the empty folder. Failure to create it shows an error. Qt adds the `.fontu` suffix and handles overwrite confirmation.

Subsequent Save uses the current filename. Save As starts at that filename. Open starts in its directory or the default folder's existing parent. The filename and an asterisk appear in the title. The status bar shows Untitled, Unsaved changes, or Saved.

Dirty means the current canonical content differs from the successful saved baseline. Every glyph's IDs, labels, order, drawings, and project definition participate. Active glyph selection, shape selection, tools, snapping, pan, zoom, Pen drafts, and undo history do not. Undo or Redo back to saved content restores the clean state. Active glyph metadata is saved opportunistically with the next Save.

Open first decodes the candidate file, then asks Save, Discard, or Cancel if current content is dirty. Close uses the same gate. Canceled or failed saving prevents replacement or closing. Invalid Open leaves completed drawings, histories, filename, and baseline intact.

File actions cancel unfinished Pen drafts and roll back unfinished pointer edits, consistent with glyph switching and focus loss. They preserve every completed drawing. The File action tooltips state this policy. Finish a Pen path with Enter or closure before saving it.

## Boundaries

Projects preserve shared bounds, guides, and units per em. This slice adds no metrics editor, character-set creator, spacing data, autosave, recent-file list, startup screen, or font export. Undo history is in memory and starts empty after reopening.

Offscreen Qt proofs establish application event routing and dialog behavior. They do not establish native macOS menu delivery, OS input, trackpad delivery, or desktop focus.
