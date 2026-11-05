#!/usr/bin/env python3
"""Exercise saved careers with an instrumented, input-only mission pilot.

The optional test executable exposes observations and accepts ordinary SDL
keyboard/mouse input. No outcome/objective, health, heat, movement, ammunition
or RNG mutation hook is enabled. This is automated native gameplay, not a
human playthrough or original-binary execution. Every attempt (including a
loss or timeout) is retained; a failed mission is never promoted to a win.
"""
import argparse
import datetime
import gzip
import hashlib
import heapq
import json
import math
import os
from pathlib import Path
import subprocess
import signal
import threading
import time
import traceback

from run_combat_variables_checks import ROOT, environment, fixture
from mercs_campaign_plan import analyse as campaign_plan


def angle(v):
    return (v + 180.0) % 360.0 - 180.0


def distance(a, b):
    return math.hypot(a[0] - b[0], a[2] - b[2])


def engagement_range(weapons):
    """Choose a movement band; actual firing still uses each weapon's range."""
    live = [w for w in weapons if w['state'] >= 0 and w['ammo'] != 0]
    energy = [w for w in live if w['ammo'] == -1]
    # A strict majority of equal-range energy mounts forms the main battery.
    # One longer laser must not set a band that leaves four medium lasers
    # idle. Tied/mixed batteries retain the longer energy range; finite
    # ranged phases can still explicitly override this ordinary movement.
    if len(energy) >= 2:
        for weapon in energy:
            if sum(w['range'] == weapon['range'] for w in energy) > len(energy) / 2:
                return weapon['range']
    # A surviving single energy mount still defines a sustainable firing
    # band. Retaining the missile maximum after torso loss stranded the
    # IVA2 pilot outside its head laser's range. Explicit ranged phases
    # below may still choose finite ammunition for a deliberate engagement.
    return max((w['range'] for w in (energy or live)), default=9000)


def spa2_harpoon_goal(state, kind, target):
    """Only the explicit authored SPA2 main harpoon destroy goal uses this flow."""
    return (kind == 'building' and state.get('scene', '').upper().startswith('SPA2')
            and target.get('record', '').casefold() == 'spa2are4'
            and tuple(target.get('plan_node') or ()) == (state.get('player_table', 0), 2))


# Settled SPA2 harpoon faces inferred from the actual S_1HARPA triangles
# and captured native thing points/bounds. Neither a bounding-box corner nor
# an early moving-path pose is used as a solid target. Current point and all
# six bounds must agree within 2 cm; otherwise the guided midpoint flow stays
# in charge. See campaign-playthrough/spa2-anchor-review for fit/ray evidence.
SPA2_SETTLED_SURFACES = (([-10256.42285, 223562.01562, 127639.16406], [-11358.29297, 199607.01562, 93526.98438, -9202.29395, 247536.6875, 161986.29688], [-9931.982718741605, 202220.19064380103, 96417.94284068495]), ([10255.99512, 223559.79688, 127640.64062], [9177.96875, 199588.85938, 93539.64062, 11333.96875, 247550.42188, 161976.54688], [9902.296161007622, 202203.3774106477, 96429.71859526057]))


def spa2_harpoon_surface(state, kind, target):
    """A bounded current-mesh inference for direct energy fire, or unknown."""
    if not spa2_harpoon_goal(state, kind, target):
        return None
    point, bounds = target.get('thing_point'), target.get('bounds')
    if not isinstance(point, (list, tuple)) or not isinstance(bounds, (list, tuple)):
        return None
    if len(point) != 3 or len(bounds) != 6:
        return None
    if not all(math.isfinite(v) for v in (*point, *bounds)):
        return None
    for expected_point, expected_bounds, surface in SPA2_SETTLED_SURFACES:
        if (max(abs(a-b) for a,b in zip(point, expected_point)) <= 2.0
                and max(abs(a-b) for a,b in zip(bounds, expected_bounds)) <= 2.0):
            return list(surface)
    return None


def selected_thing_locked(state, target):
    """Observe the actual ordinary selector and the native encoded lock target."""
    return (state.get('target', -1) == -1 and state.get('thing') == target['id']
            and state.get('lock_target') == -1000 - target['id']
            and bool(state.get('missile_lock', 0) & 0x80))


def gra3_return_throttle_cap(state, body_heading, reverse, cap):
    """Brake an ordinary approach into an armed premature-return sphere."""
    if not state.get('scene', '').upper().startswith('GRA3'):
        return cap, None
    nodes = {n['id']: n for table in state.get('tables', [])
             if table['id'] == state.get('player_table', 0) for n in table['nodes']}
    if nodes.get(1, {}).get('state') == 5 or nodes.get(2, {}).get('state') != 5:
        return cap, None
    pos = state['position']
    physical = math.radians(body_heading + (180 if reverse else 0))
    for node_id in (3, 4):
        node = nodes.get(node_id, {})
        if node.get('state') in (5, 6):
            continue
        nav = next((n for n in state['navs']
                    if n.get('name', '').casefold() == node.get('target', '').casefold()), None)
        if nav is None:
            continue
        gap = math.dist(pos, nav['position'])
        toward = ((nav['position'][0] - pos[0]) * math.sin(physical)
                  + (nav['position'][2] - pos[2]) * math.cos(physical))
        margin = 8000 + max(0, (state.get('geometry') or {}).get('radius', 0))
        if gap <= nav['radius'] + margin and toward > 0:
            return 0.0, dict(node=node_id, nav=nav['name'], distance_cm=gap,
                             radius_cm=nav['radius'], braking_margin_cm=margin)
    return cap, None


def gra3_world_clear(state, actor):
    """Fresh native world segment to the current posed point, not a hit promise."""
    query = actor.get('world_obstruction') or {}
    eye, point = state.get('weapon_eye', state.get('camera')), actor.get('aim')
    if not query.get('available') or not isinstance(eye, (list, tuple)) or not isinstance(point, (list, tuple)):
        return False
    for measured, current in ((query.get('from'), eye), (query.get('to'), point)):
        if (not isinstance(measured, (list, tuple)) or len(measured) != 3 or len(current) != 3
                or not all(math.isfinite(v) for v in (*measured, *current))
                or max(abs(a-b) for a,b in zip(measured, current)) > .1):
            return False
    return query.get('blocked') == 0


def gra3_friendly_screen(state, target, endpoint=None):
    """Conservative live-mech screen for ordinary trigger withholding only.

    A posed point and height form a deliberately broad sphere. This is not a
    native hull hit or a claim about the source of historical allied damage.
    """
    eye = state.get('weapon_eye', state.get('camera'))
    point = endpoint if endpoint is not None else target.get('aim')
    if eye is None or point is None:
        return None
    delta = [point[i] - eye[i] for i in range(3)]
    length2 = sum(v*v for v in delta)
    if length2 <= 0:
        return None
    for actor in state['actors']:
        if (actor['destroyed'] or actor.get('group', '').casefold()
                not in ('gra3usr1', 'gra3usr2')):
            continue
        centre = actor.get('aim')
        if centre is None:
            continue
        fraction = sum((centre[i]-eye[i])*delta[i] for i in range(3)) / length2
        if not 0 < fraction < 1:
            continue
        separation = math.dist(centre, [eye[i]+fraction*delta[i] for i in range(3)])
        radius = max(300.0, actor.get('height', 0) * .75)
        if separation <= radius:
            return dict(actor=actor['id'], fraction=fraction,
                        separation_cm=separation, conservative_radius_cm=radius)
    return None


def target_weapon_available(state, kind, target, weapon):
    """Can this ordinary selection acquire the guided lock we require?"""
    if (state.get('scene', '').upper().startswith('GRA1') and kind == 'actor'
            and target.get('group', '').casefold() == 'gra1ens3'
            and weapon.get('ammo', -1) > 0 and weapon.get('per_volley', 1) > 1):
        # The required demolition group stands near protected water. Choose
        # direct single-projectile guns/energy here; do not add missile splash.
        return False
    if spa2_harpoon_goal(state, kind, target) and weapon.get('guided'):
        point = target.get('thing_point')
        gap = target.get('lock_distance_cm', -1)
        return (isinstance(point, (list, tuple)) and len(point) == 3
                and all(math.isfinite(value) for value in point)
                and weapon.get('lock_min_range_cm', float('inf')) < gap <= weapon['range'])
    if kind != 'actor' or not weapon.get('guided'):
        return True
    selected = state.get('selected_weapon', -1)
    mounts = state.get('weapons', [])
    if (target.get('narc_beacon') or
            (state.get('missile_lock', 0) & 0x80 and state.get('lock_target') == target['id']
             and 0 <= selected < len(mounts) and weapon is mounts[selected])):
        return True  # Preserve actual selected locks and the native NARC exception.
    if not target.get('targetable', 1):
        return False
    if 'lock_distance_cm' not in target or 'lock_min_range_cm' not in weapon:
        return True  # Older retained observation schema, not an invented minimum.
    return weapon['lock_min_range_cm'] < target['lock_distance_cm'] <= weapon['range']


def widened(value):
    if value & 0x730:
        value |= 0x730
    if value & 3:
        value |= 3
    return value


def intersects(a, b, bounds, margin=0):
    """Read-only line/bounds query used to choose a navigable firing position."""
    enter, leave = 0.0, 1.0
    for i in range(3):
        lo, hi = bounds[i] - margin, bounds[i + 3] + margin
        delta = b[i] - a[i]
        if abs(delta) < 1e-6:
            if a[i] < lo or a[i] > hi:
                return False
        else:
            u, v = (lo - a[i]) / delta, (hi - a[i]) / delta
            if u > v: u, v = v, u
            enter, leave = max(enter, u), min(leave, v)
            if enter > leave: return False
    return enter < .985 and leave > .005


def visible(a, b, obstacles):
    return not any(intersects(a, b, o['bounds']) for o in obstacles
                   if not all(o['bounds'][i] <= b[i] <= o['bounds'][i + 3] for i in range(3)))


def path_to(pos, target, stop, obstacles, shoot=False, hazards=()):
    """Grid planning over observed boxes; results are only steering waypoints."""
    step, radius = 1500, 900
    start = (round(pos[0] / step), round(pos[2] / step))
    # Terrain follows the ordinary simulation; these boxes avoid buildings,
    # fences and bridges that cannot be walked through. Shapes 2/7 are safely
    # over-approximated, never bypassed in the game collision implementation.
    boxes = [o['bounds'] for o in obstacles if o['bounds'][4] > pos[1] + 300
             and o['bounds'][1] < pos[1] + 1500]
    bound = (min(pos[0], target[0]) - 50000, min(pos[2], target[2]) - 50000,
             max(pos[0], target[0]) + 50000, max(pos[2], target[2]) + 50000)
    occupied = {}
    def blocked(cell):
        if cell == start: return False
        if cell not in occupied:
            x, z = cell[0] * step, cell[1] * step
            occupied[cell] = any(b[0] - radius <= x <= b[3] + radius and
                                 b[2] - radius <= z <= b[5] + radius for b in boxes)
        return occupied[cell]
    def heuristic(cell):
        return max(0.0, math.hypot(cell[0] * step - target[0], cell[1] * step - target[2]) - stop)
    costs, came, todo = {start: 0.0}, {}, [(heuristic(start), start)]
    goal = None
    while todo and len(costs) < 90000:
        _, here = heapq.heappop(todo)
        point = [here[0] * step, pos[1] + 750, here[1] * step]
        if heuristic(here) <= 0 and (not shoot or visible(point, target, obstacles)):
            goal = here; break
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            q = (here[0] + dx, here[1] + dz)
            if not (bound[0] <= q[0]*step <= bound[2] and bound[1] <= q[1]*step <= bound[3]): continue
            if blocked(q) or (dx and dz and (blocked((here[0]+dx, here[1])) or blocked((here[0], here[1]+dz)))): continue
            stride = math.hypot(dx, dz) * step
            danger = sum(max(0.0, 1.0 - math.hypot(q[0] * step - h[0], q[1] * step - h[2]) / 35000)
                         for h in hazards)
            cost = costs[here] + stride * (1 + danger * 8)
            if cost >= costs.get(q, float('inf')): continue
            costs[q] = cost; came[q] = here
            heapq.heappush(todo, (cost + heuristic(q), q))
    if goal is None: return []
    cells = []
    while goal != start:
        cells.append([goal[0] * step, pos[1], goal[1] * step]); goal = came[goal]
    cells.reverse()
    return cells


