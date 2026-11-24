# Engineering decisions

1. **Qt Widgets instead of QML** for the main UI: dense professional panels, works headless under Xvfb for automated screenshots and video, and avoids a second rendering path. QML remains possible for touch surfaces.
2. **OpenGL 3.3 core via Qt OpenGL** (ES 3.0 fallback) instead of Qt RHI: effects are user-editable GLSL; RHI would require offline shader baking (qsb) for every user/AI/node shader. Metal/D3D/Vulkan backends are future work; macOS uses its OpenGL 4.1 core implementation.
3. **Render thread with immutable snapshots** and readback of the program frame each frame: one frame serves monitors, output windows, pixel mapping and recording. Cost documented (KNOWN_LIMITATIONS).
4. **Single parameter registry** with stable ids as the contract between UI, consoles, OSC, MIDI, timeline, remote and sync.
5. **Append-only personality catalogue**; lower modes are prefixes of higher modes; operator approval for every profile change; no silent console modification.
6. **GDTF as the console exchange format** for both grandMA3 and MagicQ; no fabricated proprietary formats (.hed) – CSV head sheet instead.
7. **Index channels use fixed 8-value ranges** so adding entries keeps existing DMX values meaning the same entry (count change still flagged).
8. **Pixel-aligned universe splitting by default**; only explicitly enabled universes are transmitted.
9. **Out-of-process plugin probe** using lumaflow-cli; plugins are not claimed to be sandboxed after load.
10. **Local rule-based AI** as default; external model optional and explicit; generated code always validated in an isolated program before activation.
11. **JSON project files** with schema versions and in-memory migration; refuse newer schemas.
12. **MIT licence**; Qt and FFmpeg dynamically linked (LGPL).
13. **Six layers** default composition to fill the 128-channel Advanced personality with meaningful parameters.
14. **Demo narration via espeak-ng** (no licensed voice available offline); no background music to avoid licensing issues.
