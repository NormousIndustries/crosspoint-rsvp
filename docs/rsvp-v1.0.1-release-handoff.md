# CrossPoint RSVP v1.0.1 release handoff

Prepared 12 October 2026, Australia/Sydney.

**RSVP v1.0.1 RELEASE PREPARATION BLOCKED**

The user confirms physical acceptance of V1.0.1: all five final typography
checks and the previously accepted V1B/V1C functionality passed. Earlier
documents saying physical acceptance is pending are historical records.

No firmware functionality was changed in the development worktree. No device
connection, flash, upload, reset, erase, push or publication was performed.
No release commit or `v1.0.1` tag was created.

## Blocking findings

1. **The accepted application contains personal build paths.** A printable-string
   scan of the frozen image finds 32 absolute Windows user-profile paths, all
   pointing into the local PlatformIO dependencies. The exact strings and byte
   offsets are retained only in `embedded-paths.private.json` in the local audit
   directory. Publishing this image would violate the requested exclusion of
   personal paths. Editing the binary would invalidate its accepted hash, image
   checksum and validation digest. It has therefore not been edited or packaged
   for public distribution. A path-sanitized build needs separate review and
   acceptance; the reference artifact must remain preserved.
2. **A production-only, reproducible SDK source state has not been established.**
   Production RSVP requires changes in the dirty SDK, while that same patch
   contains linked partial-window experiments. The isolated audit snapshot
   deliberately preserves the entire patch. It is forensic evidence, not a
   release source checkout. No claim is made that experimental code has been
   excluded from this snapshot or from the accepted image.
3. **Reproduction of the accepted image is incomplete.** The first isolated build,
   using byte-matching upload sources and the same compiler/dependency sources,
   produces a different application, 1,424 bytes larger. Link layout and RISC-V
   call relaxation explain an observed production-function difference, but the
   complete accepted-versus-rebuilt difference has not been proven equivalent.
   A timestamp-only explanation would be incorrect.

Per the requested stop condition for unresolved SDK experiments, release branch,
commit, tag and distributable-package construction are deferred. The local
archives below must not be attached to a public release.

## Git and source state

| Item | State |
| --- | --- |
| Development branch | `feature/rsvp-v0.1-display-test-v1.6.5` |
| Reader HEAD | `75a27e679e397e96e4a2ceea987f21103fd20986` |
| FreeInk SDK gitlink and HEAD | `111fdcc7f0176c3ee38391a160ee296bf492dbd8` |
| Nested Lucide gitlink and HEAD | `c81680e066f45b640743ca78ae36cdedda3f0318` |
| Release branch/commit | Not created |
| Annotated tag `v1.0.1` | Absent; not created |
| `origin` | `https://github.com/NormousIndustries/crosspoint-rsvp.git` |
| `upstream` | `https://github.com/crosspoint-reader/crosspoint-reader.git` |
| `fork` remote | Not configured |

The development tree initially had 11 tracked modified reader/test/generator
files and seven untracked typography-related files. The SDK had 13 tracked
modified files and untracked `DisplayMeasurement.h`. The nested Lucide checkout
was clean. Full initial status, index, patches, revisions, tags and remotes are
in the private backup. There was no pre-existing `v1.0.1` tag to conflict with.

All **1,446** upload-manifest source hashes still match the original worktree.
That includes required EPUB/HalFile integration, UI/font rendering, Smart Timing,
Context View, settings and driver sources. The accepted image is based on HEAD
**plus dirty source**, not on HEAD alone. Tagging the current HEAD would omit
the accepted typography changes and SDK additions.

The isolated clone is at
`rsvp-release.local/preparation-20261012/source-audit/`. Its parent and SDK Git
databases are independent clones; the original checkout was never reset,
cleaned, restored or switched. The development binary under `.pio` is unchanged.

Historical experiments remain under `docs/research/x3-display/`, with activity
sources stored as `.cpp.txt`/`.h.txt`, as documented in that directory's
`README.md:3-17`. They do not compile as activities. Their historical timing and
launch instructions are not release instructions. The original SDK experiments
remain intact and recoverable.

## Frozen accepted artifact and local evidence

Base directory: `rsvp-release.local/preparation-20261012/` (Git-ignored).

