"""Exercise macOS dependency validation with real Mach-O binaries."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from scripts.package_release import verify_macos_dependencies


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