def weapon_selection_key(s, wanted):
    """Use ordinary within-group cycling or next-group input from observations."""
    weapons = s['weapons']
    selected = s['selected_weapon']
    if not (0 <= wanted < len(weapons)) or selected == wanted:
        return None
    if not (0 <= selected < len(weapons)):
        return 'enter'
    return ('quote' if weapons[selected].get('group', 0) != weapons[wanted].get('group', 0)
            else 'enter')


def sol3_combined_loadout(state):
    """Scope normal mixed-group setup to the observed AC10/medium design."""
    if not state.get('scene', '').upper().startswith('SOL3'):
        return False
    weapons = state['weapons']
    return (sum(w['id'] == 47 for w in weapons) == 1
            and any(w['id'] == 56 for w in weapons)
            and all(w['id'] in (47, 56) and not w.get('guided')
                    and w.get('per_volley', 1) == 1
                    and (w['ammo'] >= 0 if w['id'] == 47 else w['ammo'] == -1)
                    for w in weapons if w['state'] >= 0))


def sol3_empty_group_gun(state, weapon):
    """An observed empty AC10 cannot be admitted by the native group mask."""
    return (state.get('scene', '').upper().startswith('SOL3')
            and weapon['id'] == 47 and weapon['ammo'] == 0
            and not weapon.get('guided') and weapon.get('per_volley', 1) == 1)


def sol3_combined_battery(state):
    """Return a fully observed group, never assign or alter the observation."""
    if not sol3_combined_loadout(state):
        return []
    weapons = state['weapons']
    if any(w['state'] >= 0 and 'group' not in w for w in weapons):
        return []
    battery = [(i, w) for i, w in enumerate(weapons)
               if w['state'] >= 0 and w['ammo'] != 0]
    if (not any(w['id'] == 47 for i, w in battery)
            or not any(w['id'] == 56 for i, w in battery)):
        return []
    observed = {i for i, w in enumerate(weapons) if w['state'] >= 0
                and w['group'] == 0 and not sol3_empty_group_gun(state, w)}
    return battery if observed == {i for i, w in battery} else []


def solaris_medium_battery(state):
    """Use the SOL2 battery or a confirmed SOL3 all-medium fallback group."""
    scene = state.get('scene', '').upper()
    if not scene.startswith(('SOL2', 'SOL3')):
        return []
    weapons = state['weapons']
    if any(w['state'] >= 0 and w['ammo'] > 0 and not w.get('guided')
           and w.get('per_volley', 1) == 1 for w in weapons):
        return []
    battery = [(i, w) for i, w in enumerate(weapons)
               if w['id'] == 56 and w['state'] >= 0 and w['ammo'] == -1]
    if len(battery) < 2:
        return []
    if scene.startswith('SOL3'):
        if any(w['state'] >= 0 and 'group' not in w for w in weapons):
            return []
        # Observe an exact medium group after the finite gun is gone. Mixed
        # setup may leave an empty AC10 in group 0; native cannot admit it.
        # Do not attempt a late reassignment or admit another weapon type.
        members = {i for i, w in battery}
        observed = {i for i, w in enumerate(weapons)
                    if w['state'] >= 0 and w.get('group') == 0
                    and not sol3_empty_group_gun(state, w)}
        if observed != members:
            return []
    return battery


def sol3_world_flank(state, actor, energy_range, active=False):
    """Choose ordinary lateral travel only for a fresh, nearby terrain block."""
    if not state.get('scene', '').upper().startswith('SOL3'):
        return False
    query = actor.get('world_obstruction') or {}
    if not query.get('available') or query.get('blocked') != 1 or query.get('hit_collision_type') != 5:
        return False
    for measured, current in ((query.get('from'), state.get('weapon_eye')),
                              (query.get('to'), actor.get('aim'))):
        if (not isinstance(measured, (list, tuple)) or not isinstance(current, (list, tuple))
                or len(measured) != 3 or len(current) != 3
                or not all(math.isfinite(v) for v in (*measured, *current))
                or max(abs(a-b) for a,b in zip(measured, current)) > .1):
            return False
    # Enter inside the medium band and use a wider release band to avoid
    # repeatedly changing between a direct approach and the flank.
    return distance(state['position'], actor['position']) <= energy_range * (1.1 if active else .9)


