# CrossPoint RSVP V1.0.1 typography refinement

Work date: 12 October 2026, Australia/Sydney. The user has physically accepted
V1.0. This task refines long-word typography only; V1.0.1 physical testing is
pending. No commit, tag, push or further feature development is performed.

**V1.0.1 TYPOGRAPHY REFINEMENT FLASHED ? READY FOR PHYSICAL TEST**

Application-only upload/hash verification and brief X3 boot observation passed.
COM5 is released; the temporary upload configuration is removed.

## Finding and implementation

The existing selector already used measured fit. `findOrp()` in
`lib/Rsvp/RsvpTypography.cpp:21-38` uses lexical character count to select the
ORP letter, including quotation/punctuation exclusion. It does not select size.
`walk()` at line 60 applies actual fp4 advances, pair kerning, ink bearings and
combining-mark anchors. `place()` at lines 106-119 anchors that ink center first,
then checks all four word-region bounds and tries candidates largest first.
The accepted arithmetic is preserved, with formatter-only whitespace changes
in `walk()`.

The coarse 132/96/64 pixel font sizes were the cause. At 132, `shadowless` fits
at x=[60,761). `entrepreneurs` reaches x=961; at 96 it reaches x=787, so V1.0
falls directly to 64. Additional display space on the left cannot be borrowed
without moving the accepted ORP. No whole-word centering is introduced.

Added complete, baked 1-bit Noto Sans tiers **92 (65 px capitals)** and
**80 (58 px capitals)**. The candidate order is now **132,96,92,80,64**.
`entrepreneurs` at 92 reaches exactly the safe exclusive right edge x=765;
its last ink column is 764. A deterministic one-pixel narrower region selects
80. Width includes the quotes/comma, and height includes descenders and raised
marks. A narrow 13-letter `iiiiiiiiiiiii` still uses normal, demonstrating that
size selection does not depend on character count.

`lib/Rsvp/RsvpTypography.h:9-10` keeps existing enum indices and adds the two
new tiers. `font()` at `RsvpTypography.cpp:44-58` returns their static assets.
The generator at `scripts/generate_rsvp_fonts.py:15` and `fonts/metrics.json`
now enumerate five sizes. Existing 132/96/64 assets are byte-for-byte preserved;
all five reproduce from the same source font and existing Pillow/FreeType
installation. No runtime font generation, scaling, stretching or squeezing.

The only activity changes are two bounded diagnostic counters in the existing
member array (`RsvpReaderActivity.h:71`) and their exit summary
(`RsvpReaderActivity.cpp:344-347`). Size selection and drawing add no allocator
calls. These 8 bytes are justified within the existing checked activity
allocation, allowing physical font-usage diagnostics without per-word logging.

## Measured raster geometry

Each before/after preview is an actual **792 x 528** bitmap, drawn from the
embedded 1-bit glyph arrays. Proposed positions come from executed production
C++ `walk()` draw callbacks on RV32/Unicorn, including Unicode mark y offsets.
All bounds below are exclusive at right/bottom. Capital height is the actual
embedded `H` ink height, distinct from a particular lowercase word's ink height.

The safe word rectangle stays **[33,153,765,347)**, focus **x=311**, baseline
**y=311**, marker tips **y=135,365**. Its horizontal budgets around focus are
278 px left / 454 px right. The accepted integer-pixel ORP rounding places a
half-pixel-width center at **310.5**, otherwise at **311**; this rule is unchanged.
The UI geometry/draw code at `src/components/RsvpReaderUi.cpp:64-69,123-137`
remains byte-identical. Markers stay locked to x=311.

| Word | V1.0 tier / capitals | V1.0.1 tier / capitals | ORP x | Ink bounds (left, top, right, bottom) |
| --- | --- | --- | ---: | --- |
| `the` | normal / 94 px | normal / 94 px | 311 | (225, 211, 419, 312) |
| `shadowless` | normal / 94 px | normal / 94 px | 310.5 | (60, 211, 761, 312) |
| `entrepreneur` | long / 69 px | long / 69 px | 311 | (144, 247, 743, 334) |
| `entrepreneurs` | extra-long / 46 px | compact / 65 px | 310.5 | (151, 250, 765, 333) |
| `extraordinary` | long / 69 px | long / 69 px | 311 | (155, 237, 762, 334) |
| `international` | long / 69 px | long / 69 px | 311 | (172, 237, 740, 312) |
| `characteristically` | extra-long / 46 px | small / 58 px | 310.5 | (129, 250, 752, 330) |
| `"entrepreneurs,"` | extra-long / 46 px | small / 58 px | 310.5 | (145, 253, 753, 330) |
| `WWWWMMM` | long / 69 px | long / 69 px | 310.5 | (89, 241, 696, 311) |
| `WMWMWMWMWM` | extra-long / 46 px | extra-long / 46 px | 311 | (105, 265, 688, 311) |
| `incomprehensibility` | extra-long / 46 px | extra-long / 46 px | 311 | (160, 262, 748, 326) |
| `iiiiiiiiiiiii` | normal / 94 px | normal / 94 px | 311 | (202, 214, 624, 311) |
| `“entrepreneurs,”` | extra-long / 46 px | small / 58 px | 310.5 | (140, 253, 754, 330) |
| `élan` | normal / 94 px | normal / 94 px | 311 | (227, 210, 473, 312) |
| `Å̂lan` | normal / 94 px | normal / 94 px | 311 | (210, 174, 473, 312) |
| `中文` | normal / 94 px | normal / 94 px | 311 | (119, 211, 371, 331) |
| `AV` | normal / 94 px | normal / 94 px | 310.5 | (192, 216, 350, 311) |
| `gyp` | normal / 94 px | normal / 94 px | 311 | (204, 239, 419, 343) |
| `...` | normal / 94 px | normal / 94 px | 311 | (303, 295, 389, 313) |
| `well-known` | long / 69 px | long / 69 px | 310.5 | (170, 237, 674, 312) |

