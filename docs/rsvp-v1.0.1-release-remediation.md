# CrossPoint RSVP v1.0.1 release remediation

Prepared 12 October 2026, Australia/Sydney.

**V1.0.1 RELEASE REMEDIATION BLOCKED**

The engineering candidate is path-sanitized, builds from pinned checkouts plus
hash-verified patches, passes the automated regressions, and reproduces exactly
in two independently compiled source directories. The complete accepted-image
comparison remains unproven. The public distribution licensing/source audit is
also incomplete. These conditions have not been waived; hardware acceptance and
publication remain closed.

No device connection, flash, reset, commit, tag, push, or publication was performed.
The original development sources and accepted application remain intact. The
accepted private firmware must remain separately archived and unpublished.

## Artifacts and measured resources

Evidence base: `rsvp-release.local/remediation-20261012/` (Git-ignored).
Previous evidence: `rsvp-release.local/preparation-20261012/` (unchanged).

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Preserved accepted application | 6,456,848 | `f25f9e60c8647694a6a155a45d9fa0639cb7907e86b6955f58eb724fd6c64467` |
| Previous first audit rebuild | 6,458,272 | `58f1df42a1065f66afe0273d97e134b87b831c3d6d0f9f8ef6cf4927086f4a1a` |
| Clean A application | 6,456,128 | `4fcfdabfdc01a8651b916ae0111665b4a7f070dfdfc7f13b566a4ccc3c827a43` |
| Clean B application | 6,456,128 | `4fcfdabfdc01a8651b916ae0111665b4a7f070dfdfc7f13b566a4ccc3c827a43` |
| Clean A/B ELF, private evidence only | 75,008,232 | `0cb5e566691eef04b4deb67eb85ebf0773b0b84ed9da887147c6a7c156ca701f` |
| Clean A/B partition binary, evidence only | 3,072 | `bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce` |

Application partition: **6,553,600 bytes (`0x640000`)**, app0 at **`0x10000`**.
Candidate flash headroom: **97,472 bytes**. Static linker RAM: **57,936 bytes**,
unchanged from the accepted build. This is not a measurement of peak heap or stack.
The candidate is an ESP32-C3, DIO/80 MHz/16 MB **application-only** image.

The reviewed candidate is held at
`private-staging/CrossPoint-RSVP-v1.0.1-X3-app.bin`, with `SHA256SUMS.txt`.
It is an engineering hold, not an approved release or hardware-accepted firmware.
The independently compiled application/ELF/map/partition sets are under
`clean-a/` and `clean-b/`. Private ELFs, raw logs, disassemblies and backups are
excluded from staging.

## Original blocker 1: personal paths

All 32 original personal-path strings were traced to pinned toolchain dependencies:
31 Arduino source files and one ESP-IDF HAL header. Their complete paths and
original offsets remain only in the existing private audit report.
`accepted-path-origins.json` records dependency-relative source names, file hashes
and macro invocation lines without home directories.

The 31 Arduino strings arise from logging macros. In the installed pinned package,
`packages/framework-arduinoespressif32/cores/esp32/esp32-hal-log.h:95–97` expands
`pathToFileName(__FILE__)`. The runtime basename helper does not prevent the complete
literal from being stored in flash. The remaining string is from `HAL_ASSERT` in
`packages/framework-arduinoespressif32-libs/esp32c3/include/hal/esp32c3/include/hal/i2c_ll.h:115,401`.
There was no need to remove logging or manually edit a compiled image.

The isolated `scripts/release_reproducible.py:8–27` applies supported
`-ffile-prefix-map`, `-fmacro-prefix-map`, and `-fdebug-prefix-map` options to the
project, PlatformIO core, and builder home prefixes. Stable destinations are
`/src/crosspoint`, `/tools/platformio`, and `/builder`. Mapping precedence makes
some dependency strings appear beneath `/builder/.platformio`; that prefix is
generic and contains no local account or machine identity.

GCC **14.2.0, esp-14.2.0_20260121** compiled a probe with all three options and
verified remapped macro/debug strings. The verbose candidate commands show the
maps on application, Arduino-library, and SDK translation units. Compiler flags
apply to newly compiled units; prebuilt toolchain archives are inventoried separately.

Both clean application scans pass:

