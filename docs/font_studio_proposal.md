# Font Studio — Product & Engineering Proposal

**Status:** Planning / implementation guide for Codex  
**Target:** Native desktop application, Qt 6 + C++  
**Primary platforms:** macOS and Windows (Linux desirable)  
**Working name:** Font Studio (rename freely)

The [typography domain reference](typography_domain_reference.md) defines the terminology used in this proposal. It distinguishes design guides, glyph metrics, and font-level line and clipping metrics. This proposal describes planned behavior; [README.md](../README.md) describes the implemented milestone.

## 1. Vision

Build a **purpose-built, low-friction font creation application** where a designer focuses on drawing glyphs rather than preparing layers, organizing Inkscape files, converting SVGs, or moving into another application to compile a font.

The finished experience should feel like a focused vector illustration tool inside a font editor:

1. Create or open a font project.
2. Choose a glyph from a grid or type a character to navigate to it.
3. Draw and edit with **primitives/shapes**, a **Bézier pen**, and an **interactive Shape Builder**.
4. See the glyph immediately in a live string preview, with font guides and spacing visible.
5. Move to the next glyph. The editor handles organization and persistence.
6. Export a usable TTF/OTF through an integrated build and validation flow.

**North star:** From blank project to an installable small Latin font without creating/naming a layer, manually exporting per-glyph SVGs, or opening a second GUI application.

This is **not** an effort to reproduce all of Inkscape, Illustrator, Glyphs or FontLab. The focus is excellent glyph creation and a complete single-user font workflow.

## 2. Core principles

- **Glyphs are first-class entities, not layers.** A glyph has outlines, components, advance width, Unicode mappings, and other metrics.
- **Drawing is the product.** Three tools are central: shape creation, pen tool, Shape Builder.
- **No compulsory layer management.** Internal objects/contours exist, but glyph navigation is the primary organizational unit. Advanced editing layers can be introduced only if justified.
- **Non-destructive until commitment.** Shape Builder must preview changes before commit and offer reliable undo. Keep source shapes where feasible; do not promise fully reversible booleans in v1.
- **WYSIWYG with font constraints.** Show baseline, x-height, cap height, ascender and descender design guides, origin, advance width, sidebearings, and consistent font units. Keep design guides separate from font-level line and clipping metrics.
- **Portability.** Prefer standards-based editable sources (UFO 3) and a versioned native project state for any richer editing data.
- **Predictability over feature count.** No hidden changes to outline geometry, no surprise rounding, explicit errors during export.
- **Test the graphics core independently of the UI.** Geometry and font serialization must be deterministic and unit-testable.

## 3. End-state product definition

### 3.1 Project workspace

- Create/open/save a font project, set family/style, units per em (UPM), design guides, and font-level line and clipping metrics.
- Glyph grid showing character, glyph name, completion/status, search and filtering.
- Single-click navigation between glyphs; optional next/previous shortcuts.
- Autosave with clear unsaved/error state; crash-safe file writes.
- Reopen the project with geometry, control handles, metrics and editing state intact.
- Named glyphs including non-Unicode glyphs; Unicode mapping for encoded glyphs.

### 3.2 Vector drawing canvas — highest priority

**A. Shape creation**
- Rectangle, ellipse, polygon, line (and optionally rounded rectangle).
- Click-drag creation with numeric transform panel; Shift for constraints, Alt/Option for center-origin creation.
- Translate, scale, rotate, duplicate; multi-select; smart snapping to guides, nodes and grid.
- Convert shapes to editable Bézier contours; preserve native shape parameters until conversion when useful.

**B. Pen / node tool**
- Create open and closed paths with line and cubic Bézier segments.
- Click for corner, click-drag for smooth control handles; close/open contours; join/split.
- Direct-select on-curve nodes and Bézier control handles; move nodes, adjust handles; smooth/corner conversion.
- Add/delete nodes, segment selection, snapping, keyboard nudging, zoom/pan.
- Clear distinction between editable source geometry and rendered output.

