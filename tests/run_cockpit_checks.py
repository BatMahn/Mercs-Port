#!/usr/bin/env python3
"""Capture repeatable native Mercs cockpit views using the supplied game data.

Build mercs and test_mercs_roundtrip first. Run, for example:
  python3 tests/run_cockpit_checks.py --game-dir /path/to/Mercs-game \
      --output-dir /path/to/cockpit-checks

Defaults: Commando/TRA1 and Kodiak/OUT3, all three edition profiles, 640x480
and 2048x1330. --edition, --mech and --size can restrict a quick check. This
does not build anything, change the supplied game folders, or claim pixel
equality with the original engine. It records the actual player, edition,
camera and HUD state alongside full rendered frames for review. Screenshots
are PPM (no imaging package needed); --png additionally uses Pillow.
--throttle and --twist exercise stride and torso motion instead of only rest;
--freeze-sky fixes the otherwise wall-clock-driven backdrop animation.
--baseline compares complete final STATE records with an earlier capture
manifest. --summary writes a portable JSON report containing only numerical
results, state/log metadata and hashes, without copying screenshots.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
import time


EDITIONS = {"dos": ("cd/MERCS/MW2.PRJ", "retail"),
            "direct3d": ("titanium/MW2.PRJ", "direct3d"),
            "3dfx": ("3d/models.prj", "3dfx")}
MECHS = {"commando": ("TRA1SCN1", 130), "kodiak": ("OUT3SCN1", 160)}
SOURCE_FILES = ("tools/glview.c", "tools/common3d.h", "src/glr.c", "src/glr.h", "src/mech3d.c", "src/mech3d.h",
                "src/mercshud.c", "src/mercshud.h", "src/mercscockpit.c", "src/mercscockpit.h",
                "src/skygnd.c", "src/skygnd.h", "tests/run_cockpit_checks.py")


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def ppm_size(path):
    # The native screenshot writer produces an uncompressed 8-bit RGB PPM.
    with Path(path).open("rb") as stream:
        assert stream.readline().strip() == b"P6", "missing RGB screenshot"
        dimensions = stream.readline()
        while dimensions.startswith(b"#"):
            dimensions = stream.readline()
        width, height = map(int, dimensions.split())
        assert stream.readline().strip() == b"255", "unexpected PPM bit depth"
        assert len(stream.read()) == width * height * 3, "incomplete rendered frame"
        return width, height


def checked_size(text):
    match = re.fullmatch(r"(\d+)x(\d+)", text)
    if not match or min(map(int, match.groups())) <= 0:
        raise argparse.ArgumentTypeError("use WIDTHxHEIGHT with positive integers")
    return text


def captured_state(record):
    state = record["state"]
    if isinstance(state, dict):
        return state
    # Also accept the initial before-capture manifest, which retained the
    # original complete STATE final line instead of its parsed dictionary.
    assert isinstance(state, list) and len(state) == 1
    return dict(token.split("=", 1) for token in state[0].split() if "=" in token)


def numerical_summary(report, capture_root, baseline_path=None):
    baseline = json.loads(baseline_path.read_text()) if baseline_path else None
    before = {case["name"]: case for case in baseline["captures"]} if baseline else {}
    summary = {
        "date_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(), "machine": platform.machine(),
        "scope": "Native full-frame cockpit smoke captures using isolated writable overlays; "
                 "actual player/profile/camera/HUD state and frame dimensions/hashes. "
                 "No original full-frame pixel-equivalence or other-OS/GPU claim.",
        "capture_count": len(report["captures"]), "build_included": False,
        "binary_sha256": report["binary_sha256"],
        "source_sha256": report["source_sha256"],
        "source_files_unchanged_during_run": report.get("source_files_unchanged_during_run"),
        "binary_unchanged_during_run": report.get("binary_unchanged_during_run"),
        "original_archive_sha256": {EDITIONS[edition][0]: digest
                                    for edition, digest in report["archives"].items()},
        "input_hash_scope": "Selected DOS, Direct3D and 3dfx PRJ archives; private install/config writes excluded.",
        "original_archives_unchanged": report["original_archives_unchanged"],
        "captures": []
    }
    if "prepare_binary_sha256" in report:
        summary["prepare_binary_sha256"] = report["prepare_binary_sha256"]
    if baseline:
        summary["baseline_manifest_sha256"] = sha256(baseline_path)
        summary["baseline_binary_sha256"] = baseline["binary_sha256"]
        summary["same_archives_as_baseline"] = all(
            baseline["archives"].get(edition) == digest for edition, digest in report["archives"].items())
    for case in report["captures"]:
        frame = capture_root / case["image"]
        log = capture_root / case["log"]
        current_state = captured_state(case)
        profile_matches = re.findall(r"(?:mercs graphics:|mercs render state:) profile=([^ ]+)", log.read_text())
        assert len(profile_matches) == 2, case["name"] + ": missing actual profile records"
        row = {"name": case["name"], "scene": case["scene"], "edition": case["edition"],
               "mech": case["mech"], "image_dimensions": list(ppm_size(frame)),
               "image_sha256": sha256(frame), "image_format": "PPM P6 RGB8",
               "log_sha256": sha256(log), "duration_seconds": case["duration_seconds"],
               "throttle": case.get("throttle", 0), "torso_twist_degrees": case.get("torso_twist_degrees", 0),
               "video_driver": case["environment"]["SDL_VIDEO_DRIVER"],
               "software_opengl": case["environment"]["LIBGL_ALWAYS_SOFTWARE"] == "1",
               "test_fps": int(case["environment"]["MERCS_TEST_FPS"]),
               "observed_profiles": profile_matches,
               "profile_matches_expected": all(value == case["edition"] for value in profile_matches),
               "state": current_state, "pit": case["pit"], "hud_rectangles": case["hud_rectangles"]}
        if baseline:
            assert case["name"] in before, "case missing from baseline: " + case["name"]
            old_state = captured_state(before[case["name"]])
            row["state_matches_before"] = current_state == old_state
            row["state_field_count"] = len(current_state)
            row["state_differences"] = {key: [old_state.get(key), current_state.get(key)]
                                        for key in sorted(set(old_state) | set(current_state))
                                        if old_state.get(key) != current_state.get(key)}
        summary["captures"].append(row)
    summary["all_profiles_match"] = all(row["profile_matches_expected"] for row in summary["captures"])
    if baseline:
        summary["all_final_states_identical_to_before"] = all(row["state_matches_before"] for row in summary["captures"])
    summary["all_passed"] = bool(report["pass"] and summary["all_profiles_match"] and
                                 summary.get("same_archives_as_baseline", True) and
                                 summary.get("all_final_states_identical_to_before", True))
    return summary


def main():
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--binary", type=Path, default=source / "mercs")
    parser.add_argument("--prepare-binary", type=Path, default=source / "test_mercs_roundtrip")
    parser.add_argument("--edition", action="append", choices=tuple(EDITIONS))
    parser.add_argument("--mech", action="append", choices=tuple(MECHS))
    parser.add_argument("--size", action="append", type=checked_size)
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--throttle", type=float, default=0.0)
    parser.add_argument("--twist", type=float, default=0.0, help="torso twist in degrees")
    parser.add_argument("--freeze-sky", action="store_true")
    parser.add_argument("--video-driver", default="offscreen")
    parser.add_argument("--png", action="store_true", help="also convert frames to PNG using Pillow")
    parser.add_argument("--baseline", type=Path, help="compare full final STATE records to this previous capture manifest")
    parser.add_argument("--summary", type=Path, help="write numerical/log/hash JSON only, without screenshot data")
    args = parser.parse_args()
    game, output = args.game_dir.resolve(), args.output_dir.resolve()
    binary, prepare_binary = args.binary.resolve(), args.prepare_binary.resolve()
    editions = args.edition or list(EDITIONS)
    mechs = args.mech or list(MECHS)
    sizes = args.size or ["640x480", "2048x1330"]
    if not 0.5 <= args.seconds <= 30:
        parser.error("--seconds must be between 0.5 and 30")
    if not -1 <= args.throttle <= 1 or not -90 <= args.twist <= 90:
        parser.error("--throttle must be -1..1 and --twist -90..90")
    if not binary.is_file():
        parser.error("missing mercs executable; build it before running captures")
    if "commando" in mechs and not prepare_binary.is_file():
        parser.error("missing test_mercs_roundtrip; build it to prepare the Commando fixture")
    if output == game or game in output.parents:
        parser.error("output must be outside the supplied game folder")
    if args.summary and (args.summary.resolve() == game or game in args.summary.resolve().parents):
        parser.error("summary must be outside the supplied game folder")
    if args.baseline and not args.baseline.is_file():
        parser.error("baseline capture manifest does not exist")
    archives = {edition: game / EDITIONS[edition][0] for edition in editions}
    for archive in archives.values():
        if not archive.is_file():
            parser.error("missing original archive: " + str(archive))
    image_module = None
    if args.png:
        from PIL import Image
        image_module = Image
    output.mkdir(parents=True, exist_ok=True)
    if (output / "captures.json").exists():
        parser.error("output already contains captures.json; choose a fresh directory to retain the previous run")
    archive_hashes = {edition: sha256(path) for edition, path in archives.items()}
    report = {"scope": "native full-frame smoke captures, isolated writable game overlays",
              "binary": str(binary), "binary_sha256": sha256(binary),
              "source_sha256": {name: sha256(source / name) for name in SOURCE_FILES},
              "archives": archive_hashes, "captures": [], "pass": False}
    if "commando" in mechs:
        report["prepare_binary_sha256"] = sha256(prepare_binary)
    (output / "captures.json").write_text(json.dumps(report, indent=2) + "\n")
    for mech in mechs:
        scene, heading = MECHS[mech]
        for edition in editions:
            for size in sizes:
                name = "%s-%s-%s" % (mech, edition, size)
                destination = output / name
                destination.mkdir(exist_ok=False)
                frame = destination / "view.ppm"
                # Every install/config write belongs to this temporary fixture;
                # original archives are exposed through read-only use of their
                # data trees. No caller career/save directory is linked.
                with tempfile.TemporaryDirectory(prefix="mercs-cockpit-") as temp:
                    base = Path(temp)
                    private_game = base / "Mercs-game"
                    private_game.mkdir()
                    for tree in ("cd", "3d", "titanium"):
                        if (game / tree).is_dir():
                            (private_game / tree).symlink_to(game / tree, target_is_directory=True)
                    overlay = private_game / "install" / EDITIONS[edition][1]
                    overlay.mkdir(parents=True)
                    env = {key: value for key, value in os.environ.items()
                           if not key.startswith(("MERCS_", "MW2_"))}
                    settings = {"SDL_VIDEO_DRIVER": args.video_driver,
                                "SDL_AUDIO_DRIVER": "dummy", "LIBGL_ALWAYS_SOFTWARE": "1",
                                "MERCS_CONFIG": str(base / "port.cfg"), "MERCS_SIZE": size,
                                "MERCS_RENDER": "0x0", "MERCS_DEBUG_PIT": "1",
                                "MERCS_DEBUG_EYE": "1", "MERCS_STATE_DUMP": "1",
                                "MERCS_HUD_RECTS": "1", "MERCS_NO_STARTUP": "1",
                                "MERCS_TEST_FPS": "30",
                                "MERCS_AUTOPILOT": "%g,%g,%g,0,%s,1,0" % (args.seconds, args.throttle, args.twist, frame)}
                    if args.freeze_sky:
                        settings["MERCS_TEST_SKY_STILL"] = "1"
                    env.update(settings)
                    if mech == "commando":
                        command = [str(prepare_binary), str(archives[edition]), str(overlay), edition, "prepare"]
                        prepared = subprocess.run(command, cwd=source, env=env, capture_output=True,
                                                  text=True, timeout=30)
                        (destination / "prepare.log").write_text(prepared.stdout + prepared.stderr)
                        assert prepared.returncode == 0, name + ": fixture setup failed"
                    command = [str(binary), str(private_game), "--mission", scene, "--edition", edition]
                    start = time.monotonic()
                    run = subprocess.run(command, cwd=source, env=env, capture_output=True,
                                         text=True, timeout=120)
                    elapsed = time.monotonic() - start
                    text = run.stdout + run.stderr
                    (destination / "run.log").write_text(text)
                    assert run.returncode == 0, name + ": mission exited with error"
                    states = [line for line in text.splitlines() if line.startswith("STATE final")]
                    assert len(states) == 1, name + ": no complete final state"
                    state = dict(token.split("=", 1) for token in states[0].split() if "=" in token)
                    assert state["pmech"].upper() == mech.upper(), name + ": wrong player fixture"
                    assert int(state["camode"]) == 0 and int(state["ejected"]) == 0, name + ": not cockpit view"
                    assert abs(float(state["heading"]) - heading) < 0.1, name + ": unexpected mission heading"
                    assert "mercs graphics: profile=" + edition + " " in text, name + ": wrong edition profile"
                    assert "profile_locked=1" in text, name + ": profile was overridden"
                    assert ppm_size(frame) == tuple(map(int, size.split("x"))), name + ": wrong frame dimensions"
                    if image_module:
                        with image_module.open(frame) as image:
                            image.save(destination / "view.png")
                    record = {"name": name, "scene": scene, "edition": edition, "mech": mech,
                              "size": size, "duration_seconds": args.seconds, "wall_seconds": elapsed,
                              "throttle": args.throttle, "torso_twist_degrees": args.twist,
                              "image": str(frame.relative_to(output)), "sha256": sha256(frame),
                              "log": str((destination / "run.log").relative_to(output)),
                              "command": command, "environment": settings, "state": state,
                              "pit": [line for line in text.splitlines() if line.startswith("cockpit repr")],
                              "graphics": [line for line in text.splitlines() if line.startswith(("mercs graphics:", "mercs render state:"))],
                              "hud_rectangles": sorted(set(line for line in text.splitlines() if line.startswith("HUDRECT ")))}
                    if (base / "port.cfg").exists():
                        record["config_sha256"] = sha256(base / "port.cfg")
                    report["captures"].append(record)
                    (output / "captures.json").write_text(json.dumps(report, indent=2) + "\n")
                    print("PASS %s: %s, heading %s, camera (%s,%s,%s)" %
                          (name, state["pmech"], state["heading"], state["cam_x"], state["cam_y"], state["cam_z"]), flush=True)
    assert archive_hashes == {edition: sha256(path) for edition, path in archives.items()}, "original archive changed"
    assert report["source_sha256"] == {name: sha256(source / name) for name in SOURCE_FILES}, "runtime source changed during captures"
    assert report["binary_sha256"] == sha256(binary), "executable changed during captures"
    report["original_archives_unchanged"] = True
    report["source_files_unchanged_during_run"] = True
    report["binary_unchanged_during_run"] = True
    report["pass"] = True
    (output / "captures.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.summary or args.baseline:
        summary = numerical_summary(report, output, args.baseline)
        if args.summary:
            args.summary.parent.mkdir(parents=True, exist_ok=True)
            args.summary.write_text(json.dumps(summary, indent=2) + "\n")
        if args.baseline:
            assert summary["all_final_states_identical_to_before"], "final STATE changed; inspect numerical summary"
            assert summary["same_archives_as_baseline"], "baseline archive hashes differ"
    print("PASS: %d full frames; original archive hashes unchanged. Review the images before accepting visual changes." % len(report["captures"]))


if __name__ == "__main__":
    main()