- No original Windows home/repository prefix bytes, in UTF-8 or either UTF-16 endian order.
- No local username path segments.
- No UTF-16 absolute-path string findings.
- The 32 diagnostic path literals now use generic prefixes; diagnostic basenames survive.
- Five broad scanner matches are explicitly reviewed: two KOReader HTTP routes
  (`lib/KOReaderSync/KOReaderSyncClient.cpp:58,91`) and three short accidental
  drive-like sequences inside accepted, byte-identical Noto Serif bitmap arrays.
  No match is silently suppressed; offsets and bitmap symbols are recorded.

See `clean-a-scan.json`, `clean-b-scan.json`, and the private detailed scans.
These scans address build metadata and recognizable personal-path content; they
do not establish a general absence of every possible encoded secret.

## Original blocker 2: interleaved SDK experiments

The pinned SDK is `111fdcc7f0176c3ee38391a160ee296bf492dbd8`.
Classification is based on call sites and state writers, not just filenames.
SDK paths below are relative to `freeink-sdk/libs/display/FreeInkDisplay/`.

| Modified SDK area | Class | Release disposition and dependency |
| --- | --- | --- |
| `include/DisplayMeasurement.h` capture and `include/FreeInkDisplay.h` / `src/FreeInkDisplay.cpp` measured wrapper | A, D | Keep caller-owned measurement. `lib/Rsvp/RsvpDisplayBackend.cpp:5–12` requires refresh count, timing and error flags; `lib/hal/HalDisplay.cpp:82–86` calls the SDK wrapper at original SDK lines 627–631. |
| `src/bus/EpdBus.h/.cpp` measurement lifecycle, `recordWait`, wait-result tracking | A, B, D | Keep original full-frame timing and error bookkeeping, including all original waits, thresholds, ISR/semaphore behavior and hooks. Original lines 111–121, 381–384, 452–472 feed accepted failure semantics. |
| `src/driver/Uc8253X3Driver.cpp` and `Uc8279Driver.cpp` FAST/BUSY measurement and completion guards | A, B, D | Keep the accepted guards at original lines 274–281 / 209–216 and their resync behavior. They also protect ordinary CrossPoint display completion. No LUT or waveform edit. |
| Window result/rectangle types, facade window API and `PanelDriver.h` virtual experiment method | C | Remove the added experimental surface. Virtual references made the driver implementations survive linking despite no production caller. |
| `EpdBus` region upload, window register setup, window-only BUSY capture/assertion helper | C | Remove. They are required only by the added window API. |
| Both drivers' window methods, pending state, restoration helpers and normal-start restoration calls | C | Remove together. Original UC8253 lines 338–363 and UC8279 lines 251–272 show the flag is set true only by the experimental entry point. Production never enters that state. |
| `test/host/pro_stubs/EpdBus.h`, `run_pro.py`, `run_uc8253_power.py`, `run_uc8279.py` | D | Retain measurement compilation adaptations; remove window-only stub methods. Existing success-only BUSY substitutions are not used as the failure-equivalence proof. |
| Retained `postSyncBusyMask` member in the capture structure | D, explicitly accounted | It has no release writer; it remains to preserve the accepted measurement structure layout. It cannot issue window operations. |
| Unrelated or uncertain SDK changes | E | No additional change was identified in the original 13 modified tracked files plus the new measurement header. |

The reader's experiment boundary was also removed only in the isolated source:
`lib/hal/HalDisplay.h:56–58` and `.cpp:97–105`, plus the unused renderer adapter
at `lib/GfxRenderer/GfxRenderer.h:441` / `.cpp:446–454` (original line references).
There are no production callers of that adapter. Existing upstream grayscale
window APIs, LUTs and panel-memory utilities remain.

The actual accepted and production-only EpdBus/UC8253/UC8279 code was executed
on RV32 with deterministic SPI, BUSY, clock and FreeRTOS substitutes. **48 cases**
cover cold full, warm FAST, half, power-off/wake, resync, missing BUSY, simulated
30-second timeout, recovery full, ISR waits, and slice-hook polling. Checked SPI
transcript hashes, uploaded-byte counts, commands, refreshes, OLD/NEW counts,
failure flags and balanced hooks match. The tests assert no OLD synchronization
after failed refresh and successful recovery. See `sdk-equivalence.json` and
`sdk-accepted-traces.json` / `sdk-production-traces.json`.

