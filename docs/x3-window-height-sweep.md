# UC8253 X3 results and supervised height sweep

This revision is built but **not flashed**. It retains every existing BUSY wait,
command sequence, LUT, voltage, temperature assumption, PLL setting, and SPI clock.
No duplicate wait or double-counted timing was found. The only driver additions
are two buffered GPIO observations after OLD upload and its data-stop command.

## A–B. Cause of doubled wait time

The supplied physical observations select the UC8253-X3 path with VER `FF,FF,FF`.
That is the existing UC8253 assumption/default, not an affirmative chip signature.

The successful experimental sequence in
`freeink-sdk/libs/display/FreeInkDisplay/src/driver/Uc8253X3Driver.cpp` is:

1. DRF followed by `waitRefreshComplete()` (line 365).
2. OLD/DTM1 upload, then `CMD_DATA_STOP` (lines 373–376).
3. `restoreExperimentalWindow()` (line 377), whose line 340 calls `waitBusy()`
   **only if the physical BUSY pin is LOW** before restoring PTL.

`EpdBus::recordWait()` (line 111 of `src/bus/EpdBus.cpp`) adds each elapsed wait
once. The polling fallbacks in `waitRefreshComplete()` return immediately after
delegating to `waitBusy()`, so the delegated wait is not also recorded by the outer
function. `busyCompleteUs` is intentionally latched on the **first** wait after
DRF, while `waitUs` accumulates **all** waits. This explains the differing metrics.

| Geometry | First DRF-to-return | All waits | Implied later wait | NEW + OLD | Total | Residual |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 792 x 528 | 381.5 ms | 381.5 ms | approximately 0 | 96.3 ms | 478.5 ms | 0.7 ms |
| 792 x 160 | 328.3 ms | 657 ms | approximately 328.7 ms | 29.7 ms | 687 ms | 0.3 ms |
| 712 x 160 | 326.3 ms | 653 ms | approximately 326.7 ms | 27.2 ms | 681 ms | 0.8 ms |

The small residuals support **real additional elapsed blocking**, not merely a
doubled counter. The exact software location is the post-OLD restoration guard.
The original full-frame FAST path does not perform that restoration wait.
There is only one explicit DRF command in the successful experimental sequence.

The aggregate observations cannot tell whether OLD upload, data-stop, or a later
phase of the controller's earlier operation produces the renewed LOW interval.
They do not establish that a second optical refresh occurred. In particular, it
would be unjustified to say that OLD synchronization necessarily starts a new
waveform, or to remove the wait while the controller reports BUSY.

New `WAITS` output makes the accounting explicit:

```text
WAITS count=2 first_us=... last_us=... sum_us=... post_sync_busy_mask=0x.. (OLD=bit0 DSP=bit1)
```

For a successful warm experimental call with two waits, `first_us` is the main
refresh wait and `last_us` is the restoration wait. `sum_us` should approximate
their sum. The two BUSY checkpoints are buffered; there is no serial output
during those observations. A mask of `02` means LOW was seen after data-stop but
not immediately after OLD upload; `03` means LOW was already seen after OLD and
was still seen after data-stop. A `00` with two waits means LOW was detected
later, at restoration. These are samples, not continuous pin traces or proof of
which command caused the transition. A logic analyzer on BUSY/CS/DC/SCLK would
resolve command-to-edge ordering if ambiguity remains.

## C. Expected 712 x 160 service time

There is no justified duplicate-wait correction, so the expected current service
time remains **approximately 681 ms**. The first reported DRF interval remains
approximately 326 ms, distinct from the full service time.

Subtracting the later 326.7 ms produces a **counterfactual** 354.3 ms service time.
That is arithmetic only: the additional BUSY interval has not been proven
avoidable, and this revision does not remove it. Even that counterfactual would
still exceed the 200 ms budget for 300 WPM.

## D. Geometry and implementation

All five modes remain manually stepped. Right now cycles **C -> D -> E -> A -> B -> C**:

| Serial geometry ID | Mode | Expected aligned memory geometry on this unit | Bytes per plane |
| --- | --- | --- | ---: |
| 0 | A: full-frame FAST | 792 x 528 | 52,272 |
| 1 | B: full-width window | 792 x 160 | 15,840 |
| 2 | C: existing region | 712 x 160 | 14,240 |
| 3 | D: shorter region | 712 x 120 | 10,680 |
| 4 | E: shortest region | 712 x 80 | 7,120 |

`X3WindowTestActivity::updateGeometry()` at line 50 retains the original runtime
region's X position and width for C/D/E. Only height and centered Y change;
height is bounded by the runtime available region. The renderer performs the
existing orientation mapping and byte alignment. Logged memory geometry remains
authoritative if device margins differ. The existing font and test words are
unchanged; the cleared/drawn test area fits inside each selected window.
Geometry changes perform the existing full cleanup and restart at pattern A.
The exterior text retains its original position below the 160-pixel region.

Changed reader files: `src/activities/util/X3WindowTestActivity.h`, `.cpp`,
`lib/I18n/translations/english.yaml`, and the two experiment guides. Changed SDK
files: `include/DisplayMeasurement.h`, `src/bus/EpdBus.h`, `.cpp`,
`src/driver/Uc8253X3Driver.cpp`, and `test/host/run_uc8253_power.py`, all under
`freeink-sdk/libs/display/FreeInkDisplay/`.

