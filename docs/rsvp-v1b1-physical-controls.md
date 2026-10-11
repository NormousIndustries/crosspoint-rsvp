# RSVP V1B.1 physical controls — built and flashed

Work date: 11 October 2026, Australia/Sydney.

Finalisation supersedes the speed-event inference in section C: physical testing
confirmed the top edge reports logical Down and the bottom reports logical Up.
The production actions are now Down → Faster and Up → Slower, with a paused
100 WPM default. The six hint positions and four front actions are unchanged.
See [V1B.1 finalisation](rsvp-v1b1-finalisation.md) for the current image, cleanup,
validation, flash evidence and final acceptance checklist. The results below
describe the earlier physical-controls build and remain as historical evidence.

**V1B.1 BUILT AND FLASHED — READY FOR PHYSICAL TEST.**
The uploader verified the application hash on COM5. Serial confirmed X3 detection,
Boot → Home, and no immediate crash/assertion during the 20-second observation.
Physical button operation and optical alignment remain for the human tester.

## A. Git branch and source state

- Repository: `C:\ai\Crosspoint-RSVP\crosspoint-rsvp`.
- Branch: `feature/rsvp-v0.1-display-test-v1.6.5`.
- HEAD: `840371a4766d865d9fdef6f61b1b4e682fb3494a`.
- SDK HEAD: `111fdcc7f0176c3ee38391a160ee296bf492dbd8`.
- Remotes: `origin` = NormousIndustries/crosspoint-rsvp; `upstream` =
  crosspoint-reader/crosspoint-reader.
- Host: Windows PowerShell; Git Bash `uname -s` returned
  `MINGW64_NT-10.0-26200`. PowerShell itself has no `uname` command.

The initial tree already contained V1A/V1B reader, menu, parser, translation,
library, test and documentation work; X3 window/height-sweep experiments; and a
dirty SDK. All were retained. No reset, stash, discard, commit, push, SDK change,
toolchain/dependency upgrade, or V1C work occurred. `platformio.ini` is unchanged.

## B. Files changed for this milestone

- `src/components/RsvpReaderUi.cpp`: six edge hints, remap-aware hardware order,
  measured collision detection, leaders and small paused status.
- `src/components/UITheme.h`: pass input and existing word placement to the GUI.
- `src/activities/reader/RsvpReaderActivity.cpp`: retain the placement through
  paused drawing. Input handling and playback logic are unchanged.
- `src/MappedInputManager.h`: expose the existing `mapFrontLabels` helper publicly;
  its implementation and global input behaviour are unchanged.
- `lib/I18n/translations/english.yaml`: use `Play / Pause` for the existing action.
- `test/rsvp/render_physical_controls.py`: actual embedded UI/word raster checks.
- This document.

Ignored `.pio` artifacts contain validation logs, previews, source/binary hashes,
rollback images, and the temporary upload guard. The task-created
`platformio.local.ini` was removed after upload; no prior local config existed.
Generated I18n/HTML files were produced by the normal build, not manually edited.

## C. Physical-to-logical mapping

The user's supplied physical order is authoritative. Source inspection confirms
the following mapping under the standard front-button settings:

| Physical position in landscape CCW | Hardware role | Logical release | RSVP action |
| --- | --- | --- | --- |
| Top edge toward left | UP, index 4 | Up | Faster, +25 WPM |
| Bottom edge toward left | DOWN, index 5 | Down | Slower, −25 WPM |
| Right edge top | RIGHT, index 3 | Right | Next word |
| Right edge second | LEFT, index 2 | Left | Previous word |
| Right edge third | CONFIRM, index 1 | Confirm | Play / Pause |
| Right edge bottom | BACK, index 0 | Back | Return to book |

Evidence: `RsvpReaderActivity.cpp:83–88` consumes logical release events;
`MappedInputManager.cpp:86–102` resolves front roles through settings and keeps
Up/Down fixed; `MappedInputManager.cpp:424–447` maps hints through those same
settings. `BoardConfig.h:901–926` selects the X3 ADC-ladder profile;
`InputManager.cpp:158` maps group-two events to UP/DOWN.

In CCW landscape, front hardware order runs bottom-to-top, so the right-edge
display order is `btn4, btn3, btn2, btn1` (`RsvpReaderUi.cpp:104–107`). Valid user
front remaps move the action hints to the corresponding physical slot. No saved
settings are changed. RSVP does not use the navigation/orientation swap or the
side page-turn toggle for these actions, matching its existing event handling.
The physical events themselves have not been pressed by this task.

