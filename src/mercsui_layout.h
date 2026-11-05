#ifndef MERCS_UI_LAYOUT_H
#define MERCS_UI_LAYOUT_H
#include <stddef.h>

/* Authored retail Mercenaries 640x480 shell geometry.
 * Recovered from original MW2SHELL.DLL constructors/tables and verified
 * against the original DOS shell; DOS main-menu placement is verified in
 * MW2SHELL.EXE at 0x6d9ab--0x6db70. Resource IDs are converted from 1-based
 * original IDs to zero-based DATABASE.MW2 indexes. No replacement art.
 *
 * Hotspot bounds remain original x1/y1/x2/y2, not width/height. Text
 * coordinates are left origins unless the original string begins with ~.
 * Sprite flags are ORIGINAL state flags: 1 paused, 4 hold last, 8 loop, 0x20 hidden.
 * Constructors explicitly show visible 0x20 sprites; the caller must apply
 * that state, select SHP frame 0/1, and honor each condition.
 * See tests/fixtures/MERCS_ORIGINAL_SHELL_LAYOUT.md for evidence and flow.
 */
typedef struct {
    int index, x1, y1, x2, y2, tx, ty;
    const char *label;
} mercs_ui_hotspot;
typedef struct {
    const char *name;
    int x, y;
    unsigned flags;
    const char *condition;
} mercs_ui_sprite;

enum { MERCS_UI_FONT_INPUT = 5, MERCS_UI_FONT_MENU = 6,
       MERCS_UI_FONT_BODY = 10 };

enum { ui_mainmenu_background = 40 };
static const mercs_ui_hotspot ui_mainmenu_hotspots[] = {
    {0, 415, 20, 624, 95, 0, 0, ""},
    {1, 415, 96, 624, 174, 0, 0, ""},
    {2, 415, 175, 624, 253, 0, 0, ""},
    {3, 415, 254, 624, 330, 0, 0, ""},
    {4, 415, 331, 624, 401, 0, 0, ""},
    {5, 415, 20, 624, 95, 0, 0, ""},
    {6, 415, 111, 624, 210, 0, 0, ""},
    {7, 415, 258, 624, 335, 0, 0, ""},
};
static const mercs_ui_sprite ui_mainmenu_sprites[] = {
    {"smercs", 437, 40, 0x21u, "always"},
    {"smechw", 437, 119, 0x21u, "always"},
    {"smrcnt", 437, 200, 0x21u, "always"},
    {"sinst", 437, 277, 0x21u, "always"},
    {"soptns", 437, 352, 0x21u, "always"},
};

enum { ui_mainhall_background = 15 };
static const mercs_ui_hotspot ui_mainhall_hotspots[] = {
    {0, 413, 256, 525, 414, 419, 265, "BOARD DROP SHIP"},
    {1, 209, 267, 319, 371, 243, 375, "TERMINAL"},
    {2, 72, 247, 165, 422, 125, 249, "OFFICE"},
    {3, 511, 105, 602, 162, 144, 133, NULL},
};
static const mercs_ui_sprite ui_mainhall_sprites[] = {
    {"smon", 239, 291, 0x8u, "always"},
    {"slightb", 513, 103, 0x8u, "always"},
    {"sdoorab", 95, 259, 0x4u, "returning from office"},
    {"sdoora", 95, 259, 0x44u, "office door activation; next event0xc01a"},
    {"sdoorb", 438, 286, 0x44u, "board drop ship activation; next event0xc033"},
};

enum { ui_news_background = 35 };
static const mercs_ui_hotspot ui_news_hotspots[] = {
    {0, 510, 423, 621, 474, 0, 0, ""},
    {1, 493, 3, 623, 402, 0, 0, ""},
};
static const mercs_ui_sprite ui_news_sprites[] = {
    {"nsexit", 523, 438, 0x21u, "always"},
    {"mletter", 0, 0, 0x4u, "mode1 and initial year3; 8x8 audio clip710frames"},
};
enum { ui_news_text_x = 34, ui_news_text_y = 67,
       ui_news_text_w = 429, ui_news_text_h = 378 };

