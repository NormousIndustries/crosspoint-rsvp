# XTEINK X3 RSVP V1C.2 — Automatic Sentence Context

Status on 11 October 2026: **V1C.2 BUILT AND FLASHED — READY FOR PHYSICAL TEST.**

Following the user's explicit “Flash V1C.2 to COM5” instruction, the validated
application was uploaded at `0x10000` and esptool verified its data hash. The
X3 booted to Home without an immediate crash. COM5 was released, and the
temporary local upload override was removed. See the upload/boot evidence below.

V1C.2 is not physically accepted. Do not proceed to V1C.3.

## Baseline and scope

The existing working tree contains accepted V1C.1 Smart Timing changes and
uncommitted SDK experiments. Neither was discarded. No compiler/framework/SDK
upgrade, SDK reset, commit, tag, push, or global button mapping change occurred.
The branch remains `feature/rsvp-v0.1-display-test-v1.6.5` at
`2abf45771b9e5635e3d834dffbd6fe833d3bc45b`; SDK HEAD remains
`111fdcc7f0176c3ee38391a160ee296bf492dbd8` with its existing experiments intact.

`build/rsvp-v1c2/before-sources.json`, `sdk-before.patch`, and `repo-before.patch`
record the working state. The preservation contract checks unchanged SDK, HAL,
parser, font assets, Smart Timing, cadence, settings, mapping, framebuffer
renderer, and FAST backend. The word/marker rendering and six hint layout
functions retain their original logic; paused status uses a new translation.

## Files changed for V1C.2

| File | Purpose |
| --- | --- |
| `lib/Rsvp/RsvpContext.h` | Bounded positional extraction and measured excerpt layout |
| `lib/Rsvp/RsvpToken.h` | SD record address and previous-record link |
| `lib/Rsvp/RsvpEpubSource.h`, `.cpp` | Complete chapter token spools, independent context handle, direct initial seek, cleanup |
| `src/activities/reader/RsvpReaderActivity.h`, `.cpp` | Automatic paused context, selection cache, timing/memory logs, final-word replay |
| `src/components/RsvpReaderUi.h`, `.cpp` | Context geometry, existing-font rendering, positional underline |
| `src/components/UITheme.h` | Context rendering through GUI/UITheme |
| `lib/I18n/translations/english.yaml` | Translatable status format and ellipsis; other languages fall back to English |
| `test/rsvp/ContextHarness.cpp` | Extraction, bounds, clusters, actual-font layout, raster fixtures |
| `test/rsvp/ActivityContextHarness.cpp` | Actual activity source with deterministic hardware substitutes |
| `test/rsvp/run_context_tests.py` | Pinned RV32 execution and layout export |
| `test/rsvp/check_context_contract.py` | Source integration/preservation contracts |
| `test/rsvp/render_context_fixture.py` | Embedded-font bitmap previews and collisions |
| `test/rsvp/render_physical_controls.py` | Optional status parameter; existing regression default preserved |
| `test/rsvp/validate_context.py`, `CMakeLists.txt` | Check orchestration and native harness registration |
| This document | Handoff and physical acceptance checklist |

Other files already modified at session start are the V1C.1/SDK working state.
Generated I18n files and ignored build artifacts are not manually edited or staged.

## Context architecture and source identity

The existing `ChapterHtmlSlimParser` visible-text sink supplies decoded text,
visible-codepoint offsets, inline fragment continuity, and structural word,
paragraph, and chapter boundaries. `RsvpTokenizer` remains unchanged.

`RsvpEpubSource.cpp:65` writes all decoded chapter tokens to
`<book cache>/rsvp-v1c2-<spineIndex>.tmp`, including records before the initial
reading offset. Each record contains a 16-byte header (start/end offsets,
length, timing class, structural flags, previous-record address) and up to
200 text bytes. This is an ephemeral RSVP spool, not a versioned EPUB page
layout cache. No book/section cache version change is needed.

While writing, `playbackStart` records the first token whose exclusive end
exceeds the requested initial offset. Once parsing finishes, playback seeks
directly there. It does not scan the chapter again to locate the initial word.

Each selected token carries `(spineIndex, startOffset, endOffset, fileOffset)`.
`RsvpContext.h:46` verifies the selected record against that identity, follows
previous-record links, and reads forward with a separate `contextFile`.
It never searches for matching text or seeks the playback `tokenFile`.
Repeated words therefore retain distinct identities.

