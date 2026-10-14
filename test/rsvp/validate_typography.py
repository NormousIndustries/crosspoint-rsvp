"""Run V1.0.1 typography checks using existing pinned local tools."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'rsvp-release.local/v1.0.1'
ENV = dict(os.environ, PYTHONUTF8='1', PYTHONIOENCODING='utf-8', RSVP_TEST_OUTPUT=str(OUT))
ENV['PYTHONPATH'] = str(ROOT / 'build/rsvp-v1c2/python-deps')

def run(name, args):
    with (OUT / (name + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(args, cwd=ROOT, env=ENV, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((OUT / (name + '.log')).read_text(encoding='utf-8')[-6000:])
        raise RuntimeError(name + ' failed')
    print('PASS: ' + name, flush=True)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if '--format' in sys.argv:
        ENV['PATH'] = str(ROOT / 'build/rsvp-v1c2/format-deps/clang_format/data/bin') + os.pathsep + ENV['PATH']
        script = OUT / 'format.sh'
        script.write_text('''#!/usr/bin/env bash
set -e
function git() {
  command git "$@"
  if [[ "$1" == "ls-files" ]]; then
    command git ls-files --others --exclude-standard | grep -v '^lib/Rsvp/fonts/'
  fi
}
export -f git
./bin/clang-format-fix -g
''')
        run('formatter', ['C:/Program Files/Git/bin/bash.exe', str(script)])
    if '--checks' in sys.argv:
        for name, script in [('executable', 'run_context_tests.py'), ('contracts', 'check_typography_contract.py'),
                             ('typography-raster', 'render_typography_refinement.py'),
                             ('context-raster', 'render_context_fixture.py'),
                             ('visual-raster', 'render_visual_fixture.py'),
                             ('physical-raster', 'render_physical_controls.py')]:
            run(name, [sys.executable, 'test/rsvp/' + script])
        run('diff-check', ['git', 'diff', '--check'])
    if '--build' in sys.argv:
        run('build', [sys.executable, '-m', 'platformio', 'run', '-e', 'default'])
    if '--cppcheck' in sys.argv:
        run('cppcheck', [sys.executable, '-m', 'platformio', 'check', '-e', 'default'])

if __name__ == '__main__':
    main()
