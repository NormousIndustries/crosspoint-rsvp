# RSVP V1B.1 finalisation

Work date: 11 October 2026, Australia/Sydney.
**V1B.1 FINALISED AND FLASHED — READY FOR ACCEPTANCE**

Build, static analysis, available raster/input validation and application-only
COM5 upload passed. No commit, push, dependency/toolchain upgrade or
V1C work is part of this task.

## Source state and preservation

Branch: `feature/rsvp-v0.1-display-test-v1.6.5`.
HEAD: `840371a4766d865d9fdef6f61b1b4e682fb3494a`.
Remotes: origin = NormousIndustries/crosspoint-rsvp; upstream =
crosspoint-reader/crosspoint-reader. Host: Windows PowerShell, with Git Bash
`uname -s` = `MINGW64_NT-10.0-26200`.

The pre-existing dirty reader, EPUB parser, UI, translations, tests, documentation
and SDK experiments were preserved. Pre-task source fingerprints, snapshots and
reader/SDK patches are in `.pio/rsvp-finalisation/before/`. No reset or discard
was used. The isolated experimental activities were archived before removal.

Known-good V1B.1 rollback: `.pio/rsvp-finalisation/before/known-good-v1b1.bin`.
SHA-256: `fd1ef0e8b8c7805056baa45e475490fba2000e86cda501ea074b60a261868c8c`.
The earlier V1B rollback under `.pio/rsvp-v1b1-rollback/` remains intact.

## Controls and default

The old `RsvpReaderActivity.cpp:86–87` inferred physical top/bottom from logical
Up/Down. Global `MappedInputManager.cpp:98–103` keeps these roles fixed; RSVP
forces landscape CCW. The user's physical acceptance established that the
approved top Faster label accompanies logical Down, and bottom Slower logical
Up. The correction is only in RSVP's release-to-action handling. Labels, global
button mappings, page-button preferences and saved settings are unchanged.

| Physical edge/position | Logical release | Action |
| --- | --- | --- |
| Top | Down | Faster, +25 WPM |
| Bottom | Up | Slower, −25 WPM |
| Right, top | Right | Next word |
| Right, second | Left | Previous word |
| Right, third | Confirm | Play / Pause |
| Right, bottom | Back | Return to book |

`RsvpCadence.h` now starts at index 2 (100 WPM). Supported speeds remain
50/75/100/125 with clamped endpoints and 25 WPM increments. There is no persisted
RSVP speed preference; each new activity starts paused at 100. Loading/paused
state transitions and cadence scheduling are unchanged.

## Files changed and cleanup

- `lib/Rsvp/RsvpCadence.h`: 100 WPM default.
- `src/activities/reader/RsvpReaderActivity.cpp/.h`: speed action correction;
  remove the first-12-word geometry log and its counter.
- `src/activities/settings/SettingsActivity.cpp/.h`: remove both menu entries,
  includes, action enums, activity allocation/navigation cases and their reserve.
- Remove `src/activities/util/RsvpDisplayTestActivity.cpp/.h` and
  `X3WindowTestActivity.cpp/.h` from firmware source, after archival.
- `lib/I18n/translations/english.yaml`: remove isolated test/window strings and
  obsolete inline control hints. Retain all six production control strings,
  paused/overflow text, RSVP menu name and the shared status format
  `STR_RSVP_TEST_PLAY_STATUS`, which production still uses.
- `test/rsvp/RsvpTest.cpp`: assert 100 WPM default, then select 125 to retain the
  existing cadence overrun/rebase and boundary checks.
- `test/rsvp/render_physical_controls.py`: corrected six-event source assertions
  and 100 WPM status raster.
- `test/rsvp/render_visual_fixture.py`, `representative_passage.txt`: 100 WPM
  preview and move the unchanged passage from firmware strings to a host fixture.
- Existing V1A/V1B/physical-control guides: updated acceptance/default guidance
  or a superseding note, preserving earlier build/physical evidence.
- This report and `docs/research/x3-display/`: preserved harness source, strings,
  raw serial logs and research findings.

