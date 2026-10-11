# Archived X3 display experiments

Archived during V1B.1 finalisation on 11 October 2026 (Australia/Sydney).
Both development activities have been removed from the production Settings UI.
Their exact pre-cleanup source is retained as `.cpp.txt` / `.h.txt`, so PlatformIO
does not compile it. `development-ui-strings.yaml.txt` retains the old English
strings, including the original 100-word passage. The passage used by host raster
checks now lives at `test/rsvp/representative_passage.txt`.

The raw `x3-height-sweep.txt`, `x3-window-test-serial.txt` and
`rsvp-v0.1a-serial.txt` files are byte-for-byte copies of the existing `.pio` logs.
The height-sweep capture is UTF-16 LE; retain that encoding when reading it.
The original guides remain at [window prototype](../../x3-window-refresh-prototype.md)
and [height sweep](../../x3-window-height-sweep.md). Their test-launch instructions
describe historical firmware; these activities are no longer in production.
Earlier firmware/source archives under `build/x3-window/height-sweep/` remain intact.

## Captured manual-step results

Only `job=manual-step` records from `x3-height-sweep.txt` are included below;
full-clean records are excluded. These ranges describe these captured samples,
not a guaranteed panel specification or a new performance measurement.

| Aligned geometry | Samples | Total service time | Wait count | First wait | BUSY checkpoints |
| --- | ---: | ---: | ---: | ---: | --- |
| 792 × 528 full-frame FAST | 5 | 478.678–479.191 ms | 1 | 381.282–381.756 ms | mask 00 |
| 792 × 160 full-width window | 3 | 686.819–688.030 ms | 2 | 328.073–328.079 ms | mask 02 |
| 712 × 160 region | 3 | 680.659–680.756 ms | 2 | 326.114–326.138 ms | mask 02 |
| 712 × 120 region | 5 | 665.030–665.570 ms | 2 | 321.724–321.793 ms | mask 02 |
| 712 × 80 region | 6 | 648.920–649.573 ms | 2 | 317.383–317.442 ms | mask 02 |

All recorded window manual steps have `post_sync_busy_mask=0x02`: BUSY LOW was
sampled after data-stop, followed by the restoration wait described in the
height-sweep guide. This is sampled electrical evidence, not proof of a second
optical refresh or permission to remove a wait. The smaller-window paths remain
slower in total than the existing full-frame FAST path in this capture.

## Preserved driver code

No SDK, HAL, panel timing, waveform, measurement, or production FAST code was
changed by finalisation. Window measurement/restoration code is interleaved with
the existing panel drivers and their host tests; removing it would broaden this
cleanup into a driver change without independent physical validation. It remains
available for future research, with no production menu entry to invoke the old
experiment. No V1C development was started.
