#!/usr/bin/env python3
"""Check authored stock-player launches and preservation of unused owned MEKs.

Uses disposable three-owned-Mech fixtures and real, two-second application
children. These are integration checks, not ordinary campaign victories.
Build `mercs` and `test_mercs_fixed_player` first; this runner never rebuilds
production binaries or touches an existing career or installation overlay.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile
import time


CASES = (
    ("MCC4CAM1", "MCC4SCN1", "stalker", "stk00std"),
    ("SIG2CAM1", "SIG2SCN1", "pegasus", "peg00std"),
    ("OUT3CAM1", "OUT3SCN1", "kodiak", "kdk00std"),
)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def private_game_tree(source, dest):
    """Real writable directories, read-only original asset references only."""
    dest.mkdir()
    for root, directories, files in os.walk(source):
        relative = Path(root).relative_to(source)
        if relative == Path("."):
            directories[:] = [d for d in directories if d != "install"]
        target = dest / relative
        target.mkdir(parents=True, exist_ok=True)
        for name in files:
            (target / name).symlink_to((Path(root) / name).resolve())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True, type=Path)
    parser.add_argument("--mercs", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    game = args.game.resolve()
    output = (args.output or project / "build/validation/campaign-playthrough/fixed-player-validation").resolve()
    output.mkdir(parents=True, exist_ok=True)
    app = (args.mercs or project / "mercs").resolve()
    helper = project / "test_mercs_fixed_player"
    for executable in (app, helper):
        if not executable.is_file() or not os.access(executable, os.X_OK):
            parser.error(f"Build the required executable before running: {executable}")
    archives = {
        "dos": game / "cd/MERCS/MW2.PRJ",
        "3dfx": game / "3d/models.prj",
        "direct3d": game / "titanium/MW2.PRJ",
    }
    before = {str(path.relative_to(game)): sha(path) for path in archives.values()}
    sources = [*project.joinpath("src").glob("*.c"),
               *project.joinpath("src").glob("*.h"),
               project / "tools/mercs.c", project / "tools/glview.c",
               project / "tools/common3d.h", project / "Makefile",
               project / "tests/test_mercs_fixed_player.c", Path(__file__).resolve()]
    report = {
        "date_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "platform": platform.platform(),
        "scope": "Controlled disposable three-owned-Mech fixtures and nine two-second "
                 "SDL/OpenGL child launches/returns; no ordinary campaign wins, full mission "
                 "completion, debrief settlement, or native Windows/macOS execution claimed.",
        "cases": [{"campaign": c, "scene": s, "skeleton": sk, "loadout": m}
                  for c, s, sk, m in CASES],
        "inputs_sha256": before,
        "native_mercs_sha256": sha(app),
        "test_helper_sha256": sha(helper),
        "source_sha256": {str(p.relative_to(project)): sha(p) for p in sorted(set(sources))},
        "controlled_roundtrips": 0,
        "maximum_deployment_roundtrips": 0,
        "actual_application_runs": 0,
        "actual_shell_imports": 0,
        "invalid_unused_mek_refusals": 0,
        "all_passed": False,
        "checks": [],
    }
    env = {k: v for k, v in os.environ.items() if not k.startswith(("MERCS_", "MW2_"))}
    env["SDL_AUDIO_DRIVER"] = "dummy"
    if sys.platform.startswith("linux"):
        env.update(SDL_VIDEO_DRIVER="offscreen", LIBGL_ALWAYS_SOFTWARE="1")

    def run(name, command, *, contains=(), extra_env=None, timeout=120, expected_code=0):
        started = time.monotonic()
        try:
            result = subprocess.run([str(v) for v in command], cwd=project,
                                    env={**env, **(extra_env or {})}, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=timeout)
            stdout, code = result.stdout, result.returncode
        except subprocess.TimeoutExpired as error:
            stdout = error.stdout or ""
            if isinstance(stdout, bytes):
                stdout = stdout.decode("utf-8", errors="replace")
            stdout += f"\nRunner timeout after {timeout} seconds.\n"
            code = None
        log = output / (name + ".log")
        log.write_text(stdout)
        passed = code == expected_code and all(value in stdout for value in contains)
        report["checks"].append({"name": name, "exit_code": code, "passed": passed,
                                  "expected_exit_code": expected_code,
                                  "seconds": round(time.monotonic() - started, 3),
                                  "log": log.name, "log_sha256": sha(log)})
        print(("PASS" if passed else "FAIL") + ": " + name, flush=True)
        if not passed:
            raise RuntimeError(f"{name}: {stdout[-12000:]}")
        return stdout

    try:
        with tempfile.TemporaryDirectory(prefix="mercs-fixed-player-") as scratch:
            scratch = Path(scratch)
            env["MERCS_CONFIG"] = str(scratch / "port.cfg")
            cache = scratch / "mesa-cache"
            cache.mkdir()
            env["MESA_SHADER_CACHE_DIR"] = str(cache)
            for edition, archive in archives.items():
                overlay = scratch / (edition + "-controlled")
                overlay.mkdir()
                run(edition + "-controlled", [helper, archive, overlay, edition],
                    contains=("SIG2 orders/movie separation", "three controlled stock-player integration fixtures"))
                report["controlled_roundtrips"] += 3
                run(edition + "-maximum-deployment", [helper, archive, overlay, edition, "capacity"],
                    contains=("synthetic 714-byte retained deployment",))
                report["maximum_deployment_roundtrips"] += 1

            private_game = scratch / "Mercs-game"
            private_game_tree(game, private_game)
            for edition, archive in archives.items():
                overlay = private_game / "install" / ("retail" if edition == "dos" else edition)
                overlay.mkdir(parents=True)
                for campaign, scene, skeleton, loadout in CASES:
                    name = f"{edition}-{scene.lower()}"
                    prepared = run(name + "-prepare", [helper, archive, overlay, edition, "prepare", campaign])
                    token = prepared.strip().splitlines()[-1]
                    if not re.fullmatch("[0-9a-f]{32}", token):
                        raise RuntimeError(f"{name}: invalid launch token")
                    deployment = {p: sha(p) for p in overlay.rglob("*")
                                  if p.is_file() and p.suffix.casefold() == ".mek"}
                    if len(deployment) != 3:
                        raise RuntimeError(f"{name}: expected exactly three staged owned MEKs")
                    if scene == "MCC4SCN1":
                        # A stock player must not bypass validation of the
                        # unused owned wingmen in its existing manifest.
                        unused = next(p for p in deployment if p.name.casefold() == "jen01mer.mek")
                        good_bytes = unused.read_bytes()
                        try:
                            unused.write_bytes(b"invalid unused owned MEK fixture")
                            run(name + "-invalid-unused-mek", [app, private_game, "--mission", scene,
                                "--edition", edition, "--return-token", token],
                                contains=("Duty Roster does not match",), expected_code=1,
                                extra_env={"MERCS_AUTOPILOT": f"0.5,0,0,0,{scratch / (name + '-invalid.ppm')},1,0",
                                           "MERCS_SIZE": "640x480", "MERCS_TEST_FPS": "30"})
                        finally:
                            unused.write_bytes(good_bytes)
                        report["invalid_unused_mek_refusals"] += 1
                    frame = scratch / (name + ".ppm")
                    run(name + "-actual-child", [app, private_game, "--mission", scene,
                        "--edition", edition, "--return-token", token],
                        contains=(f"Player: {skeleton} / {loadout}", "committed mission return", "autopilot:"),
                        extra_env={"MERCS_AUTOPILOT": f"2,0,0,0,{frame},1,0",
                                   "MERCS_SIZE": "640x480", "MERCS_TEST_FPS": "30"})
                    if any(not p.is_file() or sha(p) != digest for p, digest in deployment.items()):
                        raise RuntimeError(f"{name}: child modified an unused owned deployment MEK")
                    report["checks"][-1]["unchanged_deployment_sha256"] = {
                        str(p.relative_to(overlay)): digest for p, digest in deployment.items()}
                    if not frame.is_file() or frame.stat().st_size < 640 * 480 * 3:
                        raise RuntimeError(f"{name}: application did not render a complete frame")
                    report["checks"][-1].update(rendered_frame_sha256=sha(frame),
                                                  rendered_frame_bytes=frame.stat().st_size)
                    # PPM is the application's unchanged output. PNG is a
                    # lossless display copy; PIL is optional for headless checks.
                    try:
                        from PIL import Image
                    except ImportError:
                        pass
                    else:
                        png = output / (name + ".png")
                        with Image.open(frame) as rendered:
                            assert rendered.size == (640, 480)
                            rendered.save(png)
                        report["checks"][-1].update(frame_png=png.name, frame_png_sha256=sha(png))
                    report["actual_application_runs"] += 1
                    run(name + "-consume", [helper, archive, overlay, edition, "consume", campaign, token],
                        contains=("three unchanged owned MEKs", "no campaign win"))
                    report["actual_shell_imports"] += 1
        report["all_passed"] = all(check["passed"] for check in report["checks"])
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        after = {str(path.relative_to(game)): sha(path) for path in archives.values()}
        report["original_inputs_unchanged"] = before == after
        report["runtime_binary_unchanged"] = report["native_mercs_sha256"] == sha(app)
        if not report["original_inputs_unchanged"] or not report["runtime_binary_unchanged"]:
            report["all_passed"] = False
        (output / "native-results.json").write_text(json.dumps(report, indent=2) + "\n")
    if not report["all_passed"]:
        raise RuntimeError("Original input or runtime binary changed during the checks")
    print("PASS: 9 controlled stock-player returns, 3 maximum-size deployment returns, "
          "and 9 real two-second child launches/imports; "
          "no campaign victories claimed", flush=True)


if __name__ == "__main__":
    main()