| Local artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `accepted/CrossPoint-RSVP-v1.0.1-X3-app.bin` | 6,456,848 | `f25f9e60c8647694a6a155a45d9fa0639cb7907e86b6955f58eb724fd6c64467` |
| `accepted/accepted.elf` | 75,140,500 | `4c35d939a283ffdd25b80b810679affd5ded4a788741a3724a2d0744662e9247` |
| `accepted/v1.0-rollback.bin` | 6,196,144 | `999955b7086ed85184f0e7cf8d668e7950180e42ebf02df2de5ed6d01548b0b9` |
| `development-state.zip` | 48,888,664 | `e8a9d18f83271b1e4cbde42b46c42e8838df34bba5c8377e660dd02234a41415` |
| `nested-lucide-state.zip` | 4,448,842 | `ddab5c11536c562c7a5ab860e484767094223e8e87317f9d412ac1669f74cf49` |

The application and matching ELF were copied from the earlier upload archive,
whose hash and application-only upload are recorded in
`docs/rsvp-v1.0.1-typography-refinement.md:180-198` and the existing upload
manifest. Their new copies are read-only and outside PlatformIO output. Builds
target only the isolated snapshot and cannot overwrite these copies.

Application partition: **6,553,600 bytes (`0x640000`)** at **`0x10000`**.
Accepted image headroom: **96,752 bytes**. Static linker RAM: **57,936 bytes**;
this is not a peak heap measurement. Image header: ESP32-C3, DIO, 80 MHz,
16 MB flash. It is an **application-only image**, not a factory/recovery image.

`SHA256SUMS.txt` verifies the local images, backups and `source-build-manifest.json`.
That manifest records revisions, original source hashes, tool versions and
results. The backup includes 1,715 reader/SDK files, binary patches and untracked
SDK files; the nested repository is archived separately. Every archived file
was read back and hash-checked, and both ZIP integrity checks passed.

**Private evidence only:** the development ZIPs, ELFs/DWARF, upload metadata,
embedded-path report, logs and experimental assets may contain personal paths,
hardware identifiers or historical fixture material. Do not publish them.
No public release ZIP or factory image has been prepared.

## SDK dependency disposition

| Changes | Dependency evidence | Disposition |
| --- | --- | --- |
| `DisplayMeasurement`, measured full-frame wrapper and bus instrumentation | `lib/Rsvp/RsvpDisplayBackend.cpp:5-12` calls `HalDisplay::displayBufferMeasuredFast`; `lib/hal/HalDisplay.cpp:82-86` calls the SDK measured wrapper; `FreeInkDisplay.cpp:627-631` brackets the ordinary blocking path with measurement | Required production dependency; must be pinned/integrated, not dropped with the experiment |
| BUSY/timeout bookkeeping and full-frame completion guards | `EpdBus.cpp:111-121,381-384,452-472`; `Uc8253X3Driver.cpp:274-281`; `Uc8279Driver.cpp:209-216` | Required to preserve accepted error behavior; observed in accepted compiled X3 completion code |
| Window API, region upload, window state/restoration and virtual methods | `FreeInkDisplay.cpp:634-650`; `EpdBus.cpp:79-109`; `Uc8253X3Driver.cpp:338-378`; `Uc8279Driver.cpp:251-292`; `PanelDriver.h:55-59` | No production call site to the experimental API; dormant implementations are nevertheless linked and remain interleaved with driver lifecycle |
| Restoration hook in ordinary display start | `Uc8253X3Driver.cpp:160,188`; `Uc8279Driver.cpp:113,129` | Production path executes the hook. Removing it must be reviewed with its state invariants, not assumed harmless because RSVP requests full-frame FAST |
| Modified SDK host tests | `test/host/run_uc8253_power.py:52-53` substitutes successful wait/not-busy results | Existing test cannot establish timeout or recovery equivalence for a separated driver patch |

SDK paths in this table are relative to
`freeink-sdk/libs/display/FreeInkDisplay/`. Accepted ELF symbols confirm measured
refresh, `recordWait`, both window implementations and restoration routines.
`accepted-x3-completion.log` confirms the ordinary UC8253 completion guard checks
the recorded wait result and BUSY before synchronization.

The required production subset is identified, but a validated split, source-control
integration and clean-clone acquisition arrangement are **not complete**. The
submodule pointer alone cannot reproduce the accepted sources because it does
not include the dirty patch or untracked measurement header. Upstream MIT
copyright and licensing must be preserved when that subset is integrated.

