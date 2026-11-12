#!/usr/bin/env python3
"""Run bounded Mercenaries simulation, renderer and resource regression checks.

Original archives and executables remain external and read-only. Every writable
overlay/configuration is temporary. The longer real-mission and shell input
sequences have their own run_*_checks.py entry points; this runner does not
substitute a full playthrough for its bounded checks.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--mw2", type=Path, required=True)
    parser.add_argument("--gbl", type=Path, required=True)
    parser.add_argument("--3dfx-dll", dest="glide_dll", type=Path,
                        help="Original 3dfx MERCSW.DLL for independent weapon-table checks")
    parser.add_argument("--titanium-dll", type=Path,
                        help="Original Titanium 16-bit MERCSW.DLL for weapon-table checks")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    game, mw2, gbl = (p.resolve() for p in (args.game, args.mw2, args.gbl))
    archive = game / "cd/MERCS/MW2.PRJ"
    glide_archive = game / "3d/models.prj"
    titanium_archive = game / "titanium/MW2.PRJ"
    required = (archive, glide_archive, titanium_archive, game / "titanium/8bit/MW2.PRJ",
                game / "cd/MERCS/MW2.EXE", game / "cd/MERCS/MERCSW.DLL",
                game / "cd/MERCS/MW2SHELL.EXE", game / "cd/MERCS/SKYGND.PAR",
                game / "cd/RMG/MIS0SCN1.BWD")
    for path in required:
        if not path.is_file():
            parser.error(f"Required original fixture is missing: {path}")
    for path in (mw2, gbl):
        if not path.is_dir():
            parser.error(f"Required reference game folder is missing: {path}")

    # The normal data bundle need not contain these obsolete renderer DLLs.
    # Explicit paths are never substituted with another edition.
    glide_dll = args.glide_dll or game / "3d/MERCSW.DLL"
    titanium_dll = args.titanium_dll or game / "titanium/MERCSW.DLL"
    for supplied in (args.glide_dll, args.titanium_dll):
        if supplied is not None and not supplied.is_file():
            parser.error(f"Required original renderer DLL is missing: {supplied}")
    hardware_available = glide_dll.is_file() and titanium_dll.is_file()
    if (args.glide_dll or args.titanium_dll) and not hardware_available:
        parser.error("Provide both --3dfx-dll and --titanium-dll when their default game paths are absent")

    env = {k: v for k, v in os.environ.items() if not k.startswith(("MERCS_", "MW2_"))}
    env["SDL_AUDIO_DRIVER"] = "dummy"
    if sys.platform.startswith("linux"):
        env.setdefault("SDL_VIDEO_DRIVER", "offscreen")
        env.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
    checks = 0
    skipped = []

    def run(name, *arguments, expected=0, contains=(), timeout=90, private_fixture=False):
        nonlocal checks
        exe = project / (name + (".exe" if os.name == "nt" else ""))
        if not exe.is_file() and private_fixture:   # built only with the private captured-palette fixtures (KNOWN_ISSUES.md)
            skipped.append(f"{name} (private captured-palette fixtures, tests/fixtures/mission-order-dispatch)")
            return
        if not exe.is_file():
            raise RuntimeError(f"Missing test executable {exe.name}; build it with make {name}")
        print(f"RUN: {name}", flush=True)
        result = subprocess.run([str(exe), *(str(v) for v in arguments)],
                                cwd=project, env=env, text=True, errors="replace",
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=timeout)
        if result.returncode != expected or any(s not in result.stdout for s in contains):
            raise RuntimeError(f"{name}: exit {result.returncode}, expected {expected}\n{result.stdout}")
        print(result.stdout.rstrip(), flush=True)
        checks += 1

    with tempfile.TemporaryDirectory(prefix="mercs-check-") as temporary:
        tmp = Path(temporary)

        def private(name):
            path = tmp / name
            path.mkdir()
            return path

        env["MERCS_CONFIG"] = str(tmp / "test-port.cfg")
        env["MESA_SHADER_CACHE_DIR"] = str(private("shader-cache"))

        # Resource ownership and the original typed actor/mission records.
        run("test_mercs_paths", game, mw2, gbl, private("paths"))
        run("test_mercs_overlay", private("overlay"))
        run("test_mercs_lance", archive, private("lance"))
        run("test_mercs_camo", archive)
        run("test_mercs_gps_ai", archive)
        run("test_mercs_loadout", archive)
        run("test_mercs_prj_checksum", archive, glide_archive,
            titanium_archive, game / "titanium/8bit/MW2.PRJ")
        run("test_mercs_bwdplace", archive)
        run("test_mercs_rmg", archive, game / "cd", private("rmg"))
        run("test_mercs_instant", archive, game / "cd", private("instant"))

        # Actual native simulation consumers and original-executable goldens.
        for name in ("combat", "ammunition", "orphan_ammunition", "quadtree", "special", "lock_timing", "orders", "firegroups",
                     "ai_trait", "effects", "fx_area", "particles", "particle_events",
                     "results", "result_transport", "career"):
            run("test_mercs_" + name)
        run("test_mercs_return", archive)
        run("test_mercs_ai_transport", archive, private("ai"))
        run("test_mercs_ground", archive)
        run("test_mercs_projectile_terrain", archive, glide_archive, titanium_archive)
        run("test_mercs_projectile_effect_points")
        run("test_mercs_polygon_normals", archive, glide_archive, titanium_archive)
        run("test_mercs_aircraft", archive, glide_archive, titanium_archive)
        run("test_mercs_aircraft_coordinates", archive, glide_archive, titanium_archive)
        run("test_mercs_mission_dispatch", archive, glide_archive, titanium_archive)
        run("test_mercs_nuclear_wave", archive, glide_archive, titanium_archive)
        run("test_mercs_ammo_capacity", archive, glide_archive, titanium_archive)
        run("test_mercs_palette_controller", private_fixture=True)
        run("test_mercs_alliance", archive, glide_archive, titanium_archive)
        run("test_mercs_targeting", archive, glide_archive, titanium_archive)
        run("test_mercs_ai_los", archive)
        run("test_mercs_fall", archive)
        run("test_mercs_mission_return", archive)
        run("test_mercs_exchange", private("exchange"))
        run("test_mercs_roundtrip", archive, private("roundtrip"), "dos")
        # Titanium dynamic salvage: rules, exchange and yard actions on the
        # retail archive; the screen itself needs Titanium's shell files.
        ti_shell = game / "titanium"
        if (ti_shell / "DATABASE.MW2").is_file() and (ti_shell / "SMK").is_dir():
            run("test_mercs_salvage", archive, game / "cd/MERCS", ti_shell, private("salvage") / "shot")
        else:
            run("test_mercs_salvage", archive, game / "cd/MERCS")

        # Native GL fixtures run against real shaders as well as numerical
        # boundary cases. The provided archives supply edition-specific art.
        run("test_mercs_environment", archive)
        run("test_mercs_backdrop", "--retail", archive,
            "--accelerated", glide_archive, "--accelerated", titanium_archive)
        run("test_mercs_hud", archive)
        run("test_mercs_hud", glide_archive, "--resources-only")
        run("test_mercs_hud", titanium_archive, "--resources-only")
        run("test_mercs_cockpit", archive)
        # The all-model sweep renders 1,040 frames, including software GL runs.
        run("test_mercs_cockpit_render", game / "cd/MERCS/SKYGND.PAR", archive,
            timeout=600)
        for name in ("cockpit_depth", "palette_render", "world_render", "world_lod",
                     "ground_render", "texture_filter", "ortho", "map", "map_render", "hud_controls"):
            run("test_mercs_" + name)
        run("test_mercs_resources", archive)
        run("test_mercs_texture_clocks", archive, glide_archive, titanium_archive)
        run("test_mercs_palette_cards")
        run("test_mercs_palette_frontend", archive, private_fixture=True)
        run("test_mercs_dedicated_particle_palette", archive, glide_archive, titanium_archive, private_fixture=True)
        run("test_mercs_pose", archive, glide_archive, titanium_archive)
        run("test_mercs_inputmap", game / "cd/MERCS")
        run("test_mercs_controls", game / "cd")
        run("test_mercs_audio", archive)
        run("test_mercs_audio_events")
        run("test_mercs_voice", archive)
        run("test_mercs_voice_audio", archive, game / "cd/KEATING")

        # Translate only the LE container to expose the external original DOS
        # table. This analysis image is temporary and never run as a game.
        dos_elf = tmp / "retail-dos.elf"
        subprocess.run([sys.executable, str(project / "tools/le2elf.py"),
                        str(game / "cd/MERCS/MW2.EXE"), str(dos_elf)],
                       check=True, stdout=subprocess.DEVNULL, timeout=30)
        run("test_mercs_timing", archive, game / "cd/MERCS/MERCSW.DLL", dos_elf)
        if hardware_available:
            run("test_mercs_hardware_weapons", game / "cd/MERCS/MERCSW.DLL",
                glide_dll.resolve(), titanium_dll.resolve())
        else:
            skipped.append("independent 3dfx/Titanium DLL weapon tables (supply --3dfx-dll and --titanium-dll)")

        # Keep this configuration fresh even if another fixture tests settings
        # persistence. Positive/negative preflights must never write it.
        env["MERCS_CONFIG"] = str(tmp / "preflight-port.cfg")
        run("mercs", game, "--mission", "OUT3SCN1", "--inspect",
            contains=("32 parts, 11 nav points, track 17", "kodiak / kdk00std"))
        run("mercs", mw2, "--mission", "OUT3SCN1", "--inspect", expected=1,
            contains=("Choose a Mercs-game folder",))
        run("mercs", gbl, "--mission", "OUT3SCN1", "--inspect", expected=1,
            contains=("Choose a Mercs-game folder",))
        if (tmp / "preflight-port.cfg").exists():
            raise RuntimeError("Read-only preflight wrote the port configuration")
    for reason in skipped:
        print("SKIP: " + reason, flush=True)
    print(f"PASS: {checks} bounded Mercenaries engine/resource/preflight checks; "
          f"{len(skipped)} optional external-fixture groups skipped", flush=True)


if __name__ == "__main__":
    main()