The selected word is the word committed to recent history after successful
display completion (`RsvpReaderActivity.cpp:313`). A staged token may already
have been consumed from the SD stream but becomes selected only after its
display succeeds. The next unread token remains represented by the playback
handle; forward history replay consumes neither that handle nor staged data.

Context is cached by selected spine/offset. Paused speed changes rerender the
status without rereading context. Ordinary navigation reuses the context file
handle; crossing chapters reopens it. Earlier chapter spools remain available
for the existing 32-token recent-history replay and are removed on exit.
Both member HalFiles are released under the HAL lifecycle before spool removal
(`RsvpEpubSource.cpp:197`). No raw SdFat access is introduced.

## Sentence boundaries and fallback

Sentence ends use the accepted Smart Timing `Sentence` class; paragraph/chapter
flags and the `Paragraph` class also terminate context. Decimals, recognized
abbreviations and dotted initials retain the existing timing classification.
Quotation wrappers, contractions, ASCII/Unicode ellipses and supported Unicode
punctuation use those same rules. No new sentence-classification parser is added.

Backward lookup stops at the prior sentence/paragraph boundary. Forward lookup
includes the selected sentence-ending token before stopping. Chapter start/end
and EOF terminate safely. Link addresses must decrease backward; record fields
and lengths retain validation and aligned reads.

If context lookup fails, the source returns the selected word in a bounded
fallback buffer. The activity logs `Context fallback`, retains selection and
controls, and caches that fallback until selection changes. It does not fail
the playback source or repeatedly refresh while idle. Existing EPUB parsing
failure handling is preserved; context extraction does not reparse XHTML.

## Highlight and excerpt layout

`RsvpReaderUi.cpp:80` measures words using the production renderer and the
existing Noto Sans 16 regular font. It draws normal-weight sentence text and
a two-pixel underline beneath exactly the selected positional span, including
its adjacent punctuation. There is no ORP character styling in context.

The complete recovered sentence/window is tried first. If it exceeds measured
line capacity, whole words are removed from the side with more words around
the selection. Leading/trailing translated ellipses indicate omitted text.
The selected span always remains inside the displayed range.

A selected token too wide for one line is shortened at UTF-8/cluster boundaries
and receives an ellipsis within its underline. Tests cover Latin combining
marks, emoji ZWJ sequences, variation selectors, modifiers and flag pairs.
This helper is not a full Unicode UAX #29 segmentation engine. Unsupported
scripts/complex grapheme sequences remain a limitation of this bounded path
and the existing font/rendering infrastructure.

All rendering is clipped to the context rectangle. Font size is fixed;
sentences are not squeezed into progressively smaller type.

## Font and screen geometry

Geometry comes from oriented screen dimensions and bezel insets, through
UITheme, rather than assuming an 800 × 480 panel. Actual X3 landscape CCW:

| Metric | Pixels |
| --- | --- |
| Screen | 792 × 528 |
| Safe rectangle | x=33, y=27, width=732, height=474 |
| Context rectangle | x=41, y=67, width=464, height=374 |
| Text origin / measured wrapping width | x=45 / 456 |
| Font | Existing Noto Sans 16 regular |
| Font ascender / native line height | 36 / 45 |
| Context line step / maximum rows | 53 / 7 |
| Inter-word gap | 11 for this font (space advance + 2) |
| Selected underline | row top + 50 and + 51 |
| Status cell | y=453, height=24 |
| Faster / Slower cells | y=27 / y=477, height=24 |
| Right-edge physical anchors | (783,107), (783,211), (783,316), (783,420) |

The right third remains reserved for the verified front-button hints. The
context block is vertically centered within its own rectangle. In PLAYING,
the approved large word, fixed ORP, font tiers and both triangles are retained.
There are no context, status or hint frames during normal word presentation.

## Pause/resume and input

1. Entry loads the source and presents the initial selection in paused context.
2. Pause queues `Current`, stopping timed advancement before context rendering.
3. Resume queues `Current`, redisplays that same token in the large-word view,
   and calls `cadence.completed` with that token's unchanged Smart Timing class.
4. Only a subsequent due interval queues `TimedNext`.
5. Manual Next/Previous pauses, selects a successfully displayed target, and
   updates context/source identity. Existing chapter-history replay is retained.
6. The final word can be resumed once; after its normal interval EOF returns to
   paused context at that same word. It does not loop or skip the last word.
