"""Run production C++ with the pinned ESP compiler in a deterministic RV32 emulator.

No test firmware is flashed. Requires unicorn==2.1.4 in the local Python env or
build/rsvp-v1c1/python-deps. A nonzero result identifies the failing C++ line.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rsvp-v1c1'
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(OUT / 'python-deps'))
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32
from unicorn.riscv_const import UC_RISCV_REG_SP, UC_RISCV_REG_RA, UC_RISCV_REG_A0, UC_RISCV_REG_GP, UC_RISCV_REG_PC

tool_dir = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio')) / 'packages/toolchain-riscv32-esp/bin'
suffix = '.exe' if os.name == 'nt' else ''
def tool(name):
    return str(tool_dir / ('riscv32-esp-elf-' + name + suffix))

elf = OUT / 'smart-timing.elf'
binary = OUT / 'smart-timing.bin'
subprocess.run([tool('g++'), '-std=c++20', '-O2', '-march=rv32im', '-mabi=ilp32',
                '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti', '-fno-builtin', '-nostdlib', '-DRSVP_FREESTANDING',
                '-I' + str(ROOT / 'lib/Rsvp'), '-I' + str(ROOT / 'lib/Utf8'),
                str(ROOT / 'test/rsvp/SmartTimingHarness.cpp'), '-o', str(elf),
                '-Wl,-Ttext=0x10000,-e,testMain,--no-relax', '-lgcc'], check=True)
subprocess.run([tool('objcopy'), '-O', 'binary', str(elf), str(binary)], check=True)
symbols = subprocess.check_output([tool('nm'), str(elf)], text=True)
addresses = {line.split()[2]: int(line.split()[0], 16) for line in symbols.splitlines() if len(line.split()) == 3}
emulator = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
emulator.mem_map(0x10000, 0x200000)
emulator.mem_write(0x10000, binary.read_bytes())
emulator.reg_write(UC_RISCV_REG_SP, 0x200000)
emulator.reg_write(UC_RISCV_REG_RA, 0x1F0000)
emulator.reg_write(UC_RISCV_REG_GP, addresses.get('__global_pointer$', 0))
emulator.emu_start(addresses['testMain'], 0x1F0000, timeout=30000000, count=10000000)
failure = emulator.reg_read(UC_RISCV_REG_A0)
assert emulator.reg_read(UC_RISCV_REG_PC) == 0x1F0000, 'Test did not return normally (execution limit reached)'
assert failure == 0, f'SmartTimingHarness.cpp failed at line {failure}'
print('PASS: production C++ timing/tokenizer/history/cadence on RV32; punctuation, UTF-8, structural metadata, truncation, speeds, overrun, pause/resume, chapters')
