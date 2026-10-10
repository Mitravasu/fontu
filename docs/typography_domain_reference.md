# Typography Domain Reference — Foundations and Font-Creation Workflow

**Purpose:** Standalone typography knowledge base for the Font Studio project. Read this to understand how fonts are designed, measured, spaced, assembled, validated and exported. This is **not** the product roadmap.

**Companion:** `font_studio_proposal.md` contains the product vision, architecture and milestones.

This is a **domain reference** for understanding typography and the font-design workflow. It is separate from implementation planning. The editor can use it to inform design decisions, but it is not a sprint plan. Scope starts with **single-master, upright Latin outline fonts**; workflows for other scripts, italic, vertical type, and variable fonts require their own design later. A font may be started using sensible presets, but presets must not silently force the designer's proportions.

### 1 Three distinct kinds of measurements

Do **not** collapse these into one set of coordinates:

1. **Design guides**: visual reference positions used while drawing shapes (baseline, x-height, cap height, ascender/descender *guide positions*, overshoot zones).
2. **Glyph metrics**: advance width, outline bounds, sidebearings, anchors, components, kerning, Unicode and glyph names. These determine how individual glyphs participate in text.
3. **Font-level layout and clipping metrics**: typographic ascender/descender/line gap, horizontal header metrics, and Windows clipping ascent/descent. These affect default line spacing and clipping in consuming software and are **not automatically equal** to the tops and bottoms of representative letterforms. Export must map these explicitly according to OpenType specs.

### 2 Terminology and guide behavior

| Term | Meaning | Expected UI behavior |
|---|---|---|
| **Units per em (UPM)** | Font coordinate-system scale (often 1000 or 2048 units). The em square is a *design scale*, not the actual ink bounding box. | New-project setting; default 1000 UPM; define coordinate system independently of screen pixels. |
| **Origin** | `(0, 0)` at the glyph's horizontal origin on the baseline. | Display optional vertical origin line; consistent world-to-view transform. |
| **Baseline** | Alignment line on which most upright Latin letters sit. | Persistent y=0 guide, snap target. |
| **x-height** | Height of flat-topped lowercase letters such as `x`, measured upward from baseline. | Editable global guide; use `x`/`n` as visual checks, not as a forced bound. |
| **Cap height** | Height of flat-topped capitals such as `H`, above baseline. | Editable global guide; use `H` as visual check. |
| **Ascender guide** | Approximate top reached by lowercase ascenders such as `h`, `l`, `b`, `d`. | Editable design guide; may differ from cap height and OpenType line metrics. |
| **Descender guide** | Approximate bottom reached by descenders such as `p`, `q`, `g`, `y` below baseline. | Editable design guide, usually negative y; may differ from OpenType typographic descender. |
| **Overshoot** | Round or pointed shapes (`O`, `o`, sometimes `A`, `V`) often extend slightly beyond flat alignment lines to appear visually equal in size. | Optional overshoot bands around baseline/x-height/cap height; never auto-clip outlines to guides. |
| **Alignment zone** | Region around a nominal height/baseline representing acceptable overshoot (and useful for some hinting workflows). | Display as subtle guide band; full hinting is out of MVP scope. |
| **Small-cap height** | Reference height of purpose-designed small caps, distinct from ordinary lowercase x-height. | Optional custom metric for future support. |
| **Figure height / lining & oldstyle figures** | Numerals may use cap-height-like (lining) or varying lowercase-like (oldstyle) proportions. | Optional guide/preset once numerals are supported; do not assume one numeric height fits all designs. |
| **Accent / mark guides** | Placement reference for accents over/under letters; accents can exceed ascender/descender guides. | Future optional guides and anchors; no mandatory clipping. |
| **Italic angle** | Design angle of an italic/slanted font, measured from vertical according to font conventions. | Store font metadata for later; slanted sidebearing and spacing UX needs a separate design. |
| **Stem width / contrast** | Thickness of dominant vertical/horizontal strokes and their difference. | Style reference and optional measuring tool; not a hard global constraint. |
| **Counter / aperture / terminal** | Counter = enclosed or partially enclosed negative space; aperture = opening; terminal = stroke ending. | Important for drawing/Shape Builder examples and optical QA, not font metadata fields. |

