#!/usr/bin/env python3
"""Exercise the actual app packager with simulated Mach-O tools.

Runs on any host. This checks packaging decisions and failure recovery; it does
not replace building/signing/running a native app on macOS.
"""
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import unittest


PACKAGER = Path(__file__).resolve().parents[1] / "scripts/macos_mercs_app.sh"
METAL_FRAMEWORK = "/System/Library/Frameworks/Metal.framework/Versions/A/Metal"
MOCK_TOOL = r'''#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys

name = Path(sys.argv[0]).name
args = sys.argv[1:]
with open(os.environ["MOCK_LOG"], "a") as log:
    log.write(json.dumps([name, *args]) + "\n")
if name == "uname":
    print("Darwin")
elif name == "pkg-config":
    assert args == ["--variable=libdir", "sdl3"], args
    print(os.environ.get("MOCK_LIBDIR", "/nonexistent"))
elif name == "codesign":
    if os.environ.get("MOCK_SIGN_FAIL"):
        sys.exit(1)
elif name == "lipo":
    obj = json.loads(Path(args[-1]).read_text())
    if args[0] == "-archs":
        print(" ".join(obj["archs"]))
    elif args[0] == "-verify_arch":
        sys.exit(0 if args[1] in obj["archs"] else 1)
    else:
        raise AssertionError(args)
elif name == "otool":
    obj = json.loads(Path(args[-1]).read_text())
    if args[0] == "-L":
        print(args[-1] + ":")
        for dep in ([obj["id"]] if obj.get("id") else []) + obj["deps"]:
            print("\t" + dep + " (compatibility version 3.0.0, current version 3.4.18)")
    elif args[0] == "-l":
        for rpath in obj.get("rpaths", []):
            print("          cmd LC_RPATH\n      cmdsize 64\n         path " + rpath + " (offset 12)")
    elif args[0] != "-h":
        raise AssertionError(args)
elif name == "install_name_tool":
    path = Path(args[-1])
    obj = json.loads(path.read_text())
    if args[0] == "-id":
        obj["id"] = args[1]
    elif args[0] == "-change":
        obj["deps"] = [args[2] if dep == args[1] else dep for dep in obj["deps"]]
    else:
        raise AssertionError(args)
    path.write_text(json.dumps(obj))
else:
    raise AssertionError(name)
'''


