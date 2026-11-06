# Mercenaries port, based on the MW2 foundation (Linux / macOS / Windows via MinGW-w64)
.DEFAULT_GOAL := mercs
CC      ?= cc
CFLAGS  ?= -std=c99 -D_DEFAULT_SOURCE -O2 -Wall -Wextra -Wconversion
LDLIBS  = -lpthread -lm
UNAME_S := $(shell uname -s)
# SDL3 ships a pkg-config module, not sdl2-config or an SDLmain library.
PKG_CONFIG ?= pkg-config
SDL3_MIN_VERSION ?= 3.2.0
ifdef SDL3_DIR
  SDL3_PKG = PKG_CONFIG_LIBDIR="$(SDL3_DIR)/lib/pkgconfig" PKG_CONFIG_PATH= $(PKG_CONFIG)
else
  SDL3_PKG = $(PKG_CONFIG)
endif
SDL3_CFLAGS ?= $(shell $(SDL3_PKG) --cflags sdl3 2>/dev/null)
SDL3_LIBS ?= $(shell $(SDL3_PKG) --libs sdl3 2>/dev/null)
# Windows: MSYS2's MINGW64 shell (uname MINGW64_NT-...), or cross-compiling from Linux with
#   make mercs WINDOWS=1 CC=x86_64-w64-mingw32-gcc PKG_CONFIG=x86_64-w64-mingw32-pkg-config
ifneq (,$(findstring MINGW,$(UNAME_S)))
  WINDOWS = 1
  ifeq ($(origin CC),default)
    CC = gcc
  endif
endif
ifeq ($(WINDOWS),1)
  UNAME_S = Windows
  CFLAGS += -include src/wincompat.h
  LDLIBS = -lm
  CXXLIB = -static-libgcc -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic   # no libstdc++ / libwinpthread DLLs to ship
  DLLIB =
  WINSRC = src/wincompat.c
  WINLIBS = -lole32 -lshell32   # system DLLs: the folder picker (wincompat.c mw2_pick_folder)
  WINRES = build/mw2res.o
  WINDRES ?= $(patsubst %gcc,%windres,$(CC))
else
  DLLIB = -ldl
  WINSRC =
  WINRES =
  WINLIBS =
endif
ifeq ($(UNAME_S),Linux)
  LDLIBS += -ldl
  CXXLIB = -lstdc++
endif
ifeq ($(UNAME_S),Darwin)
  CXXLIB = -lc++
  LDLIBS += -framework CoreFoundation -framework CoreAudio -framework AudioToolbox
endif

LIB_SRC = src/prj.c src/datapath.c src/cdaudio.c src/mek.c
BIN     = prjtool cdtool mektool bwdtool mechview mechview3d cdrip test_port shelltool xmiplay test_titanium test_gamefolder
GPU_BIN   = glshot glview

all: mercs
tools: $(BIN)

# Windows: the exe's icon and version information
build/mw2res.o: packaging/windows/mw2port.rc packaging/windows/mw2port.ico
	@mkdir -p build
	$(WINDRES) -I packaging/windows $< -O coff -o $@

build/miniaudio.o: third_party/miniaudio.c third_party/miniaudio.h
	@mkdir -p build
	$(CC) -O2 -w -c $< -o $@

xmiplay: tools/xmiplay.c src/gusmid.c src/shelldb.c src/gusmid.h src/shelldb.h
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lm

shelltool: tools/shelltool.c src/shelldb.c src/smk.c src/shelldb.h src/smk.h
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

prjtool: tools/prjtool.c src/prj.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@

cdrip: tools/cdrip.c
	$(CC) $(CFLAGS) $^ -o $@