**Illustrative 1000-UPM preset (not a universal standard):** baseline=0, xHeight=500, capHeight=700, ascender guide=750, descender guide=-250; nominal em span might be -250…750. Allow every value to be customized; do not assume all outlines fit inside the em square.

### 3 Horizontal metrics: design spaces, not only shapes

For a standard left-to-right glyph with outline x-min and x-max:

- **Advance width**: distance from this glyph origin to the next glyph origin, in font units.
- **Left sidebearing (LSB)**: x-min minus glyph origin (usually x-min if origin=0).
- **Right sidebearing (RSB)**: `advanceWidth - xMax`. It can be negative, as can LSB. These quantities are distinct from the bounding box.
- **Glyph width vs advance width**: ink extent `xMax-xMin` is **not** the same as layout advance; the latter includes spacing.
- **Kerning**: *pair/group-specific adjustment* to the spacing after the default advances/sidebearings, e.g., `AV`, `To`; it does not replace sound base spacing.
- **Tracking**: a layout/application-level uniform letter-spacing adjustment, not generally baked into each glyph as pair kerning.
- **Monospaced vs proportional**: monospaced families generally share an advance across most spacing glyphs; proportional fonts do not.
- **Space glyph**: has an advance width but typically no ink contour.

**UI contract:** Show a live advance-width boundary and LSB/RSB measurements, allow dragging the right advance boundary or typing numeric widths, and show both *glyph alone* and *glyph in a string*. Warn about unintended zero/negative advances, but allow deliberate overhangs. Define behavior when outlines are edited: adjusting geometry must **not silently rescale advance width**; allow user-selectable spacing behavior and an explicit recalculate action.

### 4 Font line metrics and OpenType correctness

The export layer must distinguish design guides from the actual fields it writes:

- `OS/2.sxHeight` and `OS/2.sCapHeight`: x-height and cap-height metadata.
- `OS/2.sTypoAscender`, `sTypoDescender`, `sTypoLineGap`: intended default typographic line spacing.
- `hhea.ascender`, `hhea.descender`, `hhea.lineGap`: horizontal header line metrics used by layout implementations; align with the chosen compatibility strategy.
- `OS/2.usWinAscent`, `usWinDescent`: **nonnegative clipping extents**, not simply the alphabet's ascender/descender design guides. They must account for actual outlines/marks to avoid clipping; review resulting platform behavior.
- `OS/2.fsSelection USE_TYPO_METRICS`: controls preference for typographic metrics in supporting layout systems; choose based on compiler/platform recommendations and validate across platforms.
- `head.unitsPerEm`, horizontal metrics (`hmtx`), Unicode mappings (`cmap`) and naming (`name`) must be generated correctly.

**Example:** cap height 700 / ascender guide 750 does *not* require `sTypoAscender=750`. The intended line spacing might be 800 / -200 / 0, and clipping ascent/descent may need to be larger due to accents. All are independently configured, validated, and exported. **Avoid exposing a confusing collection of OpenType raw fields on the initial drawing screen**: provide a clear `Font Settings → Advanced Metrics` screen with a summary of computed line height and clipping ranges.

### 5 Outline and glyph concepts

