# RSVP V1B visual system — physical-test handoff

## A. Git/source state

Work date: 11 October 2026, Australia/Sydney.

- Branch: `feature/rsvp-v0.1-display-test-v1.6.5`.
- HEAD: `840371a4766d865d9fdef6f61b1b4e682fb3494a` (unchanged).
- `origin`: `https://github.com/NormousIndustries/crosspoint-rsvp.git`.
- `upstream`: `https://github.com/crosspoint-reader/crosspoint-reader.git`.
- Host: Windows PowerShell; Git Bash `uname -s` returned `MINGW64_NT-10.0-26200`.

The entry working tree already contained the V1A reader/menu/parser/translation
changes, RSVP library/activity/tests, crash and integration handoffs, X3 window
prototype changes, height-sweep documents, and a dirty SDK submodule. Those changes
remain. No reset, stash, discard, commit, push, dependency upgrade, flash, or V1C
work was performed. `platformio.ini` was not edited.

SDK HEAD remains `111fdcc7f0176c3ee38391a160ee296bf492dbd8`. V1B did not edit any
SDK file, including the pre-existing driver/window/waveform experiments. The
height-sweep and research artifacts remain in place.

## B. V1B files changed/added

Changed existing local work:

- `src/activities/reader/RsvpReaderActivity.{h,cpp}`: replace centered UI typography
  and portrait hints; add bounded geometry/heap diagnostics and overflow pause.
- `src/components/UITheme.h`: expose the RSVP layout/drawing methods.
- `lib/I18n/translations/english.yaml`: add six translated control/status keys.
- `test/rsvp/CMakeLists.txt`: register the typography tests.

Added:

- `lib/Rsvp/RsvpTypography.{h,cpp}`: ORP, shared glyph walk, discrete fit selection.
- `lib/Rsvp/fonts/rsvp_{132,96,64}.h`, `metrics.json`: generated static assets.
- `scripts/generate_rsvp_fonts.py`: repeatable local asset generation.
- `src/components/RsvpReaderUi.{h,cpp}`: GUI-owned word, markers and control zones.
- `test/rsvp/RsvpTypographyTest.cpp`: C++ ORP/metrics/fallback regression cases.
- `test/rsvp/render_visual_fixture.py`: actual-asset raster checks and static sheet.
- `test/rsvp/make_v1b_fixture.py`: separate physical acceptance EPUB.
- This handoff document.

Generated I18n/HTML files and build/test artifacts remain ignored. The pre-existing
V1A source, token spool, HalFile RAII ownership, cache formats and reader launch
integration were not changed.

## C. Font architecture selected

Evidence from the inspected pipeline:

- Built-in reader conversion tops out at 18 pt:
  `lib/EpdFont/scripts/convert-builtin-fonts.sh:8-9`.
- Built-in Noto Sans 18 regular has a 27 px `H` and 21 px `x`, far below the target;
  see its glyph table in `lib/EpdFont/builtinFonts/notosans_18_regular.h:2386`.
- `lib/EpdFont/EpdFontData.h:145-153` supplies bitmap width/height, fp4 advance,
  bearings and bitmap offsets. The whole-font structure supports both 1-bit and
  2-bit bitmaps, static kerning, compression and dynamic glyph handlers.
- `lib/GfxRenderer/GfxRenderer.cpp:603` exposes `drawGlyphBitmap`, with native
  orientation and clipping into the existing framebuffer.
- Runtime TTF **is supported on the current C3 build**, including streamed faces:
  `lib/EpdFont/TtfEpdFont.cpp:145-177`. It does not require loading a giant TTF in
  RAM. Its bitmap/table caches grow on misses (`TtfEpdFont.cpp:349`), however;
  large RSVP glyphs would add that runtime memory/cache workload.

Selected: Noto Sans Regular, embedded uncompressed 1-bit `EpdFontData`, at three
fixed pixel sizes. No SD font loading, font-map insertion, runtime rasterization,
decompression arena, glyph cache, ligature substitution, or extra framebuffer is
needed. Drawing uses the existing `EpdFont` lookups and renderer bitmap API through
`UITheme`. The visual layer has no EPUB or HalFile ownership.