HDRS = $(wildcard src/*.h tools/*.h)
RB_SRC = src/rb_gpu.c src/rb_shaders.c
RENDER_SRC = $(RB_SRC) src/hud.c src/mercshud.c src/mercsfiregroups.c src/mercscockpit.c src/mercsoptions.c src/mercsparticles.c src/shp.c src/glr.c src/mercsground.c src/skygnd.c src/anim.c src/mek.c src/mtbl.c src/msim.c src/mercsresult.c src/fx.c src/combat.c src/mercsreturn.c src/mercsmissionreturn.c src/mercsexchange.c src/mercsbridge.c src/mercsscene.c src/ai.c src/mech3d.c src/render.c src/tex.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c

# Mercenaries starts in its native folder-picker / registration / contract shell.
MERCS_SRC = tools/mercs.c tools/mercsshell.c src/mercsart.c src/mercsviewport.c src/mercsshell_audio.c src/mercstext.c src/mercspicker.c src/mercslock.c src/mercsshell_data.c src/mercscareer.c \
            src/mercsdebrief.c src/mercssave.c src/mercssave_init.c src/mercsinterchange.c src/mercsmechlab.c src/mercsinstant.c src/mercscampaign.c src/mercsrmg.c src/mercsstore.c src/mercsrepair.c src/mercspanels.c src/mercsscores.c src/mercseconomy.c src/mercscontrols.c \
            src/mercsvideo.c src/vidfx.c src/smk.c src/shelldb.c src/ttext.c src/lance.c \
            src/sfx.c src/portcfg.c src/cdaudio.c $(RENDER_SRC) $(WINSRC)
MERCS_OBJ = $(patsubst %.c,build/mercs/%.o,$(sort $(MERCS_SRC)))
MERCS_SDL_CFLAGS = $(SDL3_CFLAGS)
mercs: $(WINRES) $(MERCS_OBJ) build/mercs_sim.o build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

ifeq ($(UNAME_S),Darwin)
app: mercs
	PKG_CONFIG="$(PKG_CONFIG)" sh scripts/macos_mercs_app.sh
else
app:
	@echo "Build the Finder app on macOS with make app; use make mercs on this platform."
	@exit 2
endif

build/mercs/%.o: %.c Makefile | check-sdl3
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Ithird_party $(MERCS_SDL_CFLAGS) -DSDL_MAIN_HANDLED -MMD -MP -c $< -o $@

build/mercs_sim.o: tools/glview.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Ithird_party $(MERCS_SDL_CFLAGS) -DSDL_MAIN_HANDLED -Dmain=mercs_sim_main -MMD -MP -c $< -o $@

# Validation-only input bridge: the shipped executable has no external driver.
build/mercs_playthrough_sim.o: tools/glview.c tests/mercs_playthrough_input.h tests/mercs_playthrough_geometry.h Makefile | check-sdl3
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Ithird_party $(MERCS_SDL_CFLAGS) -DSDL_MAIN_HANDLED -Dmain=mercs_sim_main -DMERCS_PLAYTHROUGH_TEST=1 -MMD -MP -c $< -o $@

# The validation sampler includes the unchanged simulation implementation so it
# can clone private geometry caches. Only this test link replaces msim.o.
build/mercs_playthrough_geometry.o: tests/mercs_playthrough_geometry.c tests/mercs_playthrough_geometry.h src/msim.c $(HDRS) Makefile | check-sdl3
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Ithird_party $(MERCS_SDL_CFLAGS) -DSDL_MAIN_HANDLED -MMD -MP -c $< -o $@

test_mercs_playthrough: $(filter-out build/mercs/src/msim.o,$(MERCS_OBJ)) build/mercs_playthrough_geometry.o build/mercs_playthrough_sim.o build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

test_mercs_playthrough_career: build/mercs/tests/test_mercs_playthrough_career.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

test_mercs_audio_events: build/mercs/tests/test_mercs_audio_events.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include $(MERCS_OBJ:.o=.d) build/mercs_sim.d build/mercs_playthrough_sim.d build/mercs_playthrough_geometry.d

test_mercs_paths: tests/test_mercs_paths.c src/portcfg.c src/prj.c src/datapath.c src/portcfg.h src/datapath.h
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_mercs_lance: tests/test_mercs_lance.c src/lance.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

MERCS_CAMPAIGN_SRC = src/mercscampaign.c src/mercsstore.c src/mercssave.c src/mercssave_init.c src/mercsinterchange.c src/mercsmechlab.c src/mercsrmg.c \
                     src/mercsdebrief.c src/mercsshell_data.c src/mercsscene.c src/mercscareer.c src/mercsresult.c \
                     src/mercslock.c src/mercseconomy.c src/lance.c src/mek.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
test_mercs_save: tests/test_mercs_save.c src/mercssave.c src/mercssave_init.c src/mercsrmg.c src/mercscareer.c src/prj.c src/mek.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm
test_mercs_debrief: tests/test_mercs_debrief.c tests/mercs_debrief_original_fixtures.h tests/mercs_debrief_value_fixtures.h tests/mercs_debrief_sort_fixtures.h src/mercsdebrief.c src/mercscareer.c src/mercsresult.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm
test_mercs_debrief_view: tests/test_mercs_debrief_view.c tools/mercsdebrief_view.h tests/mercs_debrief_view_fixtures.h src/mercsdebrief.c src/mercssave.c src/mercscareer.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ -lm
test_mercs_debrief_assets: tests/test_mercs_debrief_assets.c tools/mercsdebrief_view.h tests/mercs_debrief_view_font_fixtures.h tests/mercs_debrief_view_layout_fixtures.h src/mercsdebrief.c src/mercssave.c src/mercscareer.c src/mercstext.c src/mercsart.c src/shelldb.c src/shp.c src/smk.c src/sfx.c src/prj.c src/datapath.c src/bwdplace.c src/bwd.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm
test_mercs_repair: tests/test_mercs_repair.c src/mercsrepair.c tests/fixtures/career-return/repair_original_fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@
test_mercs_repair_view: tests/test_mercs_repair_view.c tools/mercsrepair_view.h tests/fixtures/career-return/repair_view_original_fixtures.h $(HDRS)
	$(CC) $(CFLAGS) $(MERCS_SDL_CFLAGS) -Isrc $(filter %.c,$^) -o $@
test_mercs_campaign: tests/test_mercs_campaign.c $(MERCS_CAMPAIGN_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_interchange: tests/test_mercs_interchange.c src/mercsinterchange.c src/mercsrepair.c $(MERCS_CAMPAIGN_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Dtime=mercs_interchange_test_time -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_campaign_tail: tests/test_mercs_campaign_tail.c src/mercssave_init.c src/mercsrmg.c src/mercssave.c src/mercscareer.c src/prj.c src/mek.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/mercssave_init.c,$(filter %.c,$^)) -o $@ -lm

# Includes the actual shell handlers; uses original assets through a private
# writable fixture and controlled simulator outcomes, without real gameplay.
test_mercs_frontend: build/mercs/tests/test_mercs_frontend.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_frontend.d

test_mercs_startup: build/mercs/tests/test_mercs_startup.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_startup.d

test_mercs_shell_interaction: build/mercs/tests/test_mercs_shell_interaction.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_shell_interaction.d

test_mercs_combat_variables: build/mercs/tests/test_mercs_combat_variables.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_combat_variables.d

test_mercs_controls_view: build/mercs/tests/test_mercs_controls_view.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_controls_view.d

test_mercs_economy_ui: build/mercs/tests/test_mercs_economy_ui.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_economy_ui.d

test_mercs_mechlab_ui: build/mercs/tests/test_mercs_mechlab_ui.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_mechlab_ui.d

test_mercs_instant_ui: build/mercs/tests/test_mercs_instant_ui.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_instant_ui.d

test_mercs_interchange_ui: build/mercs/tests/test_mercs_interchange_ui.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

-include build/mercs/tests/test_mercs_interchange_ui.d

test_mercs_viewport: tests/test_mercs_viewport.c src/mercsviewport.c src/mercsviewport.h
	$(CC) $(CFLAGS) -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS)

# These fixtures intercept only the compositor/audio submission boundary.
test_mercs_art: tests/test_mercs_art.c src/mercsart.c src/shelldb.c src/shp.c src/smk.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter-out src/mercsart.c,$(filter %.c,$^)) -o $@ $(SDL3_LIBS) -lm

test_mercs_shell_audio: tests/test_mercs_shell_audio.c src/sfx.c src/mercsshell_audio.c src/shelldb.c src/prj.c src/datapath.c src/bwdplace.c src/bwd.c $(HDRS)
	$(CC) $(CFLAGS) -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter-out src/sfx.c,$(filter %.c,$^)) -o $@ $(SDL3_LIBS) -lm

test_mercs_dialog: tests/test_mercs_dialog.c tools/mercsdialog_view.h tests/fixtures/dialog/dialog_original_fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@

test_mercs_gamemenu: tests/test_mercs_gamemenu.c tools/mercsgamemenu_view.h tests/fixtures/dialog/menu_original_fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@

test_mercs_panels: tests/test_mercs_panels.c src/mercspanels.c tools/mercspanels_view.h tests/fixtures/dialog/panels_original_fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@

test_mercs_scores: tests/test_mercs_scores.c src/mercsscores.c src/mercspanels.c src/mercsdebrief.c src/mercssave.c src/mercscareer.c src/mercslock.c src/datapath.c tests/fixtures/dialog/original-scores.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_bwdplace: tests/test_mercs_bwdplace.c src/bwdplace.c src/bwd.c src/prj.c src/datapath.c tests/fixtures/combat-completion/original-bwdplace.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@

test_mercs_gps_ai: tests/test_mercs_gps_ai.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

# This fixture test includes combat.c to exercise actual critical-slot IDs.
test_mercs_loadout: tests/test_mercs_loadout.c src/combat.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/combat.c,$(filter %.c,$^)) -o $@ -lm

# Independent original-executable heat goldens and critical-slot cases.
test_mercs_combat: tests/test_mercs_combat.c src/combat.c src/mek.c src/prj.c src/datapath.c tests/fixtures/combat-validation/mercs_heat_cases.h tests/fixtures/combat-completion/original-locations.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/combat.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_ammunition: tests/test_mercs_ammunition.c src/combat.c src/mek.c src/prj.c src/datapath.c tests/fixtures/combat-completion/original-ammunition.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/combat.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_controls: tests/test_mercs_controls.c src/mercscontrols.c src/datapath.c tests/fixtures/controls-completion/original-controls-fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_hud_controls: tests/test_mercs_hud_controls.c src/mercshud.c src/prj.c src/datapath.c src/glr.c src/mercsground.c src/wtb.c tools/inputmap.h tools/mercstarget_math.h tests/fixtures/controls-completion/original-runtime.h $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Wno-unused-function -Isrc -Itools $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_firegroups: tests/test_mercs_firegroups.c src/mercsfiregroups.c tests/fixtures/combat-completion/original-firegroups.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_hardware_weapons: tests/test_mercs_hardware_weapons.c src/combat.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_rmg: tests/test_mercs_rmg.c tests/mercs_rmg_original.h $(MERCS_CAMPAIGN_SRC) src/mercsexchange.c src/mech3d.c src/wtb.c src/anim.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_instant: tests/test_mercs_instant.c src/mercsinstant.c src/mercsrmg.c src/lance.c src/mek.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/combat-completion/original-instant.h tests/fixtures/combat-completion/original-instant-rmg.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_mechlab: tests/test_mercs_mechlab.c tests/fixtures/mercs_mechlab_original.h src/mercsmechlab.c src/mercssave.c src/mercscareer.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_map: tests/test_mercs_map.c src/mercsmap.h tests/fixtures/controls-completion/original-map.h
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@

test_mercs_ortho: tests/test_mercs_ortho.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_map_render: tests/test_mercs_map_render.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_return: tests/test_mercs_return.c src/mercsreturn.c src/combat.c src/mek.c src/prj.c src/datapath.c tests/fixtures/result-audit/return_adapter_original_fixtures.h tests/fixtures/result-audit/return_init_original_fixtures.h tests/fixtures/result-audit/return_edition_stock_fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/combat.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_orphan_ammunition: tests/test_mercs_orphan_ammunition.c src/mercsreturn.c src/combat.c src/mek.c src/prj.c src/datapath.c tests/fixtures/mercs_orphan_ammunition_original.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/combat.c,$(filter %.c,$^)) -o $@ -lm

# This fixture includes ai.c to execute the actual private attack decision.
test_mercs_ai_trait: tests/test_mercs_ai_trait.c src/ai.c src/prj.c src/datapath.c tests/fixtures/ai-validation/mercs_ai_cases.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/ai.c,$(filter %.c,$^)) -o $@ -lm

# Includes msim.c to observe actual mission-to-AI view transport.
test_mercs_ai_transport: tests/test_mercs_ai_transport.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/ai-validation/mercs_ai_transport_cases.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_prj_checksum: tests/test_mercs_prj_checksum.c src/prj.c src/mek.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_environment: tests/test_mercs_environment.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_mercs_overlay: tests/test_mercs_overlay.c src/portcfg.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_mercs_inputmap: tests/test_mercs_inputmap.c tools/inputmap.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-unused-function -DSDL_MAIN_HANDLED -Isrc -Itools $(SDL3_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

# This fixture includes sfx.c to check real decoded records reach the mixer.
test_mercs_audio: tests/test_mercs_audio.c tests/fixtures/mercs_audio_pan_original.h src/sfx.c src/prj.c src/datapath.c src/bwdplace.c src/bwd.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(SDL3_CFLAGS) $(filter-out src/sfx.c,$(filter %.c,$^)) -o $@ $(SDL3_LIBS) -lm

test_mercs_voice_audio: tests/test_mercs_voice_audio.c src/sfx.c src/prj.c src/datapath.c src/bwdplace.c src/bwd.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(SDL3_CFLAGS) $(filter-out src/sfx.c,$(filter %.c,$^)) -o $@ $(SDL3_LIBS) -lm

test_mercs_voice: tests/test_mercs_voice.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_ground: tests/test_mercs_ground.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/pose-contact/original-contact.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_playthrough_geometry: tests/test_mercs_playthrough_geometry.c tests/mercs_playthrough_geometry.c tests/mercs_playthrough_geometry.h src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c tests/mercs_playthrough_geometry.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_quadtree: tests/test_mercs_quadtree.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/quadtree-integer-domain/original-quadtree.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_particles: tests/test_mercs_particles.c src/mercsparticles.c src/mercsparticles.h tests/fixtures/particles/original-particles.h
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_mercs_particle_events: tests/test_mercs_particle_events.c src/mercsparticles.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_text: tests/test_mercs_text.c src/mercstext.c src/mercsart.c src/shelldb.c src/shp.c src/smk.c src/sfx.c src/prj.c src/datapath.c src/bwdplace.c src/bwd.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_hud: tests/test_mercs_hud.c tests/mercs_compass_original_fixtures.h src/mercshud.c src/combat.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_cockpit: tests/test_mercs_cockpit.c tests/mercs_cockpit_plane_fixtures.h src/mercscockpit.c src/mech3d.c src/anim.c src/wtb.c src/bwd.c src/bwdplace.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_cockpit_motion: tests/test_mercs_cockpit_motion.c src/mercscockpit.c src/mech3d.c src/anim.c src/wtb.c src/bwd.c src/bwdplace.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_cockpit_motion_render: tests/test_mercs_cockpit_motion_render.c src/glr.c src/mercsground.c src/mercscockpit.c src/mech3d.c src/anim.c src/wtb.c src/bwd.c src/bwdplace.c src/mek.c src/prj.c src/datapath.c $(WINSRC) $(HDRS) $(RB_SRC) tests/gpu_test.h | check-sdl3
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_cockpit_render: tests/test_mercs_cockpit_render.c tests/mercs_cockpit_shading_fixtures.h src/glr.c src/mercsground.c src/wtb.c src/skygnd.c src/mercscockpit.c src/mech3d.c src/anim.c src/bwd.c src/bwdplace.c src/mek.c src/prj.c src/datapath.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_cockpit_depth: tests/test_mercs_cockpit_depth.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_palette_render: tests/test_mercs_palette_render.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_world_render: tests/test_mercs_world_render.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_world_lod: tests/test_mercs_world_lod.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_ground_render: tests/test_mercs_ground_render.c tests/mercs_ground_original.h src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_texture_filter: tests/test_mercs_texture_filter.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_economy: tests/test_mercs_economy.c src/mercseconomy.c src/mercssave.c src/mercscareer.c src/mercsdebrief.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ -lm

# Original-execution drivers consume private fixtures generated by the
# corresponding tests/fixtures/verify_original_*.py scripts.
test_mercs_economy_original: tests/test_mercs_economy_original.c src/mercseconomy.c src/mercssave.c src/mercscareer.c src/mercsdebrief.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_aerotech: tests/test_mercs_aerotech.c src/mercscareer.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_mercs_backdrop: tests/test_mercs_backdrop.c tests/mercs_backdrop_original_fixtures.h src/mech3d.c src/anim.c src/wtb.c src/bwd.c src/bwdplace.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_timing: tests/test_mercs_timing.c src/combat.c src/mek.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_resources: tests/test_mercs_resources.c src/fx.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_effects: tests/test_mercs_effects.c src/fx.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/result-audit/original-fx-lifecycle.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_pose: tests/test_mercs_pose.c src/mech3d.c src/anim.c src/wtb.c src/bwd.c src/bwdplace.c src/mek.c src/prj.c src/datapath.c tests/fixtures/pose-contact/original-pose.h tests/fixtures/pose-contact/original-camera.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_texture_clocks: tests/test_mercs_texture_clocks.c src/fx.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/fidelity-followup/original-texture-clocks.h tests/fixtures/fidelity-followup/original-texture-bindings.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

MERCS_PALETTE_ORIGINAL = tests/fixtures/mission-order-dispatch/nuclear-palette/original
MERCS_PALETTE_FIXTURES = $(MERCS_PALETTE_ORIGINAL)/original-palette-fixtures.h $(MERCS_PALETTE_ORIGINAL)/follow-up/original-shared-palette-fixtures.h
MERCS_PALETTE_INCLUDES = -I$(MERCS_PALETTE_ORIGINAL) -I$(MERCS_PALETTE_ORIGINAL)/follow-up
test_mercs_palette_controller: tests/test_mercs_palette_controller.c src/fx.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(MERCS_PALETTE_FIXTURES) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(MERCS_PALETTE_INCLUDES) $(filter %.c,$^) -o $@ -lm

test_mercs_palette_cards: tests/test_mercs_palette_cards.c src/glr.c src/mercsground.c src/wtb.c $(WINSRC) $(HDRS) $(RB_SRC)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

# The fixture includes the actual frontend and the existing private dispatch
# setup. Link normal frontend dependencies, without platform-specific section GC.
test_mercs_palette_frontend: tests/test_mercs_palette_frontend.c tests/palette_native_fixture.c tests/palette_cache_fixture.h tests/test_mercs_mission_dispatch.c tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(WINSRC) $(HDRS) $(MERCS_PALETTE_FIXTURES) tests/fixtures/mission-order-dispatch/original/mission-order-fixtures.h
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -I. -Itests -Isrc -Ithird_party $(MERCS_PALETTE_INCLUDES) $(MERCS_SDL_CFLAGS) $(filter-out src/msim.c tools/glview.c tests/test_mercs_mission_dispatch.c,$(filter %.c %.o,$^)) -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

# Actual Mercs accelerated load/callback/font gate. Both helper sources include
# their production implementation; avoid duplicate hud/msim compilation.
MERCS_ACCEL_PALETTE_FIXTURE = tests/fixtures/mission-order-dispatch/nuclear-palette/accelerated-native/accelerated-palette-fixtures.h
test_mercs_accelerated_palette_frontend: tests/hud_backend_fixture.h tests/test_mercs_accelerated_palette_frontend.c tests/accelerated_palette_hud_fixture.c tests/palette_native_fixture.c tests/test_mercs_mission_dispatch.c tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(WINSRC) $(HDRS) $(MERCS_ACCEL_PALETTE_FIXTURE) tests/fixtures/mission-order-dispatch/original/mission-order-fixtures.h
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -I. -Itests -Isrc -Ithird_party -Itests/fixtures/mission-order-dispatch/nuclear-palette/accelerated-native $(MERCS_SDL_CFLAGS) $(filter-out src/msim.c src/hud.c tools/glview.c tests/test_mercs_mission_dispatch.c,$(filter %.c %.o,$^)) -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)


# Actual cached SHP palette and deferred-upload gate.
MERCS_ACCEL_SHP_FIXTURE = tests/fixtures/mission-order-dispatch/nuclear-palette/accelerated-shp-native/accelerated-shp-fixtures.h
test_mercs_accelerated_shp_palette: tests/hud_backend_fixture.h tests/test_mercs_accelerated_shp_palette.c tests/accelerated_shp_hud_fixture.c tests/palette_native_fixture.c tests/test_mercs_mission_dispatch.c tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(WINSRC) $(HDRS) $(MERCS_ACCEL_SHP_FIXTURE) tests/fixtures/mission-order-dispatch/original/mission-order-fixtures.h
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -DMERCS_SHP_DEFERRED=1 -I. -Itests -Isrc -Ithird_party -Itests/fixtures/mission-order-dispatch/nuclear-palette/accelerated-shp-native $(MERCS_SDL_CFLAGS) $(filter-out src/msim.c src/hud.c tools/glview.c tests/test_mercs_mission_dispatch.c,$(filter %.c %.o,$^)) -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)



# Dedicated Mercs particle RGB table through actual loader, shot, frontend and GL.
# Fixture translation units include glview/glr/msim; each is compiled once.
MERCS_DEDICATED_PARTICLE_FIXTURE = tests/fixtures/mission-order-dispatch/nuclear-palette/accelerated-particle-native/dedicated-particle-fixtures.h
test_mercs_dedicated_particle_palette: tests/test_mercs_dedicated_particle_palette.c tests/dedicated_particle_renderer_fixture.c tests/dedicated_particle_sim_fixture.c tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(WINSRC) $(HDRS) $(MERCS_DEDICATED_PARTICLE_FIXTURE)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -I. -Isrc -Ithird_party -Itests/fixtures/mission-order-dispatch/nuclear-palette/accelerated-particle-native $(MERCS_SDL_CFLAGS) $(filter-out src/msim.c src/glr.c tools/glview.c,$(filter %.c %.o,$^)) -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

test_mercs_fx_area: tests/test_mercs_fx_area.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c tests/fixtures/result-audit/original-fx-area-fixtures.h tests/fixtures/result-audit/original-fx-lifecycle.h tests/fixtures/combat-completion/original-wreck.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

MERCS_COMBAT_TEST_SRC = src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
# Projectile-only terrain collision and original-derived fallback consumers.
test_mercs_projectile_terrain: tests/test_mercs_projectile_terrain.c tests/fixtures/projectile-terrain/native/original-projectile-terrain-fixtures.h tests/fixtures/projectile-terrain/native/original-titanium-fallback-fixtures.h tests/fixtures/projectile-terrain/native/retained-sol3-path-fixtures.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Werror=implicit-function-declaration -Isrc -Itests/fixtures/projectile-terrain/native -o $@ tests/test_mercs_projectile_terrain.c $(filter-out src/msim.c,$(MERCS_COMBAT_TEST_SRC)) -lm

# Original impact presentation/light coordinates through the actual simulation
# and frontend. Each included implementation is compiled only once.
MERCS_PROJECTILE_EFFECT_FIXTURE = tests/fixtures/projectile-terrain/effect-point-native/original-projectile-effect-fixtures.h
test_mercs_projectile_effect_points: tests/test_mercs_projectile_effect_points.c tests/projectile_effect_sim_fixture.c tests/projectile_effect_fixture.h tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(WINSRC) $(HDRS) $(MERCS_PROJECTILE_EFFECT_FIXTURE)
	$(CC) $(CFLAGS) -Wno-conversion -Werror=implicit-function-declaration -DSDL_MAIN_HANDLED -DMERCS_EFFECT_COORDINATES=1 -I. -Itests -Isrc -Ithird_party -Itests/fixtures/projectile-terrain/effect-point-native $(MERCS_SDL_CFLAGS) $(filter-out src/msim.c tools/glview.c,$(filter %.c %.o,$^)) -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)

test_mercs_arrival: tests/test_mercs_arrival.c tests/fixtures/arrival-coordinates/original-arrival-fixtures.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Itests/fixtures/arrival-coordinates $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_playthrough_world_ray: tests/test_mercs_playthrough_world_ray.c tests/test_mercs_playthrough_geometry.c tests/mercs_playthrough_geometry.c tests/mercs_playthrough_geometry.h tests/fixtures/campaign-playthrough/gra3-world-obstruction/inputs/retained-rays.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Itests/fixtures/campaign-playthrough/gra3-world-obstruction/inputs $(filter-out src/msim.c tests/mercs_playthrough_geometry.c tests/test_mercs_playthrough_geometry.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_polygon_normals: tests/test_mercs_polygon_normals.c tests/fixtures/polygon-normals/original-normal-fixtures.h $(MERCS_COMBAT_TEST_SRC) src/mercscockpit.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_aircraft: tests/test_mercs_aircraft.c $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

build/mercs_aircraft_selectors.h: tools/glview.c tests/extract_mercs_target_selectors.py
	python3 tests/extract_mercs_target_selectors.py tools/glview.c $@

test_mercs_aircraft_coordinates: tests/test_mercs_aircraft_coordinates.c tests/fixtures/aircraft-coordinates/original/original-aircraft-fixtures.h build/mercs_aircraft_selectors.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_alliance: tests/test_mercs_alliance.c tests/fixtures/campaign-playthrough/original-alliance-fixtures.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_targeting: tests/test_mercs_targeting.c tests/fixtures/targeting-coordinates/original-targeting-fixtures.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_lock_timing: tests/test_mercs_lock_timing.c tests/fixtures/lock-timing/original-lock-timing-fixtures.h $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_ai_los: tests/test_mercs_ai_los.c $(MERCS_COMBAT_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_special: tests/test_mercs_special.c $(MERCS_COMBAT_TEST_SRC) tests/fixtures/combat-completion/original-special.h tests/fixtures/combat-completion/original-debris.h tests/fixtures/combat-completion/original-fx-hold.h tests/fixtures/combat-completion/original-wreck.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_mission_dispatch: tests/test_mercs_mission_dispatch.c $(MERCS_COMBAT_TEST_SRC) tests/fixtures/mission-order-dispatch/original/mission-order-fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_nuclear_wave: tests/test_mercs_nuclear_wave.c tests/test_mercs_mission_dispatch.c $(MERCS_COMBAT_TEST_SRC) tests/fixtures/mission-order-dispatch/original/mission-order-fixtures.h tests/fixtures/mission-order-dispatch/nuclear-wave/original/original-wave-fixtures.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Itests/fixtures/mission-order-dispatch/nuclear-wave/original $(filter-out src/msim.c tests/test_mercs_mission_dispatch.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_fall: tests/test_mercs_fall.c $(MERCS_COMBAT_TEST_SRC) tests/fixtures/combat-completion/original-fall.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_orders: tests/test_mercs_orders.c $(MERCS_COMBAT_TEST_SRC) tests/fixtures/combat-completion/original-orders.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter-out src/msim.c,$(filter %.c,$^)) -o $@ -lm

test_mercs_results: tests/test_mercs_results.c tests/fixtures/result-audit/result_format_original_fixtures.h src/mercsresult.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@

test_mercs_exchange: tests/test_mercs_exchange.c src/mercsexchange.c src/mercsresult.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@

MERCS_SHELL_DATA_SRC = src/mercsshell_data.c src/mercsscene.c src/mercsrmg.c src/mercscareer.c src/mercsresult.c src/lance.c src/mek.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
test_mercs_ammo_capacity: tests/test_mercs_ammo_capacity.c $(MERCS_SHELL_DATA_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_shell_data: tests/test_mercs_shell_data.c tests/mercs_career_original_fixtures.h $(MERCS_SHELL_DATA_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_shell_sim: tests/test_mercs_shell_sim.c $(MERCS_SHELL_DATA_SRC) src/msim.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mech3d.c src/wtb.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

MERCS_RETURN_TEST_SRC = src/mercsmissionreturn.c src/mercsreturn.c src/mercsexchange.c src/mercsbridge.c $(MERCS_SHELL_DATA_SRC) src/msim.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mech3d.c src/wtb.c

test_mercs_statistics: tests/test_mercs_statistics.c tests/fixtures/result-audit/statistics_original_fixtures.h $(MERCS_RETURN_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_mission_return: tests/test_mercs_mission_return.c tests/fixtures/result-audit/mission_return_payload_fixtures.h tests/fixtures/result-audit/mission_return_selection_fixtures.h $(MERCS_RETURN_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_result_transport: tests/test_mercs_result_transport.c tests/fixtures/result-audit/result_format_original_fixtures.h tests/fixtures/result-audit/result_lifecycle_original_fixtures.h tests/fixtures/result-audit/result_rearm_original_fixtures.h $(MERCS_RETURN_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_career: tests/test_mercs_career.c tests/mercs_career_original_fixtures.h src/mercscareer.c src/mercscareer_data.h $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@

test_mercs_roundtrip: tests/test_mercs_roundtrip.c $(MERCS_RETURN_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

test_mercs_fixed_player: tests/test_mercs_fixed_player.c $(MERCS_RETURN_TEST_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

# Includes the real presenter to capture a frame before RenderPresent.
test_mercs_video: tests/test_mercs_video.c src/vidfx.c src/smk.c src/portcfg.c src/prj.c src/mercsvideo.c src/mercsviewport.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(MERCS_SDL_CFLAGS) $(filter-out src/mercsvideo.c,$(filter %.c,$^)) -o $@ $(SDL3_LIBS) -lm

missionsim: tools/missionsim.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm

glshot: tools/glshot.c $(RENDER_SRC) $(HDRS) | check-sdl3
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(SDL3_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

# the MT-32 emulator (Munt mt32emu, LGPL 2.1+): C++, built into a static library
CXX ?= c++
MT32_SRC = $(wildcard third_party/mt32emu/*.cpp) $(wildcard third_party/mt32emu/sha1/*.cpp) third_party/mt32emu/c_interface/c_interface.cpp third_party/mt32emu/srchelper/InternalResampler.cpp $(wildcard third_party/mt32emu/srchelper/srctools/src/*.cpp)
MT32_OBJ = $(patsubst third_party/mt32emu/%.cpp,build/mt32emu/%.o,$(MT32_SRC))
build/mt32emu/%.o: third_party/mt32emu/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -O2 -w -DMT32EMU_WITH_INTERNAL_RESAMPLER=1 -Ithird_party/mt32emu -c $< -o $@
build/libmt32emu.a: $(MT32_OBJ)
	$(AR) rcs $@ $^

# The inherited clan shell cannot create a Mercenaries career. Keep its source
# as reference.
mw2 mw2shell:
	@echo "The MW2 clan shell is a reference only in this fork; build with make mercs."
	@exit 2

glview: tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc -Ithird_party $(filter %.c,$^) $(filter %.o,$^) -o $@ $(SDL3_CFLAGS) $(SDL3_LIBS) -lm -lpthread $(DLLIB)

gpu: $(GPU_BIN)
# Compatibility target name only; both tools use SDL_GPU.
gl: gpu

mechview3d: tools/mechview3d.c src/anim.c src/mek.c src/mech3d.c src/render.c src/tex.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@ -lm

mechview: tools/mechview.c src/anim.c src/mek.c src/mech3d.c src/render.c src/tex.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@ -lm

bwdtool: tools/bwdtool.c src/mtbl.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@

mektool: tools/mektool.c src/mek.c src/datapath.c src/prj.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@

cdtool: tools/cdtool.c src/cdaudio.c build/miniaudio.o
	$(CC) $(CFLAGS) -Isrc $^ -o $@ $(LDLIBS)

test_port: tests/test_port.c src/cdaudio.c src/datapath.c build/miniaudio.o
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $^ -o $@ $(LDLIBS)

test_titanium: tests/test_titanium.c src/tex.c src/skygnd.c src/portcfg.c src/prj.c src/datapath.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@ -lm

test_gamefolder: tests/test_gamefolder.c src/portcfg.c src/prj.c src/portcfg.h
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_cockpitcfg: tests/test_cockpitcfg.c src/cockpitcfg.c src/cockpitcfg.h
	$(CC) $(CFLAGS) -Isrc tests/test_cockpitcfg.c src/cockpitcfg.c -o $@

test_inputmap: tests/test_inputmap.c tools/inputmap.h tools/inputmap_defaults.h src/datapath.c src/datapath.h
	$(CC) -std=c99 -D_DEFAULT_SOURCE -Itools -Isrc $(SDL3_CFLAGS) tests/test_inputmap.c src/datapath.c $(SDL3_LIBS) -o $@

# Focused Mercenaries checks; original archives remain external test fixtures.
MERCS_TEST_BIN = test_mercs_ammunition test_mercs_controls test_mercs_world_lod test_mercs_results test_mercs_result_transport test_mercs_career test_mercs_mission_return test_mercs_exchange test_mercs_roundtrip test_mercs_paths test_mercs_lance test_mercs_gps_ai test_mercs_loadout test_mercs_combat test_mercs_return test_mercs_ai_trait test_mercs_ai_transport test_mercs_prj_checksum test_mercs_environment test_mercs_backdrop test_mercs_overlay test_mercs_inputmap test_mercs_audio test_mercs_voice test_mercs_voice_audio test_mercs_hud test_mercs_cockpit test_mercs_cockpit_render test_mercs_cockpit_depth test_mercs_palette_render test_mercs_world_render test_mercs_timing test_mercs_resources test_mercs_effects test_mercs_fx_area
MERCS_TEST_BIN += test_mercs_ground_render test_mercs_texture_filter test_mercs_economy test_mercs_ground test_mercs_particles test_mercs_particle_events
MERCS_TEST_BIN += test_mercs_special test_mercs_orders
MERCS_TEST_BIN += test_mercs_mission_dispatch test_mercs_nuclear_wave
MERCS_TEST_BIN += test_mercs_arrival test_mercs_playthrough_world_ray
MERCS_TEST_BIN += test_mercs_projectile_terrain test_mercs_projectile_effect_points
MERCS_TEST_BIN += test_mercs_ammo_capacity
MERCS_TEST_BIN += test_mercs_palette_cards 
MERCS_TEST_BIN += test_mercs_fall
MERCS_TEST_BIN += test_mercs_hud_controls test_mercs_bwdplace test_mercs_map test_mercs_map_render
MERCS_TEST_BIN += test_mercs_firegroups
MERCS_TEST_BIN += test_mercs_hardware_weapons test_mercs_rmg test_mercs_ortho test_mercs_scores
MERCS_TEST_BIN += test_mercs_economy_ui
MERCS_TEST_BIN += test_mercs_mechlab test_mercs_mechlab_ui test_mercs_instant test_mercs_instant_ui
MERCS_TEST_BIN += test_mercs_pose test_mercs_texture_clocks test_mercs_audio_events test_mercs_aircraft
MERCS_TEST_BIN += test_mercs_cockpit_motion test_mercs_cockpit_motion_render
MERCS_TEST_BIN += test_mercs_orphan_ammunition test_mercs_quadtree test_mercs_alliance test_mercs_targeting test_mercs_lock_timing test_mercs_ai_los
MERCS_TEST_BIN += test_mercs_polygon_normals
MERCS_TEST_BIN += test_mercs_aircraft_coordinates
GAME ?= ../game-data/Mercs-game
MW2_REFERENCE ?= ../reference-game-data/MW2-game
GBL_REFERENCE ?= ../reference-game-data/GBL-game
check: mercs $(MERCS_TEST_BIN)
	python3 tests/run_mercs_checks.py --game "$(GAME)" --mw2 "$(MW2_REFERENCE)" --gbl "$(GBL_REFERENCE)" $(CHECK_ARGS)

MERCS_SHELL_TEST_BIN = test_mercs_shell_data test_mercs_shell_sim test_mercs_video test_mercs_text test_mercs_campaign test_mercs_campaign_tail test_mercs_save test_mercs_debrief test_mercs_debrief_view test_mercs_debrief_assets test_mercs_repair test_mercs_repair_view test_mercs_statistics test_mercs_frontend test_mercs_startup test_mercs_viewport test_mercs_art test_mercs_shell_audio test_mercs_dialog test_mercs_gamemenu test_mercs_panels test_mercs_shell_interaction test_mercs_combat_variables test_mercs_controls_view
MERCS_SHELL_TEST_BIN += test_mercs_economy test_mercs_mechlab test_mercs_interchange test_mercs_interchange_ui test_mercs_scores test_mercs_aerotech test_mercs_economy_original
check-shell: mercs $(MERCS_SHELL_TEST_BIN)
	python3 tests/run_mercs_shell_checks.py --game "$(GAME)" $(SHELL_CHECK_ARGS)

check-combat-variables: mercs test_mercs_combat_variables test_mercs_roundtrip
	python3 tests/run_combat_variables_checks.py --game "$(GAME)"

check-return:
	python3 tests/run_mission_return_checks.py --game "$(GAME)"

check-fixed-player: mercs test_mercs_fixed_player
	python3 tests/run_fixed_player_checks.py --game "$(GAME)"

check-campaign:
	python3 tests/run_campaign_checks.py --game "$(GAME)"

check-packaging:
	python3 tests/test_macos_mercs_app.py

clean:
	rm -rf build test_mercs_gpu_shell test_mercs_hud_backend test_mercs_gpu_settings $(BIN) $(GPU_BIN) $(MERCS_TEST_BIN) $(MERCS_SHELL_TEST_BIN) test_mercs_fixed_player test_mercs_playthrough test_mercs_playthrough_career test_mercs_playthrough_geometry mercs mercs.exe mw2 mw2.exe missionsim

# The foundation packaging scripts have not been adapted or validated for Mercs.
dist:
	@echo "Mercenaries release packaging is not configured; build with make mercs."
	@exit 2

.PHONY: all tools app check check-shell check-combat-variables check-return check-fixed-player check-campaign check-packaging clean dist mw2 mw2shell

# The objective sweep: every mission run headless by a perfect player (tools/sweep_objectives.c).
# make sweep MW2=/path/to/install   (SWEEP_ARGS="-v CYANSCN1" for one scene, verbose)
SWEEP_SRC = tools/sweep_objectives.c src/lance.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c
sweep_objectives: $(SWEEP_SRC) $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(SWEEP_SRC) -o $@ -lm
sweep_objectives_asan: $(SWEEP_SRC) $(HDRS)
	$(CC) -std=c99 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc $(SWEEP_SRC) -o $@ -lm
sweep: sweep_objectives
	MW2_INSTALL_DIR=$(MW2) ./sweep_objectives $(MW2)/MW2.PRJ $(SWEEP_ARGS)
.PHONY: sweep

# walking sweep (tests only): tools/sweep_walk.py drives glview (MW2_WALK_TRACE hooks); sweep_ai reports stuck AI mechs
sweep_ai: tools/sweep_ai.c src/msim.c src/mercsresult.c src/fx.c src/ai.c src/combat.c src/mtbl.c src/anim.c src/mek.c src/mech3d.c src/wtb.c src/bwd.c src/bwdplace.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Wno-conversion -Isrc $(filter %.c,$^) -o $@ -lm
sweep-walk: glview sweep_ai
	python3 tools/sweep_walk.py -o /tmp/walk $(WALK_ARGS)
	for m in $$(./bwdtool ../MW2-game/3d/models.prj missions | awk '/SCN/{print $$1}'); do MW2_INSTALL_DIR=$${MW2_INSTALL_DIR:-/tmp/gtest} ./sweep_ai ../MW2-game/3d/models.prj $$m 300 attack; done | grep aistuck
.PHONY: sweep-walk

# scripted in-mission action sequences, headless (tests/sequences/run_sequences.py; SEQ_ARGS="--only NAME -j 2")
# MW2_GAME_DIR (default ../MW2-game) and MW2_GTEST (an installed game directory, default /tmp/gtest)
test_sequences: glview
	python3 tests/sequences/run_sequences.py --glview ./glview $(SEQ_ARGS)

# the same under AddressSanitizer / UBSan (a separate binary, build/glview_asan)
test_sequences_asan: tools/glview.c src/sfx.c src/portcfg.c src/cdaudio.c build/miniaudio.o $(RENDER_SRC) $(HDRS)
	$(CC) -std=c99 -D_DEFAULT_SOURCE -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -Isrc -Ithird_party $(filter %.c,$^) $(filter %.o,$^) -o build/glview_asan $(SDL3_CFLAGS) $(SDL3_LIBS) -lm -lpthread $(DLLIB)
	ASAN_OPTIONS=detect_leaks=0 SEQ_TIME_SCALE=6 python3 tests/sequences/run_sequences.py --glview build/glview_asan $(SEQ_ARGS)
.PHONY: test_sequences test_sequences_asan

# Keep a missing/old SDK diagnostic ahead of parallel compiler failures.
.PHONY: check-sdl3
check-sdl3:
	@$(SDL3_PKG) --exists 'sdl3 >= $(SDL3_MIN_VERSION)' 2>/dev/null || { \
		printf '%s\n' 'SDL3 >= $(SDL3_MIN_VERSION) development files and pkg-config are required.' \
		  'Install SDL3, or set PKG_CONFIG_PATH to the directory containing sdl3.pc. See BUILDING.md.' >&2; \
		exit 1; }

mercs \
    build/mercs_sim.o \
    test_mercs_debrief_view \
    test_mercs_debrief_assets \
    test_mercs_repair_view \
    test_mercs_frontend \
    test_mercs_startup \
    test_mercs_shell_interaction \
    test_mercs_combat_variables \
    test_mercs_controls_view \
    test_mercs_economy_ui \
    test_mercs_mechlab_ui \
    test_mercs_instant_ui \
    test_mercs_interchange_ui \
    test_mercs_viewport \
    test_mercs_art \
    test_mercs_shell_audio \
    test_mercs_dialog \
    test_mercs_gamemenu \
    test_mercs_panels \
    test_mercs_hud_controls \
    test_mercs_ortho \
    test_mercs_map_render \
    test_mercs_inputmap \
    test_mercs_audio \
    test_mercs_voice_audio \
    test_mercs_text \
    test_mercs_cockpit_render \
    test_mercs_cockpit_depth \
    test_mercs_palette_render \
    test_mercs_world_render \
    test_mercs_world_lod \
    test_mercs_ground_render \
    test_mercs_texture_filter \
    test_mercs_economy \
    test_mercs_video \
    glview \
    test_inputmap \
    test_sequences_asan: | check-sdl3

# Backend shaders are checked in; ordinary builds need no shader compiler.
shaders:
	python3 scripts/gen_shaders.py $(if $(SHADER_TOOLS),--tools $(SHADER_TOOLS))
.PHONY: shaders gpu gl

# Shared real-GPU setup/readback helper used by the migrated renderer fixtures.
test_mercs_cockpit_depth test_mercs_cockpit_render test_mercs_ground_render \
    test_mercs_map_render test_mercs_ortho test_mercs_palette_render \
    test_mercs_texture_filter test_mercs_world_lod test_mercs_world_render \
    test_mercs_dedicated_particle_palette: tests/gpu_test.h

test_mercs_gpu_settings: tests/test_mercs_gpu_settings.c src/portcfg.c src/prj.c src/datapath.c $(HDRS)
	$(CC) $(CFLAGS) -Isrc $(filter %.c,$^) -o $@

test_mercs_hud_backend: tests/test_mercs_hud_backend.c src/hud.c src/prj.c src/shp.c src/datapath.c $(RB_SRC) $(HDRS) | check-sdl3
	$(CC) $(CFLAGS) -Wno-conversion -DSDL_MAIN_HANDLED -Isrc $(SDL3_CFLAGS) $(filter %.c,$^) -o $@ $(SDL3_LIBS) -lm

test_mercs_gpu_shell: build/mercs/tests/test_mercs_gpu_shell.o $(filter-out build/mercs/tools/mercs.o build/mercs/tools/mercsshell.o,$(MERCS_OBJ)) build/miniaudio.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL3_LIBS) $(LDLIBS) $(DLLIB) $(WINLIBS)
