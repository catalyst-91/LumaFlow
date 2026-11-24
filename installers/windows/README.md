# Windows installer

Built by CI (`.github/workflows/build.yml`, job `windows`): MSVC 2019 x64 + Qt 6 + FFmpeg from vcpkg,
`windeployqt`, then `cpack -G "NSIS;ZIP"`. The NSIS installer creates a Start-menu entry, an optional
desktop shortcut and an uninstaller (CPack NSIS settings in the top-level `CMakeLists.txt`).
`lumaflow.ico` is the application icon. MIDI on Windows: the WinMM backend is listed as future work
(see progress/BLOCKERS.md); the ALSA backend is Linux-only.
