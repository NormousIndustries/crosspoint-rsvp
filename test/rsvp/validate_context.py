"""Run the requested host checks without changing the pinned firmware toolchain."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rsvp-v1c2'
ENV = dict(os.environ, PYTHONUTF8='1', PYTHONIOENCODING='utf-8')
def run(name, args):
    with (OUT / (name + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run([sys.executable, *args], cwd=ROOT, env=ENV, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((OUT / (name + '.log')).read_text(encoding='utf-8')[-5000:])
        raise RuntimeError(name + ' failed')
    print('PASS: ' + name, flush=True)

def main():
    if '--cppcheck' in sys.argv:
        run('cppcheck', ['-m','platformio','check','-e','default'])
        return
    run('context-tests', ['test/rsvp/run_context_tests.py'])
    run('contracts', ['test/rsvp/check_context_contract.py'])
    run('context-raster', ['test/rsvp/render_context_fixture.py'])
    for name, script in [('visual-raster','render_visual_fixture.py'), ('physical-raster','render_physical_controls.py')]:
        run(name, ['-c', 'import sys,runpy;sys.path.insert(0,"build/rsvp-v1c2/python-deps");runpy.run_path("test/rsvp/'+script+'",run_name="__main__")'])
    # Normalize a PowerShell 5 redirected build/formatter log to UTF-8.
    for name in ('build', 'formatter'):
        path = OUT / (name + '.log')
        data = path.read_bytes()
        if data.startswith(b'\xff\xfe'):
            path.write_text(data.decode('utf-16'), encoding='utf-8')
    print('PASS: V1C.2 executable and raster validation suite')

if __name__ == '__main__': main()
