#!/usr/bin/env python3
"""Estimates the audio load of osc440 on the RP2350 without hardware.

Runs the firmware ELF on Unicorn (Cortex-M33, Thumb-2, FPU) and counts cycles
with a first-order Cortex-M33 cost model (the one PRA32-U2's estimator uses):
1 per instruction, 2 per single load, 1 + N for load/store-multiple of N
registers, 6 for an integer divide, 14 for VDIV/VSQRT, +2 after a taken branch.

Not modelled: XIP flash waits (code or data read from flash), bus contention
between the cores, interrupts. Code and tables on the audio path are in SRAM,
so the first one matters only for what still runs from flash (see --profile).

Dual core: it runs core 0's real per-block function, render_block() (wake core 1,
render, wait, mix, convert), with the inter-core FIFO always ready so its waits
fall through, and core 1's VoiceEngine::render(1) on its own. A block then takes
core 0's fixed work + the slower of the two renders (the cores run in parallel).

    pip install unicorn capstone pyelftools
    python3 tools/load_estimate.py build/osc440.elf [--profile]
"""

import argparse
import math
import struct
import sys
from collections import Counter

from capstone import CS_ARCH_ARM, CS_MODE_MCLASS, CS_MODE_THUMB, CS_MODE_V8, Cs
from capstone.arm import ARM_OP_REG
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from unicorn import (UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_MCLASS, UC_MODE_THUMB,
                     Uc, UcError)
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                               UC_ARM_REG_SP)

SAMPLE_RATE = 48000
BLOCK = 16
FLASH, SRAM = 0x10000000, 0x20000000
STACK = 0x20082000
RETURN = 0x10fffff0          # "b ." where calls return to
SCRATCH = 0x20070000         # a Bus for render() to write into (well clear of .data/.bss)

MULTIPLE = ('ldm', 'ldmia', 'ldmdb', 'stm', 'stmia', 'stmdb',
            'vldm', 'vstm', 'vldmia', 'vstmia', 'vldmdb', 'vstmdb')


# The SDK's float/double helpers use the RP2350's double coprocessor, which Unicorn
# does not emulate: these are computed in Python instead (softfp: args in r0-r3).
# Only note_on() reaches them; render() uses single-precision VFP only.
def _f(x): return struct.unpack('<f', struct.pack('<I', x & 0xffffffff))[0]
def _fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def _d(lo, hi): return struct.unpack('<d', struct.pack('<II', lo, hi))[0]
def _db(v): return struct.unpack('<II', struct.pack('<d', v))
def _s32(x): return x - (1 << 32) if x & 0x80000000 else x

STAND_INS = {
    '__wrap_exp2f':       lambda r: (_fb(2.0 ** _f(r[0])),),
    '__wrap_expf':        lambda r: (_fb(math.exp(_f(r[0]))),),
    '__wrap_exp':         lambda r: _db(math.exp(_d(r[0], r[1]))),
    '__wrap___aeabi_dadd': lambda r: _db(_d(r[0], r[1]) + _d(r[2], r[3])),
    '__wrap___aeabi_dsub': lambda r: _db(_d(r[0], r[1]) - _d(r[2], r[3])),
    '__wrap___aeabi_dmul': lambda r: _db(_d(r[0], r[1]) * _d(r[2], r[3])),
    '__wrap___aeabi_ddiv': lambda r: _db(_d(r[0], r[1]) / _d(r[2], r[3])),
    '__wrap___aeabi_i2d':  lambda r: _db(float(_s32(r[0]))),
    '__wrap___aeabi_ui2d': lambda r: _db(float(r[0])),
    '__wrap___aeabi_d2f':  lambda r: (_fb(_d(r[0], r[1])),),
    '__wrap___aeabi_d2iz': lambda r: (int(_d(r[0], r[1])) & 0xffffffff,),
    '__wrap___aeabi_d2uiz': lambda r: (int(_d(r[0], r[1])) & 0xffffffff,),
}


