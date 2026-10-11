# V1A first physical crash: HalFile ownership fix

## A. Root cause

The new RSVP adapter called `HalFile::close()` on its never-opened, default-constructed
`tokenFile`. This is an API precondition violation introduced by V1A, rather than
duplicated ownership of the EPUB or a display-driver problem.

Source evidence:

- `lib/hal/HalStorage.h:85`: `HalFile` owns one `std::unique_ptr<Impl>`; copying is deleted.
- `lib/hal/HalStorage.cpp:165`: the default constructor leaves that pointer null.
- Lines 167–169: destruction and move operations use the member's standard RAII semantics.
- Lines 225–228: wrapped file operations assert that an implementation exists.
- Line 260: public `HalFile::close()` invokes that wrapper; it is not an empty-handle reset API.
- Pre-fix `lib/Rsvp/RsvpEpubSource.h:34`: `tokenFile` was a default member.
- Pre-fix `RsvpEpubSource.cpp:12`: `beginChapter()` unconditionally closed it before
  `Storage.openFileForWrite()` at line 28.
- Pre-fix line 126: general cleanup contained the same invalid operation if entry
  failed or exited before opening a spool.

The before files are preserved in `.pio/rsvp-v1a-crash-before-files/`.

## B. Failing lifecycle and audited ownership

The normal first-load sequence is deterministic from the source:

1. `EpubReaderActivity.cpp:825` releases its normal `Section`, then constructs the
   RSVP activity using the existing shared `Epub` metadata object.
2. `RsvpReaderActivity.h:19` constructs its source in place. The source's
   `tokenFile.impl` is null; no spool has been opened.
3. ActivityManager pushes the parent's `unique_ptr` onto its stack and installs
   the new activity (`ActivityManager.cpp:196`). Neither activity object nor any
   file member is moved out of its owning object.
4. RSVP `onEnter()` requests rendering. `loadSome()` presents loading chrome and
   calls `source.beginChapter()` (`RsvpReaderActivity.cpp:187`).
5. The old `beginChapter()` reaches the unconditional `tokenFile.close()` before
   extraction, spool opening, or parser creation. The wrapped call asserts
   `impl != nullptr` at the reported `HalStorage.cpp:260`.

If the initial display fails, or Back exits before loading starts, cleanup can
instead reach the old unconditional close in `RsvpEpubSource::close()`, on the
same never-opened member. The fix covers both routes. The crash-screen assertion
alone does not distinguish these callers; the saved backtrace would confirm which
one the physical run reached.

Other paths were traced:

| Resource/path | Ownership and release |
| --- | --- |
| EPUB archive | Existing `Epub` stores path/metadata. `readItemContentsToStream()` uses scoped `ZipFile` ownership (`Epub.cpp:907`, `ZipFile.cpp:23`). No archive handle is transferred between activities. |
| Normal section | Parent owns its `Section`. Destruction invokes `suspendBuild()` (`Section.cpp:93, 685`), releasing its own parser/build handles before RSVP loading. |
| Extracted chapter | Existing `Epub::extractItemToFile()` opens a local output before closing it (`Epub.cpp:918`). Its ZIP and output handles remain local to the established extraction API. |
| Chapter parser | Owns `parseFile_`. `beginParse()` opens it; `abortParse()` tests the null-safe `isOpen()` before closing (`ChapterHtmlSlimParser.cpp:2135, 2198`). Text-only finish aborts safely; later parser destruction repeats that guarded teardown. |
| RSVP spool | Sole member owner is `RsvpEpubSource::tokenFile`. Only HAL open functions replace it. It is never copied or moved out to another activity/parser. |
| HAL open temporaries | `openFileForRead/Write()` installs a unique implementation by move assignment (`HalStorage.cpp:187, 203`). A moved-from temporary's destructor is safe; it does not call the asserting public wrapper. |
| Activity pop/replace | Manager calls `onExit()` before deleting the activity under RenderLock (`ActivityManager.cpp:222`). Source cleanup can run from failure, onExit, and its destructor; the owned slot must therefore release safely in any state. |

Explicit close followed by destructor cleanup is not the cause of the reported
null assertion: public close does not reset `impl`. `Impl::~Impl()` closes the
underlying SDK file under StorageLock (`HalStorage.cpp:156`), and the SDK file's
already-closed destructor is harmless. No moved-from RSVP file or retained old
file owner was found in the V1A handoff.

## C. Files changed by this crash fix

- `lib/Rsvp/RsvpEpubSource.cpp`: replace all three explicit spool closes with
  `releaseTokenFile()`; add chapter/open/release diagnostics.
- `lib/Rsvp/RsvpEpubSource.h`: declare the release helper and explicitly delete
  copy/move operations for this adapter, whose callbacks refer to its own address.