`entrepreneurs` gains **19 px capital height (+41.30%)**. Its size difference
from `shadowless` falls from 48 to 29 px, a **39.58% reduction** in that jump.
`characteristically`, straight-quoted and curly-quoted `entrepreneurs,` gain
**12 px (+26.09%)**. `entrepreneur`, `extraordinary` and `international` retain
69 px because the larger available tier genuinely fails. The very wide
`WMWMWMWMWM` and long `incomprehensibility` require 46 px.

The representative existing 100-word fixture retains 99 normal / 1 long, using
its original golden geometry. A word exceeding even the smallest tier is still
rejected and uses the existing paused-context/oversized behavior. A 200-W token
is explicitly tested; it is never rendered beyond the safe region. Existing
CJK replacement-glyph behavior is preserved; this is not new CJK font coverage.

[Full before/after comparison](../rsvp-release.local/v1.0.1/typography-comparison.png).
[Entrepreneurs before](../rsvp-release.local/v1.0.1/typography-03-before.png) /
[after](../rsvp-release.local/v1.0.1/typography-03-after.png).
[Quoted form before](../rsvp-release.local/v1.0.1/typography-07-before.png) /
[after](../rsvp-release.local/v1.0.1/typography-07-after.png).
`typography-report.json` records every candidate's bounds/fit/ORP, final tier,
capital height, and filenames for all 40 full-resolution before/after previews.

## Resources and preserved behavior

| Measurement | Accepted V1.0 | V1.0.1 | Difference |
| --- | ---: | ---: | ---: |
| Application image | 6,196,144 B | 6,456,848 B | +260,704 B |
| Static linker RAM | 57,936 B | 57,936 B | 0 |
| Activity object (actual DWARF) | 12,520 B | 12,528 B | +8 B |
| App partition headroom | 357,456 B | 96,752 B | -260,704 B |

ELF symbols prove both new bitmaps, glyph/kerning/interval tables and 120-byte
font descriptors reside in **`.flash.rodata`**, rather than DRAM. Assets account
for 260,370 B before descriptor/alignment/code overhead. There is no new glyph
cache, framebuffer, font-loading storage, task or allocation loop. Other measured
objects are unchanged: settings 216 B, cadence 96 B, source 1,912 B, context
2,152 B, context placement 364 B. This does not claim a measured peak-heap saving.

Candidate sizes 104/112 were investigated with the same generator. They still
failed all four requested long words that use the 96 tier, while costing about
186/213 KB each. The two added tiers address the observed 69-to-46 drop and
leave partition headroom. Another complete upper bridge would exceed the
remaining 96,752 B. No partition/driver/toolchain changes were made.

The hot path performs up to five bounded glyph walks instead of three only
when larger candidates fail, using small integer/scalar locals and existing
flash table lookups. Normal/long words return after the same one/two attempts.
No CPU speed improvement is claimed, and physical cadence remains to be checked.
The activity's complete drawFrame/render/commit path is preserved. Thus no
additional refresh requests or backend submissions are introduced.

Smart Timing, sentence context, persisted WPM/Smart Timing, EPUB parsing and
position mapping, six-button controls, HAL lifecycle and all SDK/display,
waveform and refresh sources retain the accepted V1.0 source hashes. The
pre-existing dirty SDK modifications and untracked header are also preserved.

## Validation

- `default` firmware build: SUCCESS, 131.75 s, no compiler warnings.
- Configured `pio check -e default`: PASSED, 46.79 s, no defects; suppressions unchanged.
- New production RV32 geometry harness: 20 explicit examples plus 240 generated
  narrow/wide/kerning/descender words, all larger-candidate rejection, exact-edge
  fallback, vertical fitting, drawn-vs-measured bounds, ORP/punctuation/UTF-8,
  combining marks, and 200-byte overflow rejection.