No new heap allocation, framebuffer, baseline buffer, or task is introduced.
The existing activity allocation holds an additional rectangle and the expanded
timing record. Target ELF sizes are 72 bytes per measurement, 288 bytes for the
manual activity, and 12,408 bytes for the original buffered RSVP activity. The
99-sample RSVP buffer plus reusable record grows by 800 bytes and is released on
activity destruction; stack/static arrays were not introduced.

## E. Provisional timing predictions

A rough affine fit to the supplied first DRF intervals is:

`Tfirst(H) = 302.3 ms + 0.15 ms * H`

This comes from `(381.5 - 326.3) / (528 - 160)`. Width also differs between these
two measurements, so the approximately 302 ms intercept is a provisional estimate
of the first BUSY-interval floor, not an established waveform or optical floor.
The 792 x 160 versus 712 x 160 results differ by only 2 ms at the same height.

For planning, the lower estimates extrapolate that affine trend; upper estimates
assume no further first-interval reduction below 160 pixels. Transfer time scales
from the measured 13.6 ms per plane at 160 pixels. The total estimates assume the
second wait continues to resemble the first and retain approximately 0.8 ms of
other overhead.

| Geometry | First DRF interval | NEW + OLD estimate | Total with both waits retained |
| --- | ---: | ---: | ---: |
| 712 x 120 | approximately 320–326 ms | approximately 20.4 ms | approximately 662–674 ms |
| 712 x 80 | approximately 314–326 ms | approximately 13.6 ms | approximately 643–667 ms |

These are scenario ranges, not statistical confidence intervals or guaranteed
bounds. The second operation may scale differently or the controller may have a
nonlinear height threshold. Actual measurements may fall outside these ranges.

## F. Exact next physical sequence

No upload has been performed. The built app is archived at
`build/x3-window/height-sweep/firmware.bin` (app offset 0x10000), with a merged
factory image, manifest, and separate reader/SDK source snapshots alongside it.
The previously built firmware and source are preserved in
`build/x3-window/height-sweep/before/`. Use the existing flashing procedure only
when proceeding to the next physical test; this implementation stops before it.

After the user flashes the height-sweep image:

1. Capture controller/VER and confirm UC8253 selection, non-inverted output, and
   normal Settings rendering. Open Settings -> System -> X3 window test.
2. It starts in **C, 712 x 160**. Wait for full cleanup and require `status=0`,
   `baseline_valid=1`. Press Confirm **once** and inspect clearing/exterior.
   Save the entire RESULT, RECT, PANEL, WAVE, WAITS, and HEAP block. Expect one
   explicit refresh and, if the supplied behavior repeats, two waits near 326 ms.
3. Press Right once for **D, 712 x 120**. Wait for the full cleanup/report. Confirm
   the aligned width matches C and height is 120. Press Confirm once, wait for the
   report, and inspect. Save both wait durations and the BUSY mask.
4. Press Right once for **E, 712 x 80**. Repeat the same one-transition inspection
   and capture, checking the aligned width and 80-pixel height.
5. Press Right once for **A, 792 x 528**. After cleanup, perform one manual step.
   Compare its first wait, wait count, total, and transfer times.
6. Press Right once for **B, 792 x 160**. After cleanup, perform one manual step
   and capture both waits. This compares width at the same 160-pixel height.
7. Press Right once to return to **C, 712 x 160**. After cleanup, perform one
   step to check drift relative to step 2. Press Back and verify ordinary full
   Settings/home rendering and restoration of the previous orientation.

At every stage require `fast=1`, `refreshes=1`, `status=0`, `timeout=0`, and
`assertion_missing=0` for a comparable manual step. Exclude full-clean records:
their first wait may include power-on and they may perform several refreshes.
Stop on incorrect addressing, progressive ghosting, contrast loss, exterior
artifacts, unexpected flashing, abnormal BUSY, or any failed status. Do not request
more experimental steps after a failure; use safe full cleanup/recovery.

Keep USB/charging/temperature conditions comparable. Record video at a known
frame rate to separate BUSY/service duration from visible clearing and settling.
Do not run timed playback or 10/25/100-transition batches. If the first sweep is
correct, a second manually supervised sweep can assess repeatability.

## G. 300 WPM and verification

300 WPM requires one completed transition every **200 ms**. The measured first
DRF interval alone is approximately 326 ms; the present total is 681 ms. The
provisional first-interval floor is approximately 302 ms. Current evidence
therefore **does not support 300 WPM through height/width changes alone**, even
if the second interval were eventually proven avoidable. This is not a proof
that every smaller window or controller behavior is incapable of a nonlinear
improvement. The next two heights test that uncertainty; no waveform change is
authorized by these results.

`pio run -e default` succeeded with the existing environment: 57,936 bytes static
RAM and 5,640,279 bytes flash. These are linker values, not peak runtime heap.
Build evidence is `build/x3-window/height-sweep/build.log`. The formatter wrapper
was attempted for reader and SDK changes and reported clang-format unavailable.
Configured `pio check -e default` passed with no defects; its existing SDK
exclusions remain. Diff whitespace and the updated Python harness syntax checks
passed. Host C++ execution remains unavailable in this environment.
No commit, push, flashing, or physical verification was performed in this revision.
