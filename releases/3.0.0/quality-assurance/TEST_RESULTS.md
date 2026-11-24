# Test Results – LUMAFLOW 3.0.0 (2026-11-24)

Environment: Ubuntu 24.04 container, GCC 13.3, Qt 6.4.2, FFmpeg 6.1 libs, Xvfb :99 + Mesa 25.2 llvmpipe (OpenGL 4.5 core, CPU rasteriser, 4 cores). No GPU, sound card, MIDI device, console or lighting hardware.

## Automated suites (`ctest`, build type RelWithDebInfo)
| Suite | Cases | Result | Notes |
|---|---|---|---|
| test_protocols | 15 | PASS | Art-Net, sACN (638-byte packet layout), OSC, MIDI running status/realtime/SysEx, drop-frame TC, MTC, LTC encode→decode across hour boundary |
| test_fixtures | 12 | PASS | capabilities, 16-bit DMX, universe spanning, patch validation, ZIP, GDTF multi-instance import, GDTF export round trip & validator, MVR matrix/round trip/warnings, colour engine |
| test_mapping_spatial | 13 | PASS | pixel-aligned/continuous universe split, matrix zigzag, sampling/order/limiter/gamma, virtual vs physical dimmer, conflicts, test patterns, spatial ordering/seed/wings/blocks/exclusion/mixed capabilities |
| test_console | 12 | PASS | personality prefixes & footprints, custom mode, GDTF validity, diff/approval/breaking/rollback/persistence, DMX decode, mapping scaling/deadband/smoothing/soft takeover/edges/ownership/feedback loop/learn |
| test_show | 13 | PASS | keyframes, timeline events+loop+snap, cue follow, project save/backups/restore, migrations & newer-schema refusal, dependency/relink, node graphs, FFT/beat/BPM (120±3), WAV, AI local generator, registry |
| test_engine_gpu | 14 | PASS | real GPU path: render, master/blackout, invalid shader isolation, last-good program, blend/mask/solo, scene crossfade midpoint, preview/take, strobe limiter (≤3 Hz), calibration, text/image layers, node effect on GPU, pixel map DMX from render, demo show save/load round trip, console export round trip |
| test_integration | 9 | PASS | loopback: Art-Net out (universe gating, blackout, keep-alive rate), sACN unicast out, OSC control & console OSC mapping, OSC feedback, Art-Net in → personality decode, remote HTTP 200/401/403/400, multi-instance sync (snapshot, deltas, events, RTT, loss, reconnect), crash detection, autosave, watchdog |
| test_plugins | 3 | PASS | out-of-process probe + load of example plugin, disabled/safe mode, crashing plugin isolated |
| **Total** | **91 test functions** (plus init/cleanup) | **8/8 suites pass** | integration suite repeated 3× without failure after RTT fix |

## Manual / end-to-end checks
| Check | Result |
|---|---|
| `lumaflow-cli render-all` – 37 effects compile and render, contact sheet reviewed | PASS |
| GUI launch, 18 workspace screenshots, clean exit (`--screenshots`) | PASS |
| Scripted 11:50 demo run of the real GUI (`--demo-script`) | PASS (see video) |
| `.deb` built with CPack, installed with apt, `lumaflow-cli info`, render, GUI start/quit from installed package | PASS |
| GDTF personality re-import: footprints 16/32/64/128 | PASS |
| Example node graphs compile (`compile-graph`) | PASS (3/3) |
| Demo project / MVR export / re-import via CLI | PASS |
| Benchmark 1920×1080 llvmpipe: median 18.8 fps/effect, min 2.1 fps (particle_explosion) | recorded; 60 fps target NOT verified (no GPU) |

## Defects found and fixed during verification
* Texture orientation (text/media upside down) – fixed, verified visually.
* Static-library Qt resources not initialised – fixed (`lf::initResources`).
* Sync RTT could read 0 ms on loopback (flaky test) – nanosecond timing + `rttValid`.
* AI keyword priority (generic "particle" vs "vortex") – fixed.
* Window larger than screen / exit blocked by save prompt in automation – fixed.
* Advanced mode only 90 channels – catalogue extended (6 layers) to 128, append-only.
* Console export did not overwrite existing grandma3/magicq GDTF copies (QFile::copy) – fixed, regression check added.
* Demo console simulator sent zeros on unset channels (black output) – simulator now sends current values like a console.

## Not tested (see BLOCKERS.md)
Real consoles, real fixtures/nodes, hardware GPU performance, Windows/macOS builds, MIDI/audio devices, external AI API, AppImage execution.