The harness does not establish electrical or concurrent RTOS equivalence. No
firmware heap allocation, framebuffer, task, or font asset was added by this split.
The accepted full-frame FAST backend and its submit/commit logic are unchanged.

## Clean source architecture

| Input | Exact identifier |
| --- | --- |
| Reader base | `75a27e679e397e96e4a2ceea987f21103fd20986` |
| SDK base | `111fdcc7f0176c3ee38391a160ee296bf492dbd8` |
| Lucide submodule | `c81680e066f45b640743ca78ae36cdedda3f0318` |
| `0001-sdk-production-measurement.patch` SHA-256 | `095ce90cc9f1a73e6a03a4c37196a25c5233aed46d2c316032eede36bf68524b` |
| `0002-reader-v1.0.1-release.patch` SHA-256 | `ad213a7ab56b3485d90556efe422448f7e886a7c4e8413a58b0b4a6ddeea13fd` |

The SDK patch is applied inside the pinned SDK; the reader patch is applied at
the pinned reader root. Submodule pointers stay unchanged. This is a documented
patch-series arrangement, not a claim that the upstream gitlink contains the fixes.
Patches, manifest and instructions are in `source-series/` and private staging.

The clean reconstruction uses only base Git objects, both patches, and explicit
UTF-8 text CRLF-to-LF normalization. All **6,169** manifest source hashes match
the independent reconstructed checkout. The exact reader, SDK and Lucide commits
were also fetched into separate bare databases from their recorded remote URLs;
`remote-source-verification.json` records the commit/tree identifiers. Dirty SDK
files and experiments are not needed to acquire the release source state.

The original **1,446** upload-manifest sources were compared against the candidate.
Differences are limited to the authorized SDK/experiment boundary, build metadata,
line endings, comments and include ordering. No unexpected source-content change
was found. All **674** C/C++ files in the seven external dependency packages match
the original packages exactly. No RSVP typography, UI geometry, input mapping,
EPUB, cadence, settings or context redesign is introduced.

## Clean-build reproducibility and validation

The environment `rsvp_v1_0_1` retains the accepted default profile's X3/X4 device
flags, LOG_LEVEL=2, serial/USB settings, single buffer and tuned C3 SDK configuration.
It explicitly sets runtime `CROSSPOINT_VERSION` to **1.0.1**. It does not substitute
the different logging/version flags in `gh_release`.

SOURCE_DATE_EPOCH is fixed to **1791662598** (10 October 2026 20:03:18 UTC) before
compilation. GCC emits stable `__DATE__` / `__TIME__` values, addressing the two
clock users at `src/activities/settings/SdFirmwareUpdateActivity.cpp:20` and
Arduino `cores/esp32/chip-debug-report.cpp:215`. HTML gzip already uses `mtime=0`
in `scripts/build_html.py:69–72`; baked font resources are consumed unchanged.

Clean A was cleaned; clean B began with no `.pio` state. SCons object caching was
disabled in the reconstructed source configuration. Both independently compiled
all application/SDK inputs in different source roots and produced identical
application, ELF and partition hashes. Build times: **128.93 s** and **138.71 s**.
The full application hash, including its embedded ELF hash and final digest,
matches; this is not merely equality of executable sections or cache reuse.

Both builds shared the installed pinned/tuned framework and toolchain archives.
`toolchain-archive-inputs.json` hashes those archives. A fresh-machine rebuild of
the tuned framework and cross-host reproducibility were not independently tested.
The inherited ESP application descriptor remains **1.6.5**, project
`crosspoint-rsvp`, date/time **Oct 10 2026 20:03:18**, as in the accepted image;
runtime RSVP branding is 1.0.1. This metadata discrepancy is explicitly retained,
not hidden or patched in the binary.