## D. Exact screen geometry and label coordinates

The X3 landscape framebuffer is **792 × 528**. SDK default portrait insets are
top/right/bottom/left = 9/3/3/3 (`BoardConfig.h:708–713`). The renderer rotates them
to CCW top/right/bottom/left = **3/3/3/9**
(`GfxRenderer.cpp:2480–2485`). With the current theme's 24 px minimum margin:

- Safe rectangle: (33,27), width 732, height 474.
- Word rectangle: (33,153), width 732, height 194; right/bottom exclusive 765/347.
- **Focus X=311; baseline Y=311**, unchanged from the previous physical log.
- Marker tips: (311,135), (311,365); same six-row triangles as V1B.
- Right button endpoints: **(783,107), (783,211), (783,316), (783,420)**.
- Speed button axis: **X=204**. Top tick Y=9…23; bottom tick Y=505…519.
- Status text origin: (312,453), actual ink bounds (313,457)…(486,477), exclusive.

Coordinates below are text origins in renderer space under default mappings.
The origins are font-top coordinates, not baselines:

| Label | Origin X | Normal origin Y | Collision alternative Y | Measured width |
| --- | ---: | ---: | ---: | ---: |
| Faster | 175 | 27 | — | 58 |
| Slower | 171 | 477 | — | 65 |
| Next | 718 | 95 | — | 47 |
| Previous | 684 | 199 | 121 | 81 |
| Play / Pause | 652 | 304 | 355 | 113 |
| Back | 720 | 408 | — | 45 |

Right-side labels end at X=765. Leaders start at X=769, use the rail X=773,
and terminate at X=783 with a seven-pixel tick. Each hint has its own 244 × 24
clip cell. Speed hints are centred in 244 × 24 cells beginning at X=82.

Anchor spacing uses explicit fractions of the oriented viewable height
(`RsvpReaderUi.cpp:91–110`), and the speed axis is one-quarter across the viewable
width. The schematic supplied with the request has no measured pixel positions
or photograph attachment. These anchors preserve its order and approximate
spacing; physical registration still requires the acceptance test. A future
spacing correction can adjust anchors without touching word geometry.

## E. Font and label metrics

Word assets, ORP selection, fit tiers and natural glyph styling are unchanged.
NORMAL is the existing 132 px Noto Sans face: capital H ink height 94 px. LONG and
EXTRA_LONG remain 96 and 64 px. No new font asset or glyph cache was added.

Hints use the existing theme small font, Ubuntu 10 regular: line height 24,
ascender 20, descender −4. The raster fixture reads its actual packed bitmaps,
glyph bearings, advances and sparse kerning tables; it does not substitute a host
font. English hint ink heights are 15–20 px. Exact per-sample ink bounds are in
`.pio/rsvp-v1b1-geometry.json`.

## F. Collision handling and resource impact

Each right-side hint starts level with its physical anchor. Its measured width
and line box are compared with the existing placement's actual glyph ink bounds.
Only a colliding second/third hint moves above/below the fixed word clip and keeps
a leader to its original anchor (`RsvpReaderUi.cpp:111–127`). No word is moved,
resized, re-centred, or given a paused-only font tier. For `reading`, both middle
hints stay at their anchors. `extraordinary` and `characteristically` move the
Play / Pause hint down. `something` exercises both middle-hint clearances.

The terminal 200-W token still uses the existing bounded clip and automatic
overflow pause. All six hints remain; the status becomes the existing overflow
message. Playback font selection and token/source offsets remain unchanged.

There is no new explicit heap allocation, vector, persistent buffer, framebuffer,
font-cache entry, ISR or task. The existing 32-byte Placement is passed by const
reference; label pointers and coordinates are small local values, and the status
uses the existing 96-byte stack buffer. Shared renderer text measurement/drawing
remains confined to paused chrome. Static RAM remains **57,936 bytes**. Boot heap
observations below do not establish RSVP lifecycle memory stability; that remains
a physical test.

## G. Paused and playing behaviour

Paused: unchanged word and two markers, six action hints, small status/WPM.
Playing: unchanged word and two markers, with no persistent hints or WPM status.
Speeds remain 50/75/100/125 WPM, in 25 WPM steps. Speed changes during playback do
not create notification-only refreshes. Loading/error/end and terminal overflow
retain their existing paused/transient handling.

