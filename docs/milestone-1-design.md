# Milestone 1 design

The repository starts with a proposal and no executable code. This milestone implements only the canvas and rectangle/ellipse tools requested in the task.

## Usage and contracts

The canvas calls `document.addPath(makeShape(kind, bounds))`, then selects the returned stable ID. Rendering reads `document.shapes()`. A drag takes a copy of the original path and replaces the selected path with a translation or bounds transform of that copy. Each mouse update uses the original geometry, so rounding errors do not accumulate.

- `Shape { ShapeId id; QPainterPath path; }`: an editable path in document units. Ellipses contain cubic segments. Shape type is only needed when creating geometry.
- `Document`: owns ordered paths, monotonically increasing IDs, and optional single selection. It supports add, replace, select, delete, and topmost path hit testing.
- Geometry functions: construct paths from normalized drag bounds, translate paths, and map a path's bounds to resize bounds without flattening its curves.
- `Viewport`: owns scale and screen offset. It maps points both ways and zooms around a screen anchor.
- `Canvas : QWidget`: owns the document, viewport, current tool, and one gesture state. It paints the grid, paths, preview, and selection handles; translates mouse/keyboard events into document operations.
- `main.cpp`: creates the window and exclusive Select/Rectangle/Ellipse toolbar actions.

## Decisions that affect later work

Document coordinates use x right, y up, with an unbounded plane. Screen coordinates use x right, y down. The mapping is `screen = offset + (zoom*x, -zoom*y)`. Qt mouse positions and all screen sizes are logical pixels; Qt handles device pixel scaling. One transform defines painting, input, and tests.

QPainterPath is the in-memory geometry source for this milestone. It supports arbitrary line/cubic paths, so a later pen tool can add paths to the same document and reuse selection and transforms. It is not a proposed persistent font format. Saving, node identities, and font serialization will need their own design when requested.

Selection uses stable IDs, never vector indexes or widget pointers. Drawing order is document order, and hit testing searches in reverse. Handles and hit tolerance use screen pixels independent of zoom. Resizing scales the original path to normalized bounds, preserves cubic segments, and permits crossing the opposite handle without mirroring. Degenerate shapes are rejected. The milestone validates paths with positive bounds on both axes; accepting horizontal/vertical open pen paths later will require relaxing that validation, without changing storage.

## Alternatives and synthesis

Compare a custom QWidget with a QGraphicsView and scene adapter. Choose the QWidget because one viewport transform and one geometry owner suffice for this scope. Scene items would add a second geometry/selection representation to keep synchronized. Both independent design proposals and the read-only cross-judge favored the QWidget. Borrow the scene approach's cosmetic outlines and screen-size handles. The cross-judge also identified the pointer offset at resize-handle press; preserve it so an off-center press does not jump the geometry. No plugin tools, command framework, or render backend abstraction is needed.

## Implementation and verification sequence

1. Build the CMake targets, geometry, document, and viewport; verify path operations and coordinate mapping with Qt Test.
2. Add the grid, shape rendering, drag previews, and drawing tools; verify reverse drags and tiny-drag cancellation.
3. Add selection, movement, eight resize handles, and deletion; exercise overlap, curves, and transforms through real widget events.
4. Add anchored wheel zoom, middle-button and Space+drag panning, Escape cancellation, and reset view; verify editing after navigation.
5. Launch the native app, exercise the workflow, and document build commands and limitations.

## Scope limits

Single selection; axis-aligned bounds resizing; no rotation, undo, save/load, font metrics, pen tool, or Shape Builder. Escape cancels an active edit and restores its starting geometry. Changing tools cancels the active edit. No future feature directories or stubs.

## Verification record

Built on macOS with Qt 6.11.2 / AppleClang 21. Core and offscreen canvas suites pass. The native canvas suite passes and the rendered grid, rectangle, ellipse, and eight handles were inspected. The application bundle launches through Launch Services.

An initial native test run accessed an absent shape after a drag was cancelled. Waiting for native activation exposed dependence on desktop focus. Test windows now remain nonactivating and the tests inject Qt events directly; explicit focus-loss rollback has its own test. Repeated native runs pass with this arrangement. This verifies native rendering and widget behavior, not an OS-input walkthrough. Computer Use permissions were unavailable, so that walkthrough remains unverified.

## Snapping extension

The Snap menu has independent "To grid" and "To geometry" checkboxes, initially off. Canvas owns those two preferences. The menu synchronizes with setters, so either source can change them. Changing a preference cancels and restores an active draw/move/resize gesture; setting the existing value has no effect.

`gridStep(zoom)` is shared by painting and snapping. Drawing and moving target intersections of the grid actually shown at the current zoom. Side resize handles target the nearest grid line on their adjustable axis. All candidates use an eight-logical-pixel Euclidean capture radius. When both modes are on, the smallest correction wins. Within one point query, exact ties prefer the grid, then document order and bounds-anchor order. Equal move corrections retain the first moving bounds anchor. There is no persistent target lock or hysteresis.

Geometry queries first map each eligible path to screen space and use its bounds for rejection. Targets include the nearest point on the actual outline and nine bounds anchors: corners, edge midpoints, and center. Qt's `toSubpathPolygons` approximates curves in logical screen pixels for the outline query. The stored geometry remains the original cubic path. Tests compare a circular cubic outline query with its curve position to within 0.4 screen pixels at the tested size. This is an interactive approximation, not an exact cubic-distance solver, and it has no precision contract beyond Qt's flattening. Geometry targets include bounds corners and centers even when those points do not lie on the outline.

Drawing snaps both endpoints. Moving evaluates the nine bounds anchors after the raw translation and applies the smallest correction to the original path. It never snaps the arbitrary mouse grab point. Resize evaluates the dragged handle plus its original pointer offset; edge handles intersect outline segments with the fixed horizontal or vertical line through that handle. This keeps the nonadjustable axis and opposite bounds fixed. A press/release without movement never snaps a move or resize. The edited shape is excluded by stable ID. Every update derives from the original path snapshot. An amber ring marks the chosen target only during the gesture.

The helper scans document shapes and Qt-flattened outline segments for each query. This is sufficient for the current small session-only documents. There is no spatial index, caching, alignment guide system, or snapping framework.

Core tests cover line and curve targets, constrained intersections, exact ties, closest target with both options on, exclusions, adaptive spacing, and tolerance at multiple zooms. Actual Canvas event tests cover all four menu states, menu keyboard activation, draw/move/resize, off-center and stationary handle presses, outline snapping, constrained curve resize, shape exclusion, high-zoom tolerance, grid snapping after zoom/pan, and rollback when a preference changes.

The snapping extension builds and passes both CTest suites on Qt 6.11.2. The expanded native Cocoa canvas suite passes all 24 cases. An in-gesture render was inspected with its amber snap marker at the visible grid intersection. The sandboxed native attempt could not access a display; the same suite passed outside the sandbox. Native tests still inject Qt widget events, so OS-driven input remains unverified.