- This report.
- `docs/rsvp-v1a-epub-integration.md`: identify the initial build as superseded
  by this physical-crash fix and link the current retest report.

Branch: `feature/rsvp-v0.1-display-test-v1.6.5`.
CrossPoint HEAD: `840371a4766d865d9fdef6f61b1b4e682fb3494a`.
SDK HEAD: `111fdcc7f0176c3ee38391a160ee296bf492dbd8`, with the pre-existing dirty
SDK experiments retained. Builds use those working trees plus the uncommitted fix.
The before-state fingerprints are in `.pio/rsvp-v1a-crash-before.json`.

## D. Why the fix is correct

`releaseTokenFile()` assigns `HalFile{}` into the owned slot. Defaulted move
assignment releases the old `unique_ptr<Impl>`, if any. That invokes the existing
locked `Impl` destructor before a reopen or temporary-file removal. Replacing an
empty slot destroys no implementation and calls no file operation.

This uses the existing ownership contract; it does not make public `close()`
idempotent, inspect private `impl`, remove an assertion, or bypass the storage mutex.
The `isOpen()` test in the helper controls logging only. Release always uses RAII.

The corrected paths cover first entry, successful spool completion, next-chapter
replacement, allocation/open/parse failure, exit before opening, and repeated
error/onExit/destructor cleanup. One implementation remains owned at a time.
The empty temporary adds no heap allocation or buffer; no data members were added.

Lifecycle logging is once per chapter or ownership transition:
`[RSVP1] CHAPTER load`, `FILE open mode=write`, `FILE release`, `FILE open mode=read`.
There are no per-word file logs. Existing ENTER/EXIT diagnostics remain available.

## E. Normal EPUB and display behavior

Normal `Epub`, `ZipFile`, `Section`, chapter-parser, HalStorage, and ActivityManager
ownership implementations are unchanged. RSVP continues to share metadata and
use its own temporary spool and the existing parser's chapter-read handle through
HAL. No persistent archive handle was duplicated and no parser was added.

Display backends, SDK drivers, LUTs, waveforms, DSP sequencing, partial-window
experiments, playback timing, and V1B were not changed. No flash, commit, or push
was performed.

## F. Validation

- Established default PlatformIO build: **SUCCESS**, 38.894 s.
- Configured cppcheck: **PASSED**, no defects found, 43.638 s.
- `git diff --check`: passed.
- Static review: all RSVP token-file release sites now use the ownership reset;
  no explicit `tokenFile.close()` remains. HAL's assertion is unchanged.
- Host C++ tests were not executed: no host compiler was available in this session.
- The sanctioned formatting wrapper was invoked and reported no installed
  formatter. The small change was formatted manually.
- Physical crash retest remains pending. Compilation/static analysis alone did
  not detect the original default-handle precondition violation.

Build/check logs: `.pio/rsvp-v1a-crash-build.txt`, `.pio/rsvp-v1a-crash-cppcheck.txt`.
A selective reverse patch is stored as `.pio/rsvp-v1a-crash-fix.patch`; it preserves
the pre-existing V1A implementation and experimental work. Review and use
`git apply --reverse --check` before any manual rollback.

## G. Firmware

`C:\ai\Crosspoint-RSVP\crosspoint-rsvp\.pio\build\default\firmware.bin`

SHA-256: `5265f3156ded07134a683780582ec5a1679164495d3e7a0f4223facd8eb57f0b`.
Length: 5,667,568 bytes. The adjacent `firmware.factory.bin` is also rebuilt.
This hash supersedes the initial V1A firmware identity in the integration handoff.

## H. Minimal physical retest

The owner performs firmware installation; this implementation session did not flash.

1. Preserve the old `crash_report.txt`. Install the firmware identified above
   through your existing update procedure and capture serial at 115200 baud.
2. Open the **same real EPUB**, return to the same page, select RSVP. It must show
   a paused landscape word without the close assertion. Look for CHAPTER load,
   write-open, release, then read-open lifecycle messages.
3. Play briefly, pause, step Left/Right, then Back. Confirm normal reader position
   and original orientation. Repeat entry/Back three times. Also exit promptly
   during the loading screen to exercise cleanup before a spool is opened.
4. Enter near a chapter end and cross one spine boundary. Confirm another
   release/write-open/release/read-open sequence, then exit normally.

No cache deletion or book replacement is required to test this ownership fix.
Any new assertion/crash blocks acceptance; retain the new report and serial log.

## I. Crash report requirement

The supplied assertion plus the source trace is sufficient to fix this deterministic
default-handle bug; `crash_report.txt` is not a blocker. Preserve it for attribution
of the original caller. If the retest crashes, its decoded backtrace, assert details,
firmware identity, and preceding RSVP lifecycle messages are required to identify
the remaining path. No claim is made that the original saved backtrace was inspected.

**READY FOR V1A CRASH RETEST**
