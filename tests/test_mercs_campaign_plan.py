#!/usr/bin/env python3
"""Bounded planner checks against independently decoded authored MTBL data.

Entity positions/health and selected node-state transitions below are synthetic
observations used to test planning branches. No game process or career is
changed, and these checks do not count as mission attempts or victories.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

from mercs_campaign_plan import analyse

ROOT = Path(__file__).resolve().parents[1]
AUTHOR = ROOT / 'tests/fixtures/campaign-playthrough/authored-route-notes.json'


def fixture(author, scene):
    source = author['missions'][scene + 'CAM1']
    tables = []
    for table_id, table in source['tables'].items():
        nodes = []
        for n in table['nodes']:
            kind = int(n['kind'], 16)
            nodes.append(dict(id=n['node'], kind=kind, objective=n['objective'],
                main=int(n['objective'] == 'M'), state=5 if kind == 0x10 else 0,
                shown=int(n['shown']), target=n['target'], text='',
                require_all=int(n['require_all']), delay=n.get('delay_seconds', 0xffffffff),
                started=-1, other_table=n.get('other_table', -1), other_node=n.get('other_node', -1),
                items=[dict(type=i['state'], table=i['table'], node=i['node']) for i in n['dependencies']]))
        tables.append(dict(id=int(table_id), nodes=nodes, result=0))
    navs = [dict(id=i, name=name, position=n['position_cm'], radius=n['radius_m']*100,
                 shown=int(n['shown']), selectable=int(n['shown'] and n['team'] == 0),
                 visited=0, **{'class': int(n['class'], 16)})
            for i, (name, n) in enumerate(source['navs'].items())]
    names = sorted({n['target'].upper() for t in tables for n in t['nodes']
                    if n['kind'] in (1, 2, 4, 8) and n['target'] and n['target'].upper() != 'USERSTAR'}
                   - set(source['navs']))
    actors, buildings = [], []
    for index, name in enumerate(names):
        position = [index*2000, 0, 0]
        if 'ARE' in name:
            buildings.append(dict(id=len(buildings), record=name, name=name, hp=1000,
                                  type=15, side=0, inspected=0, position=position))
        else:
            side = 0 if 'USR' in name else 1
            actors.append(dict(id=len(actors), group=name, name=name, health=1000, destroyed=0,
                inspected=0, armed=1, targetable=1, side=side, position=position,
                table=int(name[-1]) if name[-1].isdigit() else 1, **{'class': 15}))
    start = next((n['position'] for n in navs if n['name'].upper() == scene+'ST01'), [0,0,0])
    return dict(scene=scene+'SCN1', player_table=0, time=10, now=10000,
                position=list(start), tables=tables, objectives=tables[0]['nodes'],
                actors=actors, buildings=buildings, navs=navs,
                outcome=0, destroyed=0, ending=0)


def node(state, index, table=0):
    return next(n for t in state['tables'] if t['id'] == table for n in t['nodes'] if n['id'] == index)


def done(state, index, table=0):
    n = node(state, index, table)
    n['state'] = 5
    for a in state['actors']:
        if a['group'].casefold() != n['target'].casefold(): continue
        if n['kind'] in (1, 2): a['destroyed'] = 1
        if n['kind'] == 8: a['inspected'] = 1
    for b in state['buildings']:
        if b['record'].casefold() == n['target'].casefold() and n['kind'] in (1,2): b['hp'] = 0


def names(result, kinds=None):
    return {g[1].get('group', g[1].get('record', g[1].get('name', ''))).upper()
            for g in result['goals'] if kinds is None or g[0] in kinds}


def check_plan(state):
    before = copy.deepcopy(state)
    result = analyse(state)
    assert state == before, 'Planner modified its observation'
    assert not any(g[0] == 'nav' and abs(g[1]['position'][1]) >= 250000 for g in result['goals'])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--actual-observation', type=Path)
    args = parser.parse_args()
    author = json.loads(AUTHOR.read_text())
    checks = []

    s = fixture(author, 'MCC2'); r = check_plan(s)
    assert names(r) == {'MCC2ENS4'} and all(g[0] == 'inspect' for g in r['goals'])
    assert r['protected_actor_ids']
    target = next(a for a in s['actors'] if a['group'] == 'MCC2ENS4')
    target['inspected'] = 1
    r = check_plan(s)
    assert not r['goals'] and target['id'] in r['protected_actor_ids'] and r['waiting']
    target['inspected'] = 0
    checks.append('MCC2: simultaneous scan/kill selects inspection and suppresses lethal target until observed scan success')
    done(s, 3); r = check_plan(s)
    assert names(r, ('actor',)) == {'MCC2ENS4'} and not r['protected_actor_ids']
    done(s, 4); r = check_plan(s)
    assert names(r) == {'MCC2NAV3'}; checks.append('MCC2: observed scan, kill and extraction states unlock the next ordinary goal')

    s = fixture(author, 'LUT3'); r = check_plan(s)
    assert 'LUT3ENS5' in names(r, ('inspect',)) and 'LUT3ENS5' not in names(r, ('actor',))
    done(s, 1); r = check_plan(s)
    assert 'LUT3ENS5' in names(r, ('actor',)); checks.append('LUT3: inspect-before-kill hazard respected')

    s = fixture(author, 'IVA1'); r = check_plan(s)
    assert {'IVA1ENS1','IVA1ARE2','IVA1ENS2','IVA1ENS3'} <= names(r, ('actor','building'))
    assert not any('GOU' in n for n in names(r, ('nav',)))
    checks.append('IVA1: main control wrapper expands both optional returning patrol kills')
    for i in (4,5,6,9,10,11): done(s, i)
    r = check_plan(s); assert names(r) == {'IVA1NAV3'}
    checks.append('IVA1: optional extraction reach is planned before its dependent main reach activates')

    for scene, expected in [('SOL1',3),('SOL3',5),('WOL5',4)]:
        r = check_plan(fixture(author, scene))
        assert {scene+'ENS'+str(i) for i in range(1,expected+1)} <= names(r, ('actor',))
        checks.append(scene+': explicit win expands all required optional/main combatants')

    s = fixture(author, 'OUT1'); r = check_plan(s)
    areas = names(r, ('building',))
    assert len(areas) == 2 and areas <= {'OUT1ARE2','OUT1ARE3','OUT1ARE4'}
    for i in (1,2,3):
        if node(s,i)['target'].upper() in areas: done(s,i)
    r = check_plan(s); assert not names(r, ('building',)), 'Do not add the optional third facility'
    done(s,4); r = check_plan(s); assert names(r) == {'OUT1NAV4'}
    checks.append('OUT1: one authored two-facility branch, then extraction; no impossible latch route')

    s = fixture(author,'GRA1'); r = check_plan(s)
    assert names(r, ('actor',)) == {'GRA1ENS3'} and r['protected_building_ids']
    assert not r['escort']; checks.append('GRA1: protection setter selects ENS3 and distinguishes structure defence from escort')

    s=fixture(author,'GRA2')
    for a in s['actors']:
        if a['group']=='GRA2DRP1': a['health']=1000000
    r=check_plan(s)
    assert names(r,('actor',))=={'GRA2ENS1','GRA2ENS2','GRA2ENS4'}
    checks.append('GRA2: chooses one explicit success branch and resolves its off-map latch via setter')

    s=fixture(author,'IVA3'); r=check_plan(s)
    assert r['goals'][0][1]['name']=='IVA3NAV1' and 'IVA3NAV2' in names(r)
    node(s,2)['state']=6; r=check_plan(s)
    assert names(r)=={'IVA3GOU3'} and not r['goals'][0][1]['selectable']
    checks.append('IVA3: intended bridge/Sigma route and reachable hidden alternate after timeout')

    s=fixture(author,'MCC3'); r=check_plan(s)
    assert r['goals'][0][1]['name']=='MCC3NAV2' and r['goals'][0][2] < 40000
    assert {'MCC3ENS3','MCC3ENS4'}<=names(r,('actor',))
    checks.append('MCC3: hidden Psi support trigger precedes required heavy-lance attacks')

    s=fixture(author,'MCC4'); r=check_plan(s)
    assert r['escort'] and r['goals'][0][1].get('group')=='MCC4USR1'
    assert 'MCC4NAV2' not in names(r)
    for i in (1,2,3):done(s,i,1)
    node(s,4,1).update(state=3,started=10000);s.update(now=15000,time=15)
    r=check_plan(s);assert r['goals'][0][1].get('group')=='MCC4USR1'
    for i in (2,15):done(s,i)
    r=check_plan(s);assert names(r)=={'MCC4NAV2'}
    checks.append('MCC4: follow live trucks through outbound/pickup wait; player return waits for observed protection completion')

    s=fixture(author,'SIG3'); r=check_plan(s)
    assert r['escort'] and r['anchor'][1]['name']=='SIG3GOU6'
    assert 'SIG3ENS2' in names(r,('actor',))
    for i in (1,2,3):done(s,i,2)
    node(s,4,2)['state']=6
    r=check_plan(s)
    assert r['anchor'][1]['name']=='SIG3GOU7' and 'SIG3ENS4' in names(r,('actor',))
    checks.append('SIG3: defend authored Alpha pickup, then Beta after observed early-window closure; never chase airborne spawn')

    s=fixture(author,'LUT2'); r=check_plan(s)
    assert {f'LUT2ENS{i}' for i in range(1,5)}<=names(r,('actor',))
    assert [0,7] not in r['objective_nodes'] and [0,9] not in r['objective_nodes']
    checks.append('LUT2: main-labelled failure controls are never positive goals')

    s=fixture(author,'LUT4'); r=check_plan(s)
    assert {f'LUT4ENS{i}' for i in range(1,4)}<=names(r,('actor',)) and r['protected_building_ids']
    checks.append('LUT4: optional-labelled first group is required through the main wrapper; city protected')

    cycle=dict(player_table=0,position=[0,0,0],tables=[dict(id=0,nodes=[dict(id=0,kind=0x10000,
        other_table=0,main=1,state=0,require_all=1,items=[dict(type='S',table=0,node=0)])])])
    r=check_plan(cycle);assert r['blocked'] and not r['goals']
    checks.append('Cyclic/unsupported dependencies terminate without changing or asserting objective state')

    for contract in author['route']:
        check_plan(fixture(author,contract[:4]))
    checks.append('All 28 authored route graphs: bounded planning, input purity and no high-altitude latch navigation')

    actual=None
    if args.actual_observation:
        observation=json.loads(args.actual_observation.read_text())
        r=check_plan(observation)
        actual=dict(path=str(args.actual_observation),sha256=hashlib.sha256(args.actual_observation.read_bytes()).hexdigest(),
                    scene=observation.get('scene'),time=observation.get('time'),
                    goals=[dict(kind=g[0],id=g[1]['id'],target=g[1].get('group',g[1].get('name'))) for g in r['goals']],
                    protected_actor_ids=r['protected_actor_ids'],waiting=r['waiting'],blocked=r['blocked'])
        assert not r['blocked']
        checks.append('Actual native observation replay: no observation writes or game interaction')
    report=dict(scope=__doc__,passed=True,planning_checks=checks,actual_observation=actual,
        authored_data_sha256=hashlib.sha256(AUTHOR.read_bytes()).hexdigest(),
        planner_sha256=hashlib.sha256((ROOT/'tests/mercs_campaign_plan.py').read_bytes()).hexdigest())
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {len(checks)} read-only planning checks; no mission victories claimed')


if __name__=='__main__':main()