class Pilot:
    """A deterministic test pilot; all decisions become ordinary input."""
    def __init__(self, duration=1200):
        self.duration = duration
        self.actor_history = {}
        self.actor_velocity = {}
        self.samples = 0
        self.last_position = None
        self.last_move = 0.0
        self.last_report = -10
        self.goal = None
        self.route = []
        self.route_at = -100
        self.summary = []
        self.retreat = False
        self.last_hold = []
        self.orbit = 1
        self.abort_phase = 0
        self.abort_reason = None
        self.operator_abort_reason = None
        self.stuck_until = 0.0
        self.escape_heading = 0.0
        self.last_throttle_demand = 0.0
        self.mcc2_route_leg = 0
        self.iva1_patrol_anchor = None
        self.iva2_north_passed = False
        self.iva2_south_passed = False
        self.iva2_defense_target = None
        self.iva3_bridge_leg = 0
        self.spa1_engine_leg = 0
        self.spa2_engine_leg = 0
        self.thing_select_next_time = 0.0
        self.nav_progress_key = None
        self.nav_progress_distance = float('inf')
        self.nav_progress_time = 0.0
        self.nav_progress_position = None
        self.nav_progress_anchor = None
        self.geometry_side = 0
        self.geometry_decision = None

    def configure_sol3_combined(self, s):
        """Put the known AC10 and mediums together through ordinary keys."""
        if not sol3_combined_loadout(s):
            return None
        if any(w['state'] >= 0 and 'group' not in w for w in s['weapons']):
            self.operator_abort_reason = 'Combined battery lacks actual fire-group observations'
            return dict(frames=1, press=[], hold=[], mouse=(0, 0),
                        group_setup='missing-combined-observation')
        pending = [i for i, w in enumerate(s['weapons'])
                   if w['state'] >= 0 and w['ammo'] != 0 and w['group'] != 0]
        if not pending:
            if s.get('group_fire'):
                return dict(frames=1, press=['backslash'], hold=[], mouse=(0, 0),
                            group_setup='combined-chain-mode')
            return None
        if s['time'] > 12:
            self.operator_abort_reason = 'Ordinary combined group setup did not complete'
            return dict(frames=1, press=[], hold=[], mouse=(0, 0),
                        group_setup='combined-setup-progress-budget')
        chosen = s['selected_weapon'] if s['selected_weapon'] in pending else pending[0]
        if chosen == s['selected_weapon']:
            return dict(frames=1, press=['shift+1'], hold=[], mouse=(0, 0),
                        group_setup={'assign_combined_weapon': chosen})
        key = weapon_selection_key(s, chosen)
        return dict(frames=1, press=[key] if key and key != 'enter' else [],
                    hold=['enter'] if key == 'enter' and 'enter' not in self.last_hold else [],
                    mouse=(0, 0), group_setup={'select_combined_weapon': chosen})

    def configure_solaris_mediums(self, s):
        """Assign only medium lasers to group 3 using actual observed key effects."""
        battery = solaris_medium_battery(s)
        if not battery:
            return None
        if s.get('scene', '').upper().startswith('SOL3'):
            # solaris_medium_battery has already verified group 0. Preserve
            # it even when the gun empties long after the setup deadline.
            if s.get('group_fire'):
                return dict(frames=1, press=['backslash'], hold=[], mouse=(0, 0),
                            group_setup='verified-sol3-chain-mode')
            return None
        members = {i for i, w in battery}
        if not all('group' in w for w in s['weapons']):
            self.operator_abort_reason = 'Medium battery lacks actual fire-group observations'
            return dict(frames=1, press=[], hold=[], mouse=(0, 0),
                        group_setup='missing-medium-observation')
        pending = [i for i, w in enumerate(s['weapons']) if w['state'] >= 0
                   and w['group'] != (2 if i in members else 0)]
        if not pending:
            # Semicolon fires the selected group directly. Keep Space in
            # ordinary chain mode for the longer-range fallback weapons.
            if s.get('group_fire'):
                return dict(frames=1, press=['backslash'], hold=[], mouse=(0, 0),
                            group_setup='medium-chain-mode')
            return None
        if s['time'] > 12:
            self.operator_abort_reason = 'Ordinary medium group setup did not complete'
            return dict(frames=1, press=[], hold=[], mouse=(0, 0),
                        group_setup='medium-setup-progress-budget')
        chosen = s['selected_weapon'] if s['selected_weapon'] in pending else pending[0]
        if chosen == s['selected_weapon']:
            return dict(frames=1, press=['shift+3' if chosen in members else 'shift+1'],
                        hold=[], mouse=(0, 0), group_setup={'assign_medium_weapon': chosen})
        key = weapon_selection_key(s, chosen)
        return dict(frames=1, press=[key] if key and key != 'enter' else [],
                    hold=['enter'] if key == 'enter' and 'enter' not in self.last_hold else [],
                    mouse=(0, 0), group_setup={'select_medium_weapon': chosen})

    def configure_gra1_guns(self, s):
        """Assign the mission gun battery by normal keys, with observed confirmation."""
        solaris = s.get('scene', '').upper().startswith(('SOL1', 'SOL2', 'SOL3'))
        if not solaris and not s.get('scene', '').upper().startswith(('GRA1', 'GRA2', 'GRA3')):
            return None
        guns = [i for i, w in enumerate(s['weapons'])
                if w['id'] == (47 if solaris else 46) and w['state'] >= 0]
        if len(guns) < (1 if solaris else 2):
            return None
        if not all('group' in w for w in s['weapons']):
            self.operator_abort_reason = 'Gun setup lacks actual fire-group observations'
            return dict(frames=1, press=[], hold=[], mouse=(0, 0),
                        group_setup='missing-observation')
        pending = [i for i, w in enumerate(s['weapons']) if w['state'] >= 0
                   and w['group'] != (2 if i in guns else 0)]
        if not pending:
            if s.get('group_fire'):
                return dict(frames=1, press=['backslash'], hold=[], mouse=(0, 0),
                            group_setup='ordinary-chain-mode')
            return None
        if s['time'] > 12:
            self.operator_abort_reason = 'Ordinary gun group setup did not complete'
            return dict(frames=1, press=[], hold=[], mouse=(0, 0),
                        group_setup='setup-progress-budget')
        chosen = s['selected_weapon'] if s['selected_weapon'] in pending else pending[0]
        if chosen == s['selected_weapon']:
            return dict(frames=1, press=['shift+3' if chosen in guns else 'shift+1'],
                        hold=[], mouse=(0, 0), group_setup={'assign_weapon': chosen})
        key = weapon_selection_key(s, chosen)
        return dict(frames=1, press=[key] if key and key != 'enter' else [],
                    hold=['enter'] if key == 'enter' and 'enter' not in self.last_hold else [],
                    mouse=(0, 0), group_setup={'select_weapon': chosen})

    def tactical_plan(self, s, plan):
        """Choose ordinary travel/wait inputs from authored trigger evidence."""
        scene = s.get('scene', '').upper()
        if scene.startswith(('SOL1', 'SOL2', 'SOL3')):
            # Solaris has only required arena opponents. Keep focus on a
            # currently world-clear opponent and let the authored empty
            # groups finish normally; spawn markers are not destinations.
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            defenders = [g for g in plan['goals'] if g[0] == 'actor'
                         and not g[1]['destroyed']]
            if defenders:
                clear = [g for g in defenders if gra3_world_clear(s, g[1])]
                choices = clear or defenders
                if scene.startswith('SOL2'):
                    current = next((g for g in defenders if self.goal == ('actor', g[1]['id'])), None)
                    if current is not None and not gra3_world_clear(s, current[1]):
                        occluded = getattr(self, 'solaris_occlusion', None)
                        if occluded is None or occluded[0] != self.goal:
                            self.solaris_occlusion = (self.goal, s['time'])
                        # A brief terrain interruption must not permanently
                        # abandon a damaged opponent. Keep ordinary routing
                        # and all fire guards; permit another clear target
                        # after five seconds of continuous obstruction.
                        if s['time'] - self.solaris_occlusion[1] <= 5.0:
                            choices = [current]
                    else:
                        self.solaris_occlusion = None
                chosen = next((g for g in choices if self.goal == ('actor', g[1]['id'])),
                              min(choices, key=lambda g: math.dist(
                                  s.get('weapon_eye', s['camera']), g[1].get('aim', g[1]['position']))))
                ordered = [chosen] + [g for g in defenders if g[1]['id'] != chosen[1]['id']]
                plan['goals'] = [(kind, dict(actor, plan_role='arena-defense'), stop)
                                 for kind, actor, stop in ordered]
            plan['reasons'] = list(plan['reasons']) + ['Focus required arena opponents using fresh world obstruction observations']
            return plan
        if scene.startswith('GRA3'):
            # Clear the ship defenders before returning through Alpha's
            # large betrayal spheres. Friendly Thug/Dragon remain protected
            # even if a later authored branch changes their behavior.
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            friendly = [a for a in s['actors'] if a.get('group', '').casefold()
                        in ('gra3usr1', 'gra3usr2')]
            plan['protected_actor_ids'] = sorted(set(plan['protected_actor_ids'])
                                                 | {a['id'] for a in friendly})
            defenders = [a for a in s['actors'] if not a['destroyed']
                         and a.get('group', '').casefold() == 'gra3ens1']
            if defenders:
                # Prefer an actually world-clear required opponent. The
                # former box-only check spent finite rounds into unobserved
                # terrain; preserve focus only while its fresh segment clears.
                clear = [a for a in defenders if gra3_world_clear(s, a)
                         and gra3_friendly_screen(s, a) is None]
                candidates = clear or defenders
                chosen = next((a for a in candidates if self.goal == ('actor', a['id'])),
                              min(candidates, key=lambda a: math.dist(
                                  s.get('weapon_eye', s['camera']), a.get('aim', a['position']))))
                ordered = [chosen] + [a for a in defenders if a['id'] != chosen['id']]
                plan['goals'] = [('actor', dict(a, plan_role='ship-defense',
                                               plan_node=(s.get('player_table', 0), 1)),
                                  engagement_range(s['weapons']) * .7) for a in ordered]
                plan.pop('movement_anchor', None)
            plan['reasons'] = list(plan['reasons']) + ['Clear ENS1 while preserving both Draconis groups; brake before armed return spheres']
            return plan
        if scene.startswith('GRA2'):
            # Choose the authored pass route explicitly. Jenner's early
            # departure can fail the mission even while pursuing the ship.
            # Meet it at the observed start, then inspect the Catapult with
            # ordinary target/inspect input before engaging the remaining
            # required groups. The assisting Catapult stays alliance 1.
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            plan.pop('movement_anchor', None)
            nodes = {n['id']: n for t in s.get('tables', [])
                     if t['id'] == s.get('player_table', 0) for n in t['nodes']}
            groups = {name: [a for a in s['actors'] if not a['destroyed']
                             and a.get('group', '').casefold() == name]
                      for name in ('gra2ens1', 'gra2ens2', 'gra2ens3', 'gra2ens4')}
            cats = groups['gra2ens3']
            plan['protected_actor_ids'] = sorted(set(plan['protected_actor_ids'])
                                                 | {a['id'] for a in cats})
            goals = []
            for group, node in (('gra2ens1', 2), ('gra2ens2', 3), ('gra2ens4', 5)):
                for actor in groups[group]:
                    target = dict(actor, plan_role='pass-intercept',
                                  plan_node=(s.get('player_table', 0), node))
                    goals.append(('actor', target, engagement_range(s['weapons']) * .7))
            if groups['gra2ens1']:
                start = next((n for n in s['navs'] if n.get('name', '').casefold() == 'gra2st01'), None)
                if start:
                    anchor = dict(start, plan_role='pass-intercept-anchor', plan_node=None)
                    plan['movement_anchor'] = ('nav', anchor, 2000)
            elif cats and nodes.get(6, {}).get('state') != 5:
                scan = dict(cats[0], plan_role='pass-support-inspect',
                            plan_node=(s.get('player_table', 0), 6))
                goals.insert(0, ('inspect', scan, 12000))
            plan['goals'] = goals
            plan['waiting'] = not goals
            plan['objective_nodes'] = [(s.get('player_table', 0), n) for n in (1, 2, 3, 5, 6)]
            plan['reasons'] = list(plan['reasons']) + ['Intercept Jenner, inspect and preserve Catapult, then clear required pass groups']
            return plan
        if scene.startswith('GRA1'):
            # Main water protection resolves to ENS3. The first actual attempt
            # passed the immediate ENS1/ENS2 pursuers and exposed rear CT.
            # Defend at the observed start against Crab, then Sentinel, while
            # keeping an optional group alive in our target choices. Clearing
            # BOTH complete groups would release the water-demolition order.
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            first_group = [a for a in s['actors'] if not a['destroyed']
                           and a.get('group', '').casefold() == 'gra1ens1']
            second_group = [a for a in s['actors'] if not a['destroyed']
                            and a.get('group', '').casefold() == 'gra1ens2']
            defenders = first_group if second_group else []
            if not first_group and len(second_group) > 1:
                defenders = [a for a in second_group if a.get('name', '').casefold() == 'dl sentinel']
            start = next((n for n in s['navs'] if n.get('name', '').casefold() == 'gra1st01'), None)
            if defenders and start and plan['goals']:
                chosen = next((a for a in defenders if self.goal == ('actor', a['id'])), defenders[0])
                defender = dict(chosen, plan_role='initial-defense', plan_node=None)
                plan['goals'] = [('actor', defender, engagement_range(s['weapons']) * .7)] + plan['goals']
                anchor = dict(start, plan_role='initial-defense-anchor', plan_node=None)
                plan['movement_anchor'] = ('nav', anchor, 2000)
                plan['reasons'] = list(plan['reasons']) + ['Face initial pursuers at the observed start while retaining an optional group']
            else:
                plan['reasons'] = list(plan['reasons']) + ['Keep required ENS3 as the destination; do not clear the remaining optional group first']
            return plan
        if scene.startswith('SPA2'):
            # The hidden 300-second harpoon deadline is already running.
            # Aim at those actual world goals from supported northern ice;
            # their far midpoint and aircraft coordinates are never walking
            # destinations. The north anchor has about 104 m to the ice edge.
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            harpoons = [g for g in plan['goals'] if spa2_harpoon_goal(s, g[0], g[1])]
            if harpoons:
                chosen = next((g for g in harpoons if self.goal == ('building', g[1]['id'])),
                              min(harpoons, key=lambda g: distance(s['position'], g[1]['position'])))
                plan['goals'] = [chosen] + [g for g in plan['goals'] if g != chosen]
                point = [0, 199776, 80000]
            else:
                # ENS4 attacks ARE1's engines, not the western centre.
                # Reuse the north bend/east defense position actually driven
                # during SPA1, with live support still governing each move.
                bend = [-25000, 199776, 50000]
                if distance(s['position'], bend) < 4000:
                    self.spa2_engine_leg = 1
                point = bend if not self.spa2_engine_leg else [45000, 199776, 40000]
                # The optional Crab attacks the player after its normal hold.
                # Finish that close pursuer while facing it at the north
                # anchor. The first live attempt exposed rear CT while
                # travelling east with the Crab outside the torso cone.
                attacks = [g for g in plan['goals'] if g[0] == 'actor']
                eye = s.get('weapon_eye', s['position'])
                maximum = max((w['range'] for w in s['weapons']
                               if w['state'] >= 0 and w['ammo'] != 0), default=0)
                def aim_distance(goal):
                    return math.dist(eye, goal[1].get('aim', goal[1]['position']))
                nearby = [g for g in attacks if aim_distance(g) <= maximum * .92]
                defenders = [('actor', a, engagement_range(s['weapons']) * .7)
                             for a in s['actors'] if not a['destroyed'] and a['side'] == 1
                             and a.get('group', '').casefold() == 'spa2ens1']
                if defenders:
                    point = [0, 199776, 80000]
                choices = defenders or nearby or attacks
                if choices:
                    chosen = next((g for g in choices if self.goal == ('actor', g[1]['id'])),
                                  min(choices, key=aim_distance))
                    plan['goals'] = [chosen] + [g for g in plan['goals'] if g != chosen]
            anchor = ('nav', dict(id=-3500 if harpoons else -3501-self.spa2_engine_leg,
                                  name='SPA2 supported harpoon anchor' if harpoons else 'SPA2 north bend and engine defense',
                                  position=point, plan_role='deck-defense', plan_node=None), 2000)
            plan['movement_anchor'] = anchor
            if not plan['goals']:
                plan['goals'] = [anchor]
            plan['reasons'] = list(plan['reasons']) + ['Timed harpoons first; keep all travel on the supported ice anchors']
            return plan
        if scene.startswith('SPA1'):
            # Aircraft are aim targets, never walking destinations on this
            # raised ship. Protect the command-centre waves first, then cross
            # north of the central ice mound to intercept the engine wave.
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            nodes = {n['id']: n for t in s.get('tables', []) if t['id'] == 0
                     for n in t['nodes']}
            engines = nodes.get(4, {}).get('state') == 5 and nodes.get(5, {}).get('state') == 5
            if engines:
                bend = [-25000, 199776, 50000]
                if distance(s['position'], bend) < 4000: self.spa1_engine_leg = 1
                point = bend if not self.spa1_engine_leg else [45000, 199776, 40000]
            else:
                point = [-45000, 199776, 45000]
            anchor = ('nav', dict(id=-3400-(1 if engines else 0)-self.spa1_engine_leg,
                                  name='SPA1 protected deck position', position=point,
                                  plan_role='deck-defense', plan_node=None), 2500)
            plan['movement_anchor'] = anchor
            attacks = [g for g in plan['goals'] if g[0] == 'actor']
            if attacks:
                maximum = max((w['range'] for w in s['weapons']
                               if w['state'] >= 0 and w['ammo'] != 0), default=0)
                # Class-2 fighters may use an authored root Y far below the
                # aircraft mesh. Rank the observed posed aim point from the
                # actual weapon eye, not the root/nav coordinate. This is a
                # current-point range estimate, not a lock or hit guarantee.
                eye = s.get('weapon_eye', s['position'])
                def aim_distance(goal):
                    return math.dist(eye, goal[1].get('aim', goal[1]['position']))
                nearby = [g for g in attacks if aim_distance(g) <= maximum * .92]
                choices = nearby or attacks
                urgent = ('spa1ens4',) if engines else ('spa1ens2', 'spa1ens3')
                priority = min(0 if g[1].get('group', '').casefold() in urgent else 1 for g in choices)
                choices = [g for g in choices if (0 if g[1].get('group', '').casefold() in urgent else 1) == priority]
                chosen = next((g for g in choices if self.goal == ('actor', g[1]['id'])),
                              min(choices, key=aim_distance))
                plan['goals'] = [chosen] + [g for g in plan['goals'] if g != chosen]
            else:
                plan['goals'] = [anchor]
            plan['reasons'] = list(plan['reasons']) + ['Aim at required aircraft while travelling only between protected deck positions']
            return plan
        if scene.startswith('IVA3'):
            # The briefing's escape uses Rho's bridge. Its far end lies west
            # of GOU2, which is an enemy waypoint within the deck. Keep the
            # whole player on the observed authored centerline before the
            # turn toward Sigma (or the later alternate escape).
            plan = dict(plan)
            plan['route_quiet'] = True
            plan['allow_fallback'] = False
            nodes = {n['id']: n for t in s.get('tables', []) if t['id'] == 0
                     for n in t['nodes']}
            rho = next((n for n in s['navs'] if n.get('name', '').casefold() == 'iva3nav1'), None)
            reached_rho = nodes.get(1, {}).get('state') == 5 or bool(rho and rho.get('visited'))
            # The live Sigma attempt met a steep mountain before its timed
            # exit. The already-active spy predicate is an authored alternate.
            # Original terrain faces support this bounded bank corridor;
            # live contact/support observations still govern every step.
            points = ([20000, 26400, 83270], [-30000, 26400, 83270],
                      [-45000, 25838, 80000], [-45000, 17637, 28000])
            if reached_rho and nodes.get(2, {}).get('state') != 5:
                while self.iva3_bridge_leg < len(points) and distance(s['position'], points[self.iva3_bridge_leg]) < 350:
                    self.iva3_bridge_leg += 1
                if self.iva3_bridge_leg < len(points):
                    names = ('IVA3 bridge alignment', 'IVA3 bridge west clearance',
                             'IVA3 western bank bend', 'IVA3 southern bank corridor')
                    target = dict(id=-3300-self.iva3_bridge_leg,
                                  name=names[self.iva3_bridge_leg],
                                  position=points[self.iva3_bridge_leg], plan_node=None,
                                  plan_role='precision-bridge-route' if self.iva3_bridge_leg < 2 else 'precision-escape-route')
                    plan['goals'] = [('nav', target, 100)]
                    plan['waiting'] = False
                    reason = ('Keep the normal escape route on the bridge centerline until beyond its west end'
                              if self.iva3_bridge_leg < 2 else
                              'Follow the original terrain bank corridor toward the active alternate extraction')
                    plan['reasons'] = list(plan['reasons']) + [reason]
                elif nodes.get(5, {}).get('state') == 3:
                    spy = next((n for n in s['navs']
                                if n.get('name', '').casefold() == nodes[5].get('target', '').casefold()), None)
                    if spy:
                        target = dict(spy, plan_node=[0, 5], plan_role='precision-escape-route')
                        plan['goals'] = [('nav', target, max(100, spy['radius'] * .5))]
                        plan['waiting'] = False
                        plan['reasons'] = list(plan['reasons']) + ['Reach the active authored spy-contact alternative without waiting for Sigma to fail']
            return plan
        if scene.startswith('IVA2'):
            nodes = {n['id']: n for t in s.get('tables', []) if t['id'] == 0
                     for n in t['nodes']}
            if nodes.get(7, {}).get('state') != 5:
                # The previous quiet strikes left close pursuers firing at
                # the rear throughout approach and extraction. Engage one
                # observed close Sentinel/Centurion/Flashman while multiple
                # energy mounts still survive, then resume the real route.
                pursuers = [a for a in s['actors'] if a['side'] == 1 and a['armed']
                            and not a['destroyed'] and a.get('group', '').casefold() in ('iva2ens1', 'iva2ens2')]
                retained = next((a for a in pursuers if a['id'] == self.iva2_defense_target
                                 and math.dist(s['position'], a['position']) <= 65000), None)
                self.iva2_defense_target = retained['id'] if retained else None
                if (nodes.get(3, {}).get('state') == 5 and retained is None
                        and sum(w['state'] >= 0 and w['ammo'] == -1 for w in s['weapons']) >= 2):
                    close = [a for a in pursuers if a.get('targetable', 1)
                             and math.dist(s['position'], a['position']) <= 25000]
                    if close:
                        retained = min(close, key=lambda a: distance(s['position'], a['position']))
                        self.iva2_defense_target = retained['id']
                if retained:
                    plan = dict(plan)
                    target = dict(retained, plan_node=None, plan_role='pursuer-defense')
                    plan['goals'] = [('actor', target, 18000)] + list(plan['goals'])
                    plan['allow_fallback'] = False
                    plan['route_quiet'] = True
                    plan['waiting'] = False
                    plan['reasons'] = list(plan['reasons']) + ['Defeat the observed close pursuer before continuing the airfield strike']
                    return plan
                # The authored diversion is proximity to GOU1, not the
                # optional truck kill. Alpha/Beta/Gamma then trace the
                # briefing's ice-cover approach to the Delta airfield.
                # These are observed navs and ordinary travel requests;
                # only the real node state advances this sequence.
                for node_id in (2, 3, 4, 5):
                    node = nodes.get(node_id)
                    if not node or node['state'] in (5, 6):
                        continue
                    nav = next((n for n in s['navs']
                                if n['name'].casefold() == node.get('target', '').casefold()
                                and widened(n.get('class', 0)) & node.get('kind', 0)), None)
                    if nav is None:
                        continue
                    target = dict(nav, plan_node=[0, node_id], plan_role='diversion-route')
                    stop = max(100, nav['radius'] * .65)
                    if (node_id == 5 and not self.iva2_north_passed
                            and nodes.get(6, {}).get('state') not in (5, 6)):
                        trigger = next((n for n in s['navs'] if n['name'].casefold() == 'iva2gou2'), None)
                        if trigger:
                            point = [trigger['position'][0] - 10000, trigger['position'][1],
                                     trigger['position'][2] + 20000]
                            if distance(s['position'], point) < 4000:
                                self.iva2_north_passed = True
                            else:
                                # A controller waypoint around the authored
                                # 100m Flashman trigger, not a new game nav.
                                target = dict(id=-3201, name='IVA2 northern bend', position=point,
                                              plan_node=None, plan_role='diversion-route')
                                stop = 2500
                    plan = dict(plan)
                    plan['goals'] = [('nav', target, stop)]
                    plan['allow_fallback'] = False
                    plan['route_quiet'] = True
                    plan['waiting'] = False
                    plan['objective_nodes'] = list(plan['objective_nodes']) + [[0, node_id]]
                    plan['reasons'] = list(plan['reasons']) + ['IVA2 observed diversion and ice-cover approach before the airfield']
                    return plan
                airfield = nodes.get(7, {})
                targets = [b for b in s.get('buildings', []) if b['hp'] > 0
                           and b.get('record', '').casefold() == airfield.get('target', '').casefold()
                           and widened(b.get('type', 0)) & airfield.get('kind', 0)]
                if targets:
                    # Work through the same required area in a nearby,
                    # sticky order. Optional patrols must not replace these
                    # fragile structures during the strike-and-extract leg.
                    targets.sort(key=lambda b: (self.goal != ('building', b['id']),
                                                 distance(s['position'], b['position'])))
                    plan = dict(plan)
                    plan['goals'] = [('building', dict(b, plan_node=[0, 7], plan_role='airfield-strike'), 18000)
                                     for b in targets]
                    plan['allow_fallback'] = False
                    plan['route_quiet'] = True
                    plan['reasons'] = list(plan['reasons']) + ['Clear the observed required airfield targets, retaining each live target']
            else:
                plan = dict(plan)
                plan['route_quiet'] = True  # Preserve the escape destination after the strike.
                if not self.iva2_south_passed and nodes.get(13, {}).get('state') not in (5, 6):
                    point = [-135000, 0, -15000]
                    if distance(s['position'], point) < 5000:
                        self.iva2_south_passed = True
                    else:
                        # The preceding actual exit crossed close to the
                        # Stalker and GOU3. This ordinary west/south detour
                        # keeps the planned segments outside their reviewed
                        # areas; live geometry still decides local steering.
                        target = dict(id=-3202, name='IVA2 western extraction bend', position=point,
                                      plan_node=None, plan_role='diversion-route')
                        plan['goals'] = [('nav', target, 3500)]
                        plan['allow_fallback'] = False
                        plan['reasons'] = list(plan['reasons']) + ['Western/southern extraction detour; terrain remains observed locally']
            return plan
        if scene.startswith('IVA1'):
            nodes = {n['id']: n for t in s.get('tables', []) if t['id'] == 0
                     for n in t['nodes']}
            node = nodes.get(2)
            if node and node['state'] not in (5, 6):
                plant = [b for b in s.get('buildings', []) if b['hp'] > 0
                         and b.get('record', '').casefold() == node.get('target', '').casefold()
                         and widened(b.get('type', 0)) & 2]
                if plant:
                    # Adjacent rotating parts can exchange nearest order
                    # every few frames. Retain the live selected part until
                    # it is destroyed instead of repeatedly resetting aim.
                    plant.sort(key=lambda b: (self.goal != ('building', b['id']),
                                               distance(s['position'], b['position'])))
                    plan = dict(plan)
                    plan['goals'] = [('building', dict(b, plan_node=[0, 2], plan_role='turret-power'), 18000)
                                     for b in plant] + list(plan['goals'])
                    plan['reasons'] = list(plan['reasons']) + ['Destroy observed wind plant before the base turrets']
                    return plan
            if nodes.get(4, {}).get('state') == 5:
                # Both returning-patrol predicates are active from C0.0.
                # Fight them outside the enclosed base, with the intact
                # battery, before taking on nearby structures. Preserve the
                # actual post-turret position instead of inventing a route
                # into the northern mountains while the patrols are away.
                patrols = []
                for node_id in (9, 10):
                    objective = nodes.get(node_id, {})
                    if objective.get('state') in (5, 6):
                        continue
                    for actor in s['actors']:
                        if (not actor['destroyed']
                                and actor['group'].casefold() == objective.get('target', '').casefold()
                                and widened(actor.get('class', 0)) & objective.get('kind', 0)):
                            patrols.append((actor, node_id))
                if patrols:
                    if self.iva1_patrol_anchor is None:
                        self.iva1_patrol_anchor = tuple(s['position'])
                    retained = next((item for item in patrols
                                     if self.goal == ('actor', item[0]['id'])
                                     and math.dist(s['position'], item[0]['position']) < 70000), None)
                    nearby = [item for item in patrols
                              if math.dist(s['position'], item[0]['position']) <= 42000]
                    chosen = retained or (min(nearby, key=lambda item: distance(s['position'], item[0]['position']))
                                          if nearby else None)
                    plan = dict(plan)
                    plan['allow_fallback'] = False
                    plan['reasons'] = list(plan['reasons']) + ['Intercept mandatory returning patrols before entering the enclosed base']
                    if chosen:
                        ordered = [chosen] + [item for item in patrols if item != chosen]
                        plan['goals'] = [('actor', dict(actor, plan_node=[0, node_id],
                                                       plan_role='patrol-intercept'), 18000)
                                         for actor, node_id in ordered] + [g for g in plan['goals'] if g[0] != 'actor']
                    else:
                        target = dict(id=-3100, name='IVA1 observed patrol firing position',
                                      position=self.iva1_patrol_anchor, plan_node=[0, 9], plan_role='patrol-wait')
                        plan['goals'] = [('nav', target, 3000)]
                        plan['waiting'] = True
            return plan
        if not scene.startswith('MCC2'):
            return plan
        nodes = {(t['id'], n['id']): n for t in s.get('tables', []) for n in t['nodes']}
        state = lambda table, node: nodes.get((table, node), {}).get('state', 0)
        if state(0, 3) == 5 or state(0, 4) == 5:
            return plan
        if self.mcc2_route_leg == 0 and state(1, 4) == 5 and state(0, 1) == 6:
            self.mcc2_route_leg = 1
        # The previous live attempt disproved the assumed southern corridor:
        # terrain contact blocked it near spawn. Wait in place, then take the
        # northeast exit observed in the prior successful mission trace.
        points = {1: [-60000, 0, 5000], 2: [-58000, 0, 32000],
                  3: [65000, 0, 35000], 4: [65000, 0, 90000]}
        while self.mcc2_route_leg in points and distance(s['position'], points[self.mcc2_route_leg]) < 6000:
            self.mcc2_route_leg += 1
        if self.mcc2_route_leg >= 5:
            return plan
        plan = dict(plan)
        plan['allow_fallback'] = False
        plan['reasons'] = list(plan['reasons']) + ['MCC2 wait at spawn and eastern approach; observed patrol trigger state only']
        plan['waiting'] = self.mcc2_route_leg == 0
        if plan['waiting']:
            plan['goals'] = []
        else:
            target = dict(id=-2000-self.mcc2_route_leg, name='MCC2 safe approach waypoint',
                          position=points[self.mcc2_route_leg], plan_role='patrol-avoidance', plan_node=None)
            plan['goals'] = [('nav', target, 5000)]
        return plan

    def geometry_heading(self, s, requested, reverse, precision=False):
        """Turn/brake using isolated observations; never move the game object."""
        geometry = s.get('geometry')
        self.geometry_decision = None
        if geometry is None:
            return requested, 1.0, reverse  # Older recorded observation schema.
        if not geometry.get('available') or not geometry.get('directions'):
            self.geometry_decision = {'available': False, 'reason': 'unknown geometry; stopped'}
            return s['heading'], 0.0, reverse
        support_gap = max(0.0, geometry['feet_y'] - geometry['ground_y'])
        near_support = support_gap <= 125 and abs(geometry['vertical_speed']) <= .5
        if (geometry['airborne'] or abs(geometry['vertical_speed']) > .5) and not near_support:
            self.geometry_decision = {'available': True, 'reason': 'established fall; hold current heading and stop throttle'}
            return s['heading'], 0.0, bool(s.get('reverse', s['throttle'] < 0))
        physical_request = requested + (180 if reverse else 0)

        def safe(ray):
            # Samples are one metre apart. A gradual ramp can be traversable
            # even when a flat-height ray touches its upward face. A large
            # downward step remains unsafe even with no collision segment.
            if ray['max_step_down_cm'] > 125 or ray['max_step_up_cm'] > 125:
                return False
            return not ray['blocked'] or ray['hit_normal'][1] >= .7071

        by_heading = {}
        for ray in geometry['directions']:
            by_heading.setdefault(ray['angle_relative_deg'], []).append(ray)
        candidates = []
        for relative, rays in by_heading.items():
            rays.sort(key=lambda r: r['distance_cm'])
            clear = []
            for ray in rays:
                if not safe(ray): break
                clear.append(ray)
            if clear:
                ray = clear[-1]
                absolute = s['heading'] + relative
                delta = angle(absolute - physical_request)
                candidates.append((ray, absolute, delta))
        if not candidates:
            self.geometry_decision = {'available': True, 'reason': 'no observed clear direction; stopped'}
            return s['heading'], 0.0, reverse
        forward = by_heading.get(0.0, [])
        front_clear = bool(forward and safe(forward[0]))
        backing = [c for c in candidates if c[0]['distance_cm'] >= 1000
                   and abs(angle(c[1] - s['heading'])) > 110]
        if not front_clear and not reverse and backing:
            # Turning at zero throttle still creeps in the original controls.
            # Back away first when that creep would press into a nearby wall.
            chosen = min(backing, key=lambda c: abs(abs(angle(c[1] - s['heading'])) - 180))
            reverse = True
        else:
            chosen = None
        direct = [c for c in candidates if abs(c[2]) <= 11.251 and c[0]['distance_cm'] >= 3000]
        if chosen is not None:
            pass
        elif direct:
            self.geometry_side = 0
            chosen = min(direct, key=lambda c: abs(c[2]))
        else:
            def score(candidate):
                ray, absolute, delta = candidate
                opposite = self.geometry_side and delta * self.geometry_side < -1
                short = 80 if ray['distance_cm'] < 1000 else 35 if ray['distance_cm'] < 3000 else 0
                return abs(delta) + (80 if opposite else 0) + short - min(6000, ray['distance_cm']) / 2000
            chosen = min(candidates, key=score)
            if not self.geometry_side and abs(chosen[2]) > 1:
                self.geometry_side = 1 if chosen[2] > 0 else -1
        ray, physical, delta = chosen
        if not reverse and not front_clear and abs(angle(physical - s['heading'])) > 110:
            reverse = True  # Back out through a clear ray before turning.
        body = physical - (180 if reverse else 0)
        cap = 1.0 if ray['distance_cm'] >= 3000 else .331 if ray['distance_cm'] >= 1000 else .110
        if geometry['airborne'] or geometry['normal'][1] < .70 or ray['normal'][1] < .70:
            cap = min(cap, .331)
        interpolated = False
        if precision and not reverse and abs(delta) <= 11.251:
            # A narrow straight deck cannot use 22.5-degree heading snaps.
            # Permit the actual requested angle only between two observed
            # clear rays, keeping the shorter ray's throttle limit. This is
            # an input-planning inference; native contact remains decisive.
            nearby = [(c, angle(c[1] - physical_request)) for c in candidates
                      if c[0]['distance_cm'] >= 1000
                      and abs(angle(c[1] - physical_request)) <= 22.501]
            below = [c for c, d in nearby if d <= .0001]
            above = [c for c, d in nearby if d >= -.0001]
            if below and above:
                lower = min(below, key=lambda c: abs(angle(c[1] - physical_request)))
                upper = min(above, key=lambda c: abs(angle(c[1] - physical_request)))
                clear_cm = min(lower[0]['distance_cm'], upper[0]['distance_cm'])
                cap = min(cap, 1.0 if clear_cm >= 3000 else .331)
                body = requested
                interpolated = True
        self.geometry_decision = dict(available=True, relative=ray['angle_relative_deg'],
                                      clear_cm=ray['distance_cm'], max_throttle=cap,
                                      bypass_side=self.geometry_side, reverse=reverse,
                                      requested_angle_between_clear_rays=interpolated,
                                      max_step_down_cm=ray['max_step_down_cm'],
                                      max_step_up_cm=ray['max_step_up_cm'])
        return body, cap, reverse

    def command(self, s):
        self.samples += 1
        assert not s['invulnerable'] and not s['no_heat'], 'Gameplay cheats were enabled'
        assert not s.get('unlimited_ammo', 0), 'Unlimited ammunition was enabled'
        press, hold, dx, dy, frames = [], [], 0, 0, 6
        self.geometry_decision = None
        world_aim = None
        return_boundary = None
        fire_observation = None
        if s['outcome'] or s['destroyed'] or s['ending']:
            return dict(frames=30, press=['enter'] if s['ending'] else [], hold=[], mouse=(0, 0))
        combat_pending = any(n['main'] and n['state'] == 3 and n['kind'] in (1, 2) for n in s['objectives'])
        disarmed = combat_pending and not any(w['state'] >= 0 and w['ammo'] != 0 for w in s['weapons'])
        if s['time'] >= self.duration - 30 or disarmed or self.abort_phase or self.operator_abort_reason:
            sequence = ('esc', '1', 'enter')
            if not self.abort_phase:
                self.abort_reason = self.operator_abort_reason or ('No usable weapons remain' if disarmed else 'Attempt duration budget')
            key = sequence[min(self.abort_phase, len(sequence) - 1)]
            self.abort_phase += 1
            return dict(frames=6, press=[key], hold=[], mouse=(0, 0), diagnostic_abort=True,
                        reason=self.abort_reason or 'Continue normal Abort')
        group_setup = (self.configure_sol3_combined(s) if sol3_combined_loadout(s)
                       else self.configure_solaris_mediums(s) if solaris_medium_battery(s)
                       else self.configure_gra1_guns(s))
        if group_setup is not None:
            self.last_hold = list(group_setup['hold'])
            return group_setup
        if s['now'] < s['online_at'] + 500:
            return dict(frames=30, press=[], hold=[], mouse=(0, 0))
        pos = s['position']
        eye = s.get('weapon_eye', s['camera'])
        # Pose bob belongs in the current aim point, not in travel velocity.
        # Observe body displacement over about 0.2 s, including real Y motion.
        for actor in s['actors']:
            history = self.actor_history.setdefault(actor['id'], [])
            if not history or s['time'] > history[-1][0]:
                history.append((s['time'], tuple(actor['position'])))
            while len(history) > 2 and history[1][0] <= s['time'] - .2:
                history.pop(0)
            elapsed = s['time'] - history[0][0]
            self.actor_velocity[actor['id']] = (
                [(actor['position'][i] - history[0][1][i]) / elapsed for i in range(3)]
                if elapsed > 0 else [0.0, 0.0, 0.0])
        if self.last_position is None:
            self.last_position, self.last_move = pos, s['time']
        elif s['time'] - self.last_move > 2:
            geometry = s.get('geometry') or {}
            falling = geometry.get('airborne') or abs(geometry.get('vertical_speed', 0)) > .5 or abs(pos[1] - self.last_position[1]) > 300
            # Braking, reversing or turning can have little net displacement
            # while the chassis is moving normally. With geometry available,
            # recover only confirmed near-stationary contact, not those turns.
            # Native drive speed can remain high against a wall; use actual
            # observed displacement per second for this progress threshold.
            contact_stall = (not geometry or (s.get('blocked')
                             and distance(pos, self.last_position)
                             < 100 * (s['time'] - self.last_move)))
            if (self.last_throttle_demand and distance(pos, self.last_position) < 500
                    and s['time'] > self.stuck_until and not falling and contact_stall):
                if not geometry: self.orbit = -self.orbit
                elif self.geometry_side: self.orbit = self.geometry_side
                self.escape_heading = s['heading'] + self.orbit * 85
                self.stuck_until = s['time'] + 4
            self.last_position, self.last_move = pos, s['time']
        all_threats = [a for a in s['actors'] if a['side'] == 1 and a['armed'] and not a['destroyed']]
        # A distant fixed turret cannot pursue. Steer around it instead of
        # spending the mission's ammunition on every optional emplacement.
        threats = [a for a in all_threats if a.get('unit_class') != 3 or distance(pos, a['position']) < 18000]
        threats.sort(key=lambda a: distance(pos, a['position']))
        active_range = engagement_range(s['weapons'])
        plan = self.tactical_plan(s, campaign_plan(s))
        goals = plan['goals']
        # The planner resolves the authored prerequisite graph. Its order can
        # place a hidden support trigger or mandatory scan before a ready kill.
        # Preserve that order while aiming at a passing threat independently.
        protected_actors = set(plan['protected_actor_ids'])
        protected_buildings = set(plan['protected_building_ids'])
        required_actors = {g[1]['id'] for g in goals if g[0] == 'actor'}
        threats = [a for a in threats if a['id'] not in protected_actors]
        goal = goals[0] if goals else None
        if goal is None and plan['allow_fallback']:
            if threats:
                goal = ('actor', threats[0], active_range * .7)
            else:
                pending = [n for n in s['navs'] if n['selectable'] and not n['visited']]
                pending.sort(key=lambda n: distance(pos, n['position']))
                goal = ('nav', pending[0], max(100, pending[0]['radius'] * .5)) if pending else None
        if goal and goal[0] == 'nav':
            progress_key = (goal[0], goal[1]['id'], tuple(goal[1].get('plan_node') or ()))
            progress_distance = distance(pos, goal[1]['position'])
            escort_progress = (goal[1].get('plan_role') == 'escort-follow'
                               and self.nav_progress_position is not None
                               and distance(pos, self.nav_progress_position) > 1000
                               and distance(goal[1]['position'], self.nav_progress_anchor) > 1000)
            if (progress_key != self.nav_progress_key or escort_progress
                    or progress_distance < self.nav_progress_distance - 1000):
                self.nav_progress_key = progress_key
                self.nav_progress_distance = progress_distance
                self.nav_progress_time = s['time']
                self.nav_progress_position = tuple(pos)
                self.nav_progress_anchor = tuple(goal[1]['position'])
            if (s['time'] - self.nav_progress_time > 90
                    and progress_distance > goal[2] + 2000):
                self.abort_phase = 1
                self.abort_reason = 'Navigation made no material progress for 90 seconds'
                return dict(frames=6, press=['esc'], hold=[], mouse=(0, 0),
                            diagnostic_abort=True, reason=self.abort_reason)
        else:
            self.nav_progress_key = None
            self.nav_progress_distance = float('inf')
        destination = plan.get('movement_anchor') if goal and goal[0] in ('actor', 'building') else None
        if (not s.get('scene', '').upper().startswith('MCC2') and goal and threats
                and not plan.get('route_quiet')
                and goal[1].get('plan_role') != 'patrol-intercept'
                and (goal[0] == 'nav' or distance(pos, goal[1]['position']) > active_range * .85)):
            candidates = [a for a in threats
                          if distance(pos, a['position']) < active_range * .9
                          and abs(angle(math.degrees(math.atan2(a['position'][0] - pos[0],
                                                               a['position'][2] - pos[2])) - s['heading'])) < 75]
            if candidates:
                threat = candidates[0]
                if self.goal and self.goal[0] == 'actor':
                    threat = next((a for a in candidates if a['id'] == self.goal[1]), threat)
                destination = goal
                goal = ('actor', threat, active_range * .7)
        if goal is None:
            # A timed prerequisite can legitimately have no movement goal.
            # Release all controls instead of pursuing an unrelated patrol.
            self.route = []
            if abs(s['throttle']) > .01: press.append('1')
        if goal:
            kind, target, stop = goal
            precision_route = target.get('plan_role') in ('precision-bridge-route', 'precision-escape-route')
            if precision_route: frames = 1
            guided_thing = spa2_harpoon_goal(s, kind, target)
            surface = spa2_harpoon_surface(s, kind, target)
            energy_surface = bool(surface is not None and any(w['state'] >= 0 and w['ammo'] == -1 for w in s['weapons']))
            target_pos = (surface if energy_surface else list(target.get('thing_point') or target['position'])) if guided_thing else list(target.get('aim', target['position']))
            if guided_thing:
                world_aim = dict(mode='validated-settled-face-energy' if energy_surface else 'native-midpoint-guided',
                                 point_cm=list(target_pos), building=target['id'], observed_hp=target['hp'])
            def eligible(weapon):
                return (not energy_surface or weapon['ammo'] == -1) and target_weapon_available(s, kind, target, weapon)
            weapon_index = s['selected_weapon']
            selected = s['weapons'][weapon_index] if 0 <= weapon_index < len(s['weapons']) else None
            usable = []
            raw_gap = distance(pos, target_pos)
            solaris_actor = kind == 'actor' and s.get('scene', '').upper().startswith(('SOL1', 'SOL2', 'SOL3'))
            firing_gap = math.dist(eye, target_pos) if guided_thing or solaris_actor else raw_gap
            target_weapons = [w for w in s['weapons'] if eligible(w)]
            combined_loadout = sol3_combined_loadout(s)
            # A released trigger does not cancel a previously admitted pulse.
            # Reserve pending heat for single-gun fallback as well as groups.
            combined_pending_heat = (sum(w['heat'] * w.get('per_volley', 1)
                                         for w in s['weapons'] if w['state'] == 2)
                                     if combined_loadout else 0.0)
            live = [(i, w) for i, w in enumerate(s['weapons']) if w['state'] >= 0 and w['ammo'] != 0
                    and eligible(w)]
            energy_range = max((w['range'] for _, w in live if w['ammo'] == -1), default=0)
            long_range = [(i, w) for i, w in live if w['ammo'] > 0 and w['range'] > energy_range * 1.3]
            gra1_guns = [(i, w) for i, w in live if w['ammo'] > 0
                         and not w.get('guided') and w.get('per_volley', 1) == 1
                         and (w['range'] >= energy_range or (solaris_actor and w['id'] == 47))]
            gra1_ranged = bool(kind == 'actor' and gra1_guns
                               and ((s.get('scene', '').upper().startswith('GRA1')
                                     and (target.get('group', '').casefold() == 'gra1ens3'
                                          or target.get('plan_role') == 'initial-defense'))
                                    or (s.get('scene', '').upper().startswith('GRA2')
                                        and target.get('plan_role') == 'pass-intercept')
                                    or (s.get('scene', '').upper().startswith('GRA3')
                                        and target.get('plan_role') == 'ship-defense')
                                    or (solaris_actor and target.get('plan_role') == 'arena-defense'))
                               and target['id'] in required_actors)
            if gra1_ranged:
                long_range = gra1_guns
            # MCC3's required heavy groups can be engaged while withdrawing
            # toward artillery. Use retained finite ranged weapons first;
            # switch to the sustainable energy band once those are gone.
            ranged_phase = gra1_ranged or bool(kind == 'actor' and long_range
                                and s.get('scene', '').upper().startswith('MCC3')
                                and target['id'] in required_actors)
            for index, weapon in enumerate(s['weapons']):
                if weapon['state'] != 1 or weapon['ammo'] == 0:
                    continue
                if not eligible(weapon):
                    continue  # Unselectable actors and impossible lock windows need direct fire.
                if firing_gap > weapon['range'] * .92 or s['heat'] + combined_pending_heat + weapon.get('heat', 5) * weapon.get('per_volley', 1) > 52:
                    continue
                # Limited heavy ammunition is worth retaining for an armored
                # threat. Energy weapons can finish lightly damaged targets.
                if (weapon['ammo'] > 0 and weapon['ammo'] <= 4 and kind == 'actor'
                        and target['health'] < 350 and target['id'] not in required_actors):
                    continue
                if (weapon['ammo'] > 0 and kind == 'actor' and target['id'] not in required_actors
                        and s.get('scene', '').upper().startswith('MCC3')):
                    continue  # Keep finite rounds for the two required heavy groups.
                usable.append(index)
            available_range = engagement_range(target_weapons)
            preferred = None
            if guided_thing and not energy_surface and firing_gap > energy_range * .92:
                # At the supported long-range anchor, retain a live guided
                # mount through acquisition and its normal cooldown. Cycling
                # through energy mounts would repeatedly break acquisition.
                missiles = [(i, w) for i, w in live if w.get('guided')]
                if missiles:
                    preferred = next((i for i, w in missiles if i == weapon_index),
                                     next((i for i, w in missiles if w['state'] == 1), missiles[0][0]))
            if ranged_phase:
                available_range = min(w['range'] for _, w in long_range)
                if gra1_ranged:
                    # Retain the low-heat direct gun through ordinary cooldown
                    # and aim acquisition. Current heat/ammunition still gate
                    # every trigger; energy becomes the movement/fire basis
                    # when no such gun has ammunition or a surviving mount.
                    ready_guns = [i for i, w in long_range if i in usable]
                    # A ready long-range energy mount can use the two guns'
                    # simultaneous cooldown at its actual shorter range.
                    # Complete an already-selected eligible burst, then native
                    # chain selection and ordinary group cycling return to guns.
                    # In the Solaris arena, the single AC10's short
                    # cooldown is worth reserving. The first run spent 40 heat
                    # on 10 LL shots while its finite gun still held 78 rounds.
                    # Retain ordinary gun selection until it is empty/lost.
                    energy_bursts = [i for i, w in live if not solaris_actor and w['ammo'] == -1
                                     and w['range'] >= available_range * .8 and i in usable]
                    preferred = (weapon_index if weapon_index in energy_bursts else
                                 weapon_index if weapon_index in ready_guns else
                                 ready_guns[0] if ready_guns else
                                 energy_bursts[0] if energy_bursts else
                                 next((i for i, w in long_range if i == weapon_index), long_range[0][0]))
                elif raw_gap > energy_range * .92:
                    ranged = [(i, w) for i, w in long_range if raw_gap <= w['range'] * .92]
                    ready_missiles = [(i, w) for i, w in ranged if w.get('guided') and w['state'] == 1]
                    ready_guns = [(i, w) for i, w in ranged if not w.get('guided') and i in usable]
                    missiles = [(i, w) for i, w in ranged if w.get('guided')]
                    choices = ready_missiles or ready_guns or missiles or ranged
                    if choices:
                        # A ready missile retains selection through heat
                        # recovery and lock acquisition. Other mounts cannot
                        # consume its reserved full-volley heat headroom.
                        preferred = choices[0][0]
            medium_battery = solaris_medium_battery(s) if kind == 'actor' else []
            medium_phase = bool(medium_battery and firing_gap <= min(w['range'] for i, w in medium_battery) * .92)
            medium_ready = False
            medium_heat = pending_heat = 0.0
            medium_reserved_heat = 0.0
            medium_indices = {i for i, w in medium_battery}
            if medium_phase:
                # A selected cooling member still names the native group;
                # each ready member is independently admitted by the engine.
                preferred = weapon_index if weapon_index in medium_indices else medium_battery[0][0]
                medium_group = 0 if s.get('scene', '').upper().startswith('SOL3') else 2
                observed_members = {i for i, w in enumerate(s['weapons'])
                                    if w['state'] >= 0 and w.get('group') == medium_group
                                    and not sol3_empty_group_gun(s, w)}
                ready_members = [w for i, w in medium_battery if w['state'] == 1]
                medium_heat = sum(w['heat'] * w.get('per_volley', 1) for w in ready_members)
                # Remaining pulses are not observed. Reserve each firing
                # mount's whole nominal volley, including other weapon types.
                pending_heat = sum(w['heat'] * w.get('per_volley', 1)
                                   for w in s['weapons'] if w['state'] == 2)
                medium_reserved_heat = medium_heat + pending_heat
                if combined_loadout:
                    # A cooling member can become ready in combat_tick before
                    # native trigger admission on this same input update.
                    medium_reserved_heat = sum(w['heat'] * w.get('per_volley', 1)
                                               for i, w in medium_battery)
                    medium_reserved_heat += sum(w['heat'] * w.get('per_volley', 1)
                                                for i, w in enumerate(s['weapons'])
                                                if w['state'] == 2 and i not in medium_indices)
                medium_ready = bool(ready_members and observed_members == medium_indices
                                    and s['heat'] + medium_reserved_heat <= 52)
            combined_battery = (sol3_combined_battery(s)
                                if gra1_ranged and kind == 'actor' else [])
            combined_phase = bool(combined_battery and firing_gap <= min(
                w['range'] for i, w in combined_battery) * .92)
            combined_ready = False
            combined_reserved_heat = combined_external_pending = 0.0
            combined_indices = {i for i, w in combined_battery}
            if combined_phase:
                # Keep the finite gun as the aim/selection basis even during
                # its cooldown; semicolon independently admits ready peers.
                preferred = next(i for i, w in combined_battery if w['id'] == 47)
                combined_reserved_heat = sum(w['heat'] * w.get('per_volley', 1)
                                             for i, w in combined_battery)
                combined_external_pending = sum(w['heat'] * w.get('per_volley', 1)
                                                for i, w in enumerate(s['weapons'])
                                                if w['state'] == 2 and i not in combined_indices)
                combined_ready = bool(any(w['state'] == 1 for i, w in combined_battery)
                                      and s['heat'] + combined_reserved_heat
                                      + combined_external_pending <= 52)
            gra1_standoff = bool(gra1_ranged and not solaris_actor and not destination
                                  and (target.get('group', '').casefold() == 'gra1ens3'
                                       or target.get('plan_role') in ('pass-intercept', 'ship-defense', 'arena-defense')))
            if kind in ('actor', 'building'):
                stop = available_range * (.86 if gra1_standoff or solaris_actor else .7)
            aim_weapon = (s['weapons'][preferred] if preferred is not None else
                          selected if weapon_index in usable else (s['weapons'][usable[0]] if usable else selected))
            if kind == 'actor':
                speed = (aim_weapon or {}).get('speed', 50960.0)
                lead = 0 if (aim_weapon or {}).get('guided') else 1 / 30 + math.dist(eye, target_pos) / max(1, speed)
                velocity = self.actor_velocity[target['id']]
                target_pos = [target_pos[i] + max(-3000, min(3000, velocity[i])) * lead for i in range(3)]
                frames = 1
            elif kind == 'building':
                # World objects can rotate or follow authored paths. Their
                # current aim point needs the same input cadence as an actor.
                frames = 1
            gap = distance(pos, target_pos)
            goal_id = (kind, target['id'])
            obstacles = s.get('obstacles', [])
            can_shoot = kind in ('actor', 'building') and visible(eye, target_pos, obstacles)
            gra3_actor = kind == 'actor' and s.get('scene', '').upper().startswith('GRA3')
            friendly_screen = gra3_friendly_screen(s, target, target_pos) if gra3_actor else None
            if gra3_actor or solaris_actor:
                world_clear = gra3_world_clear(s, target)
                can_shoot = can_shoot and world_clear
                fire_observation = dict(world_clear=world_clear,
                                        friendly_screen=friendly_screen)
                if medium_phase:
                    fire_observation['medium_group'] = dict(members=sorted(medium_indices),
                        ready_count=sum(w['state'] == 1 for i, w in medium_battery),
                        ready_heat=medium_heat, pending_heat_reserve=pending_heat,
                        trigger_eligible=medium_ready, ordinary_key='semicolon')
                    if combined_loadout:
                        fire_observation['medium_group']['total_heat_reserve'] = medium_reserved_heat
                if combined_phase:
                    fire_observation['combined_group'] = dict(members=sorted(combined_indices),
                        ready_count=sum(w['state'] == 1 for i, w in combined_battery),
                        all_member_heat_reserve=combined_reserved_heat,
                        external_pending_heat_reserve=combined_external_pending,
                        trigger_eligible=combined_ready, ordinary_key='semicolon')
                if not world_clear:
                    # A box-only A* can otherwise return an empty route at
                    # gun range even though the terrain blocks this shot.
                    # Approach in bounded60m increments through live ordinary
                    # movement/geometry; no hypothetical height is imposed.
                    stop = min(stop, max(1000.0, gap - 6000.0))
            if goal_id != self.goal:
                print(f"  t={s['time']:.1f} goal={kind}:{target['id']} {target.get('name', '')} range={gap / 100:.0f}m", flush=True)
                self.goal = goal_id; self.route = []; self.route_at = -100; self.retreat = False
            # Back away using the original reverse control when an aircraft
            # or rushing mech enters the aim cone's near vertical limit.
            # No actor position or motion is changed by the controller.
            # The observed GRA1 charge closed about 50m during ordinary
            # reversal alone. Start braking near the gun's outer usable band,
            # and keep the chassis toward ENS3 instead of orbiting inward.
            # The Solaris forward orbit has an inward radial component.
            # Once finite guns are absent, back out of a close energy duel
            # with the ordinary reverse key and the narrower reverse cone.
            # Hysteresis avoids changing direction on every range sample.
            solaris_energy = solaris_actor and not gra1_ranged
            near = max(available_range * (.6 if solaris_energy else 0 if solaris_actor else .86 if gra1_standoff else .6), abs(target_pos[1] - eye[1]))
            release = near + (available_range * .16 if solaris_energy else 1000 if solaris_actor else available_range * (.05 if gra1_standoff else .16))
            if kind == 'actor':
                if gap < near: self.retreat = True
                elif gap > release: self.retreat = False
            else: self.retreat = False
            sol3_flank = bool(solaris_energy and not destination
                              and any(w['id'] == 56 for i, w in live)
                              and target['id'] in required_actors
                              and sol3_world_flank(s, target, available_range,
                                  getattr(self, 'sol3_flank_target', None) == goal_id))
            self.sol3_flank_target = goal_id if sol3_flank else None
            if sol3_flank and fire_observation is not None:
                fire_observation['occluded_flank'] = dict(terrain_part=target['world_obstruction']['hit_part'],
                    entry_range_cm=available_range * .9, release_range_cm=available_range * 1.1)
            route_kind = destination[0] if destination else kind
            route_target = destination[1]['position'] if destination else target_pos
            route_stop = (available_range * .7 if destination[0] in ('actor', 'building') else destination[2]) if destination else stop
            travel = distance(pos, route_target) > route_stop + (100 if precision_route else 1000) or (kind in ('actor', 'building') and not can_shoot and not destination)
            if gra1_standoff and self.retreat and can_shoot:
                travel = False  # Do not generate a forward approach inside the reverse hysteresis band.
            if destination: self.retreat = False
            if sol3_flank:
                # The box-only route led into a 40m duel while terrain still
                # blocked the shot. Keep the normal energy reverse band and
                # lateral course here; fresh clearance immediately releases
                # this branch. The ordinary geometry/drop checks still apply.
                self.route = []
                travel = False
            elif precision_route:
                # The 15m A* lattice is wider than the usable deck margin and
                # can leave a planned terrain corridor. Follow its explicit
                # vector; live geometry still brakes or changes direction
                # when support/contact requires it.
                self.route = []
                self.retreat = False
            elif travel:
                if s['time'] - self.route_at > 15 or not self.route:
                    hazards = [a['position'] for a in all_threats
                               if distance(pos, a['position']) > 45000
                               and not (route_kind == 'actor' and a['id'] == (destination[1]['id'] if destination else target['id']))]
                    self.route = path_to(pos, route_target, route_stop, obstacles, shoot=route_kind != 'nav', hazards=hazards)
                    self.route_at = s['time']
                if self.route:
                    nearest = min(range(len(self.route)), key=lambda i: distance(pos, self.route[i]))
                    if distance(pos, self.route[nearest]) < 3000:
                        self.route = self.route[nearest:]
                while self.route and distance(pos, self.route[0]) < 1200:
                    self.route.pop(0)
            else: self.route = []
            following_route = bool(self.route)
            if following_route:
                # The route is an intended physical path around an obstacle
                # or into a clear firing position. Combat reverse/orbit must
                # not turn its waypoint heading into motion the other way.
                self.retreat = False
            waypoint = route_target
            if self.route:
                waypoint = self.route[0]
                for candidate in self.route[:6]:
                    eye0 = [pos[0], pos[1] + 750, pos[2]]
                    eye1 = [candidate[0], candidate[1] + 750, candidate[2]]
                    if any(intersects(eye0, eye1, o['bounds'], 900) for o in obstacles): break
                    waypoint = candidate
            move_heading = math.degrees(math.atan2(waypoint[0] - pos[0], waypoint[2] - pos[2]))
            heading = math.degrees(math.atan2(target_pos[0] - eye[0], target_pos[2] - eye[2]))
            if destination and not travel and plan.get('movement_anchor'):
                # At the protected position, turn the chassis toward the
                # firing target so a limited torso cone can track all sides.
                move_heading = heading
            circle = sol3_flank or (kind == 'actor' and can_shoot and gap < available_range * (.9 if solaris_actor else .8) and not destination and not following_route and not gra1_standoff)
            if circle:
                # Ordinary forward/reverse movement across the enemy's aim
                # line, with the torso tracking it. Standing still presents
                # an unnecessary target; original AI shots do not lead.
                for side in (self.orbit, -self.orbit):
                    orbit_angle = 35 if self.retreat else 65 if solaris_actor else 70
                    direction = math.radians(heading + side * orbit_angle)
                    travel_sign = -1 if self.retreat else 1
                    look = [pos[0] + math.sin(direction) * 3000 * travel_sign,
                            pos[1] + 750, pos[2] + math.cos(direction) * 3000 * travel_sign]
                    if not any(intersects([pos[0], pos[1] + 750, pos[2]], look, o['bounds'], 900) for o in obstacles):
                        self.orbit = side; move_heading = heading + side * orbit_angle
                        break
                else: circle = False
            escaping_contact = s['time'] < self.stuck_until
            if escaping_contact:
                self.retreat = False
                move_heading = self.escape_heading
            move_heading, geometry_cap, self.retreat = self.geometry_heading(s, move_heading, self.retreat, precision_route)
            geometry_cap, return_boundary = gra3_return_throttle_cap(s, move_heading, self.retreat, geometry_cap)
            if precision_route and distance(pos, route_target) < 3000:
                geometry_cap = min(geometry_cap, .331)
            dh = angle(move_heading - s['heading'])
            aim_dh = angle(heading - s['heading'])
            pitch = math.degrees(math.atan2(target_pos[1] - eye[1],
                                           max(1, distance(eye, target_pos))))
            actual_yaw = s.get('aim_yaw', s['heading'] + s['twist'])
            actual_pitch = s.get('aim_pitch', s['pitch'])
            yaw_bias = angle(actual_yaw - s['heading'] - s['twist'])
            pitch_bias = actual_pitch - s['pitch']
            want_x = round(max(-511, min(511, (aim_dh - yaw_bias) * 512 / 90)))
            want_y = round(max(-511, min(511, (pitch - pitch_bias) * 512 / 60)))
            dx, dy = round(want_x - s['mouse'][0]), round(want_y - s['mouse'][1])
            if abs(dh) > (.35 if precision_route else 3):
                hold.append('right' if dh > 0 else 'left')
                if abs(dh) < 15: frames = 1
            wanted_throttle = geometry_cap if (travel or self.retreat or circle or escaping_contact) and abs(dh) < (60 if escaping_contact else 25) else 0.0
            if bool(s.get('reverse', s['throttle'] < 0)) != self.retreat: press.append('backspace')
            if abs(abs(s['throttle']) - wanted_throttle) > .02:
                press.append('0' if wanted_throttle > .9 else '4' if wanted_throttle > .2 else '2' if wanted_throttle else '1')
            if kind in ('actor', 'building'):
                error_yaw = abs(angle(heading - actual_yaw))
                error_pitch = abs(pitch - actual_pitch)
                aim_tolerance = 8 if selected and selected.get('guided') else .55
                if ((preferred is not None and weapon_index != preferred)
                        or (preferred is None and weapon_index not in usable and usable)):
                    wanted = preferred if preferred is not None else usable[0]
                    select_key = weapon_selection_key(s, wanted)
                    if select_key == 'enter':
                        if 'enter' not in self.last_hold: hold.append('enter')
                    elif select_key:
                        press.append(select_key)
                elif (can_shoot and error_yaw < aim_tolerance and error_pitch < aim_tolerance
                      and (combined_ready or (medium_ready if medium_phase else weapon_index in usable))
                      and 'space' not in self.last_hold and 'semicolon' not in self.last_hold
                      and (kind != 'actor' or target['id'] not in protected_actors)
                      and not friendly_screen
                      and (kind != 'building' or target['id'] not in protected_buildings)
                      and (not energy_surface or (s.get('reticle_bld') == target['id']
                           and s.get('target', -1) == -1 and s.get('thing') == target['id']))
                      and (not selected.get('guided')
                           or (selected_thing_locked(s, target) if guided_thing else
                               kind != 'actor' or (s.get('missile_lock', 0) & 0x80 and s.get('lock_target') == target['id'])))):
                    hold.append('semicolon' if combined_ready or medium_phase else 'space')
                if guided_thing:
                    # Q uses the retained native reticle hit. The first press
                    # after a target-cycle key enables plain-object selection;
                    # a later press can select a flags-zero harpoon. Observe
                    # actual selection, and leave release frames between pulses.
                    if (target.get('thing_point') is not None and s.get('thing') != target['id']
                            and s.get('reticle_bld') == target['id']
                            and s['time'] >= self.thing_select_next_time):
                        press.append('q')
                        self.thing_select_next_time = s['time'] + .15
                elif kind == 'actor' and target.get('targetable', 1) and s['target'] != target['id']: press.append('t')
            if kind == 'inspect':
                frames = 1
                if s['target'] != target['id']: press.append('t')
                elif gap < 18000: press.append('i')
        if s['time'] - self.last_report >= 10:
            self.last_report = s['time']
            record = dict(time=s['time'], position=pos, health=s['health'], heat=s['heat'],
                          fired=s['fired'], all_unit_hits=s['hits'], outcome=s['outcome'],
                          objectives=[(n['id'], n['state']) for n in s['objectives']],
                          enemies_alive=len(all_threats), goal=self.goal,
                          enemy_health=[(a['id'], a['health'], a['destroyed']) for a in s['actors'] if a['side'] == 1],
                          ammunition=[w['ammo'] for w in s['weapons']],
                          planned_goals=[(g[0], g[1]['id'], g[1].get('plan_role')) for g in goals],
                          planned_nodes=plan['objective_nodes'], planner_waiting=plan['waiting'],
                          planner_blocked=plan['blocked'], planner_reasons=plan['reasons'])
            self.summary.append(record)
            print(f"  t={s['time']:.1f} health={s['health']} heat={s['heat']:.1f} shots={s['fired']} all_unit_hits={s['hits']} pos={pos}", flush=True)
        self.last_hold = hold
        self.last_throttle_demand = wanted_throttle if goal else 0
        return dict(frames=frames, press=press, hold=hold, mouse=(dx, dy),
                    waypoint=waypoint if goal else None, route_points=len(self.route),
                    geometry_steering=self.geometry_decision,
                    **({'world_aim': world_aim} if world_aim else {}),
                    **({'return_boundary': return_boundary} if return_boundary else {}),
                    **({'fire_observation': fire_observation} if fire_observation else {}))


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def combat_config(base, edition):
    """Read actual saved flags even when a failed return omits CAREER JSON."""
    overlay = {'dos': 'retail', '3dfx': '3dfx', 'direct3d': 'direct3d'}[edition]
    path = base / 'Mercs-game' / 'install' / overlay / 'MW2DIF.CFG'
    if not path.is_file(): return None
    raw = path.read_bytes()
    if len(raw) < 6: return None
    fields = ('unlimited_ammo', 'invulnerable', 'splash_damage',
              'collision_damage', 'heat_tracking', 'difficulty')
    return dict(zip(fields, raw[:6]), raw_cfg_hex=raw.hex())