enum { ui_computer_background = 33 };
static const mercs_ui_hotspot ui_computer_hotspots[] = {
    {0, 13, 10, 296, 125, 0, 0, ""},
    {1, 13, 126, 296, 240, 0, 0, ""},
    {2, 13, 241, 296, 356, 0, 0, ""},
    {3, 13, 357, 296, 471, 0, 0, ""},
    {4, 511, 425, 617, 473, 0, 0, ""},
};
static const mercs_ui_sprite ui_computer_sprites[] = {
    {"offlogo", 352, 111, 0x28u, "always"},
    {"ooexit", 525, 436, 0x21u, "always"},
    {"oocont", 19, 18, 0x21u, "always"},
    {"oopers", 19, 135, 0x21u, "always"},
    {"ooinvt", 19, 251, 0x21u, "always"},
    {"oofina", 19, 366, 0x21u, "always"},
};

enum { ui_office_background = 14 };
static const mercs_ui_hotspot ui_office_hotspots[] = {
    {0, 538, 102, 639, 297, 532, 83, "DUTY ROSTER"},
    {1, 417, 183, 500, 385, 449, 149, "MECH BAY"},
    {2, 89, 160, 210, 354, 112, 147, "STARPORT"},
    {3, 138, 355, 319, 463, 278, 339, "COMPUTER"},
    {4, 230, 164, 436, 311, 292, 122, "STAR CHART"},
};
static const mercs_ui_sprite ui_office_sprites[] = {
    {"omon", 156, 363, 0x28u, "always"},
    {"starmap", 263, 296, 0x28u, "always"},
    {"offdoorc", 103, 194, 0x44u, "entering from starport"},
    {"offdoord", 427, 157, 0x44u, "entering from mech bay"},
    {"offdoora", 103, 194, 0x44u, "starport door activation; next0xc01c"},
    {"offdoorb", 427, 157, 0x44u, "mech bay door activation; next0xc01b"},
};

enum { ui_registration_background = 23 };
static const mercs_ui_hotspot ui_registration_hotspots[] = {
    {0, 502, 431, 602, 472, 289, 365, ""},
    {1, 502, 431, 602, 472, 289, 365, ""},
    {2, 317, 196, 520, 224, 0, 0, ""},
};
static const mercs_ui_sprite ui_registration_sprites[] = {
    {"oraccp", 510, 441, 0x21u, "always"},
    {"rcomstar", 51, 84, 0x20u, "always"},
};

enum { ui_contracts_background = 32 };
static const mercs_ui_hotspot ui_contracts_hotspots[] = {
    {0, 401, 431, 500, 473, 0, 0, ""},
    {1, 501, 431, 602, 473, 0, 0, ""},
    {2, 36, 379, 129, 426, 0, 0, ""},
    {3, 130, 379, 227, 426, 0, 0, ""},
};
static const mercs_ui_sprite ui_contracts_sprites[] = {
    {"ocbcn", 138, 391, 0x21u, "always"},
    {"ocbcp", 47, 389, 0x21u, "always"},
    {"ocbca", 409, 440, 0x21u, "always"},
    {"ocbce", 510, 440, 0x21u, "always"},
};
enum { ui_contracts_text_x = 47, ui_contracts_text_y = 65,
       ui_contracts_text_w = 549, ui_contracts_text_h = 286 };

enum { ui_dropship_cabin_background = 42 };
static const mercs_ui_hotspot ui_dropship_cabin_hotspots[] = {
    {0, 563, 232, 639, 313, 490, 216, "ABORT CAMPAIGN"},
    {1, 208, 209, 351, 297, 230, 201, "MISSION COMMAND"},
};
static const mercs_ui_sprite ui_dropship_cabin_sprites[] = {
    {"dmon", 220, 211, 0x68u, "always"},
    {"dthing", 302, 98, 0x68u, "always"},
    {"drop", 575, 233, 0x68u, "always"},
};

enum { ui_dropship_computer_background = 43 };
static const mercs_ui_hotspot ui_dropship_computer_hotspots[] = {
    {0, 13, 7, 238, 97, 0, 0, ""},
    {1, 13, 98, 238, 191, 0, 0, ""},
    {2, 13, 192, 238, 286, 0, 0, ""},
    {3, 13, 287, 238, 378, 0, 0, ""},
    {4, 13, 379, 238, 469, 0, 0, ""},
    {5, 13, 98, 238, 191, 0, 0, ""},
    {6, 13, 192, 238, 286, 0, 0, ""},
    {7, 13, 287, 238, 378, 0, 0, ""},
    {8, 512, 426, 618, 471, 0, 0, ""},
};
static const mercs_ui_sprite ui_dropship_computer_sprites[] = {
    {"lship", 350, 115, 0x28u, "always"},
    {"dsbe", 525, 437, 0x21u, "always"},
    {"dsbf", 19, 11, 0x21u, "mode1 finances"},
    {"dsbi", 19, 104, 0x21u, "mode1 inventory"},
    {"dsbd", 19, 198, 0x21u, "mode1 duty roster"},
    {"dsbr", 19, 292, 0x21u, "mode1 repair/reload"},
    {"dsbm", 19, 385, 0x21u, "mode1 launch mission"},
    {"cashboxc", 474, 0, 0x21u, "mode1"},
    {"dsnmlab", 19, 104, 0x21u, "mode0 Mech Lab"},
    {"dsnduty", 19, 198, 0x21u, "mode0 duty roster"},
    {"dsnlnch", 19, 292, 0x21u, "mode0 launch mission"},
};