def cost(insn):
    m = insn.mnemonic.split('.')[0]
    if m in ('vdiv', 'vsqrt'):
        return 14
    if m in ('sdiv', 'udiv'):
        return 6
    if m in MULTIPLE or m in ('push', 'pop', 'vpush', 'vpop'):
        regs = sum(1 for op in insn.operands if op.type == ARM_OP_REG) - (1 if m in MULTIPLE else 0)
        return 1 + max(regs, 1)
    if m.startswith('ld') or m.startswith('vldr'):
        return 2
    return 1


class Machine:
    def __init__(self, elf_path):
        with open(elf_path, 'rb') as f:
            elf = ELFFile(f)
            self.symbols = self._symbols(elf)
            segments = [(s['p_paddr'], s['p_vaddr'], s.data())
                        for s in elf.iter_segments() if s['p_type'] == 'PT_LOAD']

        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        for base, size in ((FLASH, 0x01000000), (SRAM, 0x00083000),
                           (0x40000000, 0x00200000), (0xd0000000, 0x00020000),
                           (0xe0000000, 0x00100000)):
            self.uc.mem_map(base, size)
        self.uc.mem_write(0xe000ed88, struct.pack('<I', 0xf << 20))   # CPACR: FPU on
        for paddr, vaddr, data in segments:   # the image, and .data where crt0 copies it
            for addr in {paddr, vaddr}:
                self.uc.mem_write(addr, data)
        self.uc.mem_write(RETURN, b'\xfe\xe7')

        self.md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS | CS_MODE_V8)
        self.md.detail = True
        self.decoded = {}
        self.cycles = 0
        self.next_pc = None
        self.stand_in_hits = 0
        self.profile = False
        self.by_function = Counter()
        self.ranges = sorted((a, a + max(s, 2), n) for n, (a, s) in self.symbols.items() if a)
        self.stand_ins = {self.symbols[n][0]: fn for n, fn in STAND_INS.items() if n in self.symbols}
        self.uc.hook_add(UC_HOOK_CODE, self._hook)

    @staticmethod
    def _symbols(elf):
        out = {}
        for sec in elf.iter_sections():
            if isinstance(sec, SymbolTableSection):
                for s in sec.iter_symbols():
                    kind = s['st_info']['type']
                    if s.name and (kind in ('STT_FUNC', 'STT_OBJECT') or s.name.startswith('__init_array')):
                        out[s.name] = (s['st_value'] & ~1, s['st_size'])
        return out

    def find(self, part):
        hits = [n for n in self.symbols if part in n and not n.endswith('_veneer')]
        if len(hits) != 1:
            sys.exit(f'symbol "{part}": {len(hits)} matches {hits[:5]}')
        return self.symbols[hits[0]][0]

    def _name_of(self, addr):
        for lo, hi, name in self.ranges:
            if lo <= addr < hi:
                return name
        return f'0x{addr:08x}'

    def _hook(self, uc, addr, size, _):
        fn = self.stand_ins.get(addr)
        if fn is not None:   # compute it here and return to the caller
            self.stand_in_hits += 1
            regs = [uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)]
            for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1), fn(regs)):
                uc.reg_write(reg, value)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
            self.next_pc = None
            return
        c = self.decoded.get(addr)
        if c is None:
            insn = next(self.md.disasm(bytes(uc.mem_read(addr, size)), addr), None)
            c = cost(insn) if insn else 1
            self.decoded[addr] = c
        if self.next_pc is not None and addr != self.next_pc:
            c_total = c + 2          # the previous instruction was a taken branch
        else:
            c_total = c
        self.next_pc = addr + size
        self.cycles += c_total
        if self.profile:
            self.by_function[addr] += c_total

    def call(self, addr, *args):
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), args):
            self.uc.reg_write(reg, value)
        self.uc.reg_write(UC_ARM_REG_SP, STACK)
        self.uc.reg_write(UC_ARM_REG_LR, RETURN | 1)
        self.next_pc = None
        start = self.cycles
        try:
            self.uc.emu_start(addr | 1, RETURN, count=50_000_000)
        except UcError as e:
            pc = self.uc.reg_read(UC_ARM_REG_PC)
            sys.exit(f'{e} at 0x{pc:08x} ({self._name_of(pc)})')
        return self.cycles - start