The test activities' sample arrays, fixed passage and isolated test UI are removed
from the firmware. Shared fonts, renderer, HAL, SDK drivers and production RSVP
EPUB/tokenization/restoration dependencies are retained. Research locations and
why interleaved low-level code remains are documented in
[the archive index](research/x3-display/README.md).

## Logging, visuals and memory

Remove the bounded per-word `RSVP2 GEOMETRY` debug output. Keep entry LAYOUT,
ENTER/EXIT, chapter/file lifecycle, errors, SUMMARY, VISUAL_SUMMARY and global
crash/heap diagnostics. Word placement, selected font tier, baseline, markers,
overflow fallback, source offsets, scheduling and display submission are unchanged.

No new firmware heap allocation, framebuffer, task, stack buffer or font cache was
introduced. No measured runtime heap saving is claimed. Static linker RAM and
application image sizes are reported separately below. The approved 132 px face
(approximately 94 px capitals), focus X=311, baseline Y=311, triangle tips
135/365, six physical hints and clean playing screen remain as in V1B.1.

## Validation and firmware sizes

- Baseline pinned `default` build: SUCCESS, 50.659 s.
- Final pinned `default` build: SUCCESS, 71.578 s, no compiler warnings/errors.
- Configured `platformio check -e default`: PASSED, 47.048 s, no defects;
  existing exclusions/suppressions unchanged.
- Actual bitmap physical-controls suite: PASS; six hints, all 24 front remaps,
  corrected release events, clipping/collisions, unchanged ORP/baseline/markers,
  identical word/marker pixels paused versus playing and no playing chrome.
- V1B geometry suite: PASS; 1,467 glyph records, golden ORP cases, kerning, focus,
  fallback and English hint bounds. The unchanged representative passage uses
  NORMAL for 99 words and LONG for one.
- Source audit: PASS. SHA checks confirm all SDK/HAL, UI rendering, fonts, EPUB
  parser/source, normal reader/menu and global input/settings match the pre-task
  snapshot. An exact transform checks the production activity differs only in
  two speed actions and geometry-log removal; cadence differs only in its default.
  New reader sessions retain `afterLoading=Paused`, successful presentation
  commits and exit-position/orientation handling. Searches and firmware linking
  confirm no production references to deleted harnesses.
- Repository formatter wrapper: attempted, reports formatter unavailable.
  Manual edit review and `git diff --check`: PASS. Host GTest execution remains
  unavailable because no host C++ compiler is installed; the updated GTest source
  is not claimed as executed. Pinned target compilation and available Python
  raster/input checks passed. No tool was installed or upgraded.
- Preview `.pio/rsvp-v1b1-visuals.png` visually inspected: approved typography,
  marker positions, label order and collision layouts retained, 100 WPM status.

| Measurement | Before | After | Difference |
| --- | ---: | ---: | ---: |
| Application image bytes | 6,202,288 | 6,187,632 | −14,656 |
| PlatformIO linked flash bytes | 6,188,205 | 6,173,559 | −14,646 |
| Static linker RAM bytes | 57,936 | 57,936 | 0 |
| Actual image headroom in 6,553,600-byte app partition | 351,312 | 365,968 | +14,656 |

The cleanup recovers **14,656 bytes (14.31 KiB) of application flash capacity**,
confirmed by both builds. Removed harness code/literals and orphaned generated
translation strings reduce flash contents; there is no claimed runtime heap or
reading-speed improvement. ESP image headers/alignment explain the small
difference from linked-size savings. RAM above excludes dynamic peak usage.

Final binary: `.pio/build/default/firmware.bin`.
SHA-256: `8417d233a68cb60252e3e2449f9db6a9ae2003226c0da74376c3b71572d658fd`.
Validated copy: `.pio/rsvp-finalisation/validated.bin`.
Logs/manifest: `.pio/rsvp-finalisation/{baseline-build,final-build,cppcheck,
physical-raster,visual-raster,audit}.log` and `artifacts.json`.

## Flash and boot

**Upload SUCCESS**, confirmed by PlatformIO/esptool: 76.135 s total;
6,187,632 bytes written at **0x10000** in 34.6 s;
**Hash of data verified**; hard reset completed.

