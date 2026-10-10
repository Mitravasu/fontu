# Glyph workflow task

The first slice gives the glyph editor a bounded drawing document, visible typography guides, and guide snapping. The next section delivers the digit glyph picker. The configurable welcome screen remains planned.

## Feature playbook

1. `how` over the affected subsystem. Complete through source tracing and independent design explorers.
2. `architect` for parallel design exploration. Complete. Two independent candidates produced isolated design notes, followed by an independent cross-judge.
3. Write the throughput checkpoint as four todo items.
   - Blocking first steps. Inspect working tree, typography docs, current tool boundaries, and snapping before choosing the document data shape.
   - Independent workstreams. Read-only design exploration runs independently. One owner implements the coupled canvas and snapping changes.
   - Shared mutable state. One implementation owner writes the source tree and controls the isolated Qt verification process. Candidate documents have distinct paths.
   - Smallest safe decomposition. One owner maintains coordinate invariants across metrics, rendering, snapping, drawing, and editing.
4. Delegate code-writing to a subagent using your configured feature model. Parent delegated this slice to its implementation owner. Independent review follows.
5. Verify on the matching surface. Complete. Fresh core and canvas CTest suites pass. The glyph and snapping production-window recipes pass with retained screenshots and action assertions.
6. Rebase into small, ordered commits. Stack follow-ups. skip: User requested no commit or PR and preserving existing work.
7. If the design is contested, `interrogate` before shipping. skip: No shipping requested. Independent review remains required.
8. Run **Opening a PR**. skip: User requested no PR.

## Architect phases

1. Ground. Viewport already maps font coordinates with y upward. Document stores paths and selection. Canvas owns gestures and whole-document undo history. snapPoint supplies shared snapping for drawing, Pen, moving, and resizing.
2. Sketch. Complete. Document-owned definition and Canvas-owned workspace candidates were compared.
3. Agree. Proceed autonomously after design synthesis.
4. Implement. Complete. One document-owned definition drives rendering, guide targets, and geometry constraints.
5. Scrap. Revisit the design if boundary rules require scattered exceptions.

## Arena phases

1. Frame. Compare ownership, shared values, typography correctness, boundary coverage, and diff size.
2. Fan out. Two read-only candidates write separate temporary design notes.
3. Cross-judge. Complete. Independent judge scored Document ownership 22 and Canvas ownership 19 against the stated rubric.
4. Pick. Complete. Document ownership gives the invariant a natural admission boundary and avoids duplicated rendering/snapping values.
5. Graft. Complete. Use actual ink bounds from the alternate candidate, rather than requiring existing cubic controls to stay inside.
6. Verify. Complete. Regression and production-window proofs pass.

## Decisions and review

Model the Domain changed the data shape to a single Document-owned GlyphDocumentDefinition. Laziness Protocol kept snapping in the existing geometry toggle and preserved the existing viewport. Prove It Works required production-window assertions and direct screenshot inspection. Test Behavior, Not Implementation led to event-driven boundary tests with concrete resulting paths. Separate Before Serializing Shared State gave each design candidate its own file and kept one source-tree writer.

The two design notes are `/private/tmp/fontu-design-a.md` and `/private/tmp/fontu-design-b.md`. Initial design exploration preserved Reset at 100 percent. Independent review and product judgment changed Reset to fit the complete bounded document. Regression fixtures establish their old 100 percent coordinate view through real resize, Reset, and pan events, without a production test API.

Independent review reproduced failed edge snapping after floating-point screen round trips. Guide and grid targets now originate and validate in document coordinates before conversion. The fresh reviewer probe passes 100 varied transforms. No remaining correctness findings were reported. Comment review found no added comments or suppressions and requested no changes.

A first production glyph proof exposed integer-event-coordinate differences at fitted zoom. The recipe now asserts the concrete document boundary reached, while accepting the interior anchor's actual event conversion. Direct screenshot inspection then found header and ascender-label overlap. Their screen spacing was corrected before the final proof.

## Evidence

`cmake --build /private/tmp/fontu-glyph-baseline --parallel` and `ctest --test-dir /private/tmp/fontu-glyph-baseline --output-on-failure` pass both suites.

The final glyph proof is `/private/tmp/fontu-glyph-glyph-final-20261009-01`. Its `02.png` shows the fitted empty document and all guide labels. Its `41.png` shows the final fitted document after bounded cubic Pen drawing, navigation, and reset. `actions.jsonl` ends with PASS and `run.json` confirms child reaping, scratch removal, and preserved evidence. The snapping regression proof is `/private/tmp/fontu-glyph-snap-20261009-01`, with 29 screenshots and the same successful cleanup flags.

The proofs inject Qt events into the unchanged production main window using the offscreen backend. Native macOS mouse input, trackpad events, and desktop focus remain unverified. The configurable welcome screen, glyph picker, persistence, spacing metrics, and font export are outside this slice. No commit, rebase, or PR was made.

