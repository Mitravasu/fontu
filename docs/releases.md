# Release builds

The [release workflow](../.github/workflows/release.yml) builds, runs all five CTest suites, bundles dependencies, checks GUI startup, and uploads a package for each platform. A pushed `v*.*.*` tag publishes a GitHub Release only after every matrix job succeeds. The tag must exactly match `v` followed by the CMake project version. For example, version `0.1.0` requires `v0.1.0`.

Manual **Actions > Release > Run workflow** runs the same build, test, and packaging steps without creating a release, even when dispatched against a tag. The workflow must be on the default branch for GitHub to show the dispatch button. Use a manual run to verify Windows and Linux before tagging the first release. Download the three artifacts from that run and test them on machines without Qt installed.

| Package | Build environment | Distribution target |
| --- | --- | --- |
| `FontU-VERSION-macos-arm64.dmg` | macOS 15, AppleClang, Unix Makefiles | Apple Silicon macOS 12+ |
| `FontU-VERSION-windows-x86_64.zip` | Windows Server 2022, Visual Studio 2022 x64 | Windows 10/11 x64 |
| `FontU-VERSION-linux-x86_64.AppImage` | Ubuntu 22.04, GCC, Unix Makefiles | x86_64 Linux with glibc 2.35+ and an X11 display or XWayland |

All jobs use Qt 6.8.3. GitHub Actions are pinned to commit hashes. The Qt installer uses aqtinstall 3.3.0 and py7zr 0.22.0. Linux uses linuxdeploy `1-alpha-20251107-1` and its Qt plugin `1-alpha-20250213-1`; the packaging script verifies both downloads against SHA-256 checksums. Runner OS labels are explicit. GitHub maintains their compiler, CMake, Python, and GitHub CLI versions, so these images are not fully reproducible.

## Local release builds

The shared [CMake presets](../CMakePresets.json) build with tests enabled. Install Qt and add its `bin` directory to `PATH`; provide `CMAKE_PREFIX_PATH` when CMake cannot find it. On Linux:

```sh
cmake --preset release-unix -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build --preset release-unix --parallel 3
ctest --preset release-unix
python3 scripts/package_release.py
```

On macOS, use `release-macos` for configure, build, and test. That preset sets the macOS deployment target to 12.0 for Qt 6.8.3. A newer local Qt version may require a higher deployment target.

On Windows, install Visual Studio 2022 with its C++ tools and Qt's `win64_msvc2022_64` package:

```powershell
cmake --preset release-windows -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build --preset release-windows --parallel 3
ctest --preset release-windows
python scripts/package_release.py
```

The package script stages the CMake install in `build-release/package` and replaces `dist` with the resulting package. It assumes tests have already passed; the workflow enforces that order. Linux also needs `xvfb-run`, XCB/XKB libraries, OpenGL libraries, and network access for the pinned packaging tools. The script uses AppImage extraction mode during packaging and startup checks, so CI does not require FUSE.

macOS uses `macdeployqt` to copy Qt frameworks and plugins into the `.app`, restores and verifies ad-hoc signatures, then uses `hdiutil` to create a DMG with an Applications shortcut. Windows uses `windeployqt` to copy Qt DLLs and plugins, then copies Visual Studio's redistributable x64 CRT DLLs beside `fontu.exe` for portable use. Extract the complete ZIP before launching. Linux uses linuxdeploy with its Qt plugin to bundle Qt, the XCB platform plugin, and other deployable dependencies in the AppImage. Desktop integration files, the application license, and the bundled Noto Sans font's SIL OFL (`NotoSans-OFL.txt`) are installed by CMake; the macOS and Windows packages also copy both licenses beside the application.

Each job starts the deployed application with Qt environment variables and development paths removed. The application must stay running for five seconds. Linux checks the resulting AppImage under Xvfb. This catches immediate loader and platform-plugin failures, but hosted runners still have system libraries installed. It does not replace testing on clean target systems or checking every UI feature. The macOS package currently contains only an ARM64 executable; it does not support Intel Macs.

## Unsigned distributions

The macOS app has no Developer ID signature or notarization. Gatekeeper may block an Internet-downloaded copy. Users who trust the download may need to allow it in **System Settings > Privacy & Security** after attempting to open it. The workflow does not remove quarantine or bypass Gatekeeper.

The Windows executable has no Authenticode signature. SmartScreen may warn about an unknown publisher. Users must decide whether they trust the download. Signing and notarization require a separate credential-backed release setup.

The Linux package requires the host's kernel, graphics drivers, display services, and compatible glibc. It bundles Qt but cannot run on every Linux distribution. Mark it executable with `chmod +x FontU-*.AppImage`. On systems without FUSE, launch it with `--appimage-extract-and-run`.

## Publishing and reruns

For a future release, update the CMake project version, merge the workflow and changes, and complete a manual test run. Create and push the matching version tag only when ready to publish. No tag is created by this setup.

Only the final release job has `contents: write` permission. It downloads and checks all three expected packages, creates a draft with automatically generated release notes, uploads the packages, and publishes the draft after all uploads succeed. A failed publication step leaves a draft instead of a public partial release. A rerun can resume that draft and replace its assets. The workflow refuses to modify a release that is already public. Concurrent runs for the same ref are serialized without cancellation.

Deployment references: [Qt macOS deployment](https://doc.qt.io/qt-6.8/macos-deployment.html), [Qt Windows deployment](https://doc.qt.io/qt-6.8/windows-deployment.html), [linuxdeploy Qt plugin](https://github.com/linuxdeploy/linuxdeploy-plugin-qt), and [GitHub runner images](https://github.com/actions/runner-images).
