# Build shapes from overlapping regions

1. Press V to use Select.
2. Click a closed filled shape.
3. Shift-click each additional shape, or use the platform Select All shortcut.
4. Click Shape Builder in the toolbar or press B.
5. Hover over the region you want to use. The region has a darker blue fill.
6. Click to extract one region, or drag across regions to combine them into one editable path.

To remove regions, hold Alt when you press the mouse button. The held preview is red. The operation stays fixed until release.

Release completes one edit. Undo restores the original shapes and selection. Escape cancels a held gesture. Completed results support Select movement, single-shape resizing, project saving, and reopening.

Remaining regions stay editable. Sources that contribute to none of the touched regions retain their exact path elements. Open paths and Stroke paths do not participate. Generated Boolean boundaries can replace curves with line segments. Shape Builder uses a scaled geometry copy to reduce visible flattening, but does not preserve original Bezier handles.

Several selected shapes move together. Resize handles appear only for one selected shape.
