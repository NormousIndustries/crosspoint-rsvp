"""Execute production context/tokenizer/font code with the pinned RV32 compiler.

Uses the established Unicorn fallback when native CMake/GTest is unavailable.
Exports layouts measured by EpdFont for the separate bitmap raster verifier.
"""
from pathlib import Path
import json
import os
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / os.environ.get('RSVP_TEST_OUTPUT', 'build/rsvp-v1c2')
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT / 'build/rsvp-v1c2/python-deps'))
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32
from unicorn.riscv_const import UC_RISCV_REG_SP, UC_RISCV_REG_RA, UC_RISCV_REG_A0, UC_RISCV_REG_GP, UC_RISCV_REG_PC, UC_RISCV_REG_MSTATUS

tool_dir = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio')) / 'packages/toolchain-riscv32-esp/bin'
suffix = '.exe' if os.name == 'nt' else ''
def tool(name):
    return str(tool_dir / ('riscv32-esp-elf-' + name + suffix))

def compile_run(name, sources, includes, extra=()):
    elf, binary = OUT / (name + '.elf'), OUT / (name + '.bin')
    subprocess.run([tool('g++'), '-std=c++20', '-O2', '-march=rv32im', '-mabi=ilp32',
                    '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
                    '-fno-exceptions', '-fno-rtti', '-fno-builtin', '-nostdlib', '-DRSVP_FREESTANDING',
                    '-ffunction-sections', '-fdata-sections',
                    *['-I' + str(ROOT / p) for p in includes], *extra,
                    *[str(ROOT / p) for p in sources], '-o', str(elf),
                    '-Wl,-Ttext=0x10000,-e,testMain,--no-relax,--gc-sections', '-lgcc'], check=True)
    subprocess.run([tool('objcopy'), '-O', 'binary', str(elf), str(binary)], check=True)
    symbols = subprocess.check_output([tool('nm'), str(elf)], text=True)
    addresses = {line.split()[2]: int(line.split()[0], 16) for line in symbols.splitlines() if len(line.split()) == 3}
    emulator = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
    emulator.mem_map(0x10000, 0x700000)
    emulator.mem_write(0x10000, binary.read_bytes())
    emulator.reg_write(UC_RISCV_REG_GP, addresses.get('__global_pointer$', 0))
    # The pinned soft-float libgcc accesses fflags while converting JSON numbers.
    emulator.reg_write(UC_RISCV_REG_MSTATUS, 0x6000)
    def call(function, arg=0):
        emulator.reg_write(UC_RISCV_REG_SP, 0x700000)
        emulator.reg_write(UC_RISCV_REG_RA, 0x6F0000)
        emulator.reg_write(UC_RISCV_REG_A0, arg)
        try:
            emulator.emu_start(addresses[function], 0x6F0000, timeout=30000000, count=20000000)
        except Exception:
            pc = emulator.reg_read(UC_RISCV_REG_PC)
            print(f'{name}: exception at PC {pc:#x}; ' + subprocess.check_output(
                [tool('addr2line'), '-f', '-e', str(elf), hex(pc)], text=True), flush=True)
            raise
        failure = emulator.reg_read(UC_RISCV_REG_A0)
        assert emulator.reg_read(UC_RISCV_REG_PC) == 0x6F0000, 'Execution limit reached'
        assert failure == 0, f'{name} failed at C++ line {failure}'
    call('testMain')
    return emulator, addresses, call

def main():
    emu, addresses, call = compile_run('context-tests',
        ['test/rsvp/ContextHarness.cpp', 'lib/EpdFont/EpdFont.cpp', 'lib/Utf8/Utf8.cpp'],
        ['lib/Rsvp', 'lib/EpdFont', 'lib/Utf8'])
    cases = []
    for index, name in enumerate(['short', 'repeated', 'dialogue', 'long', 'excerpt']):
        call('rasterCase', index)
        values = struct.unpack('<20I', emu.mem_read(addresses['rasterMeta'], 80))
        keys = ['count', 'selected', 'first', 'last', 'lines', 'leading', 'trailing', 'shortened',
                'selected_bytes', 'used_bytes', 'context_bytes', 'placement_bytes', 'token_bytes',
                'line_height', 'gap', 'width', 'leading_x', 'trailing_x', 'trailing_row']
        record = dict(zip(keys, values))
        record['name'] = name
        words = bytes(emu.mem_read(addresses['rasterWords'], 49 * 201))
        xy = struct.unpack('<98I', emu.mem_read(addresses['rasterXY'], 392))
        record['words'] = [words[i*201:(i+1)*201].split(b'\0')[0].decode('utf-8') for i in range(record['count'])]
        record['positions'] = [[xy[2*i], xy[2*i+1]] for i in range(record['count'])]
        cases.append(record)
    (OUT / 'context-layouts.json').write_text(json.dumps(cases, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    # Existing timing, source coordinates, history and cadence regressions.
    compile_run('smart-timing', ['test/rsvp/SmartTimingHarness.cpp'], ['lib/Rsvp', 'lib/Utf8'])
    compile_run('preferences', ['test/rsvp/PreferencesHarness.cpp'],
                ['lib/Rsvp', 'lib/Utf8', 'src', '.pio/libdeps/default/ArduinoJson/src'])
    activity_path = ROOT / 'src/activities/reader/RsvpReaderActivity'
    def without_includes(text):
        return '\n'.join(line for line in text.splitlines() if not line.startswith(('#include', '#pragma once')))
    (OUT / 'ActivityUnderTest.h').write_text(without_includes(activity_path.with_suffix('.h').read_text()).replace(' private:', ' public:'))
    (OUT / 'ActivityUnderTest.cpp').write_text(without_includes(activity_path.with_suffix('.cpp').read_text()))
    settings = (ROOT / 'src/CrossPointSettings.cpp').read_text()
    (OUT / 'SettingsSpeedUnderTest.cpp').write_text('bool CrossPointSettings::saveRsvpSpeed' +
        settings.split('bool CrossPointSettings::saveRsvpSpeed', 1)[1].split('void CrossPointSettings::toJson', 1)[0])
    _, _, activity_call = compile_run('activity-context', ['test/rsvp/ActivityContextHarness.cpp'], ['lib/Rsvp', 'lib/Utf8', str(OUT)],
                ['-Wno-unused-variable', '-Wno-unused-parameter', '-fno-threadsafe-statics', '-Wl,--undefined=releaseMain'])
    activity_call('releaseMain')
    print('PASS: actual RsvpReaderActivity.cpp on RV32 with deterministic HAL/UI substitutes; initial pause, idle, speeds, deadline pause, resume Current, sentence-end timing, chapter navigation, malformed fallback, queued controls, EOF replay, source return; zero extra playing context/hint frames')
    print('PASS: production context extraction/layout, tokenizer and EpdFont on RV32; normal/repeated/dialogue/contractions/decimals/abbreviations/ellipses/inline/entity/paragraph/chapter/EOF/malformed/UTF-8/cluster/bounds; existing timing/history/cadence regressions')
    print('PASS: real ArduinoJson preference defaults, four speeds x On/Off round trips, corrupt/missing values, separate positions; enabled/disabled deadlines and rebasing at all speeds')

if __name__ == '__main__':
    main()
