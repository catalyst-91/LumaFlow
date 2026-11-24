# LUMAFLOW OSC Reference

LUMAFLOW listens for OSC 1.0 over UDP (default port **8000**, Output Configuration → OSC listen port).
Messages and bundles are supported; address patterns (`*`, `?`, `[a-z]`, `{a,b}`) are matched by mappings.
Native commands below are always available; console-specific addresses are added through Control Mapping.

| Address | Arguments | Effect |
|---|---|---|
| `/lumaflow/master/intensity` | f 0..1 | Master intensity |
| `/lumaflow/param/<id>` | f 0..1 (normalised) | Any registry parameter, e.g. `/lumaflow/param/layer1.speed 0.5` |
| `/lumaflow/param/<id>/raw` | f (native units) | Same, in the parameter's own range |
| `/lumaflow/layer/<n>/effect` | s effect id, or i library index | Select the effect on layer n (1-based) |
| `/lumaflow/layer/<n>/<param>` | f 0..1 | Layer parameter (opacity, speed, scale, rotation, posx, posy, hue, hue2, saturation, brightness, contrast, p1..p4, audio, blend) |
| `/lumaflow/scene/recall` | i (1-based) | Recall scene with its transition |
| `/lumaflow/scene/store` | i [s name] | Store program into scene slot |
| `/lumaflow/preset/recall` | i preset [i layer] | Recall layer preset |
| `/lumaflow/cue/go`, `/lumaflow/cue/back` | – | Cue list navigation |
| `/lumaflow/cue/goto` | f cue number | Jump to cue |
| `/lumaflow/take` | – | Preview → program with transition |
| `/lumaflow/blackout` | i 0/1 | Output blackout |
| `/lumaflow/tap` | – | Tap tempo |
| `/lumaflow/bpm` | f | Set show tempo |
| `/lumaflow/timeline/play`, `/stop`, `/seek` | [f seconds] | Timeline transport |
| `/lumaflow/timecode` | s "hh:mm:ss:ff" | External timecode position (TC source = mtc/ltc chase) |
| `/lumaflow/status/request` | [i reply port] | Replies with `/lumaflow/status/scene` |

## Feedback (LUMAFLOW → console / controller)

Sent to every *OSC feedback target* (Output Configuration) and to each console link's OSC host/port:

| Address | Value |
|---|---|
| `/lumaflow/status/scene` | active scene index |
| `/lumaflow/status/cue` | active cue number |
| `/lumaflow/status/param/<id>` | parameter value changed by any non-OSC source |
| `/lumaflow/status/output` | "live" / "blackout" |
| mapping address | mapped value echoed for motorised/LED controls (loop-protected) |

Feedback is informational. It does **not** change a console's programmer, playbacks or internal fixture state.

## Example (command line)
```
lumaflow-cli send-osc 127.0.0.1 8000 /lumaflow/scene/recall 3
lumaflow-cli send-osc 127.0.0.1 8000 /lumaflow/layer/1/effect plasma
```
grandMA3 macro line: `SendOSC 1 "/lumaflow/cue/go"` (configure OSC line 1 in Menu → In & Out → OSC).