7. Back returns the selected spine/start offset to the EPUB reader and retains
   the existing approximate page mapping and progress persistence.

The six verified controls remain top Faster, bottom Slower, then right-edge
Next, Previous, Play/Pause, Back. User front-button remaps still move hints
with their actions. Speeds remain 50/75/100/125, default 100 WPM. Paused status
uses `%s · %u WPM`; error/loading/EOF statuses use existing translated strings.

Paused idle performs no periodic refresh or extraction. Context draws use the
unchanged production full-frame FAST submission path. The existing loading
screen/chapter suspension and auto-sleep rules are preserved.

## Bounded memory and performance evidence

| Storage | Bound / mechanism |
| --- | --- |
| Context text | 2,048 bytes, reusable activity member |
| Context words | At most 49; up to 24 before/after selection |
| Preceding text budget | 1,024 bytes to reserve room for selected/following text |
| `Context` object | 2,152 bytes on RV32 |
| `ContextPlacement` | 364 bytes, including reusable 201-byte shortening scratch |
| Token text | Existing 200-byte UTF-8-safe token limit |
| New record identity | 8 bytes per token; history remains 32 tokens |
| Source scratch | One reusable token; no chapter-sized RAM buffer |
| Framebuffer | Existing single framebuffer; no additional screen buffer |

Compiler DWARF measurements in `build/rsvp-v1c2/type-sizes.json` show the
activity growing from 9,416 to 12,520 bytes (**3,104 additional bytes in its
existing heap allocation**). Source size grows from 1,632 to 1,912 bytes,
history from 7,052 to 7,308, and each token from 220 to 228. These are compiled
object sizes, not observed device heap consumption; allocator overhead,
path strings, HalFile internals and font caches are additional.

Large scratch data is stored in activity/source members, not local function
arrays. There is no new per-word `new`, `malloc`, vector or string construction
in context extraction or layout. The activity already uses the checked
`makeUniqueNoThrow` allocation at `EpubReaderActivity.cpp:827`.

An additional HalFile owns its existing internal heap implementation because
the HAL's mutex-protected ownership is required; a raw/static SdFat handle
would bypass that protection. That handle is reused within a chapter. Small
path-string allocations occur on chapter open/cleanup, outside regular word
presentation. Existing parser/CSS allocations remain unchanged.

Reusing Noto Sans adds no font asset. Its existing decompressor may grow the
shared hot-group buffer: this font's largest uncompressed group is 34,821
bytes; ordinary ASCII uses the 9,181-byte group. This is an existing font-cache
allocation mechanism, not an additional framebuffer. Device heap testing is
still required; no observed heap saving is claimed.

The synthetic long-sentence test checks bounded record reads (at most 76 for
its 49-word window). Context does no display submissions while searching.
`RSVP2 CONTEXT` logs report `construct_us`, `transition_us`, heap, system
minimum heap and largest internal free block. `construct_us` covers context
lookup/layout/frame construction; `transition_us` adds the blocking display
submission. It measures frame service, excluding earlier input lock/queue wait.
Entry/exit and session-minimum heap/block logs are retained.

Actual device context latency and RSVP lifecycle heap measurements are
**pending physical testing**. The successful boot observation establishes
startup behavior only; it does not establish RSVP heap stability or readability.

## Automated validation

The native CMake/GTest dependencies are unavailable on this host. The
established deterministic alternative uses the pinned ESP RV32 compiler and
Unicorn 2.1.4. Production `RsvpContext`, tokenizer, cadence, history, `EpdFont`
and UTF-8 implementation execute in the emulator. The actual activity header
and `.cpp` are compiled into an ignored translation unit with deterministic
HAL, clock, storage, UI and single-thread atomic substitutes; no test wiring
is compiled into firmware. Concurrent FreeRTOS execution and real panel/SD
timings are not simulated. XHTML cases exercise decoded sink fragments and
the unchanged parser's source integration contract, not a full host EPUB parse.

All checks passed:

- Normal sentences, repeated-word positional highlight, quoted dialogue,
  apostrophes, decimals, abbreviations, ellipses, inline fragments/entities.
- UTF-8/cluster-safe excerpts, long sentences, paragraph/chapter edges, EOF,
  empty/bad/cyclic records, bounded buffers and correct source coordinates.