enum { ui_mission_briefing_background = 36 };
static const mercs_ui_hotspot ui_mission_briefing_hotspots[] = {
    {0, 212, 0, 423, 40, 0, 0, ""},
    {1, 104, 416, 275, 470, 0, 0, ""},
    {2, 400, 314, 600, 359, 0, 0, ""},
    {3, 447, 257, 548, 300, 0, 0, ""},
    {4, 338, 423, 503, 471, 0, 0, ""},
    {5, 400, 360, 600, 405, 0, 0, ""},
    {6, 504, 427, 600, 473, 0, 0, ""},
};
static const mercs_ui_sprite ui_mission_briefing_sprites[] = {
    {"lbav", 456, 264, 0x21u, "always"},
    {"lbas", 114, 422, 0x21u, "always"},
    {"lbal", 351, 433, 0x21u, "always"},
    {"lbbe", 510, 440, 0x21u, "always"},
};
enum { ui_mission_briefing_text_x = 34, ui_mission_briefing_text_y = 29,
       ui_mission_briefing_text_w = 301, ui_mission_briefing_text_h = 370 };

enum { ui_briefing_portrait_x = 355, ui_briefing_portrait_y = 22,
       ui_briefing_portrait_flags = 0x68,
       ui_register_callsign_x = 330, ui_register_callsign_y = 312,
       ui_register_unit_x = 330, ui_register_unit_y = 350 };

/* 0x1001db60, 0x1001d450 and callback 0x1001e090. PREV/NEXT change
 * the selected pilot's assignment immediately; there is no Assign button. */
enum { ui_duty_roster_background = 34 };
static const mercs_ui_hotspot ui_duty_roster_hotspots[] = {
    {0, 500, 430, 602, 475, 0, 0, ""},
    {1, 394, 430, 499, 475, 0, 0, ""},
    {2, 427, 289, 523, 340, 0, 0, ""},
    {3, 524, 289, 615, 340, 0, 0, ""},
};
static const mercs_ui_sprite ui_duty_roster_sprites[] = {
    {"cashboxd", 470, 0, 0x21u, "mode1"},
    {"bdrexit", 509, 440, 0x21u, "always"},
    {"bdrprev", 439, 298, 0x21u, "always"},
    {"bdrnext", 527, 299, 0x21u, "always"},
};
enum { ui_roster_header_center_x = 210, ui_roster_header_y = 89,
       ui_roster_pilot_x = 37, ui_roster_assignment_x = 150,
       ui_roster_first_row_y = 115, ui_roster_row_step = 10,
       ui_roster_assignment_width = 230, ui_roster_row_height = 15,
       ui_roster_preview_x = 440, ui_roster_preview_y = 114,
       ui_roster_preview_flags = 0x68,
       ui_cursor_database = 3, ui_cursor_frame = 0 };

static const char *const ui_mainmenu_hover_text[] = {
    "Command a Mercenary Army (Economy Enabled)",
    "Join a Mercenary Army (Economy Disabled)",
    "Multiplayer Mercenaries",
    "Join Battle right away",
    "Game settings and options",
};
enum { ui_mainmenu_hover_x = 29, ui_mainmenu_hover_y = 442 };

/* Draw only the selected contract's faction, from 0x100406c0. */
static const mercs_ui_sprite ui_contracts_faction_logos[] = {
    {"comstar", 492, 172, 0x61u, "matching faction"},
    {"draconis", 402, 158, 0x61u, "matching faction"},
    {"fedcom", 383, 146, 0x61u, "matching faction"},
    {"freerass", 424, 160, 0x61u, "matching faction"},
};

#endif
