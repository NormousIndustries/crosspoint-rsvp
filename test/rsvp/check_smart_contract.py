"""Source integration contracts complement execution of the production timing C++.

These checks protect the accepted renderer/input path and assert where the
activity submits and commits words. They do not simulate real button presses.
"""
from pathlib import Path
import hashlib
import subprocess

ROOT = Path(__file__).resolve().parents[2]
def source(path):
    return (ROOT / path).read_text(encoding='utf-8')

frozen = [
    'lib/Rsvp/RsvpTypography.cpp', 'lib/Rsvp/RsvpTypography.h',
    'lib/Rsvp/RsvpDisplayBackend.cpp', 'lib/Rsvp/RsvpDisplayBackend.h',
    'src/components/RsvpReaderUi.cpp', 'src/components/RsvpReaderUi.h',
    'src/MappedInputManager.cpp', 'src/MappedInputManager.h',
    'src/CrossPointSettings.cpp', 'src/CrossPointSettings.h', 'platformio.ini', 'partitions.csv',
]
frozen += [str(p.relative_to(ROOT)).replace('\\', '/') for p in (ROOT / 'lib/Rsvp/fonts').iterdir() if p.is_file()]
frozen += subprocess.check_output(['git', 'ls-files', 'lib/hal'], cwd=ROOT, text=True).splitlines()
for path in frozen:
    before = subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=ROOT)
    after = (ROOT / path).read_bytes()
    assert before.replace(b'\r\n', b'\n') == after.replace(b'\r\n', b'\n'), path

sdk_patch = subprocess.check_output(['git', '-C', 'freeink-sdk', 'diff', '--binary'], cwd=ROOT)
assert sdk_patch == (ROOT / 'build/rsvp-v1c1/sdk-before.patch').read_bytes(), 'SDK edits changed'
assert (ROOT / 'freeink-sdk/libs/display/FreeInkDisplay/include/DisplayMeasurement.h').read_bytes() == (
    ROOT / 'build/rsvp-v1c1/DisplayMeasurement.h').read_bytes()
rollback = ROOT / 'build/rsvp-v1c1/v1b1-accepted.bin'
assert hashlib.sha256(rollback.read_bytes()).hexdigest() == '8417d233a68cb60252e3e2449f9db6a9ae2003226c0da74376c3b71572d658fd'

activity = source('src/activities/reader/RsvpReaderActivity.cpp')
render = activity.split('void RsvpReaderActivity::render(RenderLock&&)', 1)[1].split('void RsvpReaderActivity::onExit()', 1)[0]
assert render.count('backend.submit()') == 1
assert render.count('source.next(staged)') == 1
assert 'if (presentation == Job::TimedNext && controls.load()) return;' in render
assert render.index('if (!completion.success)') < render.index('history.append(staged)')
assert render.index('history.append(staged)') < render.index('cadence.completed(start, end, token->timing)')
assert 'if (playing && token) cadence.completed(start, end, token->timing);' in render
assert 'if (history.current()) job = Job::Current;' in activity
assert 'token->startOffset' in activity and 'renderer.setOrientation(previousOrientation)' in activity
assert 'cadence.suspend(esp_timer_get_time())' in render
assert 'cadence.resume(esp_timer_get_time())' in activity

parser = source('lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp')
assert parser.count('nullptr, textSinkBoundaryKind(name), self->visibleTextOffset') == 2
assert 'nullptr, 2, visibleTextOffset' in parser
assert 'self->skipUntilDepth > self->depth && self->insideBody && self->nonVisibleTextDepth == 0' in parser
assert 'self->skipUntilDepth >= self->depth && self->insideBody && self->nonVisibleTextDepth == 0' in parser
adapter = source('lib/Rsvp/RsvpEpubSource.cpp')
assert 'size ? self.tokenizer.boundary(size == 1, size == 2) : self.tokenizer.wordBoundary()' in adapter
assert adapter.count('self.writeBytes(&token.timing') == 1
assert adapter.count('tokenFile.read(&token.timing') == 1
assert 'tokenFile = HalFile{};' in adapter
print('PASS: frozen SDK/HAL/visuals/input/settings; rollback hash; single-submit word path, successful commit, queued controls, resume Current, chapter suspension, structural sink and spool integration')
