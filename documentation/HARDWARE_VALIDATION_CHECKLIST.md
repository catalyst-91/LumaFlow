# Hardware Validation Checklist

Complete on real equipment before a production. Record software versions, results and the date.

| # | Test | Equipment | Expected | Result |
|---|---|---|---|---|
| 1 | `lumaflow-cli info` reports a hardware GPU (not llvmpipe) | show machine | GPU name + GL ≥ 3.3 | |
| 2 | `lumaflow-cli benchmark --width 1920 --height 1080` | show machine | ≥ 60 fps for chosen effects | |
| 3 | Fullscreen output on each display, rotation and crop | displays / LED processor | correct region, no tearing | |
| 4 | Art-Net output to node, test pattern *identify* | Art-Net node + fixtures | correct fixture lights, order | |
| 5 | sACN multicast output, priority vs. console | sACN network + console | expected merge | |
| 6 | Pixel strip universe boundaries (≥ 171 px RGB) | LED strip + controller | no split pixels | |
| 7 | Import LUMAFLOW_MediaServer.gdtf on grandMA3 | grandMA3 onPC/console (record version) | 4 modes, footprints 16/32/64/128 | |
| 8 | grandMA3 DMX control of master/effect/scene | grandMA3 → Art-Net/sACN | parameters follow, no jitter | |
| 9 | grandMA3 OSC (`SendOSC`) cue go / scene | grandMA3 | actions execute | |
| 10 | Import GDTF or build head on MagicQ from CSV | MagicQ PC/console (record version) | footprints match | |
| 11 | MagicQ DMX + OSC control | MagicQ | parameters follow | |
| 12 | Both consoles simultaneously on separate addresses | both | independent control, ownership policies hold | |
| 13 | MIDI controller learn, feedback LEDs / motor faders | MIDI controller (Linux) | mapping + feedback, no loop | |
| 14 | MTC chase from DAW / LTC from timecode generator | TC source | timeline locked; measure offset | |
| 15 | Audio input from mixer, beat/BPM detection | audio interface | stable BPM ±1 | |
| 16 | Remote control from tablet over Wi-Fi | tablet | roles enforced, reconnect works | |
| 17 | Two-instance sync primary/secondary, unplug cable | 2 machines | secondary holds, re-syncs | |
| 18 | Kill process during show → restart → recover autosave | show machine | recovery prompt, state restored | |
| 19 | Photosensitive limiter (strobe) measured with photodiode | light meter | ≤ configured Hz | |
| 20 | 8-hour soak test with recording off/on | show machine | no leaks, no stalls (watchdog log) | |
