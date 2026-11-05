"""Read-only objective planning for the ordinary-input campaign test pilot.

The input is one observation from mercs_playthrough_input.h. This module never
changes that observation, game state, files, timers, RNG, or results. Returned
goals are requests for ordinary controls, never assertions of achievement.

MTBL dependencies use the observed S/F/C states and ALL/ANY rules. Incoming
authored SUCCEED nodes can complete a latch/protection objective, so their
prerequisites matter even when individually marked optional. Explicit wins
are alternative routes; STAR_LOSE and deliberate failure are never goals.

analyse(s) returns ordered (kind, target, stopping_distance_cm) goals plus
protection/wait/escort metadata. Preserve this order in the pilot. Targets are
copies with plan_* annotations. plan(s) is the goal-only convenience wrapper.
The few mission-specific route hints are documented independently in
tests/fixtures/campaign-playthrough/mission-planning.md. They remain tactics,
not new mission rules or evidence of a victory.
"""
from dataclasses import dataclass, field
import math

OK, FAILED = 5, 6
DESTROY = (1, 2)
PROTECT, SCAN, START, ALLMAIN, REACH = 4, 8, 0x10, 0x20, 0x100
TIMER_KINDS = (0, 0x200, 0x400, 0x800)
WIN, LOSE, TOGGLE, FAIL, SUCCEED, RESET, REARM = (
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000)


def _integer(value, default=0):
    try:
        return int(value, 0) if isinstance(value, str) else int(value)
    except (ValueError, TypeError):
        return default


def _distance(a, b):
    return math.hypot(a[0] - b[0], a[2] - b[2])


def _widen(value):
    if value & 0x730: value |= 0x730
    if value & 3: value |= 3
    return value


def _latch(nav):
    # The reviewed authored latches are hundreds of thousands of centimetres
    # above the terrain. OUT1GOU1 has x=4000, so an x/z-only test is incorrect.
    # Hidden/selectable flags alone must NOT reject real IVA3/MCC3 triggers.
    return abs(nav['position'][1]) >= 250000 and not nav.get('selectable', False)


def _goal(kind, target, stop, node, role='objective', target_kind=None):
    copy = dict(target)
    copy['plan_node'] = list(node)
    copy['plan_role'] = role
    copy['plan_target_kind'] = target_kind or ('actor' if kind == 'inspect' else kind)
    return kind, copy, float(stop)


def _identity(goal):
    return goal[0], goal[1].get('plan_target_kind'), goal[1].get('id'), goal[1].get('name', '')


@dataclass
class _Branch:
    goals: list = field(default_factory=list)
    nodes: set = field(default_factory=set)
    waiting: bool = False
    wait_seconds: float = 0
    blocked: bool = False
    reasons: list = field(default_factory=list)


def _merge(branches):
    out, seen = _Branch(), set()
    for branch in branches:
        for goal in branch.goals:
            key = _identity(goal)
            if key not in seen: out.goals.append(goal); seen.add(key)
        out.nodes.update(branch.nodes)
        out.waiting |= branch.waiting
        out.wait_seconds = max(out.wait_seconds, branch.wait_seconds)
        out.blocked |= branch.blocked
        out.reasons.extend(branch.reasons)
    return out