- Actual activity: normal/deadline pause, sentence-ending pause, same-word
  resume and timing, Next/Previous and chapter replay, no hidden context
  advance, no idle refreshes, no playing context/hints, speeds, queued controls,
  malformed-context fallback, failed display commit and EPUB return offsets.
- Existing Smart Timing/history/cadence regressions and existing typography
  and physical-control raster regressions.
- Pinned `platformio run -e default`: SUCCESS, 72.59 seconds; no compiler warning
  found. Static reported RAM remains 57,936 bytes.
- Configured `platformio check -e default`: PASSED, no defects, 45.95 seconds.
- Sanctioned `./bin/clang-format-fix -g`: PASS with workspace-local version
  21.1.8, including new untracked C++ in the wrapper's inventory; unrelated
  pre-existing source edits restored byte-for-byte. Formatting followed the
  successful build and changed no implementation.

Pinned test/format dependencies are isolated under ignored
`build/rsvp-v1c2/{python-deps,format-deps}`. Firmware toolchain packages were
not upgraded. Logs: `build.log`, `cppcheck.log`, `context-tests.log`,
`contracts.log`, `context-raster.log`, `visual-raster.log`, `physical-raster.log`,
and `formatter.log` in `build/rsvp-v1c2/`.

Reproduce host checks with the pinned UTF-8 Python environment:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython test/rsvp/validate_context.py
& $pioPython test/rsvp/validate_context.py --cppcheck
```

## Raster previews

Actual compressed Noto Sans bitmaps are decompressed and rendered using the
firmware's BW non-white ink rule, with the layouts exported from production
RV32 `EpdFont` measurement. No substitute host font is used for context.
All six hints, status, text and selected underline pass bounds/collision
checks for 24 valid remaps × four speeds. The contact sheet was visually inspected.

- `build/rsvp-v1c2/context-short.png`
- `build/rsvp-v1c2/context-long.png`
- `build/rsvp-v1c2/context-dialogue.png`
- `build/rsvp-v1c2/context-repeated.png`
- `build/rsvp-v1c2/context-excerpt.png`
- `build/rsvp-v1c2/context-contact-sheet.png`
- `build/rsvp-v1c2/context-layouts.json`
- `build/rsvp-v1c2/context-raster-report.json`

## Firmware, COM5 and guarded upload

| Artifact | Value |
| --- | --- |
| Validated application | `build/rsvp-v1c2/validated.bin` |
| Application binary size | 6,194,224 bytes |
| Application partition | 6,553,600 bytes (`0x640000`) |
| Remaining headroom | **359,376 bytes** |
| Increase from accepted V1C.1 | 4,272 bytes |
| SHA-256 | `124335260af14c2a8d8af5192515477937c656ba31092087ad0a94c75006b63b` |
| Flash target | app0, `0x10000` only |
| COM5 identity | VID/PID `303A:1001`, serial `70:AF:09:5E:AA:54` |
| Preflight partition read | Matches compiled partition table |
| Preflight OTA read | Identical to accepted V1C.1 app0 metadata |
| Upload / hash verification | **SUCCESS; esptool “Hash of data verified.”** |
| Boot | **X3 detected; Boot → Home at 2.525 seconds; no immediate crash** |
| Serial port | **COM5 released; temporary local override removed** |

Preflight completed successfully. Evidence: `artifacts.json`,
`read-partitions.log`, `read-otadata.log`, `device-partitions.bin`, and
`device-otadata.bin`. It read metadata without rewriting it.

`build/rsvp-v1c2/flash-and-boot.py` uses the pinned UTF-8
PlatformIO environment and a temporary, gitignored local override. The upload
guard excludes every auxiliary bootloader/partition/OTA image and verifies
application offset, source hashes, binary hash and size before upload. The
`nobuild` upload target preserves the already validated binary after formatting.
The guard restores that preserved binary if PlatformIO clears its working build
directory when the temporary configuration changes. It supplies the application
offset from the verified app0 partition because `nobuild` skips the size-check
action that normally sets that environment value. Conflicting offsets are rejected.
The script verifies `[SUCCESS]` and `Hash of data verified`, then captures
20 seconds of X3 boot logs, checks for immediate crashes, and closes COM5
in `finally`. Its local override is also removed in `finally`.

**Upload PASS:** 6,194,224 application bytes (3,964,805 compressed) written at
`0x00010000` in 34.7 seconds; PlatformIO SUCCESS in 42.63 seconds. Esptool
verified the written data hash. The preserved/working application SHA-256
remained `124335260af14c2a8d8af5192515477937c656ba31092087ad0a94c75006b63b`.
`upload-scope.json` records only the application image; no bootloader, partition,
OTA metadata, NVS or eFuse write was included. No erase-all action ran.

**Boot observation PASS:** 1,907 bytes captured at 115200 over 20 seconds.
The X3, SD card, saved settings, two recent books and Lyra theme loaded;
Boot transitioned to Home at 2.525 seconds. At 10.021 seconds, free heap was
126,808 bytes, minimum free heap 126,256, and largest block 98,292. No immediate
panic, assertion, corrupt heap or reset loop appeared. The existing early
Wire I²C lock/null-buffer probe warnings also appeared, as in the accepted
baseline; initialization then completed. They were not changed or claimed fixed.
No physical button or RSVP playback test was performed automatically.

Evidence: `build/rsvp-v1c2/{upload,reboot}.log`, `upload-scope.json`, `boot.txt`,
`boot-result.json`, `preflight.log` and `artifacts.json`.

Two initial PlatformIO upload attempts stopped before the uploader ran:
configuration switching cleared the working binary, then `nobuild` lacked an
application-offset value. The guard corrections above resolved both without
rebuilding or changing the validated firmware. Their logs are preserved as
`upload-first-attempt.log` and `upload-second-attempt.log`. The earlier automatic
approval rejection was resolved by the user's direct chat instruction.

## Rollback

- Accepted V1C.1: `build/rsvp-v1c2/v1c1-accepted.bin`, 6,189,952 bytes.
  SHA-256 `cc586ab953805efd37ebc33d38082a6df7049a4609b29a9b409c9e2b822f5c15`.
- Existing V1B.1: `build/rsvp-v1c1/v1b1-accepted.bin`.
  SHA-256 `8417d233a68cb60252e3e2449f9db6a9ae2003226c0da74376c3b71572d658fd`.
- `v1c1-before.elf` preserves the pre-development ELF; `validated.elf`
  preserves the V1C.2 build for inspection. The rollback binary hash is the
  authoritative accepted-firmware identity.

## Known limitations and physical acceptance checklist

The bounded extraction cap may excerpt a sentence before screen layout if it
exceeds 49 words or the byte budget. Existing token truncation at 200 bytes is
retained. Smart Timing's abbreviation list is heuristic; multilingual sentence
segmentation, full UAX #29 grapheme segmentation and sentence-wide bidirectional
reordering are not added. Recent Previous remains limited to the available
32-token history, including chapter replay. Reboot/power loss can leave
ephemeral spool files; visited chapters overwrite their spool on a new session.

The upload and boot checks passed. Now test on the actual X3:

- [ ] Entry is paused in context; no large word or registration triangles.
- [ ] Short sentences and dialogue are comfortably readable.
- [ ] Only the selected repeated-word occurrence is underlined.
- [ ] Commas, quotes, brackets, apostrophes, entities and inline formatting keep
      correct text/highlight/source offsets.
- [ ] Long sentences excerpt around selection with visible omission ellipses;
      text and underline never collide with any hint or leave the screen.
- [ ] All six verified physical buttons and hints remain correctly aligned.
- [ ] Next/Previous updates context through sentence and available chapter edges.
- [ ] Pause during normal and sentence-ending words keeps N selected.
- [ ] Play redisplays N, holds its normal/Smart Timing interval, then presents N+1.
- [ ] Playing retains the accepted typography, ORP, triangles and FAST backend,
      with no context, status or hint chrome.
- [ ] Speeds 50/75/100/125 work paused and playing; default remains 100.
- [ ] Final-word resume and EOF remain stable; malformed context retains controls.
- [ ] Paused idle produces no refreshes; sleep/power behavior remains normal.
- [ ] Back restores the expected approximate EPUB page near selected source offset.
- [ ] Serial `RSVP2 CONTEXT` logs show construction/transition times; record worst
      and typical values for short/long/Unicode/chapter-crossing contexts.
- [ ] Record entry/exit heap, session/system minimum and largest free block;
      free heap stays above 50 KB, with no accumulating loss across repeated sessions.
- [ ] No parser leaks, unclosed HalFiles, assertions, watchdog resets or crashes
      during a sustained reading/navigation session.

Only the user can accept these physical checks. Upload and startup are verified;
V1C.2 physical acceptance is still pending. Stop before V1C.3.
