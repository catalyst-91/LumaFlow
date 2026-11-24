# LUMAFLOW 3.0.0 – Release Notes (2026-11-24)

First complete release of the native C++20 / Qt 6 application.

**Highlights**
* 37 built-in GPU effects (particles, energy, geometry, organic, text/logo, audio spectrum/waveform, feedback, test patterns)
* 6-layer compositor with alpha/additive/multiply/screen/masked blending, masks, solo, preview/program with timed TAKE
* Node-based effect editor compiling to GLSL (60+ node types, subgraphs, custom GLSL nodes, live GPU-validated preview)
* Video and image playback via FFmpeg with background decoding, in/out points, loop and cached reverse
* Pixel mapping (strips, matrices with zigzag/start corner, spatial mapping from patch), gamma, limiter, conflict detection
* GDTF import/export (incl. multi-instance geometry), MVR import/export, capability detection, virtual dimmer and RGBW/A conversion
* 3D stage view with live fixture colours; spatial effects with 16 ordering modes, wings/blocks/groups, reproducible random
* Art-Net 4, sACN E1.31, OSC 1.0, MIDI 1.0 (ALSA), MTC and LTC decoding
* grandMA3 / MagicQ integration: auto-generated 16/32/64/128-channel personality (GDTF), approval workflow, versioning, rollback
* Bidirectional mapping with learn, scaling, deadband, smoothing, edge triggers, soft takeover, ownership policies, loop-safe feedback
* Cue lists, scenes, presets, multi-track timeline with keyframes and event tracks
* Audio analysis (FFT bands, beat, BPM), calibration and photosensitive safety limiter
* NDI®, Spout and Syphon stream outputs (program or output region), plus output-module plugins (example frame grabber)
* Remote web UI with token roles, multi-instance UDP sync, plugin SDK (C ABI) with out-of-process probing, local AI assistant
* Autosave, versioned backups, crash recovery, last-known-good, render watchdog, safe mode
* 9 automated test suites (99 test functions) incl. GPU, network loopback and plugin isolation tests

See KNOWN_LIMITATIONS.md for what is not yet verified.