- Actual baked raster validation: all 2,445 glyph masks, every example at native
  resolution, unchanged baseline/ORP, safe display edges, marker collisions and
  24 front-button hint permutations. Key full-resolution previews inspected.
- Existing production context/tokenizer/EpdFont, Smart Timing/history/cadence,
  real ArduinoJson preferences and actual activity regressions pass, including
  repeated entry/exit and pause/resume, failed-display commit and chapter/EOF
  cases. Existing context, word and physical-control raster suites pass.
- Typography preservation contract, repository formatter wrapper and
  `git diff --check` pass. All five font assets reproduce without editing old assets.

Native CMake/GTest is not available in this environment; the existing pinned
RV32/Unicorn execution path was used. Its test ELF has a linker RWX-segment
warning because of the freestanding emulator layout, unrelated to firmware;
the firmware build has no warnings. Emulator checks do not simulate real SD,
concurrent FreeRTOS or panel timing. Physical acceptance is pending.

Reproduce using the established local dependencies:

```powershell
$env:PYTHONUTF8 = '1'
& "$env:USERPROFILE/.platformio/penv/Scripts/python.exe" test/rsvp/validate_typography.py --format --checks --build --cppcheck
```

The preservation check requires this task's archived `before.json` and rollback.
Generated fonts are created by `scripts/generate_rsvp_fonts.py`; firmware builds
consume baked headers and do not need Pillow. Source, logs, RV32 test ELFs,
resource symbols, manifests and previews are in `rsvp-release.local/v1.0.1/`.

## Firmware, rollback and physical verification

Validated application SHA-256: `f25f9e60c8647694a6a155a45d9fa0639cb7907e86b6955f58eb724fd6c64467`.
`validated.bin` and matching `validated.elf` are preserved separately from `.pio`.

Accepted V1.0 rollback: `rsvp-release.local/v1.0.1/v1.0-accepted.bin` and matching
ELF. SHA-256: `999955b7086ed85184f0e7cf8d668e7950180e42ebf02df2de5ed6d01548b0b9`.
The prior V1C.3 archive and older accepted rollback artifacts are retained.

COM5 preflight passed for USB `303A:1001`, MAC/serial `70:AF:09:5E:AA:54`;
read-only partition/OTA bytes match the accepted app0 selection. Application
capacity is `0x640000`, offset `0x10000`. The prepared uploader checks application
and source hashes, writes only this application and excludes bootloader,
partition and OTA initialization images. No erase-all, NVS/eFuse or SD write is
part of the upload.

**Upload PASS:** 6,456,848 application bytes written at `0x10000`; PlatformIO
SUCCESS in **44.28 s**, and esptool reports **?Hash of data verified.?**
`upload-scope.json` contains only application upload flags; this nobuild uploader
already supplied no auxiliary images. `upload.log` confirms one application
write address/image. Working/preserved binary hashes remain the validated hash.

**Boot PASS:** 5,170 serial bytes captured for 20 seconds at 115200. X3/Home
startup and subsequent ordinary device navigation into EPUB/RSVP were observed;
no automated button events were sent. No immediate panic, assertion, corruption
or reset loop was detected. Existing startup I2C probe warnings remain.
RSVP logged focus=311, baseline=311, word=33,153,732,194 and marker tips=135,365.
Saved preferences loaded at **125 WPM / Smart Timing On**. Activity size was
**12,528 B**; entry heap **125,620 B**, minimum **103,780 B**. The entry block
field was interleaved; chapter-ready reports heap **125,060 B**, minimum
**86,312 B**, largest block **102,388 B**. Paused context reported 17 words,
four lines, construction **32,741 us**, transition **512,491 us**, heap
**115,596 B**, largest block **90,100 B**. These are short-session observations,
not extended playback/heap-leak or physical typography acceptance.

Evidence: `upload.log`, `upload-scope.json`, `reboot.log`, `boot.txt`,
`boot-result.json`, `artifacts.json`, `resource-report.json` and the read-only
partition/OTA logs. Serial capture closed COM5 in `finally`; upload removed
`platformio.local.ini` in `finally`. The final checks confirmed it is absent.

Physical verification:

- Compare `shadowless`, `entrepreneurs` and quoted long words during playback;
  watch the ORP letter between the markers and confirm the baseline stays fixed.
- Check the exact right-edge `entrepreneurs` case for complete glyph strokes;
  inspect the wide W/M and smallest-tier examples for clipping or marker contact.
- Verify all four speeds, Smart Timing On/Off, pause/context/resume, chapter
  transitions, Previous/Next, Back and preference persistence remain accepted.
- During repeated sessions, retain serial ENTER/CHAPTER/EXIT and VISUAL_SUMMARY
  free/minimum heap/largest-block samples and compare warmed-up lifecycle points.
  New summary fields `compact` and `small` count the added tiers.

Stop here. No further feature development is included.