| Validation | Result |
| --- | --- |
| Two independent clean firmware builds | PASS; full application/ELF/partition hashes match |
| Configured cppcheck on reconstructed clean B | PASS; no defects, 40.93 s |
| Production C++ RV32/Unicorn execution | PASS; activity, tokenizer, context, EpdFont, timing/history/cadence and real ArduinoJson preferences |
| Typography / ORP / geometry | PASS; largest fit, exact edge fallback, marks/UTF-8, overflow, five tiers, 2,445 glyph masks |
| Context, word and six-control rasters | PASS; native resolution, bounds, markers, baseline and 24 mappings |
| Actual SDK success/failure/recovery comparison | PASS; 48 deterministic peripheral-substitute cases |
| Sanctioned formatter wrapper | PASS; isolated copies of all final modified/new non-generated C/C++ files remain byte-identical after the wrapper |
| Diff checks for release code | PASS on final reader/SDK C/C++ patch paths; whole-tree LF normalization also produces non-code EOL differences against some upstream blobs |
| Personal-path scans on clean A/B applications | PASS; zero personal matches, five explicitly reviewed nonpersonal matches |
| Accepted-source / external dependency preservation | PASS within the stated source/token comparison scope |
| Original development sources | PASS; 6,170 initial files rehashed, zero mismatches |

Historical preservation-contract scripts assume an unchanged dirty SDK and old
private snapshots. They are not reported as passing unchanged on this authorized
split. The new manifest/source audit covers the release changes; existing executable
and raster suites were run against the actual candidate application sources.
Emulation does not reproduce real SD latency, electrical panel timing, or task concurrency.

## Original blocker 3: exact binary comparison

The original **1,424-byte** increase is accounted for at image-segment level:

| Region | Accepted | Previous rebuild | Delta |
| --- | ---: | ---: | ---: |
| `.flash.text` / image segment 2 | 2,187,908 | 2,189,324 | +1,416 |
| `.flash.rodata` | 4,167,232 | 4,167,232 | 0 |
| `.iram0.text` | 67,570 | 67,570 | 0 |
| `.dram0.data` | 20,069 | 20,069 | 0 |
| `.dram0.bss` | 37,864 | 37,864 | 0 |
| Final image checksum/alignment/digest area | 36 | 44 | +8 |

All seven image segment sizes except segment 2 match in that original comparison.
See `image-segments.json` for addresses and offsets. The increase cannot be
attributed to embedded path length: the previous rebuild retained those same
32 dependency paths and had unchanged rodata size.

Identified causes and limits:

1. **Link layout and RISC-V call relaxation.** Accepted `EpdBus::cmd` has three
   four-byte `jal` calls to `__digitalWrite`; the rebuilt function uses eight-byte
   `auipc`/`jalr` sequences, explaining its **+12 bytes**. Other call/reference
   encodings change as functions and pooled strings move.
2. **Different weak/template definitions were selected.** DWARF attributes
   15 function symbols to different translation units. `_M_mutate` changes from
   `lib/hal/HalSystem.cpp` (**202 bytes**) to `lib/Utf8/Utf8.cpp` (**142 bytes**).
   The string constructor's C1/C2 aliases change from
   `lib/Serialization/ObfuscationUtils.cpp` to `lib/EpdFont/SdCardFontRegistry.cpp`
   (**118 to 110 bytes**, one shared body). The baseline map identifies Utf8's selected object.
   These are real compiled implementation differences, not timestamp bytes.
   Identical source/header versions do not imply identical chosen machine code.
3. **Data relocation and resources.** The original comparison has 1,770 exact
   named objects and 539 objects whose differences resolve to moved addresses;
   no remaining difference was found among those named objects. All 37 audited
   named bitmap objects, including the five RSVP tiers, are exact. Anonymous
   constant/string pools and duplicate local symbol names are not completely
   covered by this object-name audit.
4. **Metadata.** Independent earlier uncached builds changed two clock strings,
   the embedded ELF SHA-256 and final checksum/digest. Formatting alone changed
   DWARF/ELF hashes and consequently image hashes. Those mechanisms do not explain
   the executable-section increase above. The new fixed epoch and prefix mapping
   eliminate observed clean-to-clean image differences in the two new builds.
5. **Remaining unresolved comparison.** The conservative instruction normalizer
   matches 12,199 named functions in the original comparison but leaves **355**
   function comparisons unresolved. The accepted-to-clean-candidate comparison
   matches 11,974 and leaves **566** unresolved, including intended SDK/API changes,
   changed diagnostic references, template selection and normalization limitations.
   These counts are not defect counts or an equivalence proof. Complete per-input
   instruction/address/alignment accounting has not been established. Original
   accepted object files and linker map/link command are not present in the
   preserved accepted archive; only its matching ELF/application are available.

