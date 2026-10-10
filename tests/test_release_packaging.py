"""Exercise macOS release bundles with real Mach-O binaries."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from scripts.package_release import verify_macos_dependencies


@unittest.skipUnless(sys.platform == "darwin", "Requires macOS CMake, Qt, and codesign")
class MacOSInstalledAppTests(unittest.TestCase):
    def test_relocated_installed_app_loads_bundled_rpath_framework(self):
        repository = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            source = root / "source"
            source.mkdir()
            (source / "main.cpp").write_text(
                "extern int fixture(); int main() { return fixture() == 42 ? 0 : 1; }\n"
            )
            (source / "fixture.cpp").write_text("int fixture() { return 42; }\n")
            (source / "CMakeLists.txt").write_text(f'''\
cmake_minimum_required(VERSION 3.21)
project(release_rpath_test LANGUAGES CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("{repository}" fontu-project)
add_library(fixture SHARED fixture.cpp)
set_target_properties(fixture PROPERTIES
    FRAMEWORK TRUE OUTPUT_NAME QtWidgets MACOSX_RPATH TRUE)
set_property(TARGET fontu PROPERTY SOURCES "${{CMAKE_CURRENT_SOURCE_DIR}}/main.cpp")
set_property(TARGET fontu PROPERTY LINK_LIBRARIES fixture)
install(TARGETS fixture FRAMEWORK DESTINATION fontu.app/Contents/Frameworks)
''')
            qt_prefix = subprocess.check_output(
                ["qmake", "-query", "QT_INSTALL_PREFIX"], text=True
            ).strip()
            build = root / "build"
            stage = root / "installed"
            for command in (
                ["cmake", "-S", source, "-B", build, f"-DCMAKE_PREFIX_PATH={qt_prefix}",
                 "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
                 "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"],
                ["cmake", "--build", build, "--target", "fontu", "--parallel", "3"],
                ["cmake", "--install", build, "--prefix", stage],
            ):
                subprocess.run(command, check=True, capture_output=True, text=True)
            app = root / "relocated/fontu.app"
            app.parent.mkdir()
            shutil.move(stage / "fontu.app", app)
            shutil.rmtree(build)
            verify_macos_dependencies(app)
            subprocess.run(
                ["codesign", "--force", "--deep", "--sign", "-", app],
                check=True, capture_output=True, text=True,
            )
            environment = {key: value for key, value in os.environ.items()
                           if not key.startswith("DYLD_")}
            result = subprocess.run(
                [app / "Contents/MacOS/fontu"], env=environment,
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)


@unittest.skipUnless(sys.platform == "darwin", "Requires macOS clang and otool")
class MacOSDependencyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.app = self.root / "fontu.app"
        self.executable = self.app / "Contents/MacOS/fontu"
        self.executable.parent.mkdir(parents=True)
        self.source = self.root / "main.c"
        self.source.write_text("int main(void) { return 0; }\n")
        subprocess.run(["clang", str(self.source), "-o", str(self.executable)], check=True)

    def test_universal_plugin_install_name_is_not_a_dependency(self):
        library = self.app / "Contents/PlugIns/platforms/libfixture.dylib"
        library.parent.mkdir(parents=True)
        source = self.root / "fixture.c"
        source.write_text("int fixture(void) { return 42; }\n")
        subprocess.run([
            "clang", "-arch", "arm64", "-arch", "x86_64", "-dynamiclib", str(source),
            "-o", str(library), "-Wl,-install_name,@rpath/libfixture.dylib",
        ], check=True)

        verify_macos_dependencies(self.app)

    def test_external_library_dependency_is_rejected(self):
        library = self.root / "libexternal.dylib"
        source = self.root / "external.c"
        source.write_text("int external(void) { return 42; }\n")
        subprocess.run([
            "clang", "-dynamiclib", str(source), "-o", str(library),
            f"-Wl,-install_name,{library}",
        ], check=True)
        self.source.write_text("int external(void); int main(void) { return external(); }\n")
        subprocess.run([
            "clang", str(self.source), str(library), "-o", str(self.executable),
        ], check=True)

        with self.assertRaisesRegex(RuntimeError, "Unbundled dependency.*libexternal"):
            verify_macos_dependencies(self.app)


if __name__ == "__main__":
    unittest.main()
