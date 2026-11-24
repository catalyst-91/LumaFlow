# Compatibility Matrix

| Area | Linux x86_64 | Windows 10/11 x64 | macOS Apple Silicon | macOS Intel |
|---|---|---|---|---|
| Build | **Compiled & tested** (Ubuntu 24.04, GCC 13, Qt 6.4) | CI definition (MSVC, Qt 6.7) – not run | CI definition – not run | CI definition – not run |
| Package | .deb, .tar.gz **built & installed**; AppImage via CI | NSIS .exe, .zip (CI) | DMG, signing/notarisation (CI) | DMG (CI) |
| Renderer | OpenGL 3.3 core (tested on Mesa llvmpipe) | OpenGL 3.3 core | OpenGL 4.1 core (deprecated by Apple but supported) | OpenGL 4.1 core |
| Art-Net 4 / sACN / OSC | **tested (loopback)** | portable Qt sockets | portable | portable |
| MIDI devices | ALSA sequencer (not hardware-tested) | not implemented | not implemented | not implemented |
| Audio capture | ALSA (not hardware-tested), WAV, test signal | WAV, test signal | WAV, test signal | WAV, test signal |
| Video decode | FFmpeg (libav) **tested** | FFmpeg (vcpkg) | FFmpeg (Homebrew) | FFmpeg |
| Video outputs | windowed / fullscreen per screen | same | same | same |
| NDI® stream out | runtime loaded at run time (tested with an ABI test double only) | same (not built) | same (not built) | same (not built) |
| Spout stream out | – | SpoutDX / DX11 (CI build, untested) | – | – |
| Syphon stream out | – | – | Syphon Metal server (CI build, untested) | same |
| grandMA3 | GDTF + DMX + OSC (simulated only) | same | same | same |
| ChamSys MagicQ | GDTF / CSV head sheet + DMX + OSC (simulated only) | same | same | same |
| File formats | .lumaflow v3 (migrates v1/v2), .lfpreset, .lfscene, .lfeffect, .lfgraph, GDTF 1.x, MVR 1.x | same | same | same |