The existing converter requires `freetype-py` and `fontTools`, absent from the
installed PlatformIO Python. The installed ComfyUI Python already supplies
Pillow/FreeType; it generated the assets without package installation. The new
generator follows the repository's EpdGlyph/fp4 format. It parses the source's
GPOS horizontal pair kerning in design units and uses the existing fp4/clamping
convention. BASIC FreeType rasterization supplies fitted glyph advances; pair
kerning is baked separately. Context-dependent positioning is excluded. ASCII
pairs carry kerning; extended Latin glyphs carry their own advances/bearings.

Source: `lib/EpdFont/builtinFonts/source/NotoSans/NotoSans-Regular.ttf`, SHA-256
`fe8c022f48d8dd29f17b744d16f9346f4357e16f7d4f7be58b000ae7c291b614`.
The adjacent `OFL.txt` is the font license. Generated assets require no Python
dependency during normal firmware builds.

## D–E. Font asset size and pixel metrics

Each size has 489 glyph records and 9,000 bytes of glyph/interval/kerning tables,
plus a 120-byte font descriptor. ELF symbols place all these in flash `.rodata`.

| Tier | Em pixels | H ink WxH | x ink WxH | Ascent/descent | Native maximum top/bottom | Bitmap bytes | Total asset bytes |
| --- | ---: | --- | --- | --- | --- | ---: | ---: |
| NORMAL | 132 | 72×94 | 64×71 | 142/39 | 131/32 | 284,234 | 293,354 |
| LONG | 96 | 53×69 | 47×52 | 103/29 | 97/24 | 152,560 | 161,680 |
| EXTRA_LONG | 64 | 35×46 | 30×34 | 69/19 | 64/16 | 66,904 | 76,024 |

Combined assets: **531,058 bytes** before alignment padding. Header source text is
about 2.11 MB; this is not the firmware memory cost. Coverage: U+0020–017F,
U+0300–036F, U+2010–2026, euro and replacement glyph. Bitmaps are thresholded at
50% coverage, tightly cropped, then packed MSB first. The visual fixture checks
every byte range and every final ink bounding box.

## F. Fixed focus and vertical geometry

`UITheme::rsvpLayout` uses renderer dimensions and oriented physical bezel insets,
not a hardcoded panel resolution. A theme-aware margin of at least 24 px defines
the safe area. Focus is 38% into its width, leaving more room for the trailing
portion of ORP-aligned words. The baseline is the viewable vertical midpoint
plus half the normal capital height (47 px).

For the current X3 792×528 landscape profile with zero insets and current themes:

| Geometry | Logical pixels |
| --- | --- |
| Safe area | x=24, y=24, w=744, h=480 |
| Focus X | **306** |
| Baseline Y, every tier/state | **311** |
| Word region | x=24, y=153, w=744, h=194; right=768, bottom=347 exclusive |
| Upper marker tip | (306,135) |
| Lower marker tip | (306,365) |
| Paused controls | y=408; three 32 px rows; bottom=504 exclusive |

Raised combining marks also participate in measured vertical bounds. They can
trigger a smaller tier if necessary. Pausing does not recalculate the word layout.
The `RSVP2 LAYOUT` log reports the actual hardware/theme geometry once at entry.

## G–I. Exact ORP, measurement and punctuation behavior

`rsvp::findOrp` counts lexical letters/digits, excluding punctuation and combining
marks. Latin, Greek/Cyrillic and CJK ranges are recognized for indexing; bitmap
coverage is narrower as described above. Apostrophes and hyphens remain in the
display text but do not contribute to lexical length. Empty/punctuation-only
tokens select the first glyph deterministically.

| Lexical length | Zero-based selected lexical index |
| --- | ---: |
| 1 | 0 |
| 2–5 | 1 |
| 6–9 | 2 |
| 10–13 | 3 |
| 14+ | 4 |

The helper returns the selected character's UTF-8 **byte offset** in the unchanged
display token. Leading quotes/brackets therefore cannot shift the selected
letter. Trailing punctuation cannot change the count. `don't` selects `o`;
`well-known` selects its first `l`. Smart quotes and multibyte accents are handled
without byte-count approximations or temporary strings.

