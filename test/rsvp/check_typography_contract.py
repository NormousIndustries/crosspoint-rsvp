"""Freeze accepted V1.0 sources outside the typography tier/counter changes."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'rsvp-release.local/v1.0.1'
before = json.loads((OUT / 'before.json').read_text())
allowed = {
    'lib/Rsvp/RsvpTypography.cpp', 'lib/Rsvp/RsvpTypography.h', 'lib/Rsvp/fonts/metrics.json',
    'scripts/generate_rsvp_fonts.py', 'src/activities/reader/RsvpReaderActivity.h',
    'src/activities/reader/RsvpReaderActivity.cpp', 'test/rsvp/ActivityContextHarness.cpp',
    'test/rsvp/render_visual_fixture.py', 'test/rsvp/render_physical_controls.py',
    'test/rsvp/run_context_tests.py', 'test/rsvp/CMakeLists.txt',
}
for path, expected in before['sources'].items():
    if path not in allowed:
        assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
assert subprocess.check_output(['git', '-C', 'freeink-sdk', 'diff', '--binary'], cwd=ROOT) == (OUT / 'sdk-before.patch').read_bytes()
assert hashlib.sha256((OUT / 'v1.0-accepted.bin').read_bytes()).hexdigest() == before['rollback_sha256']

def old(path):
    return subprocess.check_output(['git', 'show', before['head'] + ':' + path], cwd=ROOT).decode('utf-8')

path = 'src/activities/reader/RsvpReaderActivity.cpp'
activity = (ROOT / path).read_text()
assert activity.split('void RsvpReaderActivity::onExit()', 1)[0] == old(path).split('void RsvpReaderActivity::onExit()', 1)[0]
exit_body = activity.split('void RsvpReaderActivity::onExit()', 1)[1]
old_exit = old(path).split('void RsvpReaderActivity::onExit()', 1)[1]
assert exit_body.split('  // Also protects', 1)[1] == old_exit.split('  // Also protects', 1)[1]
path = 'lib/Rsvp/RsvpTypography.cpp'
new = (ROOT / path).read_text()
baseline = old(path)
assert new.split('Orp findOrp', 1)[1].split('EpdFont font', 1)[0] == baseline.split('Orp findOrp', 1)[1].split('EpdFont font', 1)[0]
assert re.sub(r'\s+', '', new.split('WordMetrics walk', 1)[1].split('Placement place', 1)[0]) == re.sub(
    r'\s+', '', baseline.split('WordMetrics walk', 1)[1].split('Placement place', 1)[0])
assert new.split('  for (const auto size : SIZES)', 1)[1] == baseline.split('  for (const auto size : SIZES)', 1)[1]
print('PASS: accepted V1.0 rollback, all SDK/display/waveform/HAL/EPUB/preferences/timing/context/input sources frozen; identical ORP/walk/fit arithmetic, drawFrame/playback/commit path; only baked tiers and counters change')
