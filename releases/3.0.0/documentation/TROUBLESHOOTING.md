# Troubleshooting

| Symptom | Cause | Action |
|---|---|---|
| "GPU renderer failed: cannot create OpenGL 3.3 core" | driver too old / remote session / VM | Update GPU driver; on Linux headless use Xvfb + Mesa; start with `--safe-mode` to reach settings |
| Preview black, Recovery shows *shader failed to compile* | custom or AI shader error | Fix shader; other layers keep running; previous working version is kept |
| Recovery shows *render thread stalled* | very heavy shader / GPU hang | Lower quality (Performance), remove last effect; outputs hold last frame |
| No DMX on fixtures | universe not enabled for output | Output Configuration → *Output universes*; LUMAFLOW never sends to unlisted universes |
| Art-Net universe off by one | console uses 0-based numbering | *Art-Net universe offset* 1 (default: LUMAFLOW U1 = Art-Net 0) or 0 |
| Fixtures flicker between two values | two sources sending same universe | Check diagnostics *Last source*; sACN priority; disable duplicate output |
| Pixel colours in wrong order | strip uses GRB/BRG | Pixel Mapper → Order column |
| Console changes ignored | ownership policy / soft takeover | Console Mapping → Ownership; move the fader through the current value |
| Feedback loops / jumping faders | feedback to the same source | Feedback is loop-protected per mapping; disable *Feedback* on that mapping |
| OSC not received | port in use / firewall | Change OSC listen port; allow UDP 8000; check Recovery for "cannot bind" |
| Remote page says 401 | missing token | Use a token from Remote & Sync; tokens regenerate each launch unless saved |
| Video file shows nothing | missing media / codec | Media Library shows MISSING → Relink; check codec with `ffprobe` |
| Reverse playback ignored | clip too long for cache | Use clips ≤ 900 frames or render reversed media |
| Project will not open: "saved by a newer LUMAFLOW" | schema newer than this build | Update LUMAFLOW; file is never overwritten |
| Crash on startup after installing plugin | plugin fault | Start with `--safe-mode`, remove plugin in Plugin Manager |
| Recover after crash | – | Restart: LUMAFLOW offers the autosave; Recovery panel lists backups and last-known-good |

Logs: `<data dir>/lumaflow.log` (Help → Open data folder). Linux data dir: `~/.local/share/LUMAFLOW/LUMAFLOW`.