`rsvp::walk` measures and draws with the same glyph lookup, fp4 differential
rounding of previous advance + pair kern, bearings and combining-mark anchoring.
The selected glyph's center is `glyphCursorX + leftBearing + inkWidth/2`.
Its doubled coordinate retains half pixels. The word origin is:

```text
wordX = focusX - ceil(orpCenterWithinWord)
```

Consequently its rendered center lands at X=306 or X=305.5, the nearest available
integer-pixel origin with a consistent tie direction. No character receives any
style change. No character-count/average-width approximation or whole-word
centering is used. The baseline is passed directly to the bitmap draw callback.

## J. Registration markers

Exactly two monochrome filled triangles. Each is six rows tall, with half-widths
0,1,2,3,4,5. The upper triangle points downward; the lower points upward. Both
single-pixel tips are on `focusX`. Upper rows occupy Y=130–135; lower rows occupy
Y=365–370. No line, crosshair, box or ORP decoration is drawn. Both are outside
the fixed word rectangle, suitable for a future dirty-region design.

## K–L. Paused and playing contents

Paused chrome stays in six independent 372×32 px cells below the word/markers:

| Row | Left cell | Right cell |
| --- | --- | --- |
| 408–439 | Paused/loading/error/end + requested WPM | Up: Faster / Down: Slower |
| 440–471 | Back: return to book | Confirm: Play/pause |
| 472–503 | Left: Previous | Right: Next |

Labels describe **logical mapped actions**, so front-button remapping still
applies. Text uses `tr()` and the existing UI small font. Cells clip independently
and restore the previous renderer clip. English labels fit intact. The old
portrait-oriented button/side-hint helpers are not invoked by RSVP.

Playing shows white background, unchanged word and exactly two markers. Speed,
title, progress and controls are hidden. Speeds remain 50/75/100/125 WPM, default
100 after V1B.1 finalisation. V1B uses paused-only speed feedback; changing speed while playing introduces
no extra refresh solely for notification display/hiding. Chapter-loading/error
frames retain their existing transient status behavior.

## M. Long-word fallback

Try NORMAL first for every token. Check actual ink bounds after ORP translation
against both horizontal safe margins and the fixed vertical word region. If it
does not fit, remeasure LONG, then EXTRA_LONG. Focus and baseline never change;
glyphs are not stretched or squashed.

The 100-word existing V1A test passage uses NORMAL for 99 words and LONG for one.
`extraordinary` and `well-known` use LONG; `characteristically` uses EXTRA_LONG.
This distribution describes this deterministic sample, not all EPUBs.

A pathological token that exceeds even EXTRA_LONG is clipped strictly within
the word region and **pauses** with “Word too wide; use Back to read”. It is not
automatically skipped. Its original text and source offsets remain unchanged;
Back returns to its location in the ordinary EPUB reader. A 200-character all-W
fixture exercises this bounded terminal case. Fully displaying arbitrary
200-byte tokens at this fixed focus with three legible sizes is a physical width
limit, not a reason to reduce normal typography.

## N. Memory impact and diagnostics

No new runtime heap allocation was added to typography or per-word rendering.
`EpdFont` is a pointer wrapper; metrics, placement and callback context are small
stack structs. There is no `std::vector`, `std::function`, dynamic text buffer,
font map node, decompression buffer, or second framebuffer on this path.

ELF DWARF confirms: `RsvpReaderActivity` **9,000 bytes**, previously 8,952;
`RsvpReaderLayout` 56, `Orp` 8, `WordMetrics` 20, `Placement` 32. The activity's
existing checked allocation grows by **48 bytes** for layout/counters. This
activity lifetime is the reason to keep those reusable fields as members rather
than allocate new buffers per presentation. Firmware static RAM remains 57,936
bytes; font assets are flash data, not DRAM glyph caches.