class MacAppPackaging(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="mercs app test ")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "Source With Spaces"
        self.root.mkdir()
        (self.root / "scripts").mkdir()
        shutil.copy2(PACKAGER, self.root / "scripts/macos_mercs_app.sh")
        fonts = self.root / "assets/fonts"
        fonts.mkdir(parents=True)
        (fonts / "LiberationSans-Bold.ttf").write_bytes(b"font fixture")
        (fonts / "README.md").write_text("font license fixture\n")
        self.bin = Path(self.tmp.name) / "Mock Tools"
        self.bin.mkdir()
        for name in ("uname", "otool", "lipo", "install_name_tool", "codesign", "pkg-config"):
            path = self.bin / name
            path.write_text(MOCK_TOOL)
            path.chmod(0o755)
        self.log = Path(self.tmp.name) / "commands.jsonl"
        self.env = dict(os.environ)
        for key in ("MERCS_SDL3_LIBRARY", "MERCS_SDL3_DYLIB", "MERCS_APP_VERSION", "MOCK_SIGN_FAIL"):
            self.env.pop(key, None)
        self.env.update(PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        MOCK_LOG=str(self.log), PKG_CONFIG=str(self.bin / "pkg-config"))
        self.app = self.root / "Mercenaries.app"

    def binary(self, path, deps=(), archs=("arm64", "x86_64"), rpaths=(), library=False):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(dict(deps=list(deps), archs=list(archs),
                                        rpaths=list(rpaths), id=str(path) if library else None)))
        path.chmod(0o755)
        return path

    def dylib_fixture(self, **kwargs):
        lib = self.binary(self.root / "SDK With Spaces/lib/libSDL3.0.dylib",
                          deps=("/usr/lib/libSystem.B.dylib", METAL_FRAMEWORK), library=True, **kwargs)
        self.binary(self.root / "mercs", deps=(str(lib), METAL_FRAMEWORK))
        return lib

    def run_packager(self, success=True):
        result = subprocess.run(["sh", str(self.root / "scripts/macos_mercs_app.sh")],
                                env=self.env, text=True, capture_output=True, timeout=20)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def commands(self, name):
        return [row for line in self.log.read_text().splitlines()
                if (row := json.loads(line))[0] == name]

    def previous_app(self):
        self.app.mkdir()
        (self.app / "keep.txt").write_text("previous working app")

    def assert_previous_preserved(self):
        self.assertEqual((self.app / "keep.txt").read_text(), "previous working app")
        self.assertEqual(list((self.root / "build").glob("mercs-app.*")), [])

    def test_native_dylib_identity_resources_and_sign_order(self):
        self.dylib_fixture()
        self.env["MERCS_APP_VERSION"] = "0.3.1"
        self.run_packager()
        bundled = self.app / "Contents/Frameworks/libSDL3.0.dylib"
        self.assertTrue(bundled.is_file())
        expected = "@executable_path/../Frameworks/libSDL3.0.dylib"
        self.assertEqual(json.loads(bundled.read_text())["id"], expected)
        executable_deps = json.loads((self.app / "Contents/MacOS/mercs").read_text())["deps"]
        self.assertIn(expected, executable_deps)
        self.assertIn(METAL_FRAMEWORK, executable_deps)
        self.assertIn(METAL_FRAMEWORK, json.loads(bundled.read_text())["deps"])
        info = plistlib.loads((self.app / "Contents/Info.plist").read_bytes())
        self.assertEqual(info["CFBundleExecutable"], "mercs")
        self.assertEqual(info["CFBundleIdentifier"], "org.mercsport.mercenaries")
        self.assertEqual(info["CFBundleVersion"], "0.3.1")
        self.assertEqual(info["LSMinimumSystemVersion"], "10.14")
        self.assertTrue(info["NSHighResolutionCapable"])
        self.assertTrue((self.app / "Contents/Resources/assets/fonts/LiberationSans-Bold.ttf").exists())
        signing = self.commands("codesign")
        self.assertTrue(signing[0][-1].endswith("Frameworks/libSDL3.0.dylib"))
        self.assertTrue(signing[1][-1].endswith("Mercenaries.app"))
        self.assertEqual(signing[2][1:4], ["--verify", "--deep", "--strict"])

    def test_rpath_resolution_preserves_spaces(self):
        self.dylib_fixture()
        self.binary(self.root / "mercs", deps=("@rpath/libSDL3.0.dylib",),
                    rpaths=("@loader_path/SDK With Spaces/lib",))
        self.run_packager()
        self.assertEqual(self.commands("pkg-config"), [])

    def test_pkg_config_fallback_uses_sdl3_libdir(self):
        lib = self.dylib_fixture()
        self.binary(self.root / "mercs", deps=("@rpath/libSDL3.0.dylib",))
        self.env["MOCK_LIBDIR"] = str(lib.parent)
        self.run_packager()
        self.assertEqual(self.commands("pkg-config")[0][1:], ["--variable=libdir", "sdl3"])

    def test_explicit_library_override(self):
        lib = self.dylib_fixture()
        self.binary(self.root / "mercs", deps=("/old/missing/libSDL3.0.dylib",))
        self.env["MERCS_SDL3_LIBRARY"] = str(lib)
        self.run_packager()
        self.assertTrue((self.app / "Contents/Frameworks/libSDL3.0.dylib").exists())

    def test_framework_preserves_version_symlinks_and_signs_container(self):
        sdk = self.root / "Framework SDK"
        framework = sdk / "SDL3.framework"
        self.binary(framework / "Versions/A/SDL3",
                    deps=("/usr/lib/libSystem.B.dylib", METAL_FRAMEWORK), library=True)
        (framework / "Versions/A/Resources").mkdir()
        (framework / "Versions/A/Resources/Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable": "SDL3"}))
        (framework / "Versions/Current").symlink_to("A")
        (framework / "SDL3").symlink_to("Versions/Current/SDL3")
        (framework / "Resources").symlink_to("Versions/Current/Resources")
        self.binary(self.root / "mercs", deps=("@rpath/SDL3.framework/Versions/A/SDL3",), rpaths=(str(sdk),))
        self.run_packager()
        target = self.app / "Contents/Frameworks/SDL3.framework"
        self.assertTrue((target / "SDL3").is_symlink())
        self.assertEqual((target / "Versions/Current").readlink(), Path("A"))
        self.assertTrue((target / "Resources/Info.plist").is_file())
        self.assertTrue(self.commands("codesign")[0][-1].endswith("Frameworks/SDL3.framework"))

    def test_architecture_mismatch_preserves_previous_app(self):
        self.dylib_fixture(archs=("arm64",))
        self.previous_app()
        result = self.run_packager(False)
        self.assertIn("x86_64 architecture", result.stderr)
        self.assert_previous_preserved()

    def test_sdl2_shim_is_rejected(self):
        self.binary(self.root / "mercs", deps=("/opt/homebrew/lib/libSDL2.dylib",))
        self.previous_app()
        result = self.run_packager(False)
        self.assertIn("no native SDL3 library", result.stderr)
        self.assert_previous_preserved()

    def test_direct_opengl_dependency_preserves_previous_app(self):
        lib = self.dylib_fixture()
        self.previous_app()
        for dependency in (
            "/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL",
            "/opt/vendor/libGL.1.dylib",
            "/opt/vendor/libEGL.dylib",
        ):
            with self.subTest(dependency=dependency):
                self.binary(self.root / "mercs", deps=(str(lib), dependency))
                result = self.run_packager(False)
                self.assertIn("mercs still links OpenGL", result.stderr)
                self.assert_previous_preserved()
                self.assertEqual(self.commands("install_name_tool"), [])
                self.assertEqual(self.commands("codesign"), [])

    def test_unbundled_dependency_preserves_previous_app(self):
        lib = self.dylib_fixture()
        self.binary(lib, deps=("/opt/vendor/libUnexpected.dylib",), library=True)
        self.previous_app()
        result = self.run_packager(False)
        self.assertIn("unbundled dependency", result.stderr)
        self.assert_previous_preserved()

    def test_failed_signing_preserves_previous_app(self):
        self.dylib_fixture()
        self.previous_app()
        self.env["MOCK_SIGN_FAIL"] = "1"
        self.run_packager(False)
        self.assert_previous_preserved()


if __name__ == "__main__":
    unittest.main(verbosity=2)