def shell_command(base, edition, command, *extra, duration=1200, output=None, draw_every=1):
    env = environment(base)
    env.update(MERCS_PLAYTHROUGH='1', MERCS_TEST_FPS='30', MERCS_SIZE='640x480',
               MERCS_AUTOPILOT=f'{duration},-9,-999,0,{base}/last-frame.ppm,1,0',
               MERCS_SFX_SIMCLOCK='1', MERCS_PLAYTHROUGH_DRAW_EVERY=str(draw_every))
    cmd = [ROOT / 'test_mercs_playthrough_career', base / 'Mercs-game', edition,
           ROOT / 'test_mercs_playthrough', command, *extra]
    tag = output or (command + ('-' + '-'.join(map(str, extra)) if extra else ''))
    out = base / 'runs'; out.mkdir(exist_ok=True)
    stem, attempt = tag, 1
    while (out / (tag + '.json')).exists() or (out / (tag + '.stdout.log')).exists():
        attempt += 1; tag = f'{stem}-{attempt}'
    pilot, last, career = Pilot(duration), None, None
    abort_file = out / (tag + '.abort-request.txt')
    abort_request = None
    failure, failure_exception, timed_out, rc = None, None, False, None
    start = time.monotonic()
    runtime_hash = digest(ROOT / 'test_mercs_playthrough')
    career_hash = digest(ROOT / 'test_mercs_playthrough_career')
    driver_hash = digest(Path(__file__))
    planner_hash = digest(Path(__file__).with_name('mercs_campaign_plan.py'))
    snapshots = out / 'source-snapshots'; snapshots.mkdir(exist_ok=True)
    source_snapshots = {}
    for name, path, checksum in (
            ('driver', Path(__file__), driver_hash),
            ('planner', Path(__file__).with_name('mercs_campaign_plan.py'), planner_hash)):
        saved = snapshots / f'{name}-{checksum}.py'
        if not saved.exists(): saved.write_bytes(path.read_bytes())
        if digest(saved) != checksum: raise RuntimeError(f'Source snapshot changed: {saved}')
        source_snapshots[name] = str(saved)
    combat_before = combat_config(base, edition)
    try:
        with (out / (tag + '.stderr.log')).open('w') as errors, \
                (out / (tag + '.stdout.log')).open('w') as log, \
                gzip.open(out / (tag + '.observations.jsonl.gz'), 'wt') as observations, \
                (out / (tag + '.inputs.jsonl')).open('w') as inputs:
            p = subprocess.Popen([str(x) for x in cmd], cwd=ROOT, env=env, text=True,
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors, bufsize=1, start_new_session=True)
            def expire():
                nonlocal timed_out
                timed_out = True
                try: os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError: pass
            watchdog = threading.Timer(1200, expire); watchdog.daemon = True; watchdog.start()
            try:
                for line in p.stdout:
                    if line.startswith('PLAY '):
                        state = json.loads(line[5:]); observations.write(json.dumps(state, separators=(',', ':')) + '\n')
                        if abort_request is None and abort_file.is_file():
                            raw_request = abort_file.read_bytes()
                            reason = raw_request.decode('utf-8', errors='replace').strip()[:1000] or 'Operator requested normal Abort'
                            abort_request = dict(file=str(abort_file), reason=reason, observed_time=state['time'],
                                                 sha256=hashlib.sha256(raw_request).hexdigest(),
                                                 utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
                            pilot.operator_abort_reason = reason
                        last = state; action = pilot.command(state)
                        inputs.write(json.dumps(dict(time=state['time'], **action)) + '\n')
                        dx, dy = action['mouse']
                        p.stdin.write(f"frames={action['frames']} press={','.join(action['press']) or '-'} hold={','.join(action['hold']) or '-'} mouse={dx},{dy}\n")
                        p.stdin.flush()
                    else:
                        log.write(line)
                        if line.startswith('CAREER '):
                            career = json.loads(line[7:])
                        if 'Mission' in line or 'objective' in line:
                            print(line.rstrip(), flush=True)
                    if time.monotonic() - start > 1200:
                        raise TimeoutError('Native process exceeded wall-time budget')
                rc = p.wait(timeout=30)
            except BaseException as exc:
                failure, failure_exception = traceback.format_exc(), exc
                try: os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError: pass
                rc = p.wait()
            finally:
                watchdog.cancel()
    except BaseException as exc:
        failure, failure_exception = traceback.format_exc(), exc
    native_outcome = last['outcome'] if last else None
    persisted_outcome = career.get('last_outcome') if career else None
    corroborated = (native_outcome == persisted_outcome and native_outcome in (2, 3, 4))
    mission_result = None
    if command == 'launch':
        if failure or rc or not career:
            mission_result = 'harness_failure'
        elif native_outcome == 2 and persisted_outcome == 2:
            mission_result = 'success'
        elif persisted_outcome in (3, 4):
            mission_result = 'failure'
        else:
            mission_result = 'unfinished'
    report = dict(command=[str(x) for x in cmd], seconds=time.monotonic() - start,
                  exit_code=rc, observations=pilot.samples, last_state=last, career=career,
                  failure=failure, wall_time_expired=timed_out,
                  mission_executable_sha256=runtime_hash,
                  career_executable_sha256=career_hash,
                  driver_sha256=driver_hash, planner_sha256=planner_hash,
                  source_snapshots=source_snapshots,
                  progression=pilot.summary, simulation_mutation_hooks=[],
                  mission_result=mission_result, native_outcome=native_outcome,
                  ended_via_normal_abort=bool(pilot.abort_phase),
                  operator_abort_request=abort_request,
                  persisted_outcome=persisted_outcome,
                  outcome_corroborated=corroborated if command == 'launch' else None,
                  combat_variables=(career or {}).get('combat_variables') or combat_config(base, edition),
                  combat_variables_before=combat_before,
                  live_normal_flags={key: last.get(key) for key in ('unlimited_ammo', 'invulnerable', 'no_heat', 'collision_damage', 'splash_damage', 'difficulty')} if last else None,
                  setting_overrides={'transitions': False, 'fixed_simulation_fps': 30,
                                     'audio_queue_clock': 'simulation', 'cockpit_view': True, 'world_draw_interval': draw_every},
                  driver='ordinary SDL key/mouse input, white-box read-only observations',
                  observation_limits='Omniscient actor/health/nav/objective/obstacle observations, including hidden and out-of-sensor records; not a human playthrough',
                  shell_control='Direct calls to the real shell handlers, with their validation, prices and persistence')
    (out / (tag + '.json')).write_text(json.dumps(report, indent=2) + '\n')
    if failure_exception:
        raise failure_exception
    if rc or not career or (command == 'launch' and not last):
        raise RuntimeError(f'{tag} failed: exit={rc}, see {out}')
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--edition', choices=('dos', '3dfx', 'direct3d'), default='dos')
    ap.add_argument('--contract', default='BET1CAM1')
    ap.add_argument('--duration', type=int, default=1200)
    ap.add_argument('--draw-every', type=int, default=1, help='Rasterize the world every Nth frame; simulation/HUD still run every frame')
    ap.add_argument('--resume', action='store_true')
    ap.add_argument('--command', choices=('status', 'launch', 'continue', 'repair', 'rearm', 'refit', 'armor-max', 'install', 'remove-component', 'buy', 'buy-ammo', 'buy-weapon', 'buy-equipment', 'sell', 'assign', 'save', 'load', 'accept'))
    ap.add_argument('--value')
    args = ap.parse_args(); base = args.output.resolve()
    base.mkdir(parents=True, exist_ok=True)
    if not args.resume:
        folder = fixture(base, args.game.resolve())
        (base / 'port.cfg').write_text(f'game={folder}\nrenderer={args.edition}\nresolution=window\nrender=640x480\naa=0\n')
        shell_command(base, args.edition, 'new')
        shell_command(base, args.edition, 'accept', args.contract)
    if args.command:
        report = shell_command(base, args.edition, args.command, *([args.value] if args.value else []), duration=args.duration, draw_every=args.draw_every)
    else:
        report = shell_command(base, args.edition, 'launch', duration=args.duration, draw_every=args.draw_every)
    print(json.dumps({k: v for k, v in report.items() if k not in ('last_state', 'progression')}, indent=2))


if __name__ == '__main__':
    main()
