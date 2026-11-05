#!/usr/bin/env python3
"""Execute supplied retail Mercs heat routines in Unicorn, write comparison fixtures.

Original executable bytes are input only. Calls to sound/message, destruction and
BlowAmmo are isolated and recorded; this tests heat decisions, not their callees
or the complete game. RNG results are supplied deterministically to both sides.
Requires pefile and unicorn. Run with --help for input/output paths.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP

HEAP, STACK, STOP = 0x20000000, 0x30000000, 0x40000000
CTRL, OBJ, DIF = HEAP, HEAP + 0x1000, HEAP + 0x2000

def words(*n):
    return struct.pack('<' + 'I' * len(n), *(x & 0xffffffff for x in n))

def next_rand(seed, bound):
    seed = (seed * 1103515245 + 12345) & 0xffffffff
    return ((seed >> 16) & 0x7fff) % bound if bound > 0 else 0

def make_cases(dos):
    unit = 182 if dos else 181
    cases = []
    def add(name, heat, **kw):
        d = dict(name=name, dos=dos, heat_q=round(heat*65536), gain_q=0,
                 sink_q=0, ticks=1, elapsed=1, state=2, flags=0,
                 warn=0, warn_elapsed=1, human=1, invulnerable=0,
                 heat_on=1, seed=1)
        d.update(kw)
        cases.append(d)
    add('whole-degree-below-warning', 65.75, gain_q=8192)
    add('warning-at-66', 65.875, gain_q=8192)
    add('whole-degree-below-sequence', 80.75, gain_q=8192)
    add('sequence-at-81', 81)
    add('whole-degree-below-destruction', 100.75)
    add('jump-above-100-does-not-start-sequence', 101)
    add('fatal-timer-equal', 101, elapsed=20*unit)
    add('fatal-timer-past', 101, elapsed=20*unit+1)
    add('shutdown-equal', 85, flags=4, elapsed=6*unit)
    add('shutdown-past', 85, flags=4, elapsed=6*unit+1)
    add('shutdown-and-warning-same-update', 69, gain_q=65536, flags=4, elapsed=6*unit+1)
    add('restart-equal', 60, state=3, flags=4, elapsed=4*unit)
    add('restart-past', 60, state=3, flags=4, elapsed=4*unit+1)
    add('restart-whole-degree-65', 65.875, state=3, flags=4, elapsed=4*unit+1)
    add('no-restart-at-66', 66, state=3, flags=4, elapsed=4*unit+1)
    add('warning-latch-at-50', 50, warn=1, warn_elapsed=10*unit)
    add('warning-latch-at-40', 40, warn=1, warn_elapsed=10*unit)
    add('warning-reset-below-40', 39.875, warn=1, warn_elapsed=4*unit+1)
    add('warning-reset-timer-equal', 39, warn=1, warn_elapsed=4*unit)
    add('cooling-does-not-warn', 70, sink_q=50)
    add('heat-floor', 0, sink_q=50)
    add('shutdown-double-cooling', 60, sink_q=50, state=3)
    add('heat-disabled', 90, heat_on=0)
    add('ai-does-not-warn', 69, gain_q=65536, human=0)
    add('ai-sequence-no-player-alarm', 81, human=0)
    add('invulnerable-no-fatal', 101, elapsed=20*unit+1, invulnerable=1)
    add('override-clears-on-cooling', 60, flags=12)
    add('alarm-latch-clears-when-cool', 60, flags=4096)
    add('sequence-already-latched', 81, flags=4096)
    for hit in (True, False):
        bound = 4000 // 6
        seed = next(s for s in range(100000) if (next_rand(s, bound)<3) == hit)
        add('override-cookoff' if hit else 'override-no-cookoff', 101,
            flags=8, ticks=6, elapsed=6, seed=seed)
    return cases

class Original:
    def __init__(self, image, dos):
        self.dos = dos
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        if dos:
            phoff = struct.unpack_from('<I', image, 28)[0]
            phsize, phnum = struct.unpack_from('<HH', image, 42)
            segments = []
            for i in range(phnum):
                typ, off, va, _, fs, ms, _, _ = struct.unpack_from('<8I', image, phoff+i*phsize)
                if typ == 1:
                    segments.append((va, image[off:off+fs], ms))
            lo = min(v for v, _, _ in segments) & ~0xfff
            hi = max(v+s for v, _, s in segments)
            self.uc.mem_map(lo, (hi-lo+0xfff)&~0xfff)
            for va, data, _ in segments:
                self.uc.mem_write(va, data)
            self.entry = 0x2ed2c
            self.clock, self.frame, self.player, self.dif = 0xa7e1c, 0xa7e24, 0xa7e70, 0xa7e6c
            self.warn, self.warn_time, self.net = 0xa8e0c, 0x1a8020, 0xb5db8
            self.calls = {0x5c764:'rng', 0x52f24:'message', 0x516fc:'sound', 0x2ebd0:'cookoff', 0x2f0c0:'destroy'}
        else:
            pe = pefile.PE(data=image)
            self.uc.mem_map(pe.OPTIONAL_HEADER.ImageBase, pe.OPTIONAL_HEADER.SizeOfImage)
            self.uc.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
            self.entry = 0x1000465a
            self.clock, self.frame, self.player, self.dif = 0x100b38fc, 0x100b3904, 0x100adf38, 0x100adf34
            self.warn, self.warn_time, self.net = 0x100a4118, 0x100c4c8c, 0x100aa1ac
            self.calls = {0x10051f43:'rng', 0x100082c4:'message', 0x10003742:'sound', 0x100044c6:'cookoff', 0x10004b19:'destroy'}
        self.uc.mem_map(HEAP, 0x10000)
        self.uc.mem_map(STACK, 0x10000)
        self.uc.mem_map(STOP, 0x1000)
        self.uc.mem_write(STOP, b'\xcc')
        self.uc.hook_add(UC_HOOK_CODE, self.hook)

    def rd(self, at):
        return struct.unpack('<I', self.uc.mem_read(at,4))[0]

    def wr(self, at, value):
        self.uc.mem_write(at,words(value))

    def hook(self, uc, address, size, _):
        if address == STOP:
            uc.emu_stop()
            return
        action = self.calls.get(address)
        if not action:
            return
        sp = uc.reg_read(UC_X86_REG_ESP)
        arg = uc.reg_read(UC_X86_REG_EAX) if self.dos else self.rd(sp+4)
        if action == 'rng':
            self.rng_bounds.append(arg)
            uc.reg_write(UC_X86_REG_EAX, next_rand(self.case['seed'], arg))
        elif action == 'message':
            self.messages.append(arg)
        elif action == 'sound':
            self.sounds.append(arg)
        elif action == 'cookoff':
            self.cookoff += 1
        elif action == 'destroy':
            self.wr(CTRL+0x3d4,4)
            self.destroy += 1
        uc.reg_write(UC_X86_REG_EIP, self.rd(sp))
        uc.reg_write(UC_X86_REG_ESP, sp+4+(4 if self.dos and action=='sound' else 0))

    def run(self, case):
        self.case = case
        self.messages, self.sounds, self.rng_bounds = [], [], []
        self.cookoff = self.destroy = 0
        self.uc.mem_write(HEAP, bytes(0x10000))
        self.uc.mem_write(STACK, bytes(0x10000))
        now = 100000
        self.wr(CTRL, OBJ)
        self.wr(OBJ+4, 1 if case['human'] else 2)
        self.wr(self.player,1)
        self.wr(self.dif,DIF)
        self.uc.mem_write(DIF,bytes([0,case['invulnerable'],0,0,case['heat_on'],0,0,0]))
        self.wr(self.net,0)
        self.wr(self.clock,now)
        self.wr(self.frame,case['ticks'])
        self.wr(self.warn,case['warn'])
        self.wr(self.warn_time,now-case['warn_elapsed'])
        for off,key in [(0x3cc,'heat_q'),(0x3c4,'gain_q'),(0x3d0,'sink_q'),(0x3d4,'state')]:
            self.wr(CTRL+off,case[key])
        self.wr(CTRL+0x3bc,now-case['elapsed'])
        self.uc.mem_write(CTRL+0x440,struct.pack('<H',case['flags']))
        sp = STACK+0x8000
        self.uc.mem_write(sp, words(STOP, CTRL))
        self.uc.reg_write(UC_X86_REG_ESP,sp)
        self.uc.reg_write(UC_X86_REG_EAX,CTRL)
        self.uc.emu_start(self.entry, STOP, count=20000)
        state = self.rd(CTRL+0x3d4)
        events = (int(0 in self.messages) | (int(3 in self.messages)<<1) |
                  (int(13 in self.messages)<<2) | (int(case['state']==3 and state==0)<<3) |
                  (int(self.destroy>0)<<4) | (int(self.cookoff>0)<<5))
        return dict(heat_q=self.rd(CTRL+0x3cc),state=state,
                    flags=struct.unpack('<H',self.uc.mem_read(CTRL+0x440,2))[0],
                    warn=self.rd(self.warn),events=events,
                    elapsed=(now-self.rd(CTRL+0x3bc))&0xffffffff,
                    warn_elapsed=(now-self.rd(self.warn_time))&0xffffffff,
                    rng_bounds=self.rng_bounds,messages=self.messages,sounds=self.sounds)

    def critical_probe(self, ammunition=False):
        """Exercise actual critical-slot mutation with no game services required."""
        locs,mounts,bins=HEAP+0x3000,HEAP+0x4000,HEAP+0x5000
        self.uc.mem_write(HEAP,bytes(0x10000))
        self.uc.mem_write(STACK,bytes(0x10000))
        self.wr(CTRL,OBJ); self.wr(OBJ+4,2); self.wr(self.player,1)
        self.wr(CTRL+0x54,mounts); self.wr(CTRL+0x58,locs)
        self.wr(CTRL+0x5c,bins); self.wr(CTRL+0x60,1); self.wr(CTRL+0x3fc,1)
        for i in range(14):
            self.wr(mounts+i*0x40+4,-1)
        for i,item in enumerate([4801,4802]):
            self.wr(mounts+i*0x40,1)
            self.wr(mounts+i*0x40+4,48)
            self.wr(mounts+i*0x40+8,1)
            self.wr(mounts+i*0x40+0x2c,item)
        for i in range(8):
            self.wr(locs+i*40+8,100*65536)
            values=[10101] if ammunition else [4801,8001,4801,4802]
            self.uc.mem_write(locs+i*40+12,struct.pack('<'+'H'*len(values),*values))
            self.uc.mem_write(locs+i*40+36,struct.pack('<H',len(values)))
        self.uc.mem_write(bins+2,struct.pack('<HH',10,10101))
        self.uc.mem_write(bins+8,struct.pack('<H',2))
        self.wr(bins+0x12,0)
        self.wr(CTRL+0x66,10)
        sp=STACK+0x8000
        silent=0 if ammunition else 1
        if self.dos:
            self.uc.mem_write(sp,words(STOP,silent))
            for reg,value in [(UC_X86_REG_EAX,2),(UC_X86_REG_EDX,CTRL),(UC_X86_REG_EBX,5),(UC_X86_REG_ECX,0)]:
                self.uc.reg_write(reg,value)
            entry=0x2f918
        else:
            self.uc.mem_write(sp,words(STOP,2,CTRL,5,0,silent))
            entry=0x10005624
        self.uc.reg_write(UC_X86_REG_ESP,sp)
        self.uc.emu_start(entry,STOP,count=20000)
        return dict(dos=self.dos,probe='normal-ammo-pool-guard' if ammunition else 'weapon-all-slots',
                    slot_counts=[struct.unpack('<H',self.uc.mem_read(locs+i*40+36,2))[0] for i in range(8)],
                    slots=[list(struct.unpack('<12H',self.uc.mem_read(locs+i*40+12,24))) for i in range(8)],
                    weapon_states=[self.rd(mounts+i*0x40+8) for i in range(2)],
                    ammo_rounds=struct.unpack('<H',self.uc.mem_read(bins+2,2))[0],
                    pool_rounds=self.rd(CTRL+0x66),
                    internal_q=[self.rd(locs+i*40+8) for i in range(8)])

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dos-exe',required=True,type=Path)
    ap.add_argument('--win-dll',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args()
    original_dos=args.dos_exe.read_bytes()
    original_win=args.win_dll.read_bytes()
    expected={'dos':'972d5502d0083f2e1dc832b97095098e4f3b77384d0313f9461ad3cf2eed622c',
              'windows':'bc375715180221e42fa5970ef966854fc691f076762fb9c71df846b04dfb69ef'}
    actual={'dos':hashlib.sha256(original_dos).hexdigest(),'windows':hashlib.sha256(original_win).hexdigest()}
    if expected!=actual:
        raise SystemExit('This verifier is address-specific; supplied retail executable hashes do not match.')
    with tempfile.TemporaryDirectory(prefix='mercs-heat-') as td:
        elf=Path(td)/'mercs.elf'
        subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'le2elf.py'),str(args.dos_exe),str(elf)],check=True,stdout=subprocess.DEVNULL)
        images={0:original_win,1:elf.read_bytes()}
    results=[]
    critical=[]
    for dos in (1,0):
        orig=Original(images[dos],dos)
        for case in make_cases(dos):
            results.append({'input':case,'output':orig.run(case)})
        critical.append(orig.critical_probe(False))
        critical.append(orig.critical_probe(True))
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'mercs_heat_original.json').write_text(json.dumps(dict(
        binary_sha256=actual,scope='Retail DOS and Windows heat decision routines only; external effects and RNG isolated.',cases=results),indent=2)+'\n')
    (args.output/'mercs_critical_original.json').write_text(json.dumps(dict(binary_sha256=actual,probes=critical),indent=2)+'\n')
    for probe in critical:
        if probe['probe']=='weapon-all-slots':
            assert probe['slot_counts']==[2]*8
            assert probe['slots']==[[8001,4802]+[0]*10]*8
            assert probe['weapon_states']==[0xffffffff,1]
        else:
            assert probe['slot_counts']==[1,1,1,1,0,1,1,1]
            assert probe['pool_rounds']==probe['ammo_rounds']==10
            assert probe['internal_q']==[100*65536]*8
    inputs=['dos','heat_q','gain_q','sink_q','ticks','elapsed','state','flags','warn','warn_elapsed','human','invulnerable','heat_on','seed']
    outputs=['heat_q','state','flags','warn','events','elapsed','warn_elapsed']
    header=['/* Generated by tools/research/verify_mercs_combat_original.py from supplied retail binaries. */',
            '/* DOS SHA256: '+actual['dos']+' */','/* Windows SHA256: '+actual['windows']+' */',
            'typedef struct { const char *name; int '+', '.join(inputs)+'; int '+', '.join('want_'+x for x in outputs)+'; } mercs_heat_case;',
            'static const mercs_heat_case mercs_heat_cases[] = {']
    for r in results:
        a,b=r['input'],r['output']
        header.append('    {"'+('dos-' if a['dos'] else 'win-')+a['name']+'", '+', '.join(str(a[x]) for x in inputs)+', '+', '.join(str(b[x]) for x in outputs)+'},')
    header.append('};\n')
    (args.output/'mercs_heat_cases.h').write_text('\n'.join(header))
    print(json.dumps({'case_count':len(results),'binary_sha256':actual,'output':str(args.output)}))

if __name__=='__main__':
    main()
