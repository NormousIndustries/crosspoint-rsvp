# RSVP V1A: real EPUB integration

Date: 11 October 2026. Hardware target: Xteink X3 / ESP32-C3.

The first physical test exposed an invalid close of a default RSVP spool handle.
See [the crash-fix report](rsvp-v1a-crash-debug.md) for the corrected ownership,
current firmware identity, and retest procedure. Build/firmware results below
describe the original pre-test artifact; its rollback patch predates this fix.

**Canonical position:** existing `CrossPointPosition.spineIndex` plus
`visibleTextOffset` (zero-based Unicode codepoints in the spine's body text).
Page numbers are derived from the normal reader's layout. Each RSVP token has
an inclusive start and exclusive end in this same coordinate system.

## A. Git and source state

- Branch: `feature/rsvp-v0.1-display-test-v1.6.5`.
- CrossPoint HEAD: `840371a4766d865d9fdef6f61b1b4e682fb3494a`.
- Known-good base supplied for this task: CrossPoint `93e98bb7`, version 1.6.5.
- SDK HEAD: `111fdcc7f0176c3ee38391a160ee296bf492dbd8`.
- Builds use the current HEADs plus the existing local experiments and these
  uncommitted V1A changes. They are not clean baseline builds.
- Host: Windows PowerShell; Git Bash `uname -s` returned
  `MINGW64_NT-10.0-26200`.
- Remotes: `origin` is NormousIndustries/crosspoint-rsvp; `upstream` is
  crosspoint-reader/crosspoint-reader.

Pre-existing work was retained: modified `docs/x3-window-refresh-prototype.md`,
`lib/I18n/translations/english.yaml`, `X3WindowTestActivity.cpp/.h`, untracked
`docs/x3-window-height-sweep.md`, and the dirty `freeink-sdk` submodule.
The SDK contains the user's display/bus/driver measurement and window changes,
host-test changes, and untracked `DisplayMeasurement.h`. V1A did not edit any
SDK, experimental activity, or window-experiment documentation file. Only the
new `STR_RSVP` key was added to the already-modified English translation.

No reset, stash, commit, push, PR, dependency upgrade, toolchain change, or
hardware flash was performed. `platformio.ini` and local overrides were retained.

## B. Files added and changed

New firmware files:

- `lib/Rsvp/RsvpToken.h`: source ranges and fixed history ring.
- `lib/Rsvp/RsvpTokenizer.h`: bounded visible-text tokenizer.
- `lib/Rsvp/RsvpCadence.h`: speed table and start-to-start scheduler.
- `lib/Rsvp/RsvpEpubSource.h/.cpp`: chapter parser adapter and SD spool.
- `lib/Rsvp/RsvpDisplayBackend.h/.cpp`: full-frame FAST submission/completion.
- `src/activities/reader/RsvpReaderActivity.h/.cpp`: controls, lifecycle, rendering.

Changes to existing files:

- `ChapterHtmlSlimParser.h/.cpp`: optional text-only sink, bypassing layout.
- `EpubReaderActivity.cpp`: RSVP launch/result handling and offset coverage/error checks.
- `EpubReaderMenuActivity.h/.cpp`: X3-only action, sufficient fixed row capacity.
- `lib/I18n/translations/english.yaml`: `STR_RSVP`.
- `test/CMakeLists.txt`: register RSVP host tests.
- `test/chapter_html_slim_parser/ChapterHtmlSlimParserTest.cpp`: sink regression cases.

New verification/handoff files:

- `test/rsvp/CMakeLists.txt`, `RsvpTest.cpp`, `make_fixture.py`.
- This document.

Generated I18n files, firmware, logs, the acceptance EPUB, and the rollback patch
remain ignored build artifacts.

## C. Existing EPUB/progress architecture reused

Relevant source references in the implementation checkout:

- `lib/KOReaderSync/CrossPointPosition.h:8`: canonical rich position type.
- `lib/Epub/Epub/ParsedText.h:55`: layout tokens retain visible-codepoint offsets;
  `WordStore` stores bounded chunks of word bytes for normal layout.
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:1590`: canonical counting occurs
  before layout visibility filtering. Whitespace counts; markup does not.
  Head/style/script/title/rp text is excluded using `VisibleTextUtils`.
- `lib/Epub/Epub/Section.h:153`: page-to-offset lookup; line 158: offset-to-page lookup.
- `src/activities/reader/EpubReaderActivity.cpp:2773`: current reader position.
- `src/activities/reader/EpubReaderActivity.cpp:211`: existing 4/6/10-byte resume formats.
- `src/activities/reader/EpubReaderActivity.cpp:1522`: ordinary render-time progress save.
- `src/activities/reader/EpubReaderUtils.h:15` and `ProgressFile.h:31`: existing
  atomic-progress-file workflow under the book cache directory.

Rendered `TextBlock`s no longer retain individual source offsets, and their
words may already be hyphenated or reordered. `Section::getTextFromSectionFile()`
returns a whole-section string. Neither is suitable as a bounded, source-accurate
RSVP feed. Instead, the existing chapter parser exposes decoded text with its
canonical offset through a function-pointer sink. No second EPUB/XML parser was added.

`BookProgress` remains a presentation helper for stored progress. RSVP does not
change bookmark formats, KOReader mapping, or the book metadata/spine database.

## D. RSVP position model

`RsvpToken` contains spine index, start/end visible offsets, byte length, and a
NUL-terminated UTF-8 text buffer. Offsets count Unicode codepoints, not UTF-8 bytes.
The normal reader's page-start offset seeds the adapter. A token whose range
contains that offset is retained; earlier tokens are discarded during spooling.

The committed history cursor always identifies a successfully presented token.
Parsing ahead, loading another chapter, or a failed refresh does not change the
return position. The last completed token's start is authoritative on exit.

## E. Tokenizer

Whitespace and block boundaries terminate words. Apostrophes, punctuation,
and hyphens remain attached: `don't`, `morning,`, `well-known` each emit once.
Inline tag boundaries do not break a word. Unicode whitespace, NBSP, narrow NBSP,
and zero-width word separators are handled; empty/whitespace tokens are omitted.
UTF-8 codepoints may span input chunks.

The existing parser handles entities and XML line-ending normalization. It also
filters HTML `hidden`, supported CSS `display:none`, and ruby pronunciation text
from the RSVP output while retaining their canonical offset contribution.
Existing supported CSS selectors and the reader's embedded-style preference apply.
An invalid CSS cache logs a fallback to available rules and inline visibility.

There is no linguistic timing, punctuation delay, ORP styling, or sentence model.
Tokens longer than 200 UTF-8 bytes display a codepoint-safe prefix as one token;
their full source range remains intact, and truncations are counted on exit.

## F. Memory and buffering

The normal reader releases its `Section` and toolbar BW snapshot before the RSVP
activity allocation. It retains its book and resume state on the activity stack.

The source spools one chapter to `<book-cache>/rsvp-v1a.tmp`, using a 1KB inline
write buffer. Each record is two aligned uint32 offsets, a uint16 byte length,
and text bytes. Reads validate lengths and monotonically increasing source ranges.
No packed structs or unaligned loads are used. All SD operations use `HalStorage`/
`HalFile`. Member handles close before reopening/deleting; temporary locals use RAII.

`<book-cache>/html/<spine>.html` is reused when present. Otherwise existing
`Epub::extractItemToFile()` extracts only that chapter into `rsvp-v1a.html.tmp`.
The parser runs four 1KB steps per render tick. A chapter is validated completely
before any of its spooled words are presented. Both temporary files are removed
on exit/error and overwritten on a later session if power loss left them behind.

Allocation justification:

- One fallible activity allocation contains all fixed playback buffers. Putting
  the history on a task stack would exceed the stack budget; static storage would
  pin it outside RSVP. `makeUniqueNoThrow` is checked and logged.
- One fallible existing parser allocation per chapter avoids a large local object.
  Text-only mode allocates no `ParsedText`, `WordStore`, page, image, or layout LUT.
- Existing Expat, ZIP extraction, CSS hydration, and `HalFile` allocation behavior
  is reused. CSS rule pools are released after each spool completes or aborts.
- Cache-path strings are constructed at chapter load, outside word playback.
  There are no new per-word strings, vectors, heap allocations, or FreeRTOS tasks.

The fixed history contains 32 tokens, supporting 31 backward steps from the newest
token and forward replay without seeking or reparsing. There is no second framebuffer.
The X3 framebuffer remains the existing 52,272-byte buffer (792 × 528 ÷ 8).

## G. Chapters

At spool EOF, a next spine item is loaded lazily. Playback resumes if it was
playing; a manual step stays paused. Empty/whitespace/image-only items advance
through the same bounded loading flow. No repeated reload of the initial empty
chapter occurs. The ring remains intact across transitions, so backward stepping
can cross a recent boundary even though the previous chapter spool was replaced.

Malformed XML, extraction failure, short writes/reads, invalid records, CSS OOM,
and parser OOM pause at the last completed token and show the existing error label.
End of book pauses with the existing end label. Stepping backward then playing
can replay the remaining history; arbitrary whole-book backward seeking is absent.

## H. Playback and timing

Explicit states: Paused, Playing, Displaying, Loading, Exiting.
Jobs distinguish current-word/chrome redraw, manual next/previous, and timed next.
Only the existing ActivityManager render task submits frames. It holds RenderLock
through synchronous completion. Main-loop input is captured atomically and a
nonblocking lock attempt keeps polling responsive while the render task is busy.

Confirm toggles play/pause. Left/Right pause and step. Up/Down move within
50 / 75 / 100 / 125 WPM, with 100 default after V1B.1 finalisation and clamped endpoints. Back exits.
Speed changes while playing preserve playback and rebase the deadline; paused
changes redraw the status. The constexpr speed table can be extended later.

`esp_timer_get_time()` supplies monotonic microseconds. Requested intervals are
1,200,000 / 800,000 / 600,000 / 480,000 us. After one completed presentation the
next deadline is its actual submission start plus the interval. If service
overruns, exactly one next token becomes due. There are no skipped slots or
catch-up bursts, and each over-budget presentation records a deadline miss.

SUMMARY is emitted on pause, speed change, end, or exit, never per word. It reports
words presented in that play segment, first-to-last submission-start elapsed time,
requested WPM, measured start-to-start WPM, deadline misses, cumulative chapter
changes, summed display service time, heap, and minimum sampled heap/largest block.
One-frame summaries have no cadence sample and report effective WPM 0.
Manual steps/chrome redraws are excluded. Chapter loading delays count in a playing
segment's delivered cadence. ENTER/EXIT also include the boot minimum heap.

## I. Display backend

`RsvpDisplayBackend::submit()` calls only
`HalDisplay::displayBufferMeasuredFast()` and reports completion/service duration.
The existing measurement capture suppresses normal per-refresh SDK wait printing.
Timeout, missing BUSY assertion, or no recorded refresh fails the presentation;
the token is not committed and playback pauses. No automatic retry loop runs.

There is no call to experimental window refresh, partial windows, LUT editing,
PLL/voltage changes, or altered DSP sequencing. The existing SDK may perform its
own normal baseline cleanup on entry/inversion change; timed playback requests
the established full-frame FAST path. Replacing this one backend with a qualified
blocking implementation does not require changes to EPUB/token/layout/timing code.
An asynchronous replacement must retain the completion/ownership contract.

Rendering uses existing UITheme metrics, safe area, oriented bezel margins, and
`uiScaleSpec()` fonts. The baseline is shared by paused/playing frames. Playing
frames contain the word; paused frames add speed/status and translated hints.
Night-mode polarity, render mode, and renderer orientation are restored on exit.

## J. Menu entry

`EpubReaderMenuActivity::buildMenuItems()` adds RSVP after Select Chapter only when
`gpio.deviceIsX3()` is true. Its row capacity is 18 to cover optional items.
The classic list and toolbar More panel both consume this same action list.
`EpubReaderActivity::onReaderMenuConfirm()` handles the RSVP action.
TXT/XTC menus are unchanged. Entry starts paused in landscape counterclockwise;
`SETTINGS.orientation` is never changed or persisted by RSVP.

## K. Return and persistence

Back waits for a committed refresh to finish, returns an existing
`ProgressChangeResult`, and restores the previous display state. The parent sets
`pendingOffsetJump`; normal pagination maps it with
`Section::getPageForVisibleTextOffset()`. The page containing the word is sufficient.
The reader logs the resolved chapter/page and performs its existing progress save,
including a same-page return. Its automatic page turning is disabled for this handoff.

A partial cache is used directly only if its last known page start covers the
requested offset. Otherwise the existing incremental builder extends to the
content target. Mapping failure invalidates the section and retries up to the
existing limit, then shows the normal indexing error without saving a false page-0 result.

RSVP teardown also saves the exact canonical offset using `EpubReaderUtils` in
the same `progress.bin`. Page/count hints are zero until normal pagination derives
them. This protects sleep/global navigation, where child-result callbacks are
not delivered. No new bookmark/progress database or per-word SD progress writes exist.

## L. Power policy

`preventAutoSleep()` is true during Playing, Displaying, and Loading. Paused
activity follows the existing inactivity policy. No global sleep setting changes.
Activity teardown waits on the existing render mutex, closes resources, saves
the completed position, and restores orientation/mode/polarity. Explicit user
power/navigation actions still use the existing ActivityManager lifecycle.

## M. Validation

- Final default build: SUCCESS, 38.814 s, unchanged installed PlatformIO setup.
- Final configured `platformio check -e default`: PASSED, no defects, 45.883 s.
- `git diff --check`: passed; Git emitted existing LF/CRLF notices for experiment files.
- No compiler warning/error diagnostics in the final firmware build.
- Static review covered display ownership, atomic control capture, token commit,
  bounded buffers, chapter/empty-item advancement, mapping, cleanup, and SD mutex use.
- New host regression sources cover token boundaries/Unicode offsets/truncation,
  sink failures, chapter-history eviction/replay, cadence overruns, and parser
  canonical offsets without layout. **These host tests were not executed:** no
  usable host C++ compiler was available in this Windows session. The existing
  CMake/gtest suite requires a separately prepared host test environment.
- `./bin/clang-format-fix -g` was invoked through Git Bash but found no supported
  formatter. Changed code was formatted manually; automated formatting remains unchecked.
- Fixture generator ran; ZIP integrity, XHTML/package XML, numbered word sequence,
  and spine ordering were verified with Python's standard library.
- No hardware tests or physical cadence/heap stability claims are made here.

Reproduce the required checks from PowerShell:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython -m platformio run -e default
& $pioPython -m platformio check -e default
```

Logs: `.pio/rsvp-v1a-build.txt`, `.pio/rsvp-v1a-cppcheck.txt`.

## N. Firmware

- Firmware: `C:\ai\Crosspoint-RSVP\crosspoint-rsvp\.pio\build\default\firmware.bin`.
- Factory image: same directory, `firmware.factory.bin`.
- ELF/map: same directory, `firmware.elf` / `firmware.map`.
- Firmware binary length: 5,667,264 bytes.
- Firmware SHA-256: `68a426c46c35a074efcb72c3a1a753be90f08981d71264eb4ff0ebc2a78a6820`.
- PlatformIO static RAM report: 57,936 bytes; flash report: 5,653,181 / 6,553,600 bytes.
- Linker DRAM table also includes 67,570 bytes of instruction code in shared SRAM;
  the simple PlatformIO RAM percentage is not a runtime heap guarantee.

## O. Memory impact

Final ESP32-C3 DWARF type sizes from the built ELF:

| Type | Bytes | Allocation/lifetime |
| --- | ---: | --- |
| RsvpReaderActivity | 8,952 | One checked activity allocation |
| RsvpHistory | 6,924 | Included in activity; 32 × 216-byte tokens plus cursors |
| RsvpEpubSource | 1,392 | Included in activity; includes 1KB spool buffer |
| RsvpTokenizer | 244 | Included in source |
| RsvpToken | 216 | Included staged/current/history storage |
| RsvpDisplayBackend | 72 | Included in activity |
| RsvpCadence | 48 | Included in activity |
| ChapterHtmlSlimParser | 1,080 | Checked temporary chapter allocation |

Do not add the nested rows again. Playback's fixed activity is about 8.7 KiB;
loading additionally needs the parser, Expat working memory, existing bounded CSS
rule pools, file handles, path strings, and transient ZIP extraction buffers.
Those variable allocations depend on book/CSS/input shape and were not measured
on hardware. Existing pagination buffers are released before entry, and CSS/parser
buffers are released before word playback. No memory-saving benchmark is claimed.
ENTER/SUMMARY/EXIT heap and largest-block diagnostics are the physical verification.
Type-size capture: `.pio/rsvp-v1a-type-sizes.txt`.

## P. Known limitations

- Full-frame FAST still limits delivered cadence; 125 WPM is requested, not guaranteed.
- Entire chapter spooling/validation causes an entry/boundary wait. Parsing yields
  between 4KB ticks, but ZIP extraction is an existing blocking operation on the
  render task; controls queue while it runs. Back waits for that operation to finish.
- Repeated identical releases during one outstanding refresh/load coalesce into
  one pending control. Ordinary single steps are deterministic.
- Backward navigation is limited to retained history. No sentences or whole-book seek.
- Long tokens display a 200-byte prefix; wide words may clip. Whitespace-delimited
  tokenization does not provide linguistic CJK segmentation. Ruby readings are omitted.
- CSS visibility has the existing parser's supported-selector limitations.
- Errors pause safely; exit/re-enter to retry a failed source/display session.
- The normal reader's auto-page-turn session is stopped on RSVP entry.
- Exact resume-word highlighting is absent; normal return selects a containing page.
- A failed SD progress write logs an error; normal-reader return retries the usual save.
- Physical acceptance, the new host regressions, and automated formatting remain pending.

## Q. Exact first physical-test procedure

The owner must perform any firmware installation after reviewing this handoff.
The implementation session did not flash hardware.

1. Back up the SD book cache/progress and the known-good firmware. Install the
   reviewed default `firmware.bin` through your established firmware update flow.
2. Generate the deterministic EPUB if needed:
   `& $pioPython test/rsvp/make_fixture.py`. Copy
   `.pio/rsvp-v1a-acceptance.epub` to the SD card, then open it **normally** in CrossPoint.
3. Start serial capture with `& $pioPython -m platformio device monitor` at the
   configured 115200 baud. Record normal-reader free heap/largest block and orientation.
4. Navigate normally to a page around `T0100`. Open the existing menu (or toolbar
   More), select RSVP. Verify landscape, paused state, 100 WPM, and a first word
   near that page's starting word. Confirm the ENTER chapter/offset log.
5. While paused, press Right five times and Left five times, waiting for each
   refresh. Verify one step per press and forward replay of the same words.
6. Press bottom-edge Slower through 75, 50; check it clamps at 50. Press top-edge Faster through 75,
   100, 125; check it clamps at 125. Test Confirm at each setting; selected and
   measured rates may differ. Pause to obtain SUMMARY, not per-word logs.
7. At 125, run at least 300 consecutive numbered words. Verify consecutive labels
   without omissions or uncontrolled repetitions. The fixture also intentionally
   presents `don't`, `morning,`, `well-known`, `café`, and chapter headings.
8. Continue across `T0400`, the four punctuation/Unicode words, the empty spine
   item, `Chapter-two`, and `T0401`. It must resume automatically without an empty
   token or chapter-start loop. At `T0401`, pause, press Left twice: this should
   reach `Chapter-two` then the first chapter's `café`. Step Right twice to replay.
9. Play into chapter two for another 200+ words. Pause/step at a known label,
   press Back, and confirm original orientation/polarity. Verify the normal page
   contains the reached label and the RETURN/content-offset-to-page log is coherent.
   Close/reopen the book and verify resume. Inspect the book cache `progress.bin`
   if needed: bytes 0–1 spine, 2–3 page, 4–5 page count, 6–9 visible offset, little endian.
10. Repeat entry/exit at least ten times. Compare heap/largest block after returning
    to the same normal page/settings; font cache warming may change initial values,
    but there must be no monotonic loss attributable to RSVP. Check minimum sampled
    heap/block in SUMMARY and boot minimum in EXIT. Any reboot/crash blocks acceptance.
11. With an existing short auto-sleep timeout, play beyond it: no automatic sleep.
    Pause long enough for normal sleep, then wake and reopen: reached progress should
    survive. Restore the owner's timeout afterward. Repeat Back during a refresh.
12. Test a real personal EPUB from a deep page, across a real spine boundary, in
    all saved orientations and with night mode. Test both menu styles, bookmarks,
    normal page turns, TXT/XTC, and the existing experimental activities separately.
    Confirm V1A logs never show an experimental window submission.
13. Test end of the fixture, backward replay from end, an empty chapter, and a
    deliberately malformed copy on a spare/test SD card. Errors must pause/exit
    without losing the last completed canonical position.

Retain the firmware hash, serial log, book identity, chosen speeds, heap observations,
and mapped page labels with the physical-test result.

## R. Rollback

An ignored patch `.pio/rsvp-v1a-changes.patch` contains only this task's edits,
including new files and the single `STR_RSVP` English key. It deliberately excludes
SDK/window experiments and earlier English changes. Copy it outside `.pio` before
cleaning build artifacts. Review it first, then from the repository root:

```powershell
git apply --reverse --check .pio/rsvp-v1a-changes.patch
git apply --reverse .pio/rsvp-v1a-changes.patch
```

The check was verified against this handoff's working tree. Stop and review if it
fails after further edits. Do not reset/stash the tree or restore the whole shared
English YAML. Rebuild with the same environment. For device rollback, the owner
may reinstall the preserved known-good `build/baseline/firmware.bin` after checking
`build/baseline/README.md`/artifact identity. V1A introduces no permanent cache-format
change; optional leftover `rsvp-v1a.tmp` / `rsvp-v1a.html.tmp` are disposable.

## S. V1B recommendation

Run and record the physical V1A procedure first. If accepted, keep the source,
canonical position, history, scheduler, and full-frame backend contract. Implement
positional ORP layout and the two fixed registration markers as an isolated layout
change with existing safe fonts initially. Do not style a focal character or add a
vertical line. Giant fonts and any faster backend require their own memory/hardware
qualification. No V1B work was started.

Initial pre-test handoff superseded by the [V1A crash retest](rsvp-v1a-crash-debug.md).
