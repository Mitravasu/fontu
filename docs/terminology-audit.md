# Terminology audit

The [typography domain reference](typography_domain_reference.md) is the authority for font terminology. This audit covers the current shape canvas, its source identifiers and UI labels, and the README and design documents. It changes language only.

## Changed terms

- `boundsAnchors` becomes `boundsSnapPoints`. These nine computed points are bounds corners, edge midpoints, and the center. They are snap targets, not named glyph anchors for accent attachment. Call sites and descriptions use the same term.
- The Snap menu says "To shape geometry" to identify its existing targets: shape outlines and bounds snap points.
- The window says "Shape canvas". Status instructions name shapes and distinguish cancelling an edit from clearing selection.
- The proposal uses "advance width" for glyph spacing, "on-curve nodes" for editable path points, and "ascender guide" and "descender guide" for drawing references. It distinguishes design guides from font-level line and clipping metrics.

## Retained terms

`Shape`, `ShapeKind`, and `ShapeId` describe editable drawing objects. A shape is not a glyph, and one path can contain several contours. `QPainterPath`, `path`, `bounds`, `geometry`, `Document`, `Canvas`, and `Viewport` remain accurate graphics and editor terms. The tools remain Select, Rectangle, and Ellipse, grouped as "Shape tools".

Document coordinates describe the current unbounded y-up drawing plane. The app has no units-per-em setting, so calling these coordinates font units would imply a scale it does not define. The grid and origin axes remain a grid and axes; they do not implement editable font design guides. Qt rectangle top/bottom methods retain Qt's geometric meaning, including in the y-up plane.

Zoom anchors and resize anchors describe fixed points of geometric operations. Screen anchors are not glyph attachment anchors. Resize handles are not Bézier control handles. Screen pixels, hit tolerances, and snap tolerances remain screen measurements.

`SnapOptions::geometry`, the geometry-snap setters, and the `snapToGeometry` action identifier describe the existing generic geometry query. They remain valid names. No snapping calculations, target order, tolerance, constraints, defaults, or gesture cancellation behavior change.

## Concepts awaiting implementation

Glyph identity, Unicode mappings, units per em, advance width, sidebearings, font-wide design guides, overshoot zones, named glyph anchors, components, kerning, and font-level line and clipping metrics have no current model or UI. Their names belong to the corresponding future features. Renaming existing shape bounds or axes would not implement them.

The proposal describes planned behavior. The README describes the implemented milestone. The domain reference remains unchanged.
