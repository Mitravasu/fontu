# Project creation

Startup shows the creation page with **Create project** and **Open existing project**. File **New** returns to this page while keeping the current project in memory. **Back to project** returns with completed drawings, selection, active glyph, undo history, and view intact. File actions cancel unfinished input as before.

## Workspace settings

Width and height accept 16 through 8192 whole font units. Units per em accepts 16 through 16384 whole units independently of the drawing size. The workspace is centered horizontally. Its minimum Y is one quarter of the height below zero, and its maximum Y is three quarters above zero. Positive Y points upward. The page displays the resulting span.

The examples set dimensions and guides together. They retain the selected characters. Editing dimensions or UPM does not rescale guide values.

| Example | Width | Height | UPM | Baseline | x-height | Cap height | Ascender | Descender | Side bearing |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Classic 1000 | 1000 | 1000 | 1000 | 0 | 500 | 700 | 750 | -250 | 100 |
| 2048 UPM | 2048 | 2048 | 2048 | 0 | 1024 | 1434 | 1536 | -512 | 205 |
| Roomy 1000 | 1200 | 1400 | 1000 | 0 | 500 | 700 | 750 | -250 | 200 |

Classic matches the previous workspace. The 2048 example uses scaled guide proportions. Roomy leaves space outside the design guides for accents and overshoot. These examples do not define glyph advance width or exported font line metrics.

Each of the five guides has an editable value and checkbox. A checked guide renders and participates in Geometry snapping. An unchecked guide does neither, but retains its value. All values must be finite, ordered as descender, baseline, x-height, cap height, and ascender, and inside the workspace. Equality is permitted. Grid snapping and the vertical origin line remain independent.

The **Side bearing** guide is a pair of vertical lines, each that many units in from the left and right workspace edges. Exported glyphs advance by the full workspace width, so drawing every glyph between these lines, including the widest such as m, w, M, and W, gives a monospaced font even spacing. The value must be from 0 to half the workspace width. With Keep proportions on, changing Width or UPM scales it; with it off, a narrower workspace limits it temporarily. Either line drags horizontally in the preview. When checked, the lines render on the canvas, labeled below the workspace, and participate in Geometry snapping, including where they cross the horizontal guides.

## Character coverage

| Choice | Exact coverage and order |
| --- | --- |
| Numbers | U+0030 through U+0039, digits 0 through 9. Ten glyphs. |
| Letters | U+0041 through U+005A followed by U+0061 through U+007A, A through Z then a through z. 52 glyphs. |
| Programming | U+0020 through U+007E, every printable ASCII character including space. 95 glyphs. |
| Custom | Unicode scalar values in first appearance order, with duplicates removed. |

Custom preserves whitespace, controls, combining marks, and supplementary-plane characters. It does not trim or normalize text. The single-line field accepts up to 200000 UTF-16 units. Pasted nonbreaking spaces and line separators retain their original code points. A combining sequence produces one glyph for each distinct code point. An emoji sequence produces glyphs for its constituent code points. Unpaired UTF-16 surrogates fail validation. Empty text fails validation. New projects allow at most 4096 distinct characters and 200000 UTF-16 input units. Existing files retain their previous parser limits.

Each glyph has an independent stable ID. New IDs run from 1 through the collection size. Labels preserve the exact selected scalar. Space displays as **Space** in the picker. Other invisible single-scalar labels display their Unicode code point. Imported multi-character labels remain unchanged. No additional space or .notdef glyph is inserted.

## Replacement and files

Create validates the complete candidate before asking to replace dirty work. Save, Discard, or Cancel uses the same gate as Open and Close. Cancel, canceled saving, and failed saving retain the accepted project and the creation draft. Successful creation starts a clean Untitled project with empty drawing histories. Drawing an edit marks it dirty.

Open is available on the creation page and in File. The parser validates the whole file before the dirty gate. A canceled dialog or invalid file preserves the current page and project. Close protects a dirty retained project even while the creation page is visible. Save actions and editor shortcuts are disabled on the creation page. The dirty replacement gate can still save the retained project.

Version 1 `.fontu` files now write an optional `definition.guideEnabled` object with boolean `baseline`, `xHeight`, `capHeight`, `ascender`, `descender`, and `sideBearing` fields, and an optional numeric `definition.sideBearing`. Missing flags default to true, so previous version-1 projects retain visible and snapping guides. A missing side bearing defaults to one tenth of the workspace width; one below 0 or above half the width fails validation. Present fields of the wrong type fail validation. Bounds, metric values, guide enablement, UPM, glyph order, IDs, labels, and drawings round-trip. In-memory histories remain glyph-local and reset after reopening.
