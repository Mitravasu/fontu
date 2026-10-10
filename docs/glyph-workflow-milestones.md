# Glyph document workflow milestones

The intended workflow starts with an Inkscape-like new-document screen. Designers choose a drawing size, typography guides, and character coverage before entering the glyph canvas. This plan records that intent and delivers the bounded canvas first so the settings screen can configure behavior that already works.

## Milestone 1. Bounded glyph workspace and metric guides

This slice introduces one `GlyphDocumentDefinition` owned by `Document`. Rendering, guide snapping, and tool constraints read that definition. The values remain fixed for this slice. The bounded workspace is implemented. The digit glyph picker is delivered as an early part of milestone 3. The configurable welcome screen remains planned.

The provisional example uses a 1000 by 1000 font-unit workspace. Its horizontal span is -500 through 500, and its vertical span is -250 through 750. The existing viewport preserves upward-positive font coordinates. The font coordinate scale is 1000 units per em. These two uses of 1000 describe different concepts.

The five horizontal design guides are baseline 0, x-height 500, cap height 700, ascender 750, and descender -250. The vertical origin line is x=0. These values come from the illustrative preset in [the typography domain reference](typography_domain_reference.md). They are provisional document defaults, not universal typography rules or OpenType line metrics.

The canvas shows a white document against a gray surrounding area. Startup and Reset view fit the whole document. Pan and pointer-centered zoom remain available around and outside the document. Guide labels retain their screen size. The existing **To geometry + guides** control enables both shape geometry and guide snapping. **To grid** remains independent. Both controls start off. Capture distance remains eight logical screen pixels.

Drawing starts outside the workspace do nothing. Rectangle and Ellipse endpoints stop at its edges. Moves preserve the shape and stop when its ink bounds reach an edge. Resizing bounds the resulting target rectangle. Pen anchors must start inside the document, and tangent drags preserve symmetric handles while limiting their extent. Invalid path admission leaves the current document unchanged. A rejected Pen commit retains its draft. Undo, Redo, and cancellation preserve these document bounds.

The document invariant concerns actual outline bounds, not a control-point rectangle. Some valid cubic controls can lie outside the ink bounds. Existing curves retain their topology and remain cubic. Guides never clip outlines. A round letter can overshoot cap height while remaining inside this workspace. The ascender and descender happen to coincide with its edges in this example. Future presets need separate workspace padding for accents, overhangs, or additional overshoot.

The workspace width is not an advance width. Glyph identity is now explicit and independent of Unicode. Advance width, sidebearings, and exported line metrics remain future data. The first definition does not invent those fields.

## Milestone 2. New-document screen and presets

Build the configurable entry screen after the fixed canvas behavior is verified. Provide meaningful size presets, including a 1000 by 1000 glyph-design example, with explicit width and height controls. Record approximately 5000 units per dimension as a proposed upper workspace limit. Validate that limit and its usability before making it a product rule.

The user described dimensions in pixels. The glyph editor stores font units, while the viewport converts them to logical screen pixels according to zoom. A 1000 by 1000 workspace does not request a 1000 by 1000 raster export. If a later workflow needs pixel dimensions, label that mode explicitly and keep raster output size separate from font scale.

Allow designers to select and customize baseline, x-height, cap height, ascender, and descender guides. Add optional overshoot bands and custom guides when their behavior is implemented. Keep guide visibility and guide snapping explicit. Distinguish font-wide guide settings from glyph-specific geometry and spacing, as the domain reference requires. The new-document screen creates the same typed definition the renderer and snapping already consume.

## Milestone 3. Character coverage and glyph selection

The first delivered selection slice uses an explicit digits 0 through 9 collection. A scrolling right-side Glyphs panel starts on 0. Each record owns its completed Document, selection, and edit history. Switching cancels unfinished drafts and gestures and retains the shared viewport, tool, and Snap settings. The collection API accepts arbitrary stable IDs and labels. There is no configurable character-set screen yet. See [the digit design](digit-glyph-design.md).

Offer sets for letters, numbers, programming characters, and custom characters. Define exact coverage for each preset rather than relying on ambiguous names. A programming set can include punctuation and symbols used in source code. A custom set accepts a deliberate list of characters.

Create a selectable glyph collection from the chosen coverage. Preserve each glyph's own drawing when the designer changes selection. Model glyph identity separately from Unicode mappings. Account for glyphs without Unicode, multiple mappings, `.notdef`, and the empty space glyph. Do not equate one typed character with one glyph in the domain model.

## Milestone 4. Project metrics and persistence

Move shared guides and UPM into font-wide project settings. Add glyph advance width and derived sidebearings without tying spacing changes to workspace size. Define how changing workspace bounds affects existing outlines before supporting that operation. Add save and reopen behavior that preserves guide settings, coverage, glyph geometry, and spacing. The broader [font studio proposal](font_studio_proposal.md) governs preview, components, kerning, and export beyond this flow.

## Current verification evidence

Core and canvas regression suites cover bounded path admission, degenerate Pen paths at edges, typography guide snapping, transformed edge snapping, bounded shape drawing, movement, resizing, Pen tangents, Undo, Redo, and navigation. The production-window `glyph` recipe exercises real toolbar controls and Qt events, captures fitted document screenshots, and asserts stored geometry. Independent review identified the edge-guide screen round-trip issue. Guide targets now originate in document coordinates before conversion.

Evidence runs use Qt's offscreen backend with the unchanged production main window. They do not prove native macOS input, trackpad delivery, or desktop focus behavior. Screenshot and action-log paths are recorded in [the durable task record](glyph-workflow-task.md).
