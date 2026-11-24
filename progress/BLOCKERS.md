# Blockers (external – work continued on independent tasks)

| ID | Blocker | Impact | Required to resolve |
|---|---|---|---|
| B1 | No physical grandMA3 / MagicQ console or onPC software in the environment | Console integration verified only with simulated Art-Net/OSC senders | Run HARDWARE_VALIDATION_CHECKLIST items 7–12 on real consoles |
| B2 | No hardware GPU (Mesa llvmpipe only) | 60 fps @ 1080p unverified; benchmarks are CPU-rendered | Benchmark on target show hardware |
| B3 | No Windows or macOS build hosts, no Apple signing certificates | Windows/macOS installers not built; CI workflow untested | Run `.github/workflows/build.yml`; provide MACOS_CERT_* / APPLE_* secrets |
| B4 | Execution of downloaded third-party AppImage tools not permitted in this session | AppImage not produced locally | CI job `linux` builds it, or run `installers/linux/build_appimage.sh` locally |
| B5 | No /dev/snd (no MIDI sequencer, no audio card) | ALSA MIDI/audio I/O not exercised | Test with a MIDI controller and audio interface |
| B6 | MagicQ .hed format not publicly specified | No native head file; GDTF + CSV channel sheet instead | Vendor documentation or validation with MagicQ Head Editor |
| B7 | No LED fixtures, Art-Net nodes, displays | Output verified by decoding packets/frames only | Hardware checklist items 3–6 |
| B8 | No API key for external AI | External AI backend untested | Set ANTHROPIC_API_KEY and test AI Assistant → external |
| B10 | NDI runtime/receiver not installed (proprietary licence must be accepted by the user) | NDI sender tested with an ABI test double only | Install NDI Tools, enable an NDI stream, check with NDI Studio Monitor |
| B9 | Not implemented: WinMM / CoreMIDI MIDI backends, Windows/macOS audio capture, SDI outputs, async PBO readback | Feature gaps on those platforms | Future work (TASK_QUEUE.json) |
