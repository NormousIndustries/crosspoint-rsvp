# X3 RSVP v0.1A hardware test

The window prototype adds buffered `PANEL`/`WAVE` instrumentation to this
full-frame test and suppresses SDK wait messages during captured calls. See
[the window prototype handoff](x3-window-refresh-prototype.md) for current timing
definitions and the separate manual-only settings entry. The original v0.1A
description below documents the pre-instrumentation behavior.

This experiment measures the stock synchronous X3 display path. It does not
establish that the panel can deliver 300, 375, or 500 WPM until it is tested on
physical hardware. The baseline is CrossPoint 1.6.5 (`93e98bb7`) and FreeInk SDK
`111fdcc7`. Build environment and display drivers are unchanged.

## Implementation

- Settings → System → RSVP display test is visible only on detected X3 hardware.
- `RsvpDisplayTestActivity` uses ActivityManager's existing render task and
  RenderLock. It does not derive from ReaderActivity. Settings already uses
  UiAppHost; the test itself has no touch interaction table to host and uses
  UITheme's existing drawing helpers and UI-scale font.
- The activity starts paused at 300 WPM and word 1, `Morning`. It has a fixed
  English passage of exactly 100 words, stored through the English translation
  source with English fallback for other device languages.
- LandscapeCounterClockwise is temporary. Previous orientation and render mode
  are restored on exit. Layout uses runtime dimensions and oriented bezel margins.
- Entry performs one unmeasured HALF refresh to establish a clean B/W baseline.
  Play/pause and manual controls repaint the chrome outside the measured run.
  Each measured word clears only the fixed word rectangle, draws the new word,
  and submits one blocking `HalDisplay::displayBuffer(FAST_REFRESH)` call.
  There is no separate blank submission, asynchronous refresh, or second framebuffer.
- These are **full-frame stock-driver submissions**, not rectangular panel refreshes.
  The renderer rectangle limits framebuffer drawing only. The stock driver may
  promote a requested FAST waveform when controller state requires recovery.
- Playback and pending display work are separate. An atomic pause flag cancels
  unsubmitted timed work; a submitted synchronous call may complete. Displayed
  index changes only after the call returns. Main-loop controls use a nonblocking
  RenderLock attempt and retain pending control flags during a refresh.
- The next deadline is actual render start plus the requested interval. When
  service exceeds that interval, the next word is due as soon as the panel is
  available. Indices still advance by one; there are no skipped slots or bursts.
- The sample array is inline in the nothrow-allocated activity. Its 99 entries
  hold five 64-bit timestamps and an index (4,752 bytes on this target). A stack
  array would exceed the stack limit; a global array would retain RAM after exit.
  Destruction frees the activity and its measurements. There are no per-word
  string/vector allocations or SD writes in the test.

## Flash and capture logs (PowerShell)

Charge the reader and connect the **X3** with a USB data cable. Close any serial
monitor before uploading. Run from `C:\ai\Crosspoint-RSVP\crosspoint-rsvp`:

```powershell
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$pioPython = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $pioPython -m platformio device list
```

Identify the reader's USB serial port by comparing the list before/after
connecting it. The ports seen during development were Bluetooth COM3 and COM4;
neither was an attached reader. Substitute the actual reader port below:

```powershell
$readerPort = 'COM7' # Replace with the X3 USB port reported above.
& $pioPython -m platformio run -e default -t upload --upload-port $readerPort
& $pioPython -m platformio device monitor --port $readerPort --baud 115200 2>&1 |
    Tee-Object -FilePath .pio\rsvp-v0.1a-serial.txt
```

PlatformIO selects the configured bootloader/partition/app offsets. The app-only
artifact is `.pio\build\default\firmware.bin` (app offset `0x10000`); the merged
image is `.pio\build\default\firmware.factory.bin` (offset `0x0000`). Do not use
the factory image at the app offset. No upload or erase was performed by Codex.
If upload cannot connect, use your known-good X3 download-mode procedure and
retry. Exit the monitor with Ctrl+C before another upload.

## Physical test procedure

1. Open Settings → System → RSVP display test. Confirm landscape, the word
   `Morning`, and `Paused | 300 WPM | 1/100`. Capture the entry identity log.
2. Press Right, then Left. Confirm exactly one adjacent word per action. Check
   that the old word is cleared without a separate blank-screen refresh.
3. Press Back. Confirm safe return to Settings in the previous orientation.
   Re-enter the test so the measured passage begins at word 1.
4. Press Confirm to play at 300 WPM. Do not touch buttons, unplug USB, change
   charging state, force refresh, or use the screenshot shortcut during the run.
   Let it finish at word 100 (`view.`) and wait for the serial dump and paused UI.
   A full run should report 99 completed transitions, with words 2 through 100.