def f32(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('elf')
    ap.add_argument('--blocks', type=int, default=200)
    ap.add_argument('--voices', type=int, default=16, help='measure 0..N voices (both cores)')
    ap.add_argument('--mhz', default='153.6,200,250,307.2', help='clocks to report, comma-separated')
    ap.add_argument('--headroom', type=float, default=0.8, help='budget share for capacity')
    ap.add_argument('--scan', choices=('smooth', 'stepped'), default='smooth')
    ap.add_argument('--ladder', choices=('2x', '1x'), default='2x', help='ladder oversampling')
    ap.add_argument('--patch', choices=('demo', 'full', 'worst'), default='full',
                    help='demo: the firmware patch (5 slots); full: + 11 block-rate slots (16 in use); '
                         'worst: full, plus supersaw, FM and sync on every oscillator and 4 audio-rate slots')
    ap.add_argument('--profile', action='store_true', help='cycles by function, all voices')
    args = ap.parse_args()
    clocks = [float(x) for x in args.mhz.split(',')]

    m = Machine(args.elf)
    m.uc.mem_write(0xd0000050, struct.pack('<I', 0x3))   # SIO FIFO_ST: always ready and valid
    engine = m.find('g_engine')
    note_on = m.find('VoiceEngine7note_on')
    set_param = m.find('VoiceEngine9set_param')
    render = m.find('VoiceEngine6render')
    render_block = m.find('render_block')
    bus0, bus1, out = SCRATCH, SCRATCH + 0x400, SCRATCH + 0x800

    start, end = m.symbols['__init_array_start'][0], m.symbols['__init_array_end'][0]
    for p in range(start, end, 4):                       # static constructors, as crt0
        f = struct.unpack('<I', bytes(m.uc.mem_read(p, 4)))[0] & ~1
        if f:
            m.call(f)
    m.call(m.find('setup_patch'))                        # the firmware's patch (5 slots)

    # Parameter ids of TripleWaveGroup (src/synth/triple_wave_group.h, mod_matrix.h)
    WAVE_SCAN, LADDER_OVERSAMPLE, MATRIX_BASE, SLOTS = 1, 14, 400, 16
    OSC_BASE, OSC_STRIDE = 100, 16
    OSC_MODE, OSC_LEVEL, OSC_FM, OSC_SYNC = 0, 4, 8, 9
    MODE_SUPERSAW = 5
    SRC_LFO1, SRC_EXPRESSION, SRC_OSC1, SRC_OSC2, SRC_OSC3, SRC_NOISE = 1, 16, 17, 18, 19, 20
    DST_FM1, DST_FM3, DST_LEVEL1, DST_LEVEL3, DST_AMP = 9, 11, 12, 14, 15
    BLOCK_DESTS = list(range(0, 9)) + list(range(16, 26))   # not audio rate
    DEMO_SLOTS = 5
    sent = 0
    def send(pid, value):
        nonlocal sent
        m.call(set_param, engine, pid, f32(value))
        sent += 1
        if sent % 30 == 0:                               # drain: the queues hold 127 events
            m.call(render, engine, 0, bus0)
            m.call(render, engine, 1, bus1)
    def slot(i, src, dst, amount):
        send(MATRIX_BASE + i * 3 + 0, src)
        send(MATRIX_BASE + i * 3 + 1, dst)
        send(MATRIX_BASE + i * 3 + 2, amount)
    send(WAVE_SCAN, 1.0 if args.scan == 'stepped' else 0.0)
    send(LADDER_OVERSAMPLE, 1.0 if args.ladder == '2x' else 0.0)
    first_free = DEMO_SLOTS
    if args.patch == 'worst':
        send(OSC_BASE + 2 * OSC_STRIDE + OSC_MODE, MODE_SUPERSAW)          # osc 3: 7 saws
        for o in range(3):
            send(OSC_BASE + o * OSC_STRIDE + OSC_FM, 0.05)                 # FM from the neighbour
        for o in (1, 2):
            send(OSC_BASE + o * OSC_STRIDE + OSC_SYNC, 1.0)                # hard sync
        audio = [(SRC_OSC1, DST_LEVEL3), (SRC_NOISE, DST_AMP), (SRC_OSC2, DST_FM3), (SRC_OSC3, DST_LEVEL1)]
        for i, (src, dst) in enumerate(audio):
            slot(first_free + i, src, dst, 0.001)
        first_free += len(audio)
    if args.patch != 'demo':
        for i in range(first_free, SLOTS):               # block-rate: tiny amounts, full cost
            slot(i, SRC_LFO1 + i % SRC_EXPRESSION, BLOCK_DESTS[i % len(BLOCK_DESTS)], 0.001)

    budget = BLOCK / SAMPLE_RATE
    print(f'patch: {args.patch}, scan: {args.scan}, ladder {args.ladder}; budget per block {budget * 1e6:.1f} us = MHz x {budget * 1e6:.1f} cycles\n')
    print(f'{"voices":>6} {"core 0":>7} {"core 1":>7} {"fixed":>6} {"block":>7} {"/voice-smp":>11} '
          + ''.join(f'{f"@{c:g} MHz":>11}' for c in clocks))

    notes = [57, 61, 64, 69, 45, 52, 73, 76, 40, 48, 81, 84, 55, 59, 62, 67]
    empty = (0, 0, 0)
    per_voice = 0.0
    for voices in range(0, args.voices + 1):
        if voices:
            m.call(note_on, engine, notes[(voices - 1) % len(notes)], f32(1.0))
        for _ in range(8):                               # apply the events, pass the gate ramp
            m.call(render, engine, 0, bus0)
            m.call(render, engine, 1, bus1)
        m.profile = args.profile and voices == args.voices
        m.stand_in_hits = 0
        worst = (0, 0, 0, 0)
        for b in range(args.blocks):
            core0_render = m.call(render, engine, 0, bus0)
            core1_render = m.call(render, engine, 1, bus1)
            core0_total = m.call(render_block, b, out)      # renders core 0's group again
            fixed = core0_total - core0_render
            block = fixed + max(core0_render, core1_render)
            if block > worst[3]:
                worst = (core0_render, core1_render, fixed, block)
        m.profile = False
        if m.stand_in_hits:
            print(f'warning: {m.stand_in_hits} library helpers called on the audio path (not costed)')
        c0, c1, fixed, block = worst
        if voices == 0:
            empty = (c0, c1, fixed)              # render overhead of each core, fixed work
            col = f'{"-":>11}'
        else:
            per_voice = ((c0 - empty[0]) + (c1 - empty[1])) / voices / BLOCK
            col = f'{per_voice:11.1f}'
        print(f'{voices:>6} {c0:>7} {c1:>7} {fixed:>6} {block:>7} {col}'
              + ''.join(f'{100 * block / (budget * c * 1e6):10.1f}%' for c in clocks))

    print(f'\nvoices per module (both cores) within {args.headroom:.0%} of the budget:')
    overhead = empty[2] + max(empty[0], empty[1])   # a block with no voices
    for c in clocks:
        per_core = int((args.headroom * budget * c * 1e6 - overhead) / (per_voice * BLOCK))
        print(f'  {c:6g} MHz: {2 * per_core:4d} voices')

    if args.profile:
        print(f'\ncycles by function, {args.voices} voices:')
        totals = Counter()
        for addr, c in m.by_function.items():
            totals[m._name_of(addr)] += c
        for name, c in totals.most_common(10):
            region = 'SRAM' if m.symbols.get(name, (0,))[0] >= SRAM else 'FLASH'
            print(f'  {100 * c / sum(totals.values()):5.1f}%  {region:5}  {name}')


if __name__ == '__main__':
    main()