Geometry debug logging is limited to the first 12 drawn tokens per activity,
including paused redraws. `orp_screen_x2` must equal `focus_x2` or one less.
`RSVP2 VISUAL_SUMMARY` reports tier **draw counts**, overflow draw counts, entry/
exit heap and minimum internal heap/largest block. Existing cadence summaries
still report successful playback and chapter changes. No physical before/after
heap run was performed. Heap logs cannot be inferred from static RAM percentage;
the linked shared-SRAM table also includes 67,570 bytes of instruction code.

## O–P. Build and cppcheck

The prescribed UTF-8 environment and installed PlatformIO Python were used:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython -m platformio run -e default
& $pioPython -m platformio check -e default
```

Final build and configured cppcheck both passed; firmware identity is below.
Logs: `.pio/rsvp-v1b-build.log`, `.pio/rsvp-v1b-cppcheck.log`.

## Q. Static review and limitations

- Full-frame FAST still submits through `RsvpDisplayBackend.cpp:7`; the backend
  and experimental SDK code were not edited. Only the existing framebuffer is
  written. No window, partial, DSP, LUT, PLL or voltage path was added.
- Reader controls, source spooling, chapter crossing, successful-refresh position
  commits, sleep inhibition, progress persistence and orientation/render/inversion
  restoration remain in `RsvpReaderActivity`. `RsvpEpubSource` retains the V1A
  HalFile RAII fix. Normal-reader code was not edited by V1B.
- Native glyph/interval tables use aligned fields. Bitmap bytes are not cast to
  wider pointers. There is no ISR or task-lifecycle change.
- C++ GTest cases were added but could not be executed: this host has no prepared
  host C++/GTest toolchain. Actual firmware code compiled in the target build;
  the separate Python asset/raster checks ran successfully.
- The sanctioned `./bin/clang-format-fix -g` was attempted, but reports
  `'clang-format' not found in current environment`. No tool was installed or
  raw formatter invoked. Formatting was reviewed manually; `git diff --check`
  passes. Untracked additions were also inspected directly.
- The embedded face is a Latin subset. Unsupported scripts/characters use the
  replacement glyph; this phase does not add RTL shaping or CJK RSVP typography.
  Extended Latin pairs do not have baked pair kerning. Normal Latin ligatures are
  disabled so the selected lexical character has its own glyph.
- Very long translations are bounded by cell clips; their translations/layout
  need native-language hardware review. English bounds were checked.
- Hardware refresh comfort, ghosting, heap trends and control ergonomics still
  need the physical sequence below. No refresh-speed or heap-saving claim is made.
- Flash partition headroom is limited; see the final build report before adding
  future font tiers/coverage.

## R. Exact physical test sequence

1. Have the human tester install the final firmware using their normal process.
   This implementation session did not flash the device.
2. Generate the separate fixture with
   `& $pioPython test/rsvp/make_v1b_fixture.py`. Copy
   `.pio/rsvp-v1b-acceptance.epub` to the SD book folder. Existing books/caches do
   not require deletion; the fixture has a distinct identifier/name.
3. Enable normal debug serial monitoring. Open the first fixture page in the
   ordinary reader and launch RSVP. Check the first word starts at the current
   visible-text position rather than resetting a real book to its start.
4. While paused, step the initial examples: `I an the reading morning, something
   extraordinary "perhaps" well-known characteristically Hello, don't (perhaps)
   word. café élan AV gyp`. Confirm natural punctuation/letter style, large
   normal text, two fixed triangle tips, fixed baseline, smaller discrete tiers
   only where required, and clean Previous/Next controls.
5. Check `RSVP2 LAYOUT`: focus=306, baseline=311, tips=135/365 and controls=408 for
   the original V1B profile. The approved V1B.1 physical-controls layout has focus
   and marker X=311, baseline=311 and tips=135/365. Per-word `GEOMETRY` logging
   was removed during finalisation; use the raster report and entry LAYOUT log.
6. Play at default 100 WPM. Confirm the screen contains only word and markers.
   Pause/resume repeatedly; the word position and baseline must not move.
7. Use bottom-edge Slower through 75 and 50; use top-edge Faster back to 125. Confirm the requested speed
   is visible on pause and hidden during playback; verify no notification-only
   refresh occurs. Test mapped front buttons if a custom mapping is installed.
8. Step Previous/Next while paused and from playing. Cross numbered T0400 through
   the empty spine item to T0401; check continued playback, offsets and no crash.
9. At chapter two's final 200-W token, verify bounded word drawing, automatic
   pause and the overflow message. Back must return to that token's book location.
10. Repeat launch/read/Back from each of the four original reader orientations.
    RSVP intentionally forces landscape CCW; Back must restore the original
    orientation, inversion/render settings and approximate content position.
11. Run a representative real EPUB for at least 200 presented words, including a
    chapter transition. Compare `RSVP1 SUMMARY` and `RSVP2 VISUAL_SUMMARY` heap/
    largest-block values across three launch/play/pause/Back runs from the same
    reader page. Font/cache warming in the normal reader can affect entry values.
    Require no sustained downward trend or reset; investigate free internal heap
    below the development guide's 50 KB target. The new typography should not
    introduce cache growth. Retain logs, rather than claim a measured saving.
12. Confirm idle paused sleep and playback sleep inhibition, then ordinary EPUB
    reading/navigation after exit. Record firmware hash, book, controller/profile,
    orientation, heap summaries and a short video of consecutive words.

## S. Raster checks and mock metrics

Run with the existing Pillow Python:

```powershell
& 'C:\ai\ComfyUI_windows_portable\python_embeded\python.exe' test/rsvp/render_visual_fixture.py
```

Artifacts: `.pio/rsvp-v1b-visuals.png`, `.pio/rsvp-v1b-geometry.json` and
`.pio/rsvp-v1b-visual-check.log`. The inspected contact sheet uses actual embedded
word bitmaps and illustrates paused zones; caption/UI rendering is a host mock,
not an on-device screenshot. It is not a refresh/ghosting simulation.

| Token | Tier px | Word origin X | Ink left/right exclusive | ORP screen X |
| --- | ---: | ---: | --- | ---: |
| I | 132 | 284 | 289/323 | 306 |
| an | 132 | 191 | 197/336 | 306 |
| the | 132 | 218 | 220/414 | 306 |
| reading | 132 | 145 | 156/612 | 305.5 |
| morning, | 132 | 71 | 82/633 | 306 |
| something | 132 | 102 | 109/757 | 306 |
| extraordinary | 96 | 145 | 150/757 | 306 |
| "perhaps" | 132 | 69 | 78/674 | 306 |
| well-known | 96 | 164 | 165/669 | 305.5 |
| characteristically | 64 | 158 | 162/661 | 306 |

All baselines are 311. Checks passed for 1,467 generated glyphs, byte/ink bounds,
ASCII kerning, the 12 golden examples, discrete fallback, extreme overflow,
English control widths and the 100-word representative passage.

## T. Recommendation for V1C

Complete and record the physical V1B test first. If visual invariance, control
layout and heap stability pass, V1C can address punctuation timing using the
unchanged source offsets/cadence boundary. Keep the accepted focal geometry and
full-frame FAST backend. V1C was not started.

## Final verification

- Final `default` build: **SUCCESS**, 40.747 seconds, no compiler warnings/errors.
- Final configured `default` cppcheck: **PASSED**, 46.710 seconds,
  **No defects found**. No extra suppressions were introduced.
- Asset/raster verification and acceptance EPUB ZIP integrity: passed.
- Firmware: `.pio/build/default/firmware.bin`, **6,202,096 bytes**.
- SHA-256: `69c01a80f92cbbb440c2fd3be52ef82c29800f3ff4b1e9668680e45d40c64ffd`.
- Factory image, ELF and map are beside the firmware.
- PlatformIO RAM: **57,936 / 327,680 bytes**, 17.7%.
- PlatformIO flash: **6,188,027 / 6,553,600 bytes**, 94.4%; partition headroom
  **365,573 bytes**. The prior crash-fixed V1A binary was 5,667,568 bytes;
  this binary grows by 534,528 bytes, predominantly the 531,058-byte font assets.
- Static review complete. Physical testing remains the next step. Host GTest
  execution and automatic formatting limitations are recorded in section Q.
- No flash, commit, push or V1C implementation was performed.

READY FOR PHYSICAL V1B TEST