The unresolved function lists and size deltas are in
`accepted-baseline-comparison.json` and `accepted-clean-candidate-comparison.json`.
Detailed disassembly diffs and compilation-unit attribution remain in private
`*.private.*` evidence. Examples requiring further accounting include address
materialization in `Base64_Decode`, `BuildMessage` and other library functions.
Source preservation and passing tests do not close that proof gap.

The clean candidate differs from the accepted application by **−720 bytes**:
flash text **−288**, flash rodata **−440**, other ELF section sizes unchanged,
and final image alignment area **+8**. Image DROM/DRAM segment packing also shifts:
DROM **−440**, first DRAM chunk **+440**, second DRAM chunk **−440**; total
initialized DRAM remains unchanged. This is explicit packing accounting, not
an additional claimed RAM saving.

The candidate resource audit identifies five vtables shrinking **208 to 204 bytes**,
one removed experimental virtual slot each. All other compared named objects
are exact or resolve to relocated pointers. Source/SDK evidence and the 48-case
test explain the intended experiment removal, but do not prove every other
accepted-to-candidate byte difference. The original binary-difference blocker
is therefore **not fully resolved**.

## Private staging and licensing

`private-staging/` contains the clean application, SHA-256 sums, source/build
manifests, both patch identifiers, build instructions, draft release notes,
conditional application-only installation and rollback instructions, known
license texts and attribution inventory, and sanitized comparison summaries.
Its README marks the package blocked. No accepted private binary, ELF/DWARF,
raw log, credential, copyrighted EPUB or development backup is included.

Reader/SDK MIT notices and known dependency/font/icon license texts are retained.
Arduino-wolfSSL 5.7.2's manifest declares **GPL-2.0-only**. A complete inventory
of all linked Arduino/ESP-IDF, GCC runtime, embedded JS, FreeType/stb and internal
third-party notices and corresponding-source/distribution requirements is not
established by this draft package. The entire firmware is not claimed to be MIT.
See `private-staging/licensing.md` and `license-inventory.json`. Publication remains
blocked until that distribution audit is completed as well.

## Quality gate and required continuation

| Required condition | Disposition |
| --- | --- |
| Required SDK fixes reproducibly represented | PASS: pinned base plus verified patch, remote fetch and clean reconstruction |
| Experimental changes excluded or accounted for | PASS within documented source scope; unused measurement mask member explicitly retained |
| Clean firmware build | PASS |
| Automated regressions | PASS with stated emulation limits |
| Personal build-path scan | PASS |
| Clean-source reproducibility | PASS for two independent Windows source roots with recorded shared toolchain inputs |
| All accepted firmware differences understood | **NOT ESTABLISHED — blocking** |
| Original accepted firmware and worktree intact | PASS |
| Complete public distribution licensing/source material | **NOT ESTABLISHED — blocking** |

Continue by completing the relocation/weak-definition/anonymous-data comparison,
preferably recovering the accepted map/link command/object inputs if available,
or producing a complete ELF-level accounting without them. Complete the linked
component notice/source-distribution audit. Do not reinterpret the successful
source tests or matching new hashes as acceptance of the unresolved comparisons.
No release branch, commit, tag or publication is authorized by this handoff.

## Minimal physical acceptance, after the gates are cleared

Do not flash this blocked candidate now. After the evidence is reviewed, all
material gates close, and a later application-only upload is authorized:

1. Verify the exact candidate SHA-256 and matching app0 layout, retain a private
   known-good application and SD/settings backup, then observe ordinary X3 boot.
2. Enter a DRM-free EPUB. Test all six controls, four speeds, Smart Timing On/Off,
   paused sentence context/highlighting, resume, chapter transitions, EOF, Back
   and saved preference/position restoration.
3. Inspect `shadowless`, `entrepreneurs`, quoted long words, wide W/M words,
   descenders and combining marks. Confirm fixed ORP/baseline/markers, complete
   right-edge strokes, five-tier selection, and oversized-word fallback.
4. Repeat entry/play/pause/chapter/exit sessions. Compare existing ENTER/CHAPTER/EXIT
   free/minimum heap and largest-block logs at warmed-up lifecycle points; check
   for failed refresh, panic, progressive heap loss or cadence regression.

No hardware acceptance is claimed for the new image.

**V1.0.1 RELEASE REMEDIATION BLOCKED**
