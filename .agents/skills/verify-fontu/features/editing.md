# Selecting and editing

Users select one shape, move it by dragging its filled interior or stroke, resize it with handles, and delete it with the keyboard.

## Sub-features

- `edit-select` chooses Select through the toolbar or V and selects a shape by clicking its filled interior or near its stroke.
- `edit-move` moves the selected shape with a filled-interior or stroke drag.
- `edit-resize` resizes from eight corner or edge handles for bounds with nonzero width and height. A horizontal path has only left/right handles; a vertical path has only top/bottom handles.
- `edit-clear` clears selection with Escape or an empty-canvas click.
- `edit-delete` deletes selection with Delete or Backspace.
- `edit-cancel` rolls back an unfinished move or resize with Escape, tool change, or focus loss.

## How to get to it (user POV)

- Click `Select (V)` or press V, then click a shape.
- Drag a shape's filled interior or stroke to move it, or drag a visible square handle to resize it.
- Press Escape while idle or click empty canvas to clear selection.
- Press Delete or Backspace to remove the selection.
- Press Escape during an edit, change tools, or move focus away to cancel an unfinished geometry edit.

## Driving it with control-fontu

Preconditions:

- Run from the repository root. Use a new evidence directory and require fresh-window doctor success.

Run `.agents/skills/verify-fontu/helpers/control-fontu editing --evidence /tmp/fontu-proof-editing-01`.

- Draw a rectangle from `(100,100)` to `(220,200)` with the toolbar tool.
- Click `Select (V)`, then drag its interior from `(150,150)` to `(180,170)`. The selected shape moves to bounds `[130,120,120,100]`.
- Drag its bottom-right handle from `(250,220)` to `(280,240)`. Bounds become `[130,120,150,120]`.
- Press R, then V, then Escape. The selection clears. Click `(180,170)` to select the shape again; the screenshot shows handles.
- Press Delete. The canvas becomes empty. Draw another rectangle and press Backspace. The canvas is empty again.
- Require the final `PASS:` record, exit zero, visible state changes, and completed cleanup in `run.json`.

For full editing coverage, add explicit production-window steps for the other seven handles, anchor crossing, off-center handles, overlapping-shape selection order, empty-canvas deselection, and each edit cancellation entry point. These checks are mapped but not implemented by this command. Existing tests are useful regression evidence only.

The [Pen recipe](pen.md) separately proves open-path stroke selection, movement, and deletion, plus closed-path filled-interior selection. Its committed horizontal line shows the two endpoint handles. Endpoint resizing still needs a production-window recipe.

## Gotchas

- A newly drawn shape is already selected. Switch to Select before moving or resizing.
- Handles have fixed screen size. Use the rendered handle center after any zoom or pan.
- Clicking an overlap selects the most recently drawn eligible shape.
- There is no undo. Use only the helper's disposable document.
