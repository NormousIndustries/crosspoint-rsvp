# RSVP V1C.1 Smart Timing

Work date: 11 October 2026, Australia/Sydney. Target: XTEINK X3, COM5.
Scope ends at V1C.1. Physical reading acceptance remains a human test.

**V1C.1 BUILT AND FLASHED — READY FOR PHYSICAL TEST**

## Accepted V1B.1 baseline

The user reports all seven final physical acceptance tests passed. Initial branch
was `feature/rsvp-v0.1-display-test-v1.6.5`, HEAD
`2abf45771b9e5635e3d834dffbd6fe833d3bc45b` (`V1B`). The parent reader tree was
clean; `freeink-sdk` was dirty at SDK revision
`111fdcc7f0176c3ee38391a160ee296bf492dbd8`.
Origin is NormousIndustries/crosspoint-rsvp; upstream is
crosspoint-reader/crosspoint-reader. No commit, tag, push or history rewrite was
performed. Existing tag `rsvp-baseline-1.6.5` was retained.

**Baseline tag blocked:** production display measurement code and archived window
experiments remain interleaved in the uncommitted SDK changes. A parent-only
`rsvp-v1b1-accepted` tag would omit source used by the accepted firmware. See
[archived driver explanation](research/x3-display/README.md#preserved-driver-code).
The existing source commit, SDK revision, complete SDK binary patch and untracked
`DisplayMeasurement.h` copy preserve that state without misrepresenting it as a
clean tagged release. Unrelated work was neither staged nor discarded.

The accepted image matched the finalisation report before any build:

- Rollback: `build/rsvp-v1c1/v1b1-accepted.bin`, **6,187,632 bytes**.
- SHA-256: `8417d233a68cb60252e3e2449f9db6a9ae2003226c0da74376c3b71572d658fd`.
- Matching ELF: `build/rsvp-v1c1/v1b1-accepted.elf`.
- SDK preservation: `build/rsvp-v1c1/sdk-before.patch`, `DisplayMeasurement.h`.

These ignored local artifacts are outside PlatformIO's normal `.pio/build`
output, so later firmware builds do not overwrite the rollback. Keep a separate
backup of this folder before deleting ignored build artifacts.

Pinned baseline `default` rebuild: SUCCESS, 125.30 seconds, static RAM 57,936
bytes, linked flash 6,173,551 bytes. Rebuilt SHA-256:
`f80b5739678b1cee912b0dc874dd997d50215fe22ae053d32f6c0dca07423408`.
The source rebuild succeeds; it is not a byte-identical reproduction of the
accepted image. The build embeds the current branch/commit, whereas the accepted
image predates the `V1B` commit (the previous report records `840371a4`). The
rollback remains the actual accepted binary. No dependency, platform or SDK
revision upgrade was made. Platform pin: pioarduino `55.03.311`; existing ESP
compiler: GCC 14.2.0. Host detection: PowerShell on Windows; Git's `uname -s`
returned `MSYS_NT-10.0-26200`.

## Files changed

- `lib/Rsvp/RsvpTiming.h`: central constants, lexical scanner, abbreviation rules.
- `lib/Rsvp/RsvpToken.h`: compact timing and structural metadata.
- `lib/Rsvp/RsvpTokenizer.h`: classify complete source codepoints and retain a
  pending word across trailing whitespace until its structural boundary is known.
- `lib/Rsvp/RsvpEpubSource.cpp`: persist/read metadata in the temporary SD spool.
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp/.h`: distinguish ordinary word
  boundaries, paragraph/block boundaries and chapter EOF in the text-only sink.
- `lib/Rsvp/RsvpCadence.h`: adjusted deadlines, bounded counters and exclusion of
  chapter-loading time from delivered-WPM diagnostics.
- `src/activities/reader/RsvpReaderActivity.cpp`: pass source timing at successful
  presentation, report timing totals, bracket chapter loading.
- `test/rsvp/SmartTimingHarness.cpp`, `run_smart_timing.py`,
  `check_smart_contract.py`, `CMakeLists.txt`, `RsvpTest.cpp`: deterministic C++
  execution, integration contracts and the existing test's deferred-emission fix.
- This guide.

SDK, HAL, panel drivers, LUTs, BUSY handling, FAST backend, typography, ORP,
markers, UI, global settings, orientation and button mapping are unchanged.
No context view, preferences, new settings, TXT or V1C.2/V1C.3 work is included.

## Timing architecture

`RsvpTiming.h` is the sole adjustment table. `LONG_WORD_LENGTH` is **10 lexical
codepoints**. Latin/digit/Greek/Cyrillic/CJK conventions follow the existing ORP
ranges, with CJK punctuation excluded. Apostrophes, hyphens, quotation marks,
brackets and combining marks do not increase lexical length. A hyphenated word
counts its letters across parts; a contraction counts its letters. Font fallback
continues to use its existing geometry rules and is independent of this threshold.

| Winning condition | Extra ms | Interval at 100 WPM |
| --- | ---: | ---: |
| Normal | 0 | 600 ms |
| Long, at least 10 lexical codepoints | 100 | 700 ms |
| Comma | 150 | 750 ms |
| Colon/semicolon | 225 | 825 ms |
| Sentence end | 350 | 950 ms |
| Paragraph/block or chapter end | 500 | 1,100 ms |

The greatest applicable table value wins. Adjustments never stack. Classification
runs on source codepoints during tokenization, including characters beyond the
200-byte display truncation limit, and is saved with the source token. Playback
does not inspect rendered text or change its punctuation.

Terminal commas, colons, semicolons, `. ! ?`, repeated dots and Unicode ellipsis
are recognised. Closing quotes, parentheses, brackets and braces are transparent
to terminal-punctuation recognition. Curly quotes, guillemets, common CJK brackets,
fullwidth comma/colon/semicolon/period/question/exclamation and ideographic
comma/full stop are supported. Internal apostrophes and decimal dots produce no
sentence delay (`don't`, `3.14`); `3.14.` ends a sentence. `Wait...` applies one
sentence adjustment.

Case-insensitive abbreviation exceptions: `Mr. Mrs. Ms. Dr. Prof. Sr. Jr. St.
vs. etc. e.g. i.e. Fig. No. Inc. Ltd. a.m. p.m.`. Single ASCII initials and
sequences of dotted single letters (`A.`, `U.S.`) are also exempt. Ellipses
override those exceptions. This is a conservative English heuristic, not a
linguistic model: ambiguous abbreviations at an actual sentence end can suppress
the sentence adjustment, and unlisted abbreviations can receive it. A genuine
paragraph/chapter boundary still wins. No next-sentence context is inferred.

## Paragraphs and EPUB metadata

The existing text-only XHTML sink already preserved structural boundary events,
but emitted the same null-text event for all of them. It now distinguishes:

- Null text, size 0: word separation (`br`, images, `hr`); not a paragraph.
- Null text, size 1: existing block boundaries (`p`, list items/containers, `div`,
  blockquote, headings, table cells/rows, sections, `pre`, body).
- Null text, size 2: chapter EOF.

Paragraph-equivalent structural blocks receive +500 ms; this is based on actual
XHTML events, not full stops or source formatting newlines. Inline elements join
the same word. Repeated/empty/nested boundary events do not create blank tokens.
Existing hidden/non-visible text guards and visible-codepoint offsets are retained.
Trailing whitespace no longer releases the last word before its paragraph event.
Unicode paragraph separator U+2029 is also a paragraph boundary; ordinary newline
and U+2028 are whitespace only. Chapters with visible text outside a final block
receive the EOF adjustment. A final word already flushed at a closing block has
its block adjustment; EOF creates no second token or second delay.

Metadata survives history replay and SD spooling. The new temporary spool is
`<book-cache>/rsvp-v1c1.tmp`, rebuilt each chapter; no persistent EPUB/page cache
format migration is needed. Its records are start offset, end offset, text length,
one-byte timing reason, one-byte structural flags, then text. Reads use aligned
fields and validate reason/flags, length and offsets. Existing HalFile ownership,
mutex access, release-before-reopen/removal and position restoration are retained.

CSS-created paragraph semantics on otherwise inline elements are not inferred.
Malformed/nonstandard XHTML, unsupported Unicode scripts and ambiguous language
punctuation remain limitations of these bounded heuristics.

## Scheduler and controls

The existing monotonic `esp_timer_get_time()` model measures intervals from
**refresh initiation**, after drawing the framebuffer, rather than from refresh
completion:

```text
baseIntervalMs = 60000 / selectedWpm
targetIntervalMs = baseIntervalMs + maximumApplicableAdjustmentMs
nextStartUs = actualRefreshStartUs + targetIntervalMs * 1000
```

At roughly 480 ms display service, a normal 100-WPM word has approximately
120 ms remaining after completion; a sentence word has approximately 470 ms.
These are scheduling estimates, not new optical measurements. The added interval
holds the existing screen. There is one normal backend submission per presented
word, with no punctuation-pause submission. Existing paused/loading screens retain
their own UI refreshes.

Each successful submission establishes its next deadline from its actual start.
A refresh overrun makes one next word due, with no backlog or multiword skip.
Drawing, loop latency and SD reads can lower the actual start rate; the scheduler
does not promise exact optical WPM. A deadline miss specifically means display
service exceeded that word's adjusted deadline. Chapter loading is excluded from
effective reading WPM by shifting summary time anchors; it does not accumulate
catch-up slots. Chapter transitions and service time are separately reported.

Pausing retains word N and redraws the accepted paused UI. Next/Previous still
move the same source history. Resume requests Current, deliberately redisplaying
N for its complete adjusted interval before advancing. Source/history position
is committed only after successful presentation. Queued controls take precedence
over automatic advancement. Speed changes retain the existing rebase-from-button
time behavior, using the displayed word's adjustment, without a playing refresh.
The 50/75/100/125 WPM limits, paused 100 WPM default and all six accepted physical
actions/labels remain unchanged; there is no saved speed preference.

`RSVP1 SUMMARY` retains selected/delivered WPM, words, misses, service time,
chapter changes and heap information. One bounded `RSVP1 SMART` record adds
exclusive winning-reason counts, requested additional time, average service and
system minimum heap. A summary segment ends on pause, speed change, manual step,
error, book end or exit. Requested timing can include the final word even if the
user interrupts its dwell. Lower delivered WPM with punctuation counts and no
deadline misses is expected. No per-word serial logging was added.

## Validation and resource comparison

All required available validation passed:

- Pinned final firmware build: SUCCESS, 40.70 seconds; no compiler warnings/errors.
- Configured cppcheck: PASSED, no defects, 47.66 seconds; suppressions unchanged.
- `SmartTimingHarness.cpp`: actual production headers compiled using existing
  GCC 14.2.0 for RV32IM and executed in Unicorn 2.1.4. Strict warning checks passed.
  Tests cover normal/long/threshold, each adjustment, maximum precedence, quoted
  endings, contractions, decimals, abbreviations/initials, ellipses, Unicode,
  chunked codepoints, paragraph whitespace/inline joins, chapter EOF, bounded
  truncation/history, output errors, four speeds/clamps, overruns, pause/resume
  position and chapter-loading diagnostics. A failing assertion reports its C++
  line; execution must return normally rather than time out.
- Integration contracts: frozen files and SDK diff match the baseline; one
  submission in the word render path, successful-presentation commit, queued
  controls, Current resume, chapter suspension, structural sink and spool wiring.
- Existing V1B geometry raster: PASS, 1,467 glyph records, ORP, kerning, fallback,
  focus and hint bounds.
- Existing physical-controls raster: PASS, six hints, all 24 remaps, clipping,
  collisions, identical word/marker pixels and no playing chrome.
- `git diff --check`: PASS; manual formatting review completed. Repository
  `./bin/clang-format-fix -g` was attempted and reported formatter unavailable.

Host GTest remains unavailable because there is no installed host C++ compiler.
It is not reported as passed. The deterministic harness is also registered as a
CMake test for hosts with that infrastructure. RV32 execution tests the production
logic, not ESP peripherals or the complete activity event loop. The integration
checks are source contracts; they do not establish physical button or SD behavior.
Only host test dependencies were installed into ignored
`build/rsvp-v1c1/python-deps` (Unicorn 2.1.4, Pillow 12.0.0).

Re-run with the existing PlatformIO Python:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython test/rsvp/run_smart_timing.py
& $pioPython test/rsvp/check_smart_contract.py
$env:PYTHONPATH = (Resolve-Path build/rsvp-v1c1/python-deps).Path
& $pioPython test/rsvp/render_visual_fixture.py
& $pioPython test/rsvp/render_physical_controls.py
& $pioPython -m platformio check -e default
& $pioPython -m platformio run -e default
```

The source-contract checker uses the local SDK preservation artifacts. Recreate
those from the recorded SDK revision/patch if moving the tests to another machine.

| Measurement | Accepted V1B.1 | V1C.1 | Change |
| --- | ---: | ---: | ---: |
| Application image | 6,187,632 | 6,189,952 | +2,320 bytes |
| Linked flash | 6,173,559 | 6,175,869 | +2,310 bytes |
| Static linker RAM | 57,936 | 57,936 | 0 |
| Activity object, target ELF `sizeof` | 9,000 | 9,416 | +416 bytes |
| Token | 216 | 220 | +4 bytes |
| Tokenizer | 244 | 484 | +240 bytes |
| Cadence | 48 | 96 | +48 bytes |

Actual application headroom: **363,648 bytes** in the 6,553,600-byte app partition.
Activity growth consists of 128 bytes across 32 history tokens, 4 bytes in the
staged token, 240 bytes for pending token/scanner state and 48 bytes for counters
and scheduling state, with existing padding accounting for the object total.
There is no new heap allocation, task, framebuffer, font cache or large local
buffer. Fixed members live in the existing heap-allocated activity and are released
with it. No runtime heap savings or reading-speed improvement is claimed.

Final firmware SHA-256:
`cc586ab953805efd37ebc33d38082a6df7049a4609b29a9b409c9e2b822f5c15`.
Validated copy: `build/rsvp-v1c1/validated.bin`; build image:
`.pio/build/default/firmware.bin`. Source/binary manifest:
`build/rsvp-v1c1/artifacts.json`. Logs and upload helper scripts are in the same
ignored artifact folder.

## Upload and boot

Preflight passed: rollback hash, expected USB 303A:1001 serial/MAC
70:AF:09:5E:AA:54 on COM5, firmware capacity and validation logs. Read-only device
partition comparison matched `.pio/build/default/partitions.bin`; OTA bytes
matched the accepted device snapshot, retaining app0 selection at 0x10000.

**Upload SUCCESS:** PlatformIO finished in 75.77 seconds; esptool wrote
6,189,952 bytes at **0x10000** in 34.6 seconds and reported
**Hash of data verified**. The local binary still matches the validated SHA above.
The temporary ignored upload guard removed auxiliary bootloader 0x0000,
partition-table 0x8000 and boot_app0/OTA-initialization 0xe000 images, and checked
source fingerprints, image SHA, capacity and app offset before writing.
Only application sectors were erased/written. No erase-all, NVS, storage/books,
bootloader, partition table, OTA metadata or eFuse writes were issued.
The task-created `platformio.local.ini` was removed afterward.

**Boot observation PASS:** 1,907 bytes captured at 115200 over 20 seconds;
X3 detected, settings and two recent books loaded, Lyra selected, Boot → Home
at 2.525 seconds. At 10.025 seconds: free heap 126,808 bytes, minimum 126,256,
largest block 98,292. No immediate panic, assertion, corruption or reset loop
appeared. The unchanged early Wire I²C lock/null-buffer probe warnings seen in
V1B.1 also appeared; initialization then completed. These were not changed or
claimed fixed. The serial handle closed in `finally`; **COM5 is released**.
No physical button event or RSVP playback was performed automatically.
Boot observation does not establish physical acceptance or extended reading
heap stability.

Evidence: `build/rsvp-v1c1/{read-partitions,read-otadata,upload,reboot}.log`,
`upload-scope.json`, `boot.txt`, `boot-result.json`, `artifacts.json`.
PowerShell initially produced UTF-16 validation logs, so preflight stopped while
reading them; logs were normalized to UTF-8 and preflight repeated successfully
before any flash write. The upload itself succeeded on its first attempt.

## Physical acceptance procedure

1. Open a real DRM-free EPUB and enter RSVP; confirm paused at 100 WPM.
2. Compare ordinary prose rhythm with the accepted V1B.1 baseline.
3. Read dialogue with commas, colons/semicolons, quotes and sentence endings.
4. Check genuine paragraph transitions, including a chapter transition.
5. Confirm longer word holds produce no additional punctuation flashes.
6. Test 75, 100 and 125 WPM; check the 50 and 125 boundaries.
7. Verify top Faster, bottom Slower; right-edge top-to-bottom Next, Previous,
   Play/Pause, Back. Check the accepted paused hints and clean playing screen.
8. Pause word N, use Next/Previous, then resume. N should deliberately redisplay
   for a complete interval before advancing; there should be no unintended skip.
9. Exit and confirm EPUB position and original orientation restoration.
10. Repeat enter/play/pause/exit and chapter transitions while retaining
    `RSVP1 SUMMARY`, `RSVP1 SMART` and `RSVP2 VISUAL_SUMMARY` serial logs. Check
    additional-time/reason counts, display service, deadline misses, free/minimum
    heap/largest block and absence of crashes or sustained heap decline.

For rollback, verify `v1b1-accepted.bin` against its recorded SHA and write only
that application image at 0x10000 with the existing guarded uploader/esptool.
Do not use the merged factory image. No rollback has been performed.
