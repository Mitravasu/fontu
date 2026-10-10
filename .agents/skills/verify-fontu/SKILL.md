---
name: verify-fontu
description: Verify fontu's Qt desktop canvas after changes to drawing, editing, navigation, or Snap controls. Drive the production window with Qt events and capture screenshots and assertions.
---

# Verify fontu

Read [the feature index](features/README.md), then the requested feature recipe. The app is a Qt 6 / C++17 desktop canvas. Its document is session-only. Keep the user's desktop instance open and untouched.

This skill verifies Qt event routing through the unchanged production `src/main.cpp` window with the offscreen backend. It injects mouse and keyboard events into real widgets, clicks toolbar buttons and the real Snap menu, and reads document/view state only for assertions and evidence. It does not verify native OS mouse input, accessibility, macOS global menus, trackpad delivery, or desktop focus behavior. Report this boundary with every result. Existing `canvas_tests` use standalone widgets and internal setters; use them as regression checks, not as proof of the production window.

## Launch

Run from the repository root. Baseline requirements are CMake 3.21+, Qt 6.4+ Widgets and Test, and a C++17 compiler. On this macOS checkout the Qt prefix is `/opt/homebrew/opt/qt`.

```sh
cmake -S . -B /private/tmp/fontu-baseline -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build /private/tmp/fontu-baseline --parallel
ctest --test-dir /private/tmp/fontu-baseline --output-on-failure
```

Use canonical absolute build paths on macOS; `/tmp` is a symlink and can produce invalid Qt AUTOMOC relative includes. Use a unique baseline build directory if that path is already owned by another run. Continue only after the baseline succeeds. For Linux or Windows, supply an installed Qt prefix and validate the helper there before claiming support. The helper is currently proven on macOS.

Launch and drive one isolated feature using the helper:

```sh
.agents/skills/verify-fontu/helpers/control-fontu drawing --evidence /tmp/fontu-proof-drawing-20261009-01
```

Choose a new evidence directory for every invocation. The helper refuses an existing directory. It builds in a private temporary directory, starts one owned process with `QT_QPA_PLATFORM=offscreen`, waits for the production window, runs doctor, drives the feature, and closes and reaps its process. It returns nonzero on failure. An initial `Doctor passed` record in `actions.jsonl` is readiness; a final `PASS:` record and exit zero are feature completion. A successful build alone is insufficient.

## Doctor

The helper's first operation is a read-only check within the instance it just started. It requires the visible `fontu • Shape canvas` production window, a Canvas central widget, the `Shape tools` toolbar, one Snap menu, and an empty document. The first screenshot and state record capture that baseline. `run.json` records the owned PID, executable, Qt backend, feature, and source hashes.

If anything looks wrong, start a fresh helper run and inspect its first record, `run.json`, and logs before further driving. This short-lived app has no remote control port or attach command. Do not attach to the user's desktop instance.

## Drive

`control-fontu` accepts `drawing`, `editing`, `navigation`, and `snapping`. Each invocation starts a fresh empty document and executes the corresponding recipe in the feature map. Toolbar handles are their exact action text. Snap handles are `snapToGrid` and `snapToGeometry`. Canvas coordinates are logical pixels relative to the widget. The helper uses the production window's default 1100 by 760 sizing.

For Snap runs, the helper makes the production menu local with `setNativeMenuBar(false)` because the offscreen backend has no OS menu. It clicks that menu's widget and its actions. This proves the production toggle connections and checks all four toggle combinations; it does not prove the macOS menu bar or snapped geometry.

For user reports involving OS-specific behavior, use Computer Use on a separately launched, positively identified disposable instance if native observation and input permissions are available. Verify ownership before any input. This skill's event proof cannot substitute for that check. If safe isolation or permissions are unavailable, record the unmet precondition and leave that entry point unverified.

## Evidence

Evidence stays in the exact directory passed to `--evidence`, outside the temporary build. Retain `run.json`, `build.log`, `app.log`, `actions.jsonl`, and numbered PNGs. Visible popup menus have additional `-menu.png` images linked in their state record. Failure diagnostics also survive. Each state record names its action, feature, backend, tool, selection, snap options, zoom, shape bounds, and screenshot. Drag previews capture the held gesture before release. Read-only state assertions accompany screenshots so a drawn shape is checked by its expected bounds and path type.

Require the initial doctor record, action records for every entry point claimed, the corresponding screenshots, the final `PASS:` record, and exit zero. Inspect screenshots for visible app identity and the claimed change. Report a skipped entry point separately. `snapping` currently proves menu toggles only. Geometry snapping, all eight resize handles, focus-loss cancellation, crossing resize anchors, overlap order, and wheel limits need additional production-window or native recipes before a full feature claim.

There are no persistence, export, network, or external-service features in this milestone. Closing the verification window discards its disposable document. The helper writes build/evidence files only; it uses no mock document and invokes no Canvas setters to prepare state.

## Cleanup

The helper closes its own production window on success or a drive exception. The controller reaps its owned child. On timeout or interruption it terminates that exact subprocess and escalates to killing that same child only if needed. It never kills by process name or touches another app instance. Its private temporary build directory is removed after success or failure.

After every run, require `child_reaped: true`, `scratch_removed: true`, and `evidence_preserved: true` in `run.json`; inspect retained `actions.jsonl` and PNGs. The controller has a 30-second drive timeout and 120-second build limits. Run in the foreground so it retains process ownership through cleanup. If the controller itself is forcibly killed, report cleanup as unresolved instead of trusting a stale PID file.

The optional baseline build is separate from helper scratch state. Remove only the baseline directory you created when finished. Evidence must survive cleanup.

## Helpers

The executable helper is `helpers/control-fontu`, a Python 3 controller. Its `--qt-prefix` option overrides the macOS default. `helpers/CMakeLists.txt` builds `helpers/driver.cpp` against the repository's production libraries. The driver compiles unchanged `src/main.cpp` under a renamed entry point and starts its exact production window. It contains no duplicate window implementation or test-only app endpoint.

Keep this map current with `/maintain-verification-skill` when controls or features change.
