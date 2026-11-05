#!/usr/bin/env python3
"""Execute original retail Mercs GPS5 setup and exhausted-weapons decisions.

Requires pefile/unicorn and the sibling combat verifier's image loader.
The GPS transfer, configuration, level/flag initializer and reaction branch
execute original instructions. Unrelated AI services/manoeuvre updates are
isolated; RNG results follow a controlled native seed to compare decisions,
not to claim original random-stream parity. No original bytes are emitted.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from verify_mercs_combat_original import Original, HEAP, STACK, STOP, CTRL, OBJ, words
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP

EXPECTED = {
    'dos': '972d5502d0083f2e1dc832b97095098e4f3b77384d0313f9461ad3cf2eed622c',
    'windows': 'bc375715180221e42fa5970ef966854fc691f076762fb9c71df846b04dfb69ef',
}
BASELINE_SHA256 = '473f610a9e30d31a30eb663b8bb3cca1d735e02ba5949f99e59dc5aa8ce4a25a'
TABLE, PLAYER = HEAP + 0x3000, HEAP + 0x4000


def signed(n):
    return n - 0x100000000 if n >= 0x80000000 else n


def advance(seed, bound):
    seed = (seed * 1103515245 + 12345) & 0xffffffff
    return seed, ((seed >> 16) & 0x7fff) % bound


def seed_for(first, second):
    for seed in range(1000000):
        state, a = advance(seed, 4)
        _, b = advance(state, 100)
        if a == first and b == second:
            return seed
    raise RuntimeError('No bounded fixture seed found')


class OriginalAI(Original):
    def __init__(self, image, dos):
        super().__init__(image, dos)
        self.offset = dict(
            low=0x186 if dos else 0x188,
            threshold=0x1aa if dos else 0x1ac,
            level=0x185 if dos else 0x187,
            flags=0x1ee if dos else 0x1f2,
            man=0x1c2 if dos else 0x1c4,
            prev=0x1c3 if dos else 0x1c6,
            forced=0x1c4 if dos else 0x1c8,
            queue=0x1d4 if dos else 0x1d8,
            jumping=0x1e6 if dos else 0x1ea,
        )
        self.init_entry = 0x2ac7c if dos else 0x100269a7
        self.attack_entry = 0x37168 if dos else 0x10041cb0
        self.unit_array = 0x1a5530 if dos else 0x100eef40
        self.config_table = 0xa8ad8 if dos else 0x100ad1bc

    def hook(self, uc, address, size, _):
        if address == STOP:
            uc.emu_stop()
            return
        if address not in self.services:
            return
        action = self.services[address]
        sp = uc.reg_read(UC_X86_REG_ESP)
        if action == 'rng':
            bound = uc.reg_read(UC_X86_REG_EAX) if self.dos else self.rd(sp+4)
            self.rng, result = advance(self.rng, bound)
            self.rolls.append({'bound': bound, 'value': result})
        elif action == 'allocate':
            result = HEAP+0x6000
        else:
            result = action
        uc.reg_write(UC_X86_REG_EAX, result & 0xffffffff)
        uc.reg_write(UC_X86_REG_EIP, self.rd(sp))
        uc.reg_write(UC_X86_REG_ESP, sp+4)

    def call(self, entry, arg, arg2=0):
        sp = STACK + 0x8000
        self.uc.mem_write(sp, words(STOP, arg, arg2))
        self.uc.reg_write(UC_X86_REG_ESP, sp)
        self.uc.reg_write(UC_X86_REG_EAX, arg)
        self.uc.reg_write(UC_X86_REG_EDX, arg2)
        self.uc.emu_start(entry, STOP, count=100000)
        if self.uc.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError(f'Original did not return: {entry:x}')

    def initialize(self, raw, previous, level=1, unit_class=1):
        self.uc.mem_write(HEAP, bytes(0x10000))
        self.uc.mem_write(STACK, bytes(0x10000))
        self.services = ({0x2af60:0, 0x360d8:0, 0x2d984:0, 0x52fac:0}
                         if self.dos else
                         {0x10026e24:0, 0x1004d47f:0, 0x1002b4cf:0, 0x10008371:0})
        self.wr(CTRL, OBJ)
        self.wr(OBJ, unit_class)
        self.wr(OBJ+4, 2)
        self.wr(OBJ+8, 2)
        self.wr(OBJ+0x10, 2)
        self.wr(OBJ+0x20, CTRL)
        self.wr(PLAYER+8, 1)
        self.wr(self.player, 1)
        self.wr(self.unit_array+4, PLAYER)
        self.wr(self.config_table, TABLE)
        self.uc.mem_write(OBJ+0x15c, struct.pack('<8H', 1,250,250,250,level,raw,0,0))
        self.wr(OBJ+self.offset['threshold'], previous)
        self.call(self.init_entry, OBJ)
        return dict(low_byte=self.uc.mem_read(OBJ+self.offset['low'],1)[0],
                    threshold=signed(self.rd(OBJ+self.offset['threshold'])),
                    level=self.uc.mem_read(OBJ+self.offset['level'],1)[0],
                    flags=self.rd(OBJ+self.offset['flags']))

    def reaction(self, case):
        setup = self.initialize(case['raw'], case['previous'], case['level'], case['unit_class'])
        off = self.offset
        for key in ('man','prev'):
            value = case[key]
            if self.dos:
                self.uc.mem_write(OBJ+off[key], bytes([value & 255]))
            else:
                self.uc.mem_write(OBJ+off[key], struct.pack('<h',value))
        self.wr(OBJ+off['forced'], 0 if case['forced']==-1 else case['forced'])
        self.wr(OBJ+off['queue'], 0)
        self.wr(OBJ+off['jumping'], int(case['man'] in (4,5,9,11)))
        self.uc.mem_write(OBJ+0x176, struct.pack('<h',case['ai_state']))
        self.wr(CTRL+0x3b8, case['moving'])
        self.wr(CTRL+0x3f4, 10)
        self.wr(CTRL+0x420, case['can_jump'])
        self.wr(CTRL+0x3cc, case['heat']*65536)
        self.rng, self.rolls = case['seed'], []
        self.services = ({
            0x38a84:0, 0x3813c:0, 0x383dc:0, 0x2c28c:0, 0x2cb00:0,
            0x39850:0, 0x386c8:0, 0x39804:0, 0x37e1c:0,
            0x39220:case['empty'], 0x5c764:'rng',
        } if self.dos else {
            0x10043f2c:0, 0x100431e6:0, 0x10043567:0, 0x100438da:0,
            0x1004398b:0, 0x10043947:0, 0x1004521e:0, 0x10042d09:0,
            0x10044933:case['empty'], 0x10051f43:'rng',
        })
        self.call(self.attack_entry, OBJ, 1)
        forced = signed(self.rd(OBJ+off['forced']))
        return dict(setup=setup, forced=-1 if forced==0 else forced,
                    queue=self.rd(OBJ+off['queue']), rolls=self.rolls, rng=self.rng)

    def dos_zero_allocation(self):
        """Original calloc/memset execute; only raw allocation is supplied."""
        assert self.dos
        target,size=HEAP+0x6000,0x24c
        self.uc.mem_write(target,bytes([0xa5])*size)
        self.services={0x6d8e6:'allocate'}
        self.call(0x41acc,size)
        assert bytes(self.uc.mem_read(target,size))==bytes(size)
        return dict(entry='0x41acc',calloc='0x6d84e',memset='0x6d870',
                    raw_allocator_intercepted='0x6d8e6',bytes=size,
                    initial_fill=165,all_zero_after=True)


def reaction_cases():
    cases = []
    def add(name, raw=0, first=0, second=34, **kw):
        case = dict(name=name,raw=raw,previous=0,level=1,unit_class=1,
                    seed=seed_for(first,second),moving=1,empty=1,can_jump=0,
                    heat=0,man=0,prev=0,ai_state=3,forced=-1)
        case.update(kw)
        cases.append(case)
    for raw in (0,1,2,4):
        for roll in (0,1,32,33,34,99):
            add(f'attribute-{raw}-roll-{roll}',raw=raw,second=roll)
    add('high-word-ignored-zero',raw=0x1200)
    add('high-word-ignored-two',raw=0x1202,second=99)
    add('unknown-prior-equality',raw=3,previous=77,second=77)
    add('unknown-prior-past',raw=3,previous=77,second=78)
    add('unknown-prior-negative',raw=255,previous=-7,second=0)
    for unit_class in (0,2,3,4,5,8):
        add(f'class-{unit_class}-cannot-charge',unit_class=unit_class,second=99)
    for level in (0,2,3,4,5,6,7):
        add(f'level-{level}-gate',level=level,second=99)
    add('not-moving',moving=0)
    add('working-weapons',empty=0)
    add('already-fleeing',ai_state=4)
    add('already-charging',man=6)
    add('jump-manoeuvre',man=4)
    add('prequeued-reaction-overwritten',forced=11)
    add('jump-selected',first=1,can_jump=1,heat=39)
    add('heat-40-forces-fallback',first=1,can_jump=1,heat=40)
    add('previous-five-forces-fallback',first=1,can_jump=1,prev=5)
    add('first-random-zero-forces-fallback',first=0,can_jump=1)
    add('no-jets-forces-fallback',first=1,can_jump=0)
    return cases


def gps_inventory(path):
    data = path.read_bytes()
    rows = []
    for ti in range(struct.unpack_from('<H',data,24)[0]):
        ent = 26+24*ti
        if data[ent:ent+4].rstrip(b'\0') != b'BWD':
            continue
        index = struct.unpack_from('<I',data,ent+4)[0]
        capacity = struct.unpack_from('<H',data,index+16)[0]
        for rid in range(1,capacity):
            at,size = struct.unpack_from('<II',data,index+22+8*rid)
            if not at or size<74:
                continue
            record = data[at:at+size]
            assert record[:4]==b'DATA' and len(record)==size
            assert sum(record[12:])&0xffffffff == struct.unpack_from('<I',record,8)[0]
            body = record[62:]
            if body[:4]!=b'BWD\0':
                continue
            pos = 12
            while pos+8<=len(body):
                size = struct.unpack_from('<I',body,pos+4)[0]
                assert size>=8 and pos+size<=len(body)
                if body[pos:pos+4]==b'GPS\0' and size>=100:
                    gps = body[pos+8:pos+size]
                    rows.append(dict(record=record[30:46].split(b'\0')[0].decode('latin1'),
                        control=gps[6],raw=struct.unpack_from('<H',gps,18)[0],
                        level=gps[16],name=gps[46:68].split(b'\0')[0].decode('latin1')))
                pos += size
    return dict(prj_sha256=hashlib.sha256(data).hexdigest(),gps_count=len(rows),
                histogram=dict(sorted(Counter(r['raw'] for r in rows).items())),
                ai_histogram=dict(sorted(Counter(r['raw'] for r in rows if r['control']==2).items())),
                records=rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dos-exe',required=True,type=Path)
    ap.add_argument('--win-dll',required=True,type=Path)
    ap.add_argument('--prj',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    args = ap.parse_args()
    dos,win = args.dos_exe.read_bytes(),args.win_dll.read_bytes()
    actual = dict(dos=hashlib.sha256(dos).hexdigest(),windows=hashlib.sha256(win).hexdigest())
    if actual!=EXPECTED:
        raise SystemExit('Address-specific verifier: original binary hashes do not match.')
    with tempfile.TemporaryDirectory(prefix='mercs-ai-') as td:
        elf = Path(td)/'mercs.elf'
        subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'le2elf.py'),
                        str(args.dos_exe),str(elf)],check=True,stdout=subprocess.DEVNULL)
        images = {1:elf.read_bytes(),0:win}
    setup_cases, results, allocation = [], [], None
    for is_dos in (1,0):
        original = OriginalAI(images[is_dos],is_dos)
        for raw in (0,1,2,3,4,127,128,255,0x1200,0x1201,0x1202,0x1204,0xffff):
            for previous in (0,77):
                setup_cases.append(dict(dos=is_dos,raw=raw,previous=previous,
                                        output=original.initialize(raw,previous)))
        for case in reaction_cases():
            results.append(dict(dos=is_dos,input=case,output=original.reaction(case)))
        if is_dos:
            allocation=original.dos_zero_allocation()
    n = len(reaction_cases())
    for a,b in zip(results[:n],results[n:]):
        assert a['output']==b['output'], (a,b)
    inventory = gps_inventory(args.prj)
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'mercs_ai_original.json').write_text(json.dumps(dict(
        source_baseline_sha256=BASELINE_SHA256,binary_sha256=actual,
        scope=__doc__,setup_cases=setup_cases,reaction_cases=results,
        dos_zero_allocation=allocation,
        passed_setup=len(setup_cases),passed_reactions=len(results)),indent=2)+'\n')
    (args.output/'mercs_ai_gps_inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
    fields=['raw','previous','level','unit_class','moving','empty','can_jump','heat','man','prev','ai_state','forced']
    header=['/* Generated from original Mercs instructions; see verify_mercs_ai_original.py. */',
            'typedef struct { int raw, previous, threshold; } mercs_ai_setup_case;',
            'static const mercs_ai_setup_case mercs_ai_setup_cases[] = {']
    for c in setup_cases:
        header.append(f"    {{{c['raw']}, {c['previous']}, {c['output']['threshold']}}},")
    header += ['};','typedef struct { const char *name; int '+', '.join(fields)+
               '; unsigned seed; int threshold, normalized_level, want_forced, want_queue; unsigned want_rng; } mercs_ai_reaction_case;',
               'static const mercs_ai_reaction_case mercs_ai_reaction_cases[] = {']
    for record in results:
        c,o=record['input'],record['output']
        values=[str(c[k]) for k in fields]+[str(c['seed'])+'u',str(o['setup']['threshold']),
                str(o['setup']['level']),str(o['forced']),str(o['queue']),str(o['rng'])+'u']
        header.append('    {"'+('dos-' if record['dos'] else 'win-')+c['name']+'", '+', '.join(values)+'},')
    header += ['};','']
    (args.output/'mercs_ai_cases.h').write_text('\n'.join(header))
    print(json.dumps(dict(setup_cases=len(setup_cases),reaction_cases=len(results),
                          authored_gps=inventory['gps_count'],histogram=inventory['histogram'])))


if __name__=='__main__':
    main()