5. Press Up once to select 375 WPM. This also pauses. From word 100, Confirm
   restarts the passage at word 1. Let it finish and capture the complete dump.
6. Press Up once for 500 WPM and repeat. Requested intervals are respectively
   200,000, 160,000, and 120,000 microseconds. Do not infer delivered speed from
   the selected setting.
7. Repeat each full run at least once to distinguish consistent behavior from
   an outlier. Up cycles 300 → 375 → 500 → 300; Down cycles in reverse. Both
   pause and close the current measurement run before changing speed.
8. Separately test Confirm pause/resume, Left/Right during playback, and Back
   during playback. A committed refresh may finish once after the button
   release; queued timed work must stop. Wait for report output to finish before
   entering another command. Each resume creates a fresh measurement run.
9. Re-enter/exit several times and check for crashes or a persistent decline
   in free heap/largest block. Exit logs are taken before ActivityManager frees
   the activity, so the activity's allocation is still included in those values.

## Diagnostics and interpretation

All RSVP timestamps and durations are microseconds; timestamps are monotonic
time since boot. RSVP diagnostics use `LOG_INF` with origin `RSVP`.

- Entry: board name, controller (UC8253/UC8279), controller variant, native
  panel dimensions, activity size, sample storage size, and submission path.
- `SUMMARY`: selected WPM, requested interval, completed transitions, missed
  deadline count, delivered WPM, and overall WPM.
- `TIMING`: mean raster/display/service time, maximum service, maximum lateness.
- `CADENCE`: minimum/maximum completion-to-completion intervals and pair count.
- `HEAP`: minimum sampled free internal 8-bit heap and largest block, last
  values, plus the boot low watermark (sum of per-region lows since boot).
  Run minima are sampled at setup and after each display call; they cannot
  capture transient allocations inside the call. The separate boot watermark
  covers transient lows but is not exclusive to this run.
- `BASELINE`: the unmeasured setup render start and display return.
- One `STAMP` and `DELTA` record per completed transition: intended deadline,
  render-task start, raster completion, display-call start/return, raster time,
  display time, total service, start-to-start/completion-to-completion intervals,
  lateness, and a deadline-miss flag.

Raster time = raster completion minus render start. Display time = display return
minus call start. Service = display return minus render start. Lateness = positive
render start minus intended deadline. A miss means the display call returned after
intended deadline + requested interval: the transition was not complete before the
following scheduled slot. The count is one per affected transition, not the number
of skipped intervals; no intervals are skipped.

Delivered WPM = `60,000,000 × (N−1) / (last return − first return)`, using adjacent
completed timed words. Overall WPM includes the initial scheduled hold and uses
`60,000,000 × N / (last return − setup return)`. Runs with fewer than two completed
transitions cannot establish steady delivered cadence. The first DELTA interval
uses the unmeasured setup baseline; later intervals compare timed transitions.

RSVP makes no serial writes during the timed transactions. **Stock SDK BUSY-wait
serial messages remain inside the display call**, and their overhead is included
in display/service times. This is a stock-path measurement with serial attached,
not an isolated optical waveform measurement. Keep the monitor attached for all
comparable runs. The synchronous HAL API returns no refresh-success status; a
driver BUSY timeout may still return, so logs must be checked against visible panel
behavior. Counted completion means display-call return, not optically verified text.

## Report back

Send the serial capture and a row for each full run:

| Run | Selected WPM | Completed | Missed | Delivered WPM | Mean/max service | Completion min/max | Min free/largest |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 300 | | | | | | |
| 2 | 375 | | | | | | |
| 3 | 500 | | | | | | |

Include controller/variant, whether the panel visibly advances every word,
ghosting/readability, any blank flashes or full-screen flashing, pause/exit
responsiveness, orientation restoration, freezes/reboots/timeouts, and whether
memory remains stable across repeated entries. A short video of each speed helps
compare optical behavior with call-return timestamps.

Continue to v0.1B only if measured cadence and visible readability support the
required speeds. A 500 WPM selection with much lower delivered WPM is a failure
to meet that target. If the stock path is too slow, stop RSVP application work and
scope a separate X3 driver/windowed-refresh investigation.

## Risks and boundaries

This firmware has not been flashed or hardware-tested here. Repeated FAST refreshes
may cause ghosting or unreadable transitions; observe the panel and stop if needed.
Auto-sleep is inhibited while the test is open, including when paused; exit after
testing. Stock global power/home actions still apply. A driver BUSY timeout can
delay exit longer than a normal refresh. Keep the known-good baseline firmware
available for rollback, and avoid an erase-all operation.

EPUB parsing, reader positions, persistent settings, SDK sources, PlatformIO
configuration, and fonts are unchanged. No commit, push, or future RSVP phase is
part of this test. The formatting wrapper must be used if formatting is available;
it reported that clang-format was unavailable in this development shell.