## Toolchain, build and validation

| Tool/component | Version/profile |
| --- | --- |
| Host | Windows PowerShell; `uname -s`: `MSYS_NT-10.0-26200` |
| Python | 3.13.13 |
| PlatformIO Core | 6.1.19 |
| PioArduino platform | 55.03.311, URL pinned in `platformio.ini:10` |
| Arduino / libraries | 3.3.11 / `5.5.5+sha.b774170ff46` |
| ESP-IDF | 5.5.5 |
| RISC-V compiler | GCC 14.2.0, `esp-14.2.0_20260121` |
| esptool | 5.3.0 |
| Host execution/raster | Unicorn 2.1.4; Pillow 12.0.0 |
| Environment | `default`, not `gh_release` |

The accepted configuration remains in the snapshot's `platformio.ini`.
Relevant flags include single framebuffer, USB CDC, `DESTRUCTOR_CLOSES_FILE=1`,
`XML_GE=0`, `XML_CONTEXT_BYTES=1024`, UTF-8 names, batched SPI transfer,
`-std=gnu++2a`, `-fno-exceptions`, X3/X4 runtime detection, serial logging and
`LOG_LEVEL=2`. The inherited custom SDK configuration and component removals
must also be retained (`platformio.ini:177-223`). Changing to `gh_release`
changes logging/version flags and would not rebuild the accepted configuration.

The frozen image's ESP application descriptor still says **1.6.5**, with compile
time **Oct 10 2026 20:03:18**. Runtime development version:
`1.6.5-dev-feature/rsvp-v0.1-display-test-v1.6.5-75a27e67`.
The requested release identity is v1.0.1; the binary has not been relabeled.

- Isolated baseline firmware: PASS, 123.90 s; no compiler-warning match in log.
- Configured `pio check -e default`: PASS, no defects, 41.47 s.
- Production C++ RV32/Unicorn checks: PASS for tokenizer/context/EpdFont,
  timing/history/cadence, real ArduinoJson preferences and actual activity logic
  with deterministic HAL/UI substitutes. These do not emulate panel/SD timing.
- Typography raster and ORP, context, existing word raster and physical-control
  raster regressions: PASS. Typography preservation contract and `git diff
  --check`: PASS before formatting.
- Sanctioned `./bin/clang-format-fix -g`: successful in isolated source only.
  It reformatted existing `lib/Rsvp/RsvpTiming.h` whitespace; whitespace-stripped
  text is identical. The original file was untouched. The wrapper covers tracked
  modified files; the untracked harness was not separately checked by that call.
- Dependency audit: all 674 C/C++ source/header files in the seven external
  dependency packages match the original workspace's files.

First rebuilt application: **6,458,272 bytes**, SHA-256
`58f1df42a1065f66afe0273d97e134b87b831c3d6d0f9f8ef6cf4927086f4a1a`.
It differs from the accepted image. Its `.flash.text` is 1,416 bytes larger;
`.flash.rodata` size is unchanged. `baseline-comparison.json` records 351 symbol
size differences. In `EpdBus::cmd`, the same three `__digitalWrite` calls change
from four-byte `jal` instructions to eight-byte `auipc`/`jalr` sequences after
the bus moves earlier in the link layout. This accounts for that function's
12-byte increase; it is not proof of equivalence for the entire firmware.

The formatted-source clean rebuild has the same size and SHA-256
`9d9d8db7f8ccce68e1727277cd5743785f62e439b32ca0adcd43f4880e9d5ea0`.
Compared with the first audit build, only 65 bytes change: the embedded ELF hash
at `[0xb0,0xd0)` and the final checksum/digest. Debug information changed after
whitespace formatting. Another clean build with unchanged source returns the
same application/ELF/partition hashes, but **reuses SCons cache**, including the
ELF, and therefore does not prove independent compilation.

