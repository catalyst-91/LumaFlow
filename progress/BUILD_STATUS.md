# Build Status – LUMAFLOW 3.0.0

| Target | Platform | Status | Evidence |
|---|---|---|---|
| lumacore / lumarender / lumaengine (static libs) | Linux x86_64 GCC 13 | ✅ builds, no errors | `cmake --build build` |
| lumaflow (GUI) | Linux | ✅ builds and runs (Xvfb) | screenshots, demo video |
| lumaflow-cli | Linux | ✅ | render-all, benchmark, export-profiles |
| tests (8 suites) | Linux | ✅ 8/8 pass | TEST_RESULTS.md |
| Example plugin (C) | Linux | ✅ | test_plugins |
| Release build + CPack DEB/TGZ | Linux | ✅ `lumaflow_3.0.0_amd64.deb`, `lumaflow-3.0.0-Linux.tar.gz` | installed with apt and run |
| AppImage | Linux | ⛔ blocked locally (B4) – CI job defined | installers/linux/build_appimage.sh |
| Windows NSIS/ZIP | Windows x64 | ⛔ not built (B3) – CI job defined | .github/workflows/build.yml |
| macOS DMG arm64/x86_64 (+sign/notarise) | macOS | ⛔ not built (B3) – CI job defined | installers/macos/package_macos.sh |

Reproduce: see documentation/DEVELOPER_GUIDE.md. Configure options: LUMAFLOW_BUILD_GUI, LUMAFLOW_BUILD_TESTS, LUMAFLOW_WITH_FFMPEG, LUMAFLOW_WITH_ALSA.

## Release gates
| Gate | Status | Evidence / gap |
|---|---|---|
| 1 Application foundation | ✅ PASS | app launches, UI renders, project storage tests, reproducible CMake build |
| 2 Rendering | ✅ PASS | 37 effects render, parameters tested, preview/program, invalid shader isolation tests |
| 3 Show control | ✅ PASS | presets/scenes persist & recall, cue lists, timeline automation tests |
| 4 Console integration | ⚠️ PARTIAL | OSC/Art-Net/sACN tested on loopback, MIDI parser tested, GDTF validated & round-tripped; **no real console tests; MIDI device I/O untested** |
| 5 Spatial & pixel mapping | ✅ PASS (software) | MVR/GDTF import, ordering, virtual dimmers, channel output and universe boundaries tested; no physical LEDs |
| 6 Reliability | ✅ PASS | autosave, backup restore, sync reconnect, network bind errors reported, invalid shaders don't crash |
| 7 Distribution | ⚠️ PARTIAL | Linux packages built & installed; Windows/macOS/AppImage only defined in CI |
| 8 Documentation & video | ✅ PASS | manual 82 pp, quick start, script, storyboard, SRT, 11:50 demo video from the real app; console segment labelled simulated |