EPUB entry position, manual stepping, chapter transitions, successful-refresh
position commits, exit position, orientation restoration, HalFile lifecycle,
sleep inhibition, and the full-frame FAST backend are retained. Driver timing,
waveforms, LUTs, DSP, PLL and VCOM were not edited.

## H. Raster validation

Passed `test/rsvp/render_physical_controls.py` using the already installed Pillow
Python at `C:\ai\ComfyUI_windows_portable\python_embeded\python.exe`.
Samples: I, reading, extraordinary, characteristically, something, gyp, 200 W's.
Checks include all six hints, boundaries, pairwise pixel collisions, fixed
ORP/baseline/markers, identical word/marker pixels paused versus playing, and
collision layouts for all 24 valid front-role permutations. Source assertions
connect the fixture to the event/label/visibility policy. These are static checks,
not execution of real button events or an on-device optical screenshot.

Also passed the existing V1B raster/geometry suite: 1,467 baked glyph records,
golden ORP cases, kerning and fallback. The existing 100-word passage still uses
NORMAL for 99 words and LONG for one. The new full-resolution preview was visually
inspected; central typography and collision-free edge hints were retained.

- Individual actual-size preview: `.pio/rsvp-v1b1-reading-paused.png` (792 × 528).
- Four-sample contact sheet: `.pio/rsvp-v1b1-visuals.png` (1584 × 1056; four 792 × 528 frames).
- Playing preview: `.pio/rsvp-v1b1-playing.png`.
- Extra cases: `.pio/rsvp-v1b1-{I,extraordinary,characteristically,something,gyp,oversized}-paused.png`.
- Reports: `.pio/rsvp-v1b1-geometry.json`, `.pio/rsvp-v1b1-visual-check.log`,
  `.pio/rsvp-v1b1-v1b-regression.log`.

## I–K. Build, cppcheck, flash headroom and exact binary