| Concept | Why it matters | Product consequence |
|---|---|---|
| Open/closed Bézier contours | Filled glyphs normally use closed contours; holes depend on contour semantics | Provide path closure, detect open contours at export, preserve cubic curves. |
| Winding and fill rules | Decide what's filled and what's cut out (`O`, `B`) | Preview using the selected rule and validate consistency after Shape Builder operations. |
| Overlaps and self-intersections | May cause unexpected fills/compilation issues | Non-destructive editing, explicit overlap cleanup during build, diagnostics. |
| Extremum nodes | Important for manageable curves and downstream processing/hinting | Optional highlight/add-extrema assistance later. |
| Optical compensation | Overshoot, stem adjustments, joins and negative space can be visually unequal despite geometric equality | Show multiple zoom levels and string preview; optional overlay comparisons; do not force symmetry. |
| Composite glyphs | Build `Ä` from `A` plus diaeresis, etc. | Component references, automatic updates, transform, cycle detection. |
| Anchors | Named attachment positions (`top` and `_top`) facilitate accent alignment/mark positioning | Add anchor model even if UI arrives after initial glyph drawing. |
| Unicode vs glyph name | A glyph can have no Unicode or multiple mappings; ligatures are not just a single typed character | Store glyph identity separately from Unicode mappings; support `.notdef`. |
| Ligatures and OpenType features | `fi` ligature, substitutions and mark positioning need shaping features | Defer feature authoring, but do not architect preview/export as if one Unicode character always maps to one glyph. |

### 6 Recommended designer workflow (what the user actually does)

**Step 1 — Define design intent, not files/layers.** Start project with family/style and optional preset: Latin upright proportional, 1000 UPM. Set editable proportions (baseline, x-height, cap height, ascender/descender guides) using a compact interactive diagram and numeric controls. Choose intended character coverage (start with uppercase/basic Latin; expand explicitly). Project creates a glyph grid automatically, including `.notdef` and `space`.

**Step 2 — Establish a visual system with control letters.** Before drawing A–Z, try `H`, `O`, `n`, `o`, `p` (or just `H`,`O` for uppercase-only). These probe flat vs rounded alignment, dominant stem thickness, curved bowls, x-height and descender. Compare `H` vs `O` for overshoot. Don't assume the numeric guide alone creates visual consistency.

**Step 3 — Draw with the three primary tools.** Make stems/bowls with shapes, draw irregular details with Pen, use Shape Builder to unite/subtract regions and form counters. The correct glyph is already active; saving/persistence is automatic. Undo and reopening preserve editor source objects.

**Step 4 — Space before detailed kerning.** Set advance and sidebearings using repeated strings such as `HHHH`, `OOOO`, `HOHO`, `nnnn`, `oooo`, `nono`. Show current glyph in neighbors: `HH[A]HH`, `OO[A]OO` (illustrative templates). Give feedback in design units and with optical preview—not arbitrary pixel positions. Establish default spacing *before* fixing special pairs.

**Step 5 — Preview real words continuously.** Use an editable sample-string panel that renders in-project outlines (and eventually shaped text). Compare uppercase/lowercase, rounded/flat, narrow/wide glyphs, mixed words and target sizes. Empty/unfinished glyphs must have an obvious placeholder, not silently fall back to system font.

**Step 6 — Reuse components and accents.** After base glyphs exist, construct accented letters as references with anchors rather than copy/paste static paths. Adjust accents separately and see composites update. Explicitly decompose if the user wants a unique outline.

**Step 7 — Kern exceptions only after spacing works.** Start with classic problem pairs `AV`, `To`, `Ta`, `Wa`, `Yo`; support pair adjustments, then kerning groups to reduce duplicate work. UI should make before/after effect visible. Do not auto-kern as a hidden export step.

**Step 8 — Validate, build and inspect.** Run checks for missing glyphs, malformed contours, unintended empty characters, metrics, clipping risk, unsupported features and export compiler diagnostics. Build TTF (later OTF) from UFO using an internal compiler adapter, open it in another app, and compare representative text at multiple sizes. A successful compiler exit alone is not sufficient quality assurance.

### 7 Low-friction design requirements