**C. Shape Builder — flagship interaction**
- Select overlapping closed shapes; tool displays a **hover highlight for the actual bounded region** under the cursor.
- Click/drag across regions to **union** those regions; use a modifier to **subtract/remove** regions.
- Show live preview, commit atomically, and undo in one step.
- Work with holes/counters (`O`, `B`, `P`), multiple overlapping shapes and disconnected results.
- Preserve sensible closed contours and fill rules in output; no visibly broken gaps from precision errors.
- For v1, explicitly constrain it to closed filled paths and document any curve-approximation behavior.

**Also essential:** selection, undo/redo, copy/paste, alignment, grid/guides, zoom, pan, reset view, and keyboard shortcuts.

### 3.3 Typography workflow

- Standard design guides: baseline, x-height, cap height, ascender guide, descender guide; editable in font settings.
- Per-glyph advance width and left/right sidebearing editing.
- Text preview renders current in-project outlines, **not a system font pretending to be the output**.
- Kerning pairs/groups later; basic manual kerning for production end state.
- Composite references (e.g. accents reused across characters), with cycle detection.
- Basic contour validation: open contours, self-intersections, degenerate segments, winding/fill, bounds, missing `.notdef`.

### 3.4 Export

- Integrated **Build Font** action produces installable TTF; OTF support is an end-state goal.
- No mandatory round-trip through FontForge or manual SVG exports.
- Clear compiler logs and user-friendly validation failures with glyph links.
- Export should work offline once dependencies are installed/bundled.
- Reproducible builds for identical project input and configured compiler versions.

### 3.5 Deliberate non-goals (initially)

- Full Illustrator/Inkscape vector-art feature parity.
- Photo editing, raster brushes, gradients, effects or color fonts.
- Variable font designspaces, interpolation, hinting UI, multi-master workflows.
- Advanced OpenType feature authoring, scripting marketplace or collaboration.
- AI-generated glyphs or auto-design (not needed to prove the workflow).

## 4. UX: principal screen and interactions

**Layout:**

- **Left:** collapsible glyph navigator with search and status.
- **Center:** vector canvas with font guides, outlines, controls, selected region feedback.
- **Top/adjacent:** compact toolstrip: Select, Node, Rectangle/Ellipse, Pen, Shape Builder, Hand.
- **Right:** context inspector for shape properties, anchors, transforms, spacing and advance width. Avoid a default giant Layers pane.
- **Bottom:** editable live string preview and optional validation/build status.

**Critical walkthrough:**

1. Open `A` in glyph navigator.
2. Draw two angled stems with Pen or polygons and draw a crossbar rectangle.
3. Select shapes; use Shape Builder to combine the desired regions; edit handles where needed.
4. Set advance width/sidebearings; preview `HAMBURGEFONTS`.
5. Jump to `B`, repeat. No manual layers, intermediate exports, or template cleanup.
6. Save, close, reopen; Build Font. Check the resulting TTF in a different application.

## 5. Architecture proposal

Use distinct modules with stable contracts so Codex can tackle work incrementally.

```text
font-studio/
  app/              # Qt window, toolbars, panels, shortcuts, dialogs
  canvas/           # Viewport, hit testing, event routing, rendering, guides
  tools/            # Select, primitive, pen, node, shape builder state machines
  geometry/         # Geometry model, Béziers, intersections/booleans, topology
  document/         # Font, glyph, contours, components, metrics, commands/undo
  storage/          # Native project serialization, autosave, migrations
  fontio/           # UFO import/export, compiler adapter, validations
  preview/          # Glyph layout, own-outline preview, kerning (eventually)
  tests/            # Golden geometry, serialization, E2E canvas/build tests
```

**Source of truth:** A font-domain document model containing glyphs, editable shapes, contours, components, metrics, and an undoable command history. Do **not** use `QGraphicsItem` or `QPainterPath` as the persisted document format.

**Rendering:** Qt Widgets + `QPainter`/`QPainterPath` are practical for an MVP; abstract viewport rendering so it can be optimized later. Use transform-invariant hit-test tolerances and explicit world/font-unit ↔ screen mappings.