class _Planner:
    def __init__(self, state):
        self.s = state
        self.player = _integer(state.get('player_table', 0))
        self.pos = state.get('position', [0, 0, 0])
        self.scene = str(state.get('scene', '')).upper()[:4]
        self.nodes, self.tables = {}, {}
        tables = state.get('tables')
        if not tables:
            tables = [{'id': self.player, 'nodes': state.get('objectives', [])}]
        for fallback, table in enumerate(tables):
            index = _integer(table.get('id', fallback))
            if not 0 <= index < 64: continue
            self.tables[index] = table
            for fallback_node, node in enumerate(table.get('nodes', [])[:48]):
                self.nodes[index, _integer(node.get('id', fallback_node))] = node
        self.setters = {}
        for key, node in self.nodes.items():
            if _integer(node.get('kind')) == SUCCEED:
                target = _integer(node.get('other_table', -1)), _integer(node.get('other_node', -1))
                self.setters.setdefault(target, []).append(key)
        self.cache, self.visits = {}, 0
        self.protected_actors, self.protected_buildings = set(), set()
        self.scan_keys = set()
        self.anchors = []
        self.escort = False

    def state(self, key):
        return _integer(self.nodes.get(key, {}).get('state', 0))

    def pending(self, key):
        return self.state(key) not in (OK, FAILED)

    def main(self, key):
        node = self.nodes[key]
        return bool(node.get('main')) or node.get('objective') == 'M'

    def items(self, node):
        return [item for item in node.get('items', []) if item.get('type') in ('S', 'F', 'C')][:8]

    def item_met(self, item):
        value = self.state((_integer(item.get('table')), _integer(item.get('node'))))
        return value == OK if item['type'] == 'S' else value == FAILED if item['type'] == 'F' else value in (OK, FAILED)

    def ready(self, node):
        if _integer(node.get('kind')) == START: return True
        tests = [self.item_met(item) for item in self.items(node)]
        return all(tests) if node.get('require_all', False) else any(tests)

    def targets(self, node):
        name, kind = str(node.get('target', '')).casefold(), _integer(node.get('kind'))
        if not name or name == 'null': return []
        found = []
        for entity_kind, items, name_field, class_field in (
            ('actor', self.s.get('actors', []), 'group', 'class'),
            ('nav', self.s.get('navs', []), 'name', 'class'),
            ('building', self.s.get('buildings', []), 'record', 'type')):
            for entity in items:
                if str(entity.get(name_field, '')).casefold() != name: continue
                value = _integer(entity.get(class_field, kind))
                if _widen(value) & kind: found.append((entity_kind, entity))
        return found[:40]

    def alive(self, kind, target):
        return target.get('hp', 0) > 0 if kind == 'building' else not target.get('destroyed', False)

    def remaining(self, node):
        delay = _integer(node.get('delay', 0xffffffff))
        if delay >= 0x80000000: delay -= 0x100000000
        if delay < 0: return math.inf
        started = _integer(node.get('started', -1))
        elapsed = 0 if started < 0 else max(0, _integer(self.s.get('now', 0)) // 1000 - started // 1000)
        return max(0, delay + 1 - elapsed)

    def unsafe(self, key):
        node = self.nodes.get(key, {})
        kind = _integer(node.get('kind'))
        if kind in (LOSE, FAIL): return True
        if kind == SUCCEED:
            target = (_integer(node.get('other_table', -1)), _integer(node.get('other_node', -1)))
            return _integer(self.nodes.get(target, {}).get('kind')) in (LOSE, FAIL)
        return False

    def score(self, branch):
        # Only an ordering heuristic for ordinary controls, not a solver or a
        # claim that these actions will succeed in the real combat/terrain.
        score = min(branch.wait_seconds, 100000) / 30
        for kind, target, _ in branch.goals:
            score += 1 + _distance(self.pos, target['position']) / 80000
            if kind in ('actor', 'building'):
                score += max(0, target.get('health', target.get('hp', 0))) / 2000
        # The documented intended IVA3 route uses the bridge/Sigma first;
        # the hidden real extraction remains eligible after Sigma has failed.
        if self.scene == 'IVA3' and (self.player, 2) in branch.nodes and self.state((self.player, 2)) != FAILED:
            score -= 100
        return branch.blocked, score, len(branch.goals), sorted(branch.nodes)

    def prerequisites(self, node, trail):
        items = self.items(node)
        if self.ready(node): return _Branch()
        if not items: return _Branch(blocked=True, reasons=['No satisfied activation predicate'])
        branches = []
        for item in items:
            if self.item_met(item): continue
            key = _integer(item.get('table')), _integer(item.get('node'))
            if item['type'] == 'F':
                branches.append(_Branch(blocked=True, nodes={key}, reasons=['A future failure is not a positive gameplay goal']))
            else:
                branches.append(self.solve(key, trail))
        if node.get('require_all', False): return _merge(branches)
        return min(branches, key=self.score) if branches else _Branch(waiting=True)

    def follower(self, table, key, target_name=''):
        followers = [a for a in self.s.get('actors', [])
                     if a.get('table') == table and a.get('side') != 1 and not a.get('destroyed')]
        if not followers:
            return _Branch(blocked=True, reasons=[f'Foreign table {table} requires its own living leader'])
        # The SIG3 DropShip waits/moves through authored pickup locations; do
        # not chase its airborne/off-map spawn while waiting for its arrival.
        if self.scene == 'SIG3':
            nav = next((n for n in self.s.get('navs', []) if n.get('name', '').casefold() == target_name.casefold()), None)
            if nav and not _latch(nav):
                return _Branch(goals=[_goal('nav', nav, max(100, nav.get('radius', 20000)*.5), key, 'escort-rendezvous')], waiting=True)
        leader = min(followers, key=lambda a: a['id'])
        return _Branch(goals=[_goal('nav', leader, 10000, key, 'escort-follow', 'actor')], waiting=True)

    def direct(self, key, trail):
        node = self.nodes[key]
        kind = _integer(node.get('kind'))
        if not self.ready(node): return self.prerequisites(node, trail)
        if self.unsafe(key): return _Branch(blocked=True, reasons=['Failure control is not a positive goal'])
        if kind in (START, 0x1000, WIN, TOGGLE, SUCCEED, RESET, REARM):
            return _Branch(waiting=True)
        if kind in TIMER_KINDS:
            remaining = self.remaining(node)
            return _Branch(waiting=True, wait_seconds=remaining, blocked=math.isinf(remaining),
                           reasons=[f'Observe authored timer/order at {key[0]}.{key[1]}'])
        if kind == PROTECT:
            remaining = self.remaining(node)
            return _Branch(waiting=True, wait_seconds=remaining, blocked=math.isinf(remaining),
                           reasons=[f'Preserve targets of {key[0]}.{key[1]} while its completion predicate runs'])
        if kind == ALLMAIN:
            other = [k for k in self.nodes if k[0] == key[0] and k != key and self.main(k) and self.state(k) != OK]
            if other: return _merge(self.solve(k, trail) for k in other)
        if kind == REACH and key[0] != self.player:
            return self.follower(key[0], key, str(node.get('target', '')))
        if kind not in (*DESTROY, SCAN, ALLMAIN, REACH):
            return _Branch(blocked=True, reasons=[f'No positive ordinary-control plan for kind {kind:#x}'])
        goals, blocked = [], False
        for entity_kind, target in self.targets(node):
            if kind in DESTROY:
                if entity_kind == 'nav': blocked = True; continue
                if self.alive(entity_kind, target):
                    goals.append(_goal(entity_kind, target, 18000 if entity_kind == 'building' else 20000, key))
            elif entity_kind == 'nav':
                if _latch(target): blocked = True; continue
                if kind in (SCAN, ALLMAIN) and target.get('visited'): continue
                if kind in (SCAN, ALLMAIN) and not target.get('shown') and not target.get('selectable'):
                    blocked = True; continue
                radius = target.get('radius', 20000) or 20000
                # REACH is a current proximity test: a past visit does not
                # satisfy a newly armed or re-armed reach node.
                goals.append(_goal('nav', target, max(100, radius*.5), key))
            elif kind == REACH:
                if self.alive(entity_kind, target):
                    goals.append(_goal('nav', target, 10000, key, 'rendezvous', entity_kind))
            elif not target.get('inspected'):
                if self.alive(entity_kind, target):
                    goals.append(_goal('inspect', target, 12000, key, 'inspect', entity_kind))
                else: blocked = True
        return _Branch(goals=goals, waiting=not goals, blocked=blocked)

    def solve(self, key, trail=()):
        if key in self.cache: return self.cache[key]
        if key not in self.nodes: return _Branch(blocked=True, reasons=[f'Missing observed node {key}'])
        if self.state(key) == OK: return _Branch()
        if self.state(key) == FAILED: return _Branch(blocked=True, nodes={key}, reasons=[f'Observed failed node {key}'])
        self.visits += 1
        if key in trail or len(trail) >= 96 or self.visits > 8192:
            return _Branch(blocked=True, reasons=[f'Cyclic/bounded dependency at {key}'])
        path = (*trail, key)
        candidates = [self.direct(key, path)]
        # A setter's own prerequisites are sufficient; the latch's ordinary
        # prerequisites need not be true for an authored control to complete it.
        for setter in self.setters.get(key, []):
            if setter != key and self.pending(setter) and not self.unsafe(setter):
                candidates.append(self.solve(setter, path))
        best = min(candidates, key=self.score)
        result = _merge([best, _Branch(nodes={key})])
        if not result.blocked: self.cache[key] = result
        return result

    def protections(self, selected):
        for key, node in self.nodes.items():
            if not self.pending(key): continue
            kind = _integer(node.get('kind'))
            needed = key in selected or (key[0] == self.player and self.main(key))
            if not needed or kind not in (SCAN, PROTECT): continue
            if kind == SCAN: self.scan_keys.add(key)
            for entity_kind, target in self.targets(node):
                if entity_kind == 'nav' or not self.alive(entity_kind, target): continue
                if entity_kind == 'actor': self.protected_actors.add(target['id'])
                else: self.protected_buildings.add(target['id'])
                if kind == PROTECT:
                    role = 'escort-follow' if entity_kind == 'actor' else 'defend-structure'
                    anchor = _goal('nav', target, 12000, key, role, entity_kind)
                    if self.scene == 'SIG3' and entity_kind == 'actor' and target.get('group', '').casefold() == 'sig3usr2':
                        pickup = 'sig3gou7' if self.state((2, 3)) == OK else 'sig3gou6'
                        nav = next((n for n in self.s.get('navs', []) if n.get('name', '').casefold() == pickup), None)
                        if nav and not _latch(nav):
                            anchor = _goal('nav', nav, max(100, nav.get('radius', 20000)*.5), key, 'escort-rendezvous')
                    self.anchors.append(anchor)
                    self.escort |= entity_kind == 'actor'

    def hints(self, goals):
        # MCC3's support only activates inside the real, hidden Psi trigger.
        if self.scene == 'MCC3' and self.pending((self.player, 2)):
            support = any(a.get('group', '').casefold() == 'mcc3usr1' and not a.get('destroyed') for a in self.s.get('actors', []))
            nav = next((n for n in self.s.get('navs', []) if n.get('name', '').casefold() == 'mcc3nav2'), None)
            if support and nav and not _latch(nav):
                goals.insert(0, _goal('nav', nav, min(35000, max(100, nav.get('radius', 40000)*.875)), (self.player, 2), 'artillery-trigger'))
        # The intended IVA3 escape passes the authored bridge before Sigma.
        if self.scene == 'IVA3' and any(g[1].get('name', '').casefold() == 'iva3nav2' for g in goals):
            nav = next((n for n in self.s.get('navs', []) if n.get('name', '').casefold() == 'iva3nav1'), None)
            if nav and not nav.get('visited') and self.state((self.player, 1)) != OK:
                goals.insert(0, _goal('nav', nav, max(100, nav.get('radius', 10000)*.5), (self.player, 1), 'bridge-route'))
        # Protect the live convoy while its own table traverses both outbound
        # and return legs; never replace its current position with player NAV2.
        if self.scene == 'MCC4' and self.pending((self.player, 2)):
            convoy = [g for g in self.anchors if g[1].get('group', '').casefold() == 'mcc4usr1']
            if convoy:
                leader = min(convoy, key=lambda g: g[1]['id'])
                goals.insert(0, leader)
        return goals

    def run(self):
        roots = [k for k in self.nodes if k[0] == self.player and self.main(k) and self.pending(k) and not self.unsafe(k)]
        wins = [k for k, n in self.nodes.items() if _integer(n.get('kind')) == WIN and
                _integer(n.get('other_table', -1)) == self.player and self.pending(k)]
        alternatives = [self.solve(k) for k in wins]
        if roots: alternatives.append(_merge(self.solve(k) for k in roots))
        branch = min(alternatives, key=self.score) if alternatives else _Branch(waiting=True)
        self.protections(branch.nodes)
        # Mandatory identification has precedence over every lethal goal,
        # including a kill that is active at the same time as its scan.
        scans = _merge(self.solve(k) for k in sorted(self.scan_keys))
        goals = _merge([scans, branch]).goals
        goals = [g for g in goals if not (
            g[0] == 'actor' and g[1]['id'] in self.protected_actors or
            g[0] == 'building' and g[1]['id'] in self.protected_buildings)]
        # Preserve prerequisite order; only equally ready requests use the
        # current range/health tie-breaker. Inspection always remains first.
        inspections = [g for g in goals if g[0] == 'inspect']
        others = [g for g in goals if g[0] != 'inspect']
        if self.scene == 'LUT2':
            # Authored enemy exit points share (0,0,-80000). Keep the closest
            # required escape threat ahead of an easier nearby patrol.
            others.sort(key=lambda g: (_distance(g[1]['position'], [0, 0, -80000])
                if g[0] == 'actor' else float('inf')))
        goals = inspections + self.hints(others)
        anchor = None
        if self.anchors:
            convoy = [g for g in self.anchors if g[1].get('plan_role') in ('escort-follow', 'escort-rendezvous')]
            anchor = min(convoy or self.anchors, key=lambda g: _distance(self.pos, g[1]['position']))
            if not goals: goals = [anchor]
        unique, seen = [], set()
        for goal in goals:
            key = _identity(goal)
            if key not in seen: unique.append(goal); seen.add(key)
        return dict(goals=unique, protected_actor_ids=sorted(self.protected_actors),
                    protected_building_ids=sorted(self.protected_buildings),
                    waiting=bool(not unique and (roots or wins)), escort=self.escort,
                    anchor=anchor, blocked=branch.blocked,
                    allow_fallback=not bool(self.s.get('tables')),
                    objective_nodes=[list(k) for k in sorted(branch.nodes)],
                    reasons=list(dict.fromkeys(branch.reasons))[:24])


def analyse(state):
    """Return recommendations only; never write to the supplied observation."""
    if state.get('outcome') or state.get('destroyed') or state.get('ending'):
        return dict(goals=[], protected_actor_ids=[], protected_building_ids=[],
                    waiting=False, escort=False, anchor=None, blocked=False,
                    allow_fallback=False, objective_nodes=[], reasons=['Observed mission ending'])
    return _Planner(state).run()


def plan(state):
    return analyse(state)['goals']
