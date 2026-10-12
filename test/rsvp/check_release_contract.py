"""V1C.3 integration checks complement the executable and bitmap regressions."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'rsvp-release.local/v1c3'
before = json.loads((OUT / 'before.json').read_text())
allowed = {
    'lib/Rsvp/RsvpCadence.h', 'lib/I18n/translations/english.yaml',
    'src/CrossPointSettings.cpp', 'src/CrossPointSettings.h', 'src/SettingsList.h',
    'src/activities/reader/RsvpReaderActivity.cpp', 'src/activities/settings/SettingsActivity.cpp',
    'test/rsvp/ActivityContextHarness.cpp', 'test/rsvp/run_context_tests.py',
    'test/rsvp/render_context_fixture.py', 'test/rsvp/CMakeLists.txt',
}
for path, expected in before['source_sha256'].items():
    if path not in allowed:
        assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
assert subprocess.check_output(['git', '-C', 'freeink-sdk', 'diff', '--binary'], cwd=ROOT) == (OUT / 'sdk-before.patch').read_bytes()
assert hashlib.sha256((OUT / 'v1c2-accepted.bin').read_bytes()).hexdigest() == before['rollback']['sha256']

def source(path): return (ROOT / path).read_text(encoding='utf-8')
def old(path): return subprocess.check_output(['git', 'show', before['head'] + ':' + path], cwd=ROOT).decode('utf-8')
def section(text, first, last): return text.split(first, 1)[1].split(last, 1)[0]

path = 'src/activities/reader/RsvpReaderActivity.cpp'
activity = source(path)
assert section(activity, 'bool RsvpReaderActivity::drawFrame', 'void RsvpReaderActivity::failSource') == section(
    old(path), 'bool RsvpReaderActivity::drawFrame', 'void RsvpReaderActivity::failSource')
render = section(activity, 'void RsvpReaderActivity::render(RenderLock&&)', 'void RsvpReaderActivity::onExit()')
assert render == section(old(path), 'void RsvpReaderActivity::render(RenderLock&&)', 'void RsvpReaderActivity::onExit()')
assert render.count('backend.submit()') == 1 and render.count('source.next(staged)') == 1
assert render.index('if (!completion.success)') < render.index('history.append(staged)')
assert 'if (presentation == Job::TimedNext && controls.load()) return;' in render
assert 'if (history.current()) job = Job::Current;' in activity
assert 'if (playing && token) cadence.completed(start, end, token->timing);' in render
assert 'cadence.suspend(esp_timer_get_time())' in render and 'cadence.resume(esp_timer_get_time())' in activity
loop = section(activity, 'void RsvpReaderActivity::loop()', 'bool RsvpReaderActivity::drawFrame')
assert 'saveToFile' not in loop and 'saveRsvpSpeed' not in loop
assert activity.count('saveRsvpSpeed') == 1
exit_body = activity.split('void RsvpReaderActivity::onExit()', 1)[1]
assert exit_body.index('source.close()') < exit_body.index('saveRsvpSpeed') < exit_body.index('saveProgress')
for button, event in [('Confirm','TOGGLE'), ('Left','PREVIOUS'), ('Right','NEXT'), ('Down','FASTER'), ('Up','SLOWER'), ('Back','EXIT')]:
    assert f'wasReleased(Button::{button})) incoming |= {event}' in loop
settings = source('src/CrossPointSettings.cpp')
assert 'doc["rsvpWpm"] = rsvpWpm;' in settings and 'rsvp::loadSettings(doc, rsvpWpm, rsvpSmartTiming);' in settings
assert 'rsvpSmartTiming' in source('src/SettingsList.h').split('// --- Reader ---')[1].split('// --- Controls ---')[0]
assert 'if (rsvpWpm == wpm) return true;' in settings
assert '/.crosspoint/settings.json' in source('src/CrossPointSettings.h')
assert 'rsvp' not in source('src/CrossPointState.h').lower()
assert 'rsvp' not in source('lib/Epub/Epub/ReaderRenderSpec.h').lower()
print('PASS: rollback and SDK patch/hash preservation; frozen HAL/parser/fonts/context/backend/ORP/input/progress; unchanged successful-display commits and render path; exit-only speed save; Reader toggle; separate global preferences')
