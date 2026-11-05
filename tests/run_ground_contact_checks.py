#!/usr/bin/env python3
"""Drive real Mercs movement on TRA1 without changing the user's game folder.

Requires `make mercs test_mercs_roundtrip`. Uses SDL offscreen/Mesa on Linux;
the C ground fixture separately checks all three editions' authored spawns.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[1]


def run_route(binary, game, heading):
    with tempfile.TemporaryDirectory(prefix="mercs-ground-") as temp:
        base = Path(temp)
        folder = base / "Mercs-game"
        folder.mkdir()
        for tree in ("cd", "3d", "titanium"):
            (folder / tree).symlink_to(game / tree, target_is_directory=True)
        overlay = folder / "install/direct3d"
        overlay.mkdir(parents=True)
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(("MERCS_", "MW2_"))}
        env.update(
            SDL_VIDEO_DRIVER="offscreen", SDL_AUDIO_DRIVER="dummy",
            LIBGL_ALWAYS_SOFTWARE="1", MESA_SHADER_CACHE_DISABLE="true",
            MERCS_CONFIG=str(base / "port.cfg"), MERCS_SIZE="320x240",
            MERCS_RENDER="0x0", MERCS_STATE_DUMP="/0.25",
            MERCS_NO_STARTUP="1", MERCS_TEST_FPS="30",
            MERCS_TEST_FALLSIM="1", MERCS_TEST_PASSIVE="1",
            MERCS_START="-60000,165000", MERCS_START_HEADING=str(heading),
            MERCS_AUTOPILOT=f"30,1,0,0,{base / 'view.ppm'},0,0",
        )
        prepared = subprocess.run(
            [str(SOURCE / "test_mercs_roundtrip"), str(game / "titanium/MW2.PRJ"),
             str(overlay), "direct3d", "prepare"],
            cwd=SOURCE, env=env, capture_output=True, text=True, timeout=30)
        if prepared.returncode:
            raise RuntimeError(prepared.stdout + prepared.stderr)
        result = subprocess.run(
            [str(binary), str(folder), "--mission", "TRA1SCN1", "--edition", "direct3d"],
            cwd=SOURCE, env=env, capture_output=True, text=True, timeout=180)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        lines = (result.stdout + result.stderr).splitlines()
        states = [dict(re.findall(r"(\w+)=([^\s]+)", line))
                  for line in lines if line.startswith("STATE")]
        falls = [line for line in lines if line.startswith("FALLSIM")]
        if len(states) < 100 or not states[-1].get("t", "0").startswith("30."):
            raise RuntimeError("The mission did not complete its 30-second route")
        gaps = [float(s["y"]) - float(s["gnd"]) for s in states]
        final = states[-1]
        return {
            "heading_degrees": heading, "state_samples": len(states),
            "buried_reports": len(falls), "first_buried_report": falls[0] if falls else None,
            "minimum_player_clearance_cm": min(gaps),
            "final_xyz_cm": [float(final[k]) for k in ("x", "y", "z")],
            "final_ground_cm": float(final["gnd"]),
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path)
    parser.add_argument("--mercs", type=Path, default=SOURCE / "mercs")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--allow-baseline-failure", action="store_true")
    args = parser.parse_args()
    with ThreadPoolExecutor(max_workers=2) as pool:
        rows = list(pool.map(lambda heading: run_route(args.mercs.resolve(), args.game.resolve(), heading),
                             (0, 45, 90, 180)))
    report = json.dumps(rows, indent=2) + "\n"
    if args.json:
        args.json.write_text(report)
    print(report, end="")
    if not args.allow_baseline_failure and any(r["buried_reports"] or r["minimum_player_clearance_cm"] < 0
                                               for r in rows):
        raise SystemExit("FAIL: a route retained a buried actor or player")


if __name__ == "__main__":
    main()
