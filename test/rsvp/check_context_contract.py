"""Integration and preservation checks for V1C.2, alongside executable RV32 tests."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rsvp-v1c2'
def source(path): return (ROOT / path).read_text(encoding='utf-8')
before = json.loads((OUT / 'before-sources.json').read_text())
prefixes = ('freeink-sdk/', 'lib/hal/', 'lib/Rsvp/fonts/', 'lib/Rsvp/RsvpTypography',
            'lib/Rsvp/RsvpDisplayBackend', 'lib/Rsvp/RsvpCadence', 'lib/Rsvp/RsvpTiming',
            'lib/Rsvp/RsvpTokenizer', 'lib/Epub/', 'lib/GfxRenderer/', 'lib/EpdFont/',
            'src/MappedInputManager', 'src/CrossPointSettings')
for path, expected in before.items():
    if path.startswith(prefixes) or path in ('platformio.ini', 'partitions.csv'):
        assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
assert subprocess.check_output(['git', '-C', 'freeink-sdk', 'diff', '--binary'], cwd=ROOT) == (OUT / 'sdk-before.patch').read_bytes()
assert hashlib.sha256((OUT / 'v1c1-accepted.bin').read_bytes()).hexdigest() == 'cc586ab953805efd37ebc33d38082a6df7049a4609b29a9b409c9e2b822f5c15'
assert hashlib.sha256((ROOT / 'build/rsvp-v1c1/v1b1-accepted.bin').read_bytes()).hexdigest() == '8417d233a68cb60252e3e2449f9db6a9ae2003226c0da74376c3b71572d658fd'

activity = source('src/activities/reader/RsvpReaderActivity.cpp')
render = activity.split('void RsvpReaderActivity::render(RenderLock&&)', 1)[1].split('void RsvpReaderActivity::onExit()', 1)[0]
assert render.count('backend.submit()') == 1 and render.count('source.next(staged)') == 1
assert 'if (presentation == Job::TimedNext && controls.load()) return;' in render
assert render.index('if (!completion.success)') < render.index('history.append(staged)')
assert render.index('history.append(staged)') < render.index('cadence.completed(start, end, token->timing)')
assert 'if (playing && token) cadence.completed(start, end, token->timing);' in render
assert 'if (history.current()) job = Job::Current;' in activity
assert 'cadence.suspend(esp_timer_get_time())' in render
assert 'cadence.resume(esp_timer_get_time())' in activity
draw = activity.split('bool RsvpReaderActivity::drawFrame', 1)[1].split('void RsvpReaderActivity::failSource', 1)[0]
assert 'if (token && !paused)' in draw and 'if (!oversized) UITheme::getInstance().drawRsvpWord' in draw
assert draw.index('if (!paused && !oversized) return false;') < draw.index('source.context(*token, context)')
assert draw.count('drawRsvpContext') == 1
for button, event in [('Confirm','TOGGLE'),('Left','PREVIOUS'),('Right','NEXT'),('Down','FASTER'),('Up','SLOWER'),('Back','EXIT')]:
    assert f'wasReleased(Button::{button})) incoming |= {event}' in activity

ui = source('src/components/RsvpReaderUi.cpp')
old_ui = subprocess.check_output(['git', 'show', 'HEAD:src/components/RsvpReaderUi.cpp'], cwd=ROOT).decode()
word_signature = 'void UITheme::drawRsvpWord'
paused_signature = 'void UITheme::drawRsvpPaused'
def compact(text): return ''.join(text.split())
assert compact(ui.split(word_signature)[1].split(paused_signature)[0]) == compact(old_ui.split(word_signature)[1].split(paused_signature)[0])
assert compact(ui.split(paused_signature)[1].replace('tr(STR_RSVP_CONTEXT_STATUS)', 'tr(STR_RSVP_TEST_PLAY_STATUS)')) == compact(old_ui.split(paused_signature)[1])
assert 'renderer.getScreenWidth()' in ui and 'renderer.getOrientedViewableTRBL' in ui
assert 'getLineHeight(CONTEXT_FONT) + 8' in ui and 'layout.safe.width * 2 / 3 - 24' in ui
assert 'renderer.setClipRect(layout.context.x' in ui
adapter = source('lib/Rsvp/RsvpEpubSource.cpp')
context_function = adapter.split('bool RsvpEpubSource::context(')[1].split('void RsvpEpubSource::releaseTokenFile')[0]
assert 'tokenFile' not in context_function and 'contextFile' in context_function
assert 'rsvp::extractContext' in context_function and 'contextScratch' in context_function
assert 'if (!tokenFile.seek(playbackStart == UINT32_MAX ? writtenBytes : playbackStart))' in adapter
assert 'token.endOffset > self.startOffset' in adapter
assert 'self.writeBytes(&self.previousRecord' in adapter
assert 'tokenFile = HalFile{};' in adapter and 'contextFile = HalFile{};' in adapter
assert adapter.index('contextFile = HalFile{};', adapter.index('void RsvpEpubSource::close')) < adapter.index('Storage.remove(tokenPath.c_str())')
assert not any(fragment in adapter for fragment in ('FsFile', 'SdMan', 'SDCardManager', 'reinterpret_cast'))
print('PASS: SDK/HAL/parser/fonts/timing/input/settings/backend preserved; both rollback hashes; successful-display cursor commit; Current resume; no context on playing path; separate HalFile cursor; direct initial spool seek; unchanged physical hints/word markers; clipped orientation-aware context')
