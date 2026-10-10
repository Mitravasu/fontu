# Verify Pen paths

Run `.agents/skills/verify-fontu/helpers/control-fontu pen --evidence /tmp/fontu-proof-pen-01` from the repository root. Choose a new evidence directory. Require the fresh-window doctor record before interpreting the scenario.

The scenario drives these controls through the production window:

1. Click **Pen (P)** in the toolbar. Click two horizontal anchors. Verify that the document stays empty until **Enter** commits the exact line with stroke-only paint.
2. Press **V**, then **P**. Click three anchors to make a zigzag. Press **Enter** and verify the exact corner path.
3. Click the first anchor, then drag the second anchor downward. Capture the held drag preview. Add two clicked anchors and press **Enter**. Verify the exact mapped cubic controls, symmetric tangent, and final straight segment.
4. Start another path. Press **Escape**, then **Enter**. Verify that the cancelled draft inserts nothing.
5. Start a horizontal draft. Pan with the middle button and zoom with a wheel event. Press **Enter** and verify that the stored document points match the original draft.
6. Enable **To grid** through the toolbar's Snap group before calculating canvas coordinates. Click near a grid intersection and drag a later anchor toward another intersection. Press **Enter** and verify the snapped anchors and reflected tangent control.
7. Disable grid and enable **To shape geometry**. Click near both endpoints of a committed Pen line. Press **Enter** and verify the exact existing endpoints.
8. Start a draft, then click the Snap toolbar toggle to disable geometry snapping. Press **Enter** and verify that changing the snap option cancels the draft.
9. Click **Select (V)**. Click a committed zigzag stroke, drag its body, and press **Delete**. Verify the selected shape ID, exact translation, and removal.
10. Click **Pen (P)**. Try to close a two-anchor draft and verify that no shape commits. Cancel that draft with **Escape**.
11. Click three triangle anchors. Hover five screen pixels from the first anchor and inspect the closing ring and filled preview. Press there and verify that the filled closed path commits immediately with exact elements and becomes selected. Release and press **Enter**. Verify that neither action inserts another shape.
12. Press **V**, then **Escape** to clear selection. Click the triangle interior and verify its selected ID.
13. Press **P**. Make a three-anchor path with a dragged final tangent. Click the first anchor and verify the exact closing cubic, its outgoing tangent control, filled paint, and selection.
14. Make another triangle draft. Pan and zoom before clicking seven screen pixels from its mapped first anchor. Verify the exact closed document coordinates.
15. Start another three-anchor draft and click nine screen pixels from the first anchor. Verify that the document count stays unchanged. Cancel with **Escape**.

Require the final `PASS:` record and exit zero. Inspect the held curve preview, committed mixed path, Snap toolbar states, moved stroke, deletion, closing hover previews, filled triangle interior selection, curved closure, and navigation closure screenshots. Require the cleanup fields in `run.json` described by [the evidence contract](../SKILL.md#evidence).

## Coverage boundary

This scenario injects Qt events into the unchanged production window with the offscreen backend. It verifies toolbar and shortcut routing, open stroke-only paths, Enter commit, Escape cancellation, draft navigation, grid and committed-path snapping, snap-option cancellation, and stroke selection, movement, and deletion. It also verifies click closure through toolbar and shortcut selection, minimum anchor count, filled paint, immediate selection, interior selection, exact closing segments, and the screen-space radius after navigation. Path elements and paint in `actions.jsonl` preserve the asserted document coordinates and paint state.

The helper clicks the production Snap toolbar buttons. Changing a snap option cancels drafts. The scenario toggles snapping before calculating grid coordinates.

Native OS input, macOS global menus, trackpad delivery, and desktop focus behavior remain unverified. This scenario does not cover every curve direction, snapped curve geometry targets, all selection resize handles, or a draft during Space pan. **Enter** commits open stroke-only paths. Clicking within eight screen pixels of the first anchor closes drafts with at least three anchors. The first anchor has no tangent, and later anchor drags create symmetric handles.
