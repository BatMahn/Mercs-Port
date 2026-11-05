#!/usr/bin/env python3
"""Compare native effect tables/dispatch with original DOS and Windows code.

Dependencies: pefile and Unicorn. Only the original type-dispatch prefix is
executed; execution stops before object, audio or effect allocation. This does
not claim full special-weapon or renderer equivalence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP
from extract_mercs_effects import pe_offset, elf_offset

STACK, STOP = 0x08000000, 0x09000000


class Dispatcher:
    def __init__(self, data, dos):
        self.dos = dos
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.result = None
        if dos:
            ph = struct.unpack_from('<I', data, 28)[0]
            for i in range(struct.unpack_from('<H', data, 44)[0]):
                kind, off, va, _, size, mem_size, _, _ = struct.unpack_from('<8I', data, ph + i * 32)
                if kind != 1:
                    continue
                self.uc.mem_map(va, (mem_size + 4095) & ~4095)
                self.uc.mem_write(va, data[off:off + size])
        else:
            pe = pefile.PE(data=data)
            self.uc.mem_map(pe.OPTIONAL_HEADER.ImageBase, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
            self.uc.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
        self.uc.mem_map(STACK, 0x10000)
        self.uc.mem_map(STOP, 0x1000)
        self.uc.hook_add(UC_HOOK_CODE, self.hook)

    def hook(self, uc, address, size, _):
        if address == (0x64c0d if self.dos else 0x1004ae51):
            pointer = uc.reg_read(UC_X86_REG_EDX if self.dos else UC_X86_REG_EAX)
            base = 0xbe6dc if self.dos else 0x100a9670
            assert (pointer - base) % 44 == 0
            self.result = (pointer - base) // 44
            uc.emu_stop()
        elif self.visual == 11 and uc.mem_read(address, 1) == b'\xe8':
            self.result = -1  # original debris callback has no ordinary impact-card return
            uc.emu_stop()
        elif address == STOP:
            self.result = -1
            uc.emu_stop()

    def run(self, visual, flags):
        self.visual = visual
        self.result = None
        sp = STACK + 0xf000
        for r in (UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX, UC_X86_REG_ECX):
            self.uc.reg_write(r, 0)
        if self.dos:
            self.uc.mem_write(sp, struct.pack('<13I', STOP, *([0] * 12)))
            self.uc.reg_write(UC_X86_REG_EBX, (visual | flags) & 0xffffffff)
        else:
            self.uc.mem_write(sp, struct.pack('<13I', STOP, 0, 0, (visual | flags) & 0xffffffff, *([0] * 9)))
        self.uc.reg_write(UC_X86_REG_ESP, sp)
        self.uc.emu_start(0x64ad8 if self.dos else 0x1004ab46, STOP + 1, count=10000)
        if self.result is None:
            raise RuntimeError('Original dispatch did not reach an expected boundary')
        return self.result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--windows', type=Path, required=True)
    p.add_argument('--dos-elf', type=Path, required=True)
    p.add_argument('--native', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    win, dos = a.windows.read_bytes(), a.dos_elf.read_bytes()
    if hashlib.sha256(win).hexdigest() != 'bc375715180221e42fa5970ef966854fc691f076762fb9c71df846b04dfb69ef':
        raise ValueError('Unknown original retail Windows image')
    native = json.loads(subprocess.check_output([str(a.native.resolve()), '--dump'], text=True))
    tables = [[struct.unpack_from('<11i', data, offset + i * 44) for i in range(36)]
              for data, offset in [(win, pe_offset(win, 0x100a9670)), (dos, elf_offset(dos, 0xbe6dc))]]
    for i, got in enumerate(native['tables']):
        w, d = tables[0][i], tables[1][i]
        expected = [d[0], w[0], w[2], w[4], w[8] if w[6] else 0, w[9] if w[6] else 0]
        assert got == expected, ('table', i, got, expected)
    observations = []
    for engine_dos, image in [(False, win), (True, dos)]:
        machine = Dispatcher(image, engine_dos)
        for visual, flags, got in native['dispatch']:
            expected = machine.run(visual, flags)
            assert got == expected, ('dispatch', engine_dos, visual, flags, got, expected)
            observations.append({'engine': 'dos' if engine_dos else 'windows', 'visual': visual,
                                 'flags': flags, 'original': expected, 'native': got})
        for invalid in (-1, 36, 255):
            assert machine.run(invalid, 0) == -1
    output = {
        'windows_sha256': hashlib.sha256(win).hexdigest(), 'dos_elf_sha256': hashlib.sha256(dos).hexdigest(),
        'native_sha256': hashlib.sha256(a.native.read_bytes()).hexdigest(),
        'table_rows_matched': 36, 'fields_per_row': 6,
        'original_dispatch_cases_matched': len(observations),
        'original_invalid_cases_passed': 6,
        'scope': 'Original dispatch prefix only; original tables read directly; no allocation/audio/render callbacks executed',
        'observations': observations,
    }
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(output, indent=2) + '\n')
    print('PASS: 36 effect table rows, %d original DOS/Windows dispatch cases, 6 invalid inputs' % len(observations))


if __name__ == '__main__':
    main()
