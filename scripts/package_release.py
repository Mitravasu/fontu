"""Package the installed Release build. Run after CTest succeeds."""

import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import urllib.request


ROOT = Path(__file__).resolve().parents[1]
# The bundled reference-glyph font's SIL OFL must ship with every package.
FONT_LICENSE = ROOT / "resources/fonts/OFL.txt"
FONT_LICENSE_NAME = "NotoSans-OFL.txt"
LINUX_TOOLS = {
    "linuxdeploy": (
        "1-alpha-20251107-1",
        "c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d",
    ),
    "linuxdeploy-plugin-qt": (
        "1-alpha-20250213-1",
        "15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724",
    ),
}


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def download_linux_tools(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for name, (tag, checksum) in LINUX_TOOLS.items():
        filename = f"{name}-x86_64.AppImage"
        url = f"https://github.com/linuxdeploy/{name}/releases/download/{tag}/{filename}"
        destination = directory / filename
        with urllib.request.urlopen(url, timeout=120) as response:
            destination.write_bytes(response.read())
        if hashlib.sha256(destination.read_bytes()).hexdigest() != checksum:
            raise RuntimeError(f"Checksum mismatch for {filename}")
        destination.chmod(0o755)


def windows_runtime(destination):
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installation = subprocess.check_output(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        text=True,
    ).strip()
    candidates = sorted((Path(installation) / "VC/Redist/MSVC").glob("*/x64/Microsoft.VC143.CRT"))
    if not candidates:
        raise RuntimeError("Visual C++ x64 redistributable runtime not found")
    for dll in candidates[-1].glob("*.dll"):
        shutil.copy2(dll, destination)
    for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        if not (destination / name).is_file():
            raise RuntimeError(f"Missing redistributable runtime: {name}")


def verify_macos_dependencies(app):
    binaries = [app / "Contents/MacOS/fontu", *app.rglob("*.dylib")]
    binaries.extend(path for path in app.glob("Contents/Frameworks/*.framework/Versions/*/Qt*")
                    if path.is_file())
    for binary in binaries:
        # otool -L includes a library's own install name, which is not a dependency.
        identities = subprocess.check_output(["otool", "-D", str(binary)], text=True).splitlines()[1:]
        dependencies = subprocess.check_output(["otool", "-L", str(binary)], text=True).splitlines()[1:]
        for line in dependencies:
            # Universal binaries have an unindented header for each architecture.
            if not line.startswith("\t"):
                continue
            dependency = line.strip().split(" (")[0]
            if dependency in identities or not dependency.startswith("/"):
                continue
            if not dependency.startswith(("/usr/lib/", "/System/Library/")):
                raise RuntimeError(f"Unbundled dependency in {binary}: {dependency}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-release")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    version = (build / "release-version.txt").read_text().strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise RuntimeError(f"Invalid project version: {version}")
    system = platform.system()
    architecture = platform.machine().lower()
    architecture = {"aarch64": "arm64", "amd64": "x86_64"}.get(architecture, architecture)
    stage = build / "package"
    dist = ROOT / "dist"
    if stage.exists():
        shutil.rmtree(stage)
    if dist.exists():
        shutil.rmtree(dist)
    dist.mkdir()
    prefix = stage / "usr" if system == "Linux" else stage
    run("cmake", "--install", build, "--config", "Release", "--prefix", prefix)
    smoke = ROOT / "scripts/smoke_package.py"
    if system == "Darwin":
        app = stage / "fontu.app"
        (app / "Contents/Resources").mkdir(exist_ok=True)
        shutil.copy2(ROOT / "LICENSE", app / "Contents/Resources/LICENSE")
        shutil.copy2(FONT_LICENSE, app / "Contents/Resources" / FONT_LICENSE_NAME)
        run("macdeployqt", app, "-always-overwrite", "-verbose=2")
        verify_macos_dependencies(app)
        # Deployment changes Mach-O files. Restore valid ad-hoc signatures on ARM64.
        run("codesign", "--force", "--deep", "--sign", "-", app)
        run("codesign", "--verify", "--deep", "--strict", app)
        run(sys.executable, smoke, app / "Contents/MacOS/fontu")
        image_contents = stage / "dmg"
        image_contents.mkdir()
        shutil.move(app, image_contents / app.name)
        (image_contents / "Applications").symlink_to("/Applications")
        run("hdiutil", "create", "-volname", "FontU", "-srcfolder", image_contents,
            "-format", "UDZO", "-ov", dist / f"FontU-{version}-macos-{architecture}.dmg")
    elif system == "Windows":
        directory = stage / "bin"
        executable = directory / "fontu.exe"
        run("windeployqt", "--release", "--no-compiler-runtime", executable)
        windows_runtime(directory)
        shutil.copy2(ROOT / "LICENSE", directory)
        shutil.copy2(FONT_LICENSE, directory / FONT_LICENSE_NAME)
        run(sys.executable, smoke, executable)
        shutil.make_archive(str(dist / f"FontU-{version}-windows-{architecture}"), "zip", directory)
    elif system == "Linux":
        if architecture != "x86_64":
            raise RuntimeError("The pinned AppImage tools require x86_64")
        tools = build / "appimage-tools"
        download_linux_tools(tools)
        environment = os.environ.copy()
        environment.update(
            APPIMAGE_EXTRACT_AND_RUN="1", ARCH="x86_64", VERSION=version,
            QMAKE=shutil.which("qmake") or "qmake",
            OUTPUT=str(dist / f"FontU-{version}-linux-x86_64.AppImage"),
        )
        run(tools / "linuxdeploy-x86_64.AppImage", "--appdir", stage,
            "--executable", stage / "usr/bin/fontu",
            "--desktop-file", ROOT / "packaging/fontu.desktop",
            "--icon-file", ROOT / "packaging/fontu.svg", "--plugin", "qt",
            "--output", "appimage", env=environment, cwd=tools)
        appimage = dist / f"FontU-{version}-linux-x86_64.AppImage"
        appimage.chmod(0o755)
        run("xvfb-run", "-a", sys.executable, smoke, appimage)
    else:
        raise RuntimeError(f"Unsupported platform: {system}")
    packages = list(dist.iterdir())
    if len(packages) != 1 or packages[0].stat().st_size == 0:
        raise RuntimeError("Expected exactly one nonempty release package")


if __name__ == "__main__":
    main()