**Geometry:** Native segments should represent lines and cubic Béziers, with stable shape/contour/node IDs. The rendering path is a projection of this model.

**Important boolean caveat:** Qt `QPainterPath::united/intersected/subtracted` is useful for prototypes, but Qt documents that Bézier curves may be flattened during set operations. It is **not automatically suitable as the final lossless font-outline Boolean engine**. Clipper2 is a robust C++ *polygon* clipping option; it also requires flattening curves and (if editable curves are desired) re-fitting. Evaluate curved-path Boolean approaches separately instead of silently accepting polygonized font outlines.

**Shape Builder architecture:**
1. Gather selected closed contours and their intersections.
2. Construct a planar arrangement / subdivided boundary graph or equivalent region map.
3. Determine bounded regions and holes based on fill rules.
4. On hover, map cursor to the actual region; render a temporary highlighted preview.
5. On add/remove gestures, compute kept-region membership and the resulting outer and inner contours.
6. Reconstruct valid boundaries, maintaining exact cubic subsegments where the backend supports them; else use a controlled, testable approximation path and surface that limitation.
7. Commit via one undoable document command.

**Font source/export:** UFO 3 / GLIF as the interchange target; GLIF represents outlines, points, anchors, components, advance widths, and Unicode mappings. Keep editor-specific parametric shapes, state and any unpublished edits in a versioned native document or UFO `lib` extension. Use `fontmake` via `QProcess` for a first font compiler, with captured diagnostics and pinned dependencies. Validate builds by opening output with a font parser and comparing outlines/metrics where feasible.

## 6. Incremental milestones for Codex

Treat each milestone as a separate detailed design + implementation task. Do **not** ask Codex to build the complete application in one prompt.

| Phase | Deliverable | Acceptance gate |
|---|---|---|
| 0. Foundation | CMake Qt 6 app, test setup, clean modules, project file skeleton | Launch + automated test command works on chosen OS |
| 1. Canvas | Pan/zoom, coordinate transforms, guides, selection, undo/redo | Stable view and selection; deterministic state after undo |
| 2. Shapes | Rectangle, ellipse, polygon, transforms, snapping | Create, edit, save/reopen native shapes |
| 3. Pen + nodes | Cubic pen, closed contours, direct node editing | Draw and revise `A`/`S`-like curves; reopen losslessly |
| 4. Boolean spike | Union/difference/intersection on test geometry; document curve fidelity | Golden tests: overlaps, tangencies, holes, near-coincident edges |
| 5. Shape Builder | Hover regions, additive/subtractive region gestures | Build `A`, `B`, `O` from overlapping shapes with one-action undo |
| 6. Glyph model | A–Z grid, glyph navigation, font design guides, advance widths/sidebearings | Design 10 glyphs without touching layers or external apps |
| 7. Preview | Actual in-project outline preview and basic text layout | Changes to a glyph visibly update sample words |
| 8. Export | UFO generation, fontmake integration, errors/validation, TTF | Install/use a generated font outside the editor |
| 9. Typography polish | Kerning, composites, improved editing and build diagnostics | Produce a small, usable Latin font end-to-end |
| 10. Shipping | Installers, preference storage, reliability/performance review | Reproducible release for target platform(s) |

**Order adjustment:** Build a minimal UFO → TTF *technical spike* early (after phase 0 or 1) with one hardcoded glyph to prove the pipeline. Keep the complete export UI in phase 8.

### Definition of Done (per milestone)

- Works from a fresh build with documented setup.
- Existing project files still load (or migration added).
- Unit tests for model/geometry; integration tests for user-visible behaviour.
- Undo/redo and save/load verified for new editing operations.
- Known limitations explicitly documented.
- No extra UI features unrelated to the milestone.

## 7. Main engineering risks and decisions

