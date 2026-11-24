# LUMAFLOW MIDI Reference

* Linux: ALSA sequencer. LUMAFLOW creates virtual ports **LUMAFLOW In** / **LUMAFLOW Out**; connect them with `aconnect` or your DAW.
* Windows (WinMM) and macOS (CoreMIDI) device backends are **not implemented in this release** (see `progress/BLOCKERS.md`). The parser, mapping engine and MTC decoder are platform-independent.

## Mapping keys
| Message | Key format | Normalised value |
|---|---|---|
| Control change | `cc:<channel 0-15>:<controller>` | value / 127 |
| Note on/off | `note:<channel>:<note>` | velocity / 127 (note off = 0) |
| Program change | `pc:<channel>` | program / 127 |
| Pitch bend | `pb:<channel>` | 14-bit / 16383 |
| Channel pressure | `at:<channel>` | value / 127 |

Use **Learn** in Console Mapping to create mappings by moving a control. Per mapping: input/output range, invert, deadband, smoothing,
trigger edge (rising/falling/both) with threshold, soft takeover and feedback (CC/note velocity sent back to the controller).

## Timecode
MIDI Time Code quarter frames and full-frame SysEx are decoded (24, 25, 29.97 DF, 30 fps). Select *mtc* as the timeline TC source.
The quarter-frame decoder compensates the 2-frame assembly latency.

## Default example mappings (`examples/mappings_example.json`)
CC1 → layer 1 speed (smoothed) · CC7 → master (soft takeover) · Note 36 → cue GO · Note 37 → tap tempo.
