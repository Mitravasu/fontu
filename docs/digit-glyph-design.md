# Digit glyph editing

## Problem

The editor previously had one Document and one completed-edit history. Glyph selection must retain each completed drawing and history while preventing unfinished input from changing another glyph.

## Usage

`Canvas canvas(digitGlyphs())` creates the explicit digits collection. `canvas.glyphs()` supplies ID and label metadata for the picker. `canvas.setActiveGlyph({42})` atomically selects a known identity. `canvas.document()` reads its drawing. Arbitrary collections use `std::vector<GlyphSeed>` with unique IDs and nonempty display labels. Empty collections, duplicate IDs, and empty labels throw `std::invalid_argument`. Glyph IDs have no Unicode meaning.

## Shape

`GlyphId` wraps a uint64 identity. Each private Canvas GlyphRecord holds its seed, Document, before/after history, and history cursor. A single active index selects the record. Public metadata is derived from records, so there is no mirrored list of glyph identities. Model the Domain keeps completed state together.

`setActiveGlyph` resolves identity before changing state. Unknown IDs return false. Re-selecting the active ID returns true without altering work. A real switch cancels the outgoing gesture and Pen draft before selecting the incoming record. It clears pointer, hover, snap marker, Space state, and cursor state, then publishes selection and history signals. Document selection remains per glyph. Tools, Snap preferences, and viewport stay shared.

The Glyphs dock holds a QListWidget whose rows derive from seeds. UserRole stores stable identity. The current row synchronizes from Canvas, and clicks restore canvas focus even when the row was already selected. The list supplies scrolling for larger collections. Laziness Protocol keeps the existing toolbar receiver and snapping code unchanged.

## Synthesis decision

Candidate A, `/private/tmp/fontu-digit-design-a.md`, placed editing records in one Canvas. Candidate B, `/private/tmp/fontu-digit-design-b.md`, proposed an external collection with non-owning session binding. An independent cross-judge scored corrected A 24 and final B 19 across isolation, arbitrary collections, interface depth, shared behavior, and diff size. A keeps activation atomic and avoids B's extra lifetime and binding protocol.

A's draft-preservation proposal was rejected because the chosen user flow cancels unfinished drafts when switching. B's clear separation of completed session state and transient Canvas state was retained. Stacked Canvas widgets were rejected because they allocate a widget per glyph and require toolbar rebinding. Both final candidates support arbitrary IDs without Unicode assumptions.

## Tradeoffs accepted

The collection is fixed during a session, and Canvas owns its editing state. This avoids deletion and rebinding policies before persistence exists. Every Document retains the same immutable default GlyphDocumentDefinition. Font-wide editable metrics and file persistence remain later work. One shared viewport keeps glyph comparisons at the same scale and position. Completed histories use existing whole-Document snapshots without a size limit.

## Verification

Event-driven tests exercise distinct paths, independent Undo and Redo, selection restoration, canceled Pen draft history, interrupted move and Draw gestures, unknown IDs, 80 arbitrary glyphs, scrolling, picker synchronization, and same-row focus restoration. Prove It Works requires a production-window recipe with real row clicks and toolbar actions in addition to regression tests. The Qt offscreen proof does not establish native macOS input, trackpad, or desktop focus behavior.