| Area | Difficulty | Why | Plan |
|---|---|---|---|
| Primitive shape creation | Low–medium | Mouse interactions, constraints, transforms | Qt + parametric objects; build early |
| Cubic pen / node editing | Medium–high | Bézier handles, smooth/corner rules, joins, UX polish | Explicit state machine and geometry tests |
| Basic union/subtract | Medium | Backend exists, but degeneracies and curve fidelity matter | Isolated geometry service; benchmark options |
| Illustrator-like Shape Builder | **High–very high** | Must find *individual overlap regions*, support hover/drag and reconstruct clean boundaries | Dedicated prototype and geometry golden cases |
| Font file compilation | Medium | Standards and external compilers reduce scope | UFO writer + fontmake adapter |
| Font spacing/kerning UX | Medium–high | Design ergonomics and glyph layout details | Start with advance/sidebearings; grow into kerning |
| Cross-platform packaging | Medium | Dependencies, compiler bundling, app signing | Defer until core experience is validated |

**Most significant scope trap:** Calling ordinary `union()` on all selected paths is *not* a Shape Builder. True Shape Builder requires the editor to identify and interact with the particular faces created by overlaps. Likewise, a boolean that converts every Bézier into hundreds of polygon vertices may render correctly but leave poor editable font outlines.

## 8. Focused proof-of-concept tests

Before committing to the Shape Builder's final backend, create visual/golden fixtures:

1. Two overlapping rectangles → three hoverable regions; union or removal as expected.
2. Two overlapping ellipses → hoverable lens; smooth output curves retained or approximation measured.
3. Nested circles → central hole and ring; correct winding and hit testing.
4. Crossed letter stems + crossbar → `A` topology with expected counter.
5. Near-tangent curves and almost-coincident edges → no slivers, crashes or invalid contours.
6. Undo/redo of region stroke → exact prior model restored.
7. Save/reopen → identical geometry and metrics.
8. Exported glyph → preserved advance width, fill/counters, installed-font appearance.

Track geometry node count, Boolean runtime, hover latency and curve approximation error if flattening is used.

## 9. Recommended initial scope cut

**v0.1 = three good drawing tools and three letters.**

- Qt desktop window and working glyph canvas.
- Rectangle/ellipse/polygon creation and selection.
- Cubic pen and node editing.
- Shape Builder limited to selected, closed, filled shapes.
- Three glyphs (`A`, `B`, `O`), persistent project, undo/redo.
- Hardcoded/experimental UFO-to-TTF command-line integration.

Do **not** build kerning groups, variable fonts, plugins, themes or 300-character coverage until this proves pleasant to use. The quality bar is: **drawing `A`, `B` and `O` feels easier than the current InkFont + Inkscape workflow.**

## 10. Instructions for Codex when beginning each phase

For each phase:

1. Read this proposal and inspect the actual current repository; do not assume components already exist.
2. Write a **short implementation design**: data structures, UI events, geometry assumptions, tests and explicit non-goals.
3. Identify dependencies and risk before coding, especially for geometry.
4. Implement the smallest usable vertical slice with automated tests.
5. Run the project, exercise its real UI workflow, and document limitations.
6. Keep architecture coherent but avoid premature generic frameworks.
7. Do not move to the next phase until the listed acceptance gate passes.

When uncertain, preserve **editable cubic font outlines** and a reliable **undo/save/load** workflow over superficial visual polish.

## 11. Reference material

- Previous prototype: [InkFont](https://github.com/Mitravasu/inkfont)
- [Qt `QPainterPath` documentation](https://doc.qt.io/qt-6/qpainterpath.html) — cubic rendering, union, subtraction, and documented flattening caveat
- [Clipper2](https://github.com/AngusJohnson/Clipper2) — C++ polygon boolean library; not a cubic-native editor by itself
- [UFO 3 GLIF specification](https://unifiedfontobject.org/versions/ufo3/glyphs/glif/)
- [fontmake](https://github.com/googlefonts/fontmake)
- [Fontra](https://github.com/fontra/fontra) — reference for end-to-end glyph editing architecture and interaction

---

**Success condition:** The project results in a dedicated, usable font design environment centered on shapes, a pen, and Shape Builder. The user spends their time designing glyphs, not manipulating document layers or shuffling exports among applications.
