# Known Limitations (LUMAFLOW 3.0.0)

Honest list of what is *not* verified or not implemented in this release.

## Not verified on real hardware
* **No physical lighting console was available.** grandMA3 and ChamSys MagicQ integration is implemented with documented
  protocols (Art-Net 4, sACN E1.31, OSC 1.0, MIDI 1.0) and tested against simulated senders/receivers on the loopback
  interface only. Console OSC address templates (`/gma3/...`, `/pb/...`) are starting points that must be checked on the console.
* The generated **GDTF** personality is validated structurally (well-formed XML, unique offsets, defined attributes, GUID,
  re-import round trip with matching footprints). It has **not** been imported into grandMA3 or MagicQ software yet, and the
  official GDTF XSD was not run.
* **No native MagicQ .hed head file** is produced: the .hed format is not publicly specified, and emitting an unverified file
  would be misleading. MagicQ users import the GDTF (versions with GDTF support) or build the head in the MagicQ Head
  Editor from the provided channel sheet (`fixture-profiles/magicq/*.csv`).
* No LED fixtures, LED processors, Art-Net nodes or video output hardware (SDI/HDMI capture) were connected. DMX output was
  verified by decoding the transmitted packets.
* MIDI device I/O (ALSA) could not be exercised: the build container has no `/dev/snd/seq`. MIDI parsing, mapping, learn and
  MTC decoding are unit tested.
* Audio capture via ALSA could not be exercised (no sound card); analysis was tested with synthetic signals and WAV files.

## Performance
* All rendering in this build environment used **Mesa llvmpipe (CPU software rasteriser, 4 cores)**. Measured median ~19 fps per
  effect at 1920×1080 (see `benchmark_llvmpipe_1080p.json`); the GUI runs at 30–45 fps at 1280×720 with adaptive quality.
  The 60 fps @ 1080p target is **not verified** – it requires a hardware GPU. Run `lumaflow-cli benchmark` on target hardware.
* The program frame is read back from the GPU every frame (needed for pixel mapping, recording and output windows). On
  discrete GPUs a PBO-based asynchronous readback would reduce latency; not implemented.

## Platform coverage
* Only the **Linux x86_64** build was compiled and tested here. Windows (NSIS/ZIP) and macOS (Apple Silicon + Intel DMG,
  signing/notarisation) builds are defined in `.github/workflows/build.yml` but were **not executed** in this environment.
* MIDI backends for Windows (WinMM) and macOS (CoreMIDI) are not implemented; audio capture backends for those platforms are not
  implemented (test signal and WAV file inputs work everywhere).
* The **AppImage** packaging script exists (`installers/linux/build_appimage.sh`) and runs in CI; it was not run locally because
  the third-party AppImage tools could not be executed in this environment. `.deb` and `.tar.gz` packages were built and installed.

## Functional limitations
* Synchronisation between instances is LAN/UDP based (typically a few ms); it is **not frame-accurate** and has no genlock.
* LTC/MTC sync accuracy has not been measured against a reference clock; frame-accurate timecode chase is **not claimed**.
* **NDI** stream output is implemented by loading the user-installed NDI runtime; it was verified only against a test double of the runtime (ABI-level checks), not with the real NDI runtime and a receiver. The NDI runtime is not bundled.
* **Spout** (Windows, SpoutDX/DirectX 11) and **Syphon** (macOS, Metal) stream outputs are implemented against the real SDK APIs (Spout2 pinned commit 2bd42d2, Syphon pinned commit f476167) but are compiled only in the Windows/macOS CI builds, which were not run here. Syphon's `flipped:` orientation is set for top-down frames and needs confirmation with a Syphon client.
* SDI / capture-card outputs are not bundled; use an output-module plugin.
* Effect selection over DMX is limited to the first 32 library entries per index channel (8 values per entry); further effects are
  selectable by OSC, MIDI mapping or the UI.
* Video reverse playback requires clips that fit the frame cache (≤ 900 frames, < 768 MB); longer clips play forward only.
* Native plugins run in-process after an out-of-process probe; they are not sandboxed once loaded.
* The external AI backend (Anthropic Messages API) is implemented but untested here (no API key); the local generator is rule-based.
* MVR export writes fixtures, positions, rotations, addresses and embedded GDTF types; groups are kept in the project but not
  written as MVR GroupObjects.
* The 3D stage is a software-projected wireframe/point view, not a photometric beam visualiser.