- **Guide overlay on by default:** labeled baseline/x-height/cap height/ascender/descender; optional overshoot zones; user-controlled opacity, toggles and snapping. Guides are font-wide, not duplicated per glyph/layer.
- **Metrics inspector:** per-glyph advance, outline bounds, LSB and RSB; editable numeric controls and draggable advance boundary. Show units.
- **Context without context-switching:** pin ghost outlines of reference characters (`H`, `O`, `n`, `o`) behind active glyph, with adjustable opacity; optional split preview.
- **Font Settings vs Glyph Settings:** font-wide guides and line metrics live in project settings; advance/outline/anchors live on individual glyph. Avoid accidental global modification from the canvas.
- **No invisible magic:** automatic snapping and auto-generated composite placement may be toggled/undone; warning state is visible.
- **Progressive disclosure:** advanced metrics, GPOS/features, kerning groups, hinting need not obstruct the novice drafting workflow.
- **Helpful defaults, editable values:** offer presets labeled as *examples*, not typography laws; no forced outline bounding box.
- **Offline ownership:** editable local native project with repeatable UFO exports; build outputs are artifacts, not canonical editable source.

### 8 Domain concepts and verification examples

These are examples of how to verify that typography concepts are represented accurately in software, without prescribing implementation order:

| When | Concrete new deliverable | Acceptance test |
|---|---|---|
| Foundation / canvas | Font units, baseline=0, guide-coordinate transformations, vertical metrics model | Pan/zoom does not alter font-unit positions; snap to guide behaves deterministically. |
| Shapes / pen | Optional overshoot zones, H/O/n/o reference overlays | `O` can extend beyond cap guide; guide overlay does not clip it. |
| Glyph model | Project-wide guide settings; glyph identity, Unicode, advance width, LSB/RSB | Edit `H`, save, reopen; widths and negative sidebearing cases round-trip. |
| Preview | Inline neighbor string + glyph outlines and advances | Editing `H` updates `HOHO`; incomplete glyph visibly differs from completed glyph. |
| Export technical spike | Project UPM, line metrics and cap/x-height metadata correctly serialized | Inspect TTF with fontTools; check `head`, `OS/2`, `hhea`, `hmtx`, `cmap`. |
| Typography polish | Anchors/components and pair kerning, then kerning groups | Adjust base `A`, composite `Ä` updates; apply `AV` kerning without mutating base advance. |
| Release QA | Multiple-size and platform rendering checks; clipping and overflow detection | Accents/descenders display uncropped in multiple apps; font installs and types correctly. |

### 9 Conceptual scope

**Must know and model from day one:** UPM, baseline, x-height, cap height, ascender/descender *design guides*, advance width, sidebearings, contours, glyph identity/Unicode, and separation of design metrics from OpenType line metrics.

**Add when producing real text:** overshoot preview, spacing tests, `.notdef`/space, line-metric export, component references and anchors, kerning pairs, contour validation.

**Defer:** full hinting/alignment zone authoring, variable-font masters, optical-size axes, right-to-left shaping, CJK/Indic specialist workflows, advanced OpenType features, oldstyle numeral production, automated kerning and auto-generated glyphs. Architecture must not rule out these features, but v1 need not implement them.

### 10 Sources and normative references

- [Glyphs Handbook — drawing, guides, spacing, kerning and reuse](https://handbook.glyphsapp.com/en/single-page/)
- [Glyphs Handbook — kerning and kerning groups](https://handbook.glyphsapp.com/kerning/)
- [Glyphs Handbook — components and anchors](https://handbook.glyphsapp.com/components/)
- [UFO 3 `fontinfo.plist` specification](https://unifiedfontobject.org/versions/ufo3/fontinfo.plist/)
- [UFO 3 GLIF spec — glyph advance, Unicode, contours, anchors, components](https://unifiedfontobject.org/versions/ufo3/glyphs/glif/)
- [Microsoft OpenType `OS/2` specification](https://learn.microsoft.com/en-us/typography/opentype/spec/os2)

---

**Design intent:** The font editor should teach and enforce *consistent mental models*, not silently make all the typographic decisions for the designer. Automate file/layer mechanics; expose artistic decisions through simple guides, live previews, and understandable controls.

**Success condition:** The project results in a dedicated, usable font design environment centered on shapes, a pen, and Shape Builder. The user spends their time designing glyphs, not manipulating document layers or shuffling exports among applications.