The cache-disabled clean build also passes (123.38 s), with no object retrieval
from SCons cache. It remains **6,458,272 bytes**, SHA-256
`89766515387bbf5e28879a2a0c2365f0fc07c831955e0e9fbef846109e8b500c`.
Its `.flash.text`, `.iram0.text` and `.dram0.data` match the preceding
formatted-source build byte-for-byte, and no symbol sizes change. The application
diff is confined to 70 bytes: two compile-time clock strings, the embedded ELF
hash, and final checksum/digest. The two clock strings are backed by
`src/activities/settings/SdFirmwareUpdateActivity.cpp:20` and Arduino's
`cores/esp32/chip-debug-report.cpp:215`, both using `__TIME__`. The partition
binary matches across builds. Thus independent builds reproduce the observed
executable sections, **not the complete binary hash**. Deterministic macro
timestamps and embedded ELF metadata remain reproducibility work.

These builds retain unresolved SDK experiments and do not meet the
accepted-artifact/privacy and production-source gates. The temporary
cache-disable override existed only in the audit clone and was removed.

## Draft release notes — do not publish

**CrossPoint RSVP v1.0.1 — XTEINK X3**

CrossPoint RSVP is an independent firmware fork built on CrossPoint Reader.
It integrates RSVP reading for DRM-free EPUBs on the XTEINK X3.

- Large positional-ORP typography and two fixed focal registration markers.
- Five baked font tiers adapt to long words while keeping the focus fixed.
- Automatic sentence context when paused, with the exact selected word highlighted.
- Smart punctuation-aware rhythm, with persistent speed and Smart Timing preferences.
- Six hardware-aligned buttons for playback, speed, Previous/Next and Back.
- Reading-position restoration and chapter navigation.
- Speed choices: 50, 75, 100 and 125 WPM.

Display refresh limits the usable speed; 125 WPM is the highest selectable
setting. Smart Timing can make the effective reading rate differ from selected
WPM. EPUB return/restoration uses the established page-level position mapping.
DRM-protected books are not supported by this release claim. Partial-window
acceleration is not a supported user feature; before publication, resolve the
linked experimental implementation and replace this preparation caveat with
an accurate final exclusion statement.

Thanks to the upstream CrossPoint Reader project, FreeInk SDK, and the font,
rendering, parsing, storage and networking component authors. This is not an
official upstream CrossPoint release.

## Installation/update instructions — conditional draft

No installation is authorized or performed during this preparation. These
commands document the previously accepted application-only method for a later
approved update. They are not a release-ready installer.

Compatibility is limited to the tested XTEINK X3 ESP32-C3 configuration with the
existing matching 16 MB partition layout and **app0 selected at `0x10000`**.
Do not assume a stock device, a different OTA selection or another XTEINK model
has that layout. First establish matching bootloader, partition table and OTA
selection, and back up the SD/settings and known-good firmware. Unknown layouts
need a separately validated installation/factory package.

Install esptool 5.3.0 in a dedicated Python environment; identify the device's
actual serial port and close other serial programs. Check the downloaded file's
SHA-256 against the eventual published checksum before writing it. Once the
privacy/source blockers are resolved and the replacement image is accepted:

```powershell
$port = 'COM_PORT_FROM_DEVICE_MANAGER'
python -m esptool --chip esp32c3 --port $port --baud 921600 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 80m --flash-size detect 0x10000 CrossPoint-RSVP-v1.0.1-X3-app.bin
python -m esptool --chip esp32c3 --port $port verify-flash 0x10000 CrossPoint-RSVP-v1.0.1-X3-app.bin
```

Only an application image is specified. Bootloader, partition table, OTA data,
NVS and SD files are excluded from these writes. Do not use erase-all.
Espressif documents automatic write verification and `verify-flash` in the
[ESP32-C3 esptool instructions](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/esptool/advanced-commands.html).
The recorded accepted upload is supporting evidence for this application-only
method (`docs/rsvp-v1.0.1-typography-refinement.md:187-198`), not evidence for a
fresh factory installation.

After an eventual update, verify normal boot, DRM-free EPUB entry, all four
speeds, Smart Timing, pause/context highlighting, chapter navigation and saved
position. Monitor existing ENTER/CHAPTER/EXIT heap/largest-block logs during
repeated reading sessions; no new firmware diagnostic allocation is introduced.

## Recovery and rollback — conditional draft

The local V1.0 rollback is also **application-only** and is retained as private
evidence; it has not passed this task's public-asset privacy audit. On the same
verified app0 layout, an approved rollback uses the same `write-flash` and
`verify-flash` commands with the known-good application file at `0x10000`.
Verify its recorded SHA-256 first. Preserve the SD card and settings.