COM5 was enumerated as USB VID/PID 303A:1001, serial/MAC 70:AF:09:5E:AA:54,
matching the previously verified X3. Esptool identified ESP32-C3 revision v0.4.
Read-only preflight confirmed the device partition table matches the build and
the OTA records are byte-identical to the known-good device metadata: sequence
1, undefined/bootable state, matching sequence CRC, app0 selected.

The established command ran through the existing PlatformIO Python:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython -m platformio run -e default -t upload --upload-port COM5
```

A temporary ignored `platformio.local.ini` appended an application-only guard to
the unchanged base scripts. The guard excluded bootloader 0x0000, partition table
0x8000 and boot_app0/OTA initialization 0xe000; checked validated binary SHA,
source fingerprints, image size and app offset before permitting the write.
Only application sectors were erased/written as part of that upload. No erase-all,
NVS, user storage/books, calibration, bootloader, partition table, OTA data or
eFuse write was issued. The task-created local config was removed afterward.

The first post-upload serial helper failed before opening COM5 because a copied
UTF-8 BOM was decoded incorrectly. Upload and hash verification had already
succeeded. The helper was replaced and only the read-only identity/reset plus
serial observation repeated; no second flash write was performed.

**Boot observation PASS:** 5,985 bytes captured at 115200 over 20 seconds;
X3 detected, settings/two recent books loaded, Lyra selected, Boot → Home at
2.524 s. At 10.006 s: free heap 126,948 bytes, minimum 126,396, largest block
98,292. The same capture subsequently records EPUB loading/page rendering and
EpubReaderMenu entry. No immediate panic, assertion, stack/heap corruption or
reset loop was seen. No automated physical button event was sent; this log does
not establish final RSVP acceptance or extended lifecycle stability.

The early Wire I²C lock/null-TX-buffer probe warnings also occurred in the prior
V1B.1 capture; initialization then reports SDK RTC/IMU and reaches Home. These
unchanged warnings are retained as a known observation, not declared fixed.
The capture closes its serial handle in `finally`; **COM5 is released**.

Evidence under `.pio/rsvp-finalisation/`: `read-partitions.log`,
`read-otadata.log`, device metadata binaries, `upload.log`, `upload-scope.json`,
`reboot.log`, `boot.txt`, `boot-result.json` and `boot-capture.log`.

Rollback, if subsequently needed: verify the preserved known-good V1B.1 hash and
use the existing esptool to write only
`.pio/rsvp-finalisation/before/known-good-v1b1.bin` at 0x10000 on COM5.
Do not use a merged factory image. No rollback was performed in this task.

## Known limitations

The existing full-frame FAST backend limits delivered cadence; 100 WPM is a
requested speed selected by the user's comfort testing, not a guaranteed optical
rate. Existing Latin coverage, long-word tiers, overflow pause and ghosting
characteristics remain. Physical acceptance after this upload remains a human
check. Static raster/source checks do not prove real button events, SD behaviour
or extended heap stability on hardware. No host C++ compiler/GTest runner or
formatter is installed; neither was installed or upgraded for this task.

## Final physical acceptance

1. Confirm RSVP Display Test and X3 Window Test are absent from Settings → System.
2. Open an EPUB and enter RSVP from the normal reader menu.
3. Confirm the session starts paused at 100 WPM.
4. Press top-edge Faster: verify 125 WPM and the 125 boundary.
5. Press bottom-edge Slower: verify 100, then 75, then 50 and the 50 boundary.
6. Confirm right-edge Next, Previous, Play / Pause and Back retain their order.
7. Confirm typography, ORP, baseline, markers, edge labels, clean playing screen
   and long-word fallback match the approved UI.
8. Exit RSVP and confirm EPUB content position and original orientation return.

For extended stability, repeat enter/play/pause/Back and a chapter transition;
retain `RSVP1 SUMMARY` / `RSVP2 VISUAL_SUMMARY` serial logs and check for crashes
or sustained heap/largest-block decline. The HalFile lifecycle fix is retained.