Used the prescribed UTF-8 environment and existing PlatformIO Python:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython -m platformio run -e default
& $pioPython -m platformio check -e default
```

Build: SUCCESS, 72.02 s. Configured cppcheck: PASSED, no defects, 45.87 s.
Logs: `.pio/rsvp-v1b1-build.log`, `.pio/rsvp-v1b1-cppcheck.log`.

The first build exposed private helper access; that was fixed and the final build
and checks passed. The repository formatter wrapper was attempted but no formatter
is installed; no raw formatter or upgrade was used. Manual review and
`git diff --check` passed. The previously unavailable host GTest toolchain was not
installed; firmware compilation plus required Python raster checks were used.

| Capacity measure | Bytes |
| --- | ---: |
| Existing app0/app1 partition capacity, 0x640000 each | 6,553,600 |
| PlatformIO linked flash usage, 94.4% | 6,188,205 |
| Linked-size headroom | 365,395 |
| Actual firmware.bin upload length, 94.6394% | 6,202,288 |
| **Actual image headroom (conservative)** | **351,312 (343.08 KiB)** |

The actual ESP image includes section padding/headers beyond the linked usage.
Use the actual image headroom when planning further growth.

Binary: `C:\ai\Crosspoint-RSVP\crosspoint-rsvp\.pio\build\default\firmware.bin`.
SHA-256: `fd1ef0e8b8c7805056baa45e475490fba2000e86cda501ea074b60a261868c8c`.
Validated copy: `.pio/rsvp-v1b1-validated.bin`.
Source/binary fingerprint manifest: `.pio/rsvp-v1b1-artifacts.json`.

## L. Automatic COM5 flashing result

**FLASH SUCCESS**, confirmed by PlatformIO and esptool, not inferred from build.
COM5 was enumerated and opened/released successfully before upload. Read-only
partition/OTA checks confirmed the on-device partition table matches the build
and app0 is selected. No unrelated process was stopped.

The established command was used:

```powershell
& $pioPython -m platformio run -e default -t upload --upload-port COM5
```

The installed platform normally includes auxiliary flash images. An ignored,
task-only post script removed the bootloader at 0x0000, partition table at 0x8000,
and boot_app0/OTA initialization at 0xe000 from upload flags. It checked every
source fingerprint and the validated binary SHA before allowing the upload.
Only **0x10000 application data** was written. No full-chip erase, NVS, books,
calibration or eFuse write was issued. Application sectors were erased normally
as part of writing the new application.

The temporary configuration initially failed on a UTF-8 BOM, and the initial
guard checked the app offset before PlatformIO resolved it. Both stopped before
any flash write and were corrected. Final upload: SUCCESS in 75.42 s;
6,202,288 bytes written at 0x10000 in 34.6 s; **Hash of data verified**; hard reset.
The uploaded SHA equals the previously validated image.

Evidence: `.pio/rsvp-v1b1-upload.log`, `.pio/rsvp-v1b1-upload-scope.json`,
`.pio/rsvp-v1b1-read-partitions.log`, `.pio/rsvp-v1b1-read-otadata.log`.
The temporary local configuration was removed after completion.

## M. Boot-verification result

**BOOT OBSERVED — no immediate crash/assertion.** At 115200 baud, 1,907 serial
bytes were captured over 20 seconds. Firmware detected X3, loaded settings and
two recent books, selected Lyra, initialized the display/fonts, entered Boot,
then entered Home at 2.524 s. At 10.028 s: free heap 126,808 bytes, minimum
126,256 bytes, largest block 98,292 bytes. Low-power mode was observed.

Early hardware/RTC probing also logged Wire I²C lock and null TX-buffer warnings.
The log subsequently reports SDK RTC found and successful initialization/Home.
No I²C code was changed by V1B.1; this observation is not an assertion that every
boot subsystem is warning-free.

Logs: `.pio/rsvp-v1b1-boot.txt`, `.pio/rsvp-v1b1-boot-result.json`.
The serial handle was closed and COM5 released. No physical button event was
simulated and no RSVP playback was started by the task. This observation does
not verify prolonged reading, real button mapping or optical appearance.

## N. Known limitations

- Anchor order is authoritative; pixel spacing is estimated from the schematic
  and viewable geometry, pending a physical alignment check.
- Valid front remaps are supported; duplicate/invalid stored mappings are not a
  supported remapping configuration. Hints describe the actual logical action.
- The English UI bitmaps were validated. Long translations remain independently
  clipped; other languages and dynamic fallback fonts need their own review.
- Existing Latin word-font coverage, terminal oversized-word pause, and full-frame
  FAST ghosting/refresh characteristics remain as documented for V1B.
- Formatter unavailable; host GTest unavailable. Neither was installed/upgraded.
- No extended RSVP heap/lifecycle or physical round-trip test was performed here.

## O. Physical acceptance test

1. Open a real EPUB at an existing position and enter RSVP. Confirm the first word
   starts at the reader position and the six hints line up with physical buttons.
2. Test the separate top Faster and bottom Slower buttons through
   125 → 100 → 75 → 50 → 75 → 100 → 125 WPM.
3. Test right-edge Next and Previous, then third-position Play / Pause. Confirm
   hints/WPM vanish during playback and return on pause without a word/marker jump.
4. Step short, medium, long and extra-long samples. Confirm any displaced hint's
   leader still points at its physical slot; report collisions or wrong spacing.
5. Test a chapter transition, then Back to the book. Confirm current position and
   the original reader orientation are restored; repeat from each original orientation.
6. If using custom front remaps, verify the action hints follow the buttons.
7. For stability, retain serial `RSVP1 SUMMARY` and `RSVP2 VISUAL_SUMMARY` logs over
   repeated enter/play/pause/Back cycles. Check no sustained heap/largest-block
   decline, crashes, unexpected SD writes, or sleep regression.

## P. Rollback

The pre-task known-good V1B image and ELF were preserved before building:

- `.pio/rsvp-v1b1-rollback/firmware-v1b.bin`.
- `.pio/rsvp-v1b1-rollback/firmware-v1b.elf`.
- Rollback SHA-256: `69c01a80f92cbbb440c2fd3be52ef82c29800f3ff4b1e9668680e45d40c64ffd`.

If rollback is needed, close the human-owned serial monitor, verify the image hash,
and upload only the old application to the same app0 offset. Do not use the
combined factory image or erase-all. With the existing tools:

```powershell
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
Get-FileHash .pio/rsvp-v1b1-rollback/firmware-v1b.bin -Algorithm SHA256
& $pioPython "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" `
  --chip esp32c3 --port COM5 --baud 921600 `
  --before default-reset --after hard-reset write-flash -z `
  0x10000 .pio/rsvp-v1b1-rollback/firmware-v1b.bin
```

Rollback was documented, not performed. Books, NVS, calibration and the existing
partition table are outside this application-only write.