If the application will not boot, reconnect through the X3's documented
bootloader/recovery procedure, then restore the known-good application only
after confirming the layout/OTA selection. This package cannot recover a missing
or corrupt bootloader, partition table or OTA selection. Obtain a separately
verified full recovery image and its instructions for those cases. No hardware
button recovery sequence has been guessed or tested here.

## Licensing and public asset gate

Reader source: MIT (`LICENSE`, CrossPoint Reader organization, 2025).
FreeInk SDK: MIT (`freeink-sdk/LICENSE`, FreeInk, 2026).
RSVP Noto Sans assets: SIL OFL 1.1; retain the Noto copyright and full license
from `lib/EpdFont/builtinFonts/source/NotoSans/OFL.txt`.
Retain relevant notices for other baked fonts and Lucide icons too.

The complete firmware is not automatically MIT merely because the reader and
SDK are MIT. Its Arduino/ESP-IDF, ArduinoJson, JPEGDEC, PNGdec, QRCode, SdFat,
WebSockets, wolfSSL and other compiled components need their actual applicable
notices and source-distribution obligations carried into the eventual package.
That final third-party distribution audit is deferred with the blocked source
snapshot; no claim of completed license compliance is made.

The future public assets must contain only the accepted, privacy-audited image,
its checksums, final release notes, install/update/rollback instructions,
complete pinned source/build manifest and applicable notices/source provisions.
Exclude private archives, DWARF ELFs, logs, credentials, local configuration,
books and DRM material. The preserved reference image cannot currently be used
as that public image because of its embedded profile paths.

## Quality gate and next required work

| Gate | Result |
| --- | --- |
| Accepted firmware frozen and hashed | PASS |
| Original development source/device preserved | PASS |
| SDK experiments and nested-submodule state backed up | PASS |
| Isolated audit firmware build and configured cppcheck | PASS |
| Deterministic C++ and raster/ORP regressions | PASS, with emulator limits above |
| Complete formatting check of final release source | Not established |
| Accepted binary reproduced byte-for-byte | FAIL |
| Independent clean-build reproducibility | Executable sections match; complete image hashes differ due clock strings/ELF hash |
| Unnecessary SDK experiments excluded from release source/image | FAIL |
| Public image contains no personal paths | FAIL |
| Clean release clone and exact production SDK revisions obtainable | Not established |
| Complete public assets and licensing | Deferred |
| Release commit and correct annotated tag | Deferred |

Resume by reviewing/pinning the production measurement and BUSY/error-handling
subset, validating its normal/failure/recovery behavior, excluding window
experiments, and producing a path-sanitized build without touching the accepted
reference. Make compile-time clock/ELF metadata deterministic. Trace linker/library ordering to explain the remaining accepted
binary delta, complete clean-clone and uncached checks, and obtain acceptance
of any changed distributable image before preparing the source tag.

## Eventual GitHub commands — not executed

There is no release commit to push today. After every gate passes, create and
review `release/v1.0.1` and the annotated tag in the completed isolated release
repository. The release commit must contain the exact SDK acquisition mechanism;
do not tag the forensic clone or push a dangling SDK submodule commit.

After separate explicit push/publication approval, inspect remotes again. The
repository guide prefers `fork` for feature/release pushes. If it is still absent,
and the same personal repository remains the approved destination:

```powershell
git remote -v
git remote add fork https://github.com/NormousIndustries/crosspoint-rsvp.git
git show --no-patch v1.0.1
git status --short
git push fork release/v1.0.1
git push fork refs/tags/v1.0.1
gh release create v1.0.1 --repo NormousIndustries/crosspoint-rsvp --verify-tag --title 'CrossPoint RSVP v1.0.1 — XTEINK X3' --notes-file release-notes.md CrossPoint-RSVP-v1.0.1-X3-app.bin SHA256SUMS.txt release-notes.md installation.md recovery.md source-build-manifest.json LICENSES.zip
```

Run from the completed release checkout and curated asset directory; the listed
public assets do not yet exist. Do not add `fork` twice if it already exists.
Do not upload the private files listed earlier. These commands are future
instructions, not permission to execute them.

**RSVP v1.0.1 RELEASE PREPARATION BLOCKED**