All legacy production recipes also pass. Drawing, navigation, editing, and status use `/private/tmp/fontu-glyph-drawing-20261009-01`, `/private/tmp/fontu-glyph-navigation-20261009-01`, `/private/tmp/fontu-glyph-editing-20261009-01`, and `/private/tmp/fontu-glyph-status-20261009-01`. Snapping uses `/private/tmp/fontu-glyph-snap-20261009-01`. Pen and history use `/private/tmp/fontu-glyph-pen-final-20261009-01` and `/private/tmp/fontu-glyph-history-final-20261009-01`. Earlier Pen and history runs failed because their subsequent Reset actions changed the fixture view. Their real-event setup now restores the expected 100 percent coordinate view before continuing existing assertions. Every successful run confirms owned-process cleanup.

The final widget suite also checks that Pen closure hover outside the document shows no actionable closure cursor, ignores the outside press, and closes successfully from inside the edge.

## Digit glyph flow

This slice adds an initial explicit collection of digits 0 through 9 and a right-hand glyph picker. Glyph IDs are independent of display labels and Unicode. Each glyph owns its completed drawing, selection, and completed-edit history. Switching cancels unfinished Pen drafts and pointer gestures. Viewport, tools, and snapping stay shared. Glyph 0 is selected at startup.

### Feature playbook

1. `how` over the affected subsystem. Complete. Independent source tracing confirms Document owns geometry and selection, while Canvas owns history and drafts.
2. `architect` for parallel design exploration. Complete. Compare Canvas-owned records against external editing sessions. The final independent judge prefers Canvas ownership 24 to 19.
3. Write the throughput checkpoint as four todo items.
   - Blocking first steps. Complete baseline and trace Document, history, gesture rollback, and production-window composition before choosing ownership.
   - Independent workstreams. Read-only design candidates write separate temporary files. Source edits and Qt process control have one owner.
   - Shared mutable state. One owner changes the coupled Canvas and picker code. Active selection is read from Canvas.
   - Smallest safe decomposition. One implementation owner keeps switching, document history, toolbar refresh, and gesture cancellation consistent.
4. Delegate code-writing to a subagent using your configured feature model. Parent delegated to this single mutation owner. Independent review follows.
5. Verify on the matching surface. Complete. Fresh core and canvas suites pass. The production glyphs recipe passes with real picker clicks, toolbar actions, and retained screenshots.
6. Rebase into small, ordered commits. Stack follow-ups. skip: User requested no commit or rebase.
7. If the design is contested, `interrogate` before shipping. skip: Design comparison and independent review agree on the corrected record shape. No shipping requested.
8. Run **Opening a PR**. skip: User requested no PR.

### Architect and arena checkpoints

Ground is complete. Sketch compares two distinct ownership structures. Agreement proceeds autonomously. Implementation starts after synthesis, then production evidence checks the chosen boundary. Scrap applies if glyph switching requires duplicated state or special cases. Arena phases are Frame, Fan out, Cross-judge, Pick, Graft, and Verify. The rubric covers state isolation, arbitrary collections, interface depth, existing tool behavior, and diff size.

The fresh baseline build and both CTest suites pass in `/private/tmp/fontu-digits-baseline`.

The synthesized design is [digit-glyph-design.md](digit-glyph-design.md). Independent review found one same-row click focus defect. `itemClicked` now returns focus even without `currentItemChanged`, and a real row-click plus R-key regression covers it. New collection tests and both fresh CTest suites pass. Production proof passes.

### Digit flow evidence

Fresh `cmake --build /private/tmp/fontu-digits-baseline --parallel` and `ctest --test-dir /private/tmp/fontu-digits-baseline --output-on-failure` pass both suites after the focus fix. `git diff --check` reports no whitespace errors.

The digit production proof is `/private/tmp/fontu-digits-proof-20261009-02`. Its 32 screenshots cover initial 0, same-row keyboard focus, separate drawings, per-glyph Undo and Redo, completed selection restoration, canceled Pen drafts, interrupted move rollback, and scrolling at 800 by 500. Direct inspection of `15.png` confirms selected digit 1 with its retained ellipse. `32.png` confirms selected digit 0 with its retained rectangle and visible scrollbar. The action log ends in PASS, and run.json confirms exit zero, child reaping, scratch removal, and preserved evidence.

The workspace regression proof is `/private/tmp/fontu-digits-workspace-20261009-01` with 41 screenshots. History and Pen regression proofs are `/private/tmp/fontu-digits-history-20261009-02` with 94 screenshots and `/private/tmp/fontu-digits-pen-20261009-01` with 90 screenshots. All end in PASS with all cleanup flags true. Legacy recipes retain their original 1100-pixel canvas fixture by increasing window width for the new dock before establishing 100 percent view. The new glyph proof uses the actual default window and smaller-window layouts.

The first digit proof failed an incorrect recipe assumption that ten 48-pixel rows overflow at a 600-pixel window height. They fit. The corrected 500-pixel fixture demonstrates scrolling. The first history run reached a legacy Pen input at x=960 beyond the newly narrower canvas. The legacy fixture now preserves its original canvas area. Both failed runs retained evidence and cleaned up their processes. No production workaround was needed.

The proofs use Qt event injection into the unchanged production main window with the offscreen backend. Native macOS input, trackpad delivery, and desktop focus remain unverified. Arbitrary 80-glyph collection scrolling and stable non-row IDs have event-driven widget coverage. The collection remains in memory with fixed contents. No splash screen, configurable coverage, persistence, or export was added. No commit, rebase, or PR was made by this owner.
