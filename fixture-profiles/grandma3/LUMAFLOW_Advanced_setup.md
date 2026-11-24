# grandMA3 setup - LUMAFLOW Advanced 128ch (v1)

1. Copy `LUMAFLOW_MediaServer.gdtf` to a USB stick under `grandMA3/shared/resource/fixturetypes/` or to the console's library folder.
2. Menu > Patch > Fixture Types > Import > select *LUMAFLOW Media Server* > mode **Advanced 128ch**.
3. Insert one fixture at the DMX address configured in LUMAFLOW (Console Integration > DMX input), footprint 128 channels.
4. In LUMAFLOW enable Art-Net or sACN input for that universe. grandMA3 DMX output: Menu > DMX Protocols > Art-Net/sACN, set the universe to Output.
5. Program effects with the fixture's attributes (Effects1 = effect select on layer 1, Dimmer = master intensity, etc.) and store cues as usual.

OSC (optional): Menu > In & Out > OSC: add a line with LUMAFLOW's IP, port 8000, Send enabled. Use `SendOSC` commands in macros, e.g.
`SendOSC 1 "/lumaflow/scene/recall,i,3"` or `SendOSC 1 "/lumaflow/cue/go"`.
LUMAFLOW feedback is sent as OSC to configured targets; grandMA3 can receive it when OSC Receive is enabled, but feedback does not
change the console programmer or playback state. (Verify these menu paths against your grandMA3 software version.)

| Ch | Fine | Parameter | Name | GDTF attribute | Ranges |
|---:|---:|---|---|---|---|
| 1 | 2 | `master.intensity` | Master Intensity | Dimmer |  |
| 3 |  | `layer1.effect` | Layer 1 Effect | Effects1 | 0-7 Particle Cloud, 8-15 Particle Explosion, 16-23 Particle Storm, 24-31 Particle Vortex, 32-39 Particle Trails, 40-47 Particle Tunnel, 48-55 Attract / Repel, 56-63 Audio Particles, 64-71 Plasma, 72-79 Lightning, 80-87 Electric Arcs, 88-95 Energy Rings, 96-103 Pulsating Field, 104-111 Neon Lines, 112-119 Kaleidoscope, 120-127 Julia Fractal, 128-135 Mandala, 136-143 Geometric Tunnel, 144-151 Rotating Pattern, 152-159 Morphing Geometry, 160-167 Wireframe Terrain, 168-175 Smoke, 176-183 Fluid, 184-191 Ink Clouds, 192-199 Flow Field, 200-207 Turbulence, 208-215 Organic Morph, 216-223 Glitch, 224-231 Strobe, 232-239 Gradient, 240-247 Text, 248-255 Logo / Image |
| 4 |  | `layer1.opacity` | Layer 1 Opacity | Effects1Adjust1 |  |
| 5 |  | `layer1.speed` | Layer 1 Speed | Effects1Rate |  |
| 6 |  | `layer1.scale` | Layer 1 Scale | Effects1Adjust2 |  |
| 7 |  | `layer1.rotation` | Layer 1 Rotation | Effects1Adjust3 |  |
| 8 |  | `layer1.hue` | Layer 1 Hue shift | Effects1Adjust4 |  |
| 9 |  | `layer1.saturation` | Layer 1 Saturation | Effects1Adjust5 |  |
| 10 |  | `show.scene` | Show Scene | MediaContent1 | 0-7 Plasma Drift, 8-15 Vortex Rings, 16-23 Kaleido Neon, 24-31 Liquid Trails, 32-39 Tunnel Pulse, 40-47 Storm |
| 11 |  | `show.transition` | Show Transition time | Effects1Fade |  |
| 12 |  | `layer2.effect` | Layer 2 Effect | Effects2 | 0-7 Particle Cloud, 8-15 Particle Explosion, 16-23 Particle Storm, 24-31 Particle Vortex, 32-39 Particle Trails, 40-47 Particle Tunnel, 48-55 Attract / Repel, 56-63 Audio Particles, 64-71 Plasma, 72-79 Lightning, 80-87 Electric Arcs, 88-95 Energy Rings, 96-103 Pulsating Field, 104-111 Neon Lines, 112-119 Kaleidoscope, 120-127 Julia Fractal, 128-135 Mandala, 136-143 Geometric Tunnel, 144-151 Rotating Pattern, 152-159 Morphing Geometry, 160-167 Wireframe Terrain, 168-175 Smoke, 176-183 Fluid, 184-191 Ink Clouds, 192-199 Flow Field, 200-207 Turbulence, 208-215 Organic Morph, 216-223 Glitch, 224-231 Strobe, 232-239 Gradient, 240-247 Text, 248-255 Logo / Image |
| 13 |  | `layer2.opacity` | Layer 2 Opacity | Effects2Adjust1 |  |
| 14 |  | `master.strobe` | Master Strobe | Shutter1Strobe |  |
| 15 |  | `show.control` | Show Control | Control1 | 0-7 Idle, 8-15 Cue Go, 16-23 Cue Back, 24-31 Blackout, 32-39 Tap tempo, 40-47 Freeze, 48-55 Take preview |
| 16 |  | `audio.reactivity` | Audio Audio reactivity | Effects9Adjust1 |  |
| 17 | 18 | `layer1.posx` | Layer 1 Position X | Effects1Adjust6 |  |
| 19 | 20 | `layer1.posy` | Layer 1 Position Y | Effects1Adjust7 |  |
| 21 |  | `layer1.p1` | Layer 1 Param 1 | Effects1Adjust8 |  |
| 22 |  | `layer1.p2` | Layer 1 Param 2 | Effects1Adjust9 |  |
| 23 |  | `layer1.p3` | Layer 1 Param 3 | Effects1Adjust10 |  |
| 24 |  | `layer1.p4` | Layer 1 Param 4 | Effects1Adjust11 |  |
| 25 |  | `layer1.brightness` | Layer 1 Brightness | Effects1Adjust12 |  |
| 26 |  | `layer1.contrast` | Layer 1 Contrast | Effects1Adjust13 |  |
| 27 |  | `layer1.hue2` | Layer 1 Colour B hue | Effects1Adjust14 |  |
| 28 |  | `layer2.speed` | Layer 2 Speed | Effects2Rate |  |
| 29 |  | `layer2.scale` | Layer 2 Scale | Effects2Adjust2 |  |
| 30 |  | `layer2.hue` | Layer 2 Hue shift | Effects2Adjust4 |  |
| 31 |  | `layer3.effect` | Layer 3 Effect | Effects3 | 0-7 Particle Cloud, 8-15 Particle Explosion, 16-23 Particle Storm, 24-31 Particle Vortex, 32-39 Particle Trails, 40-47 Particle Tunnel, 48-55 Attract / Repel, 56-63 Audio Particles, 64-71 Plasma, 72-79 Lightning, 80-87 Electric Arcs, 88-95 Energy Rings, 96-103 Pulsating Field, 104-111 Neon Lines, 112-119 Kaleidoscope, 120-127 Julia Fractal, 128-135 Mandala, 136-143 Geometric Tunnel, 144-151 Rotating Pattern, 152-159 Morphing Geometry, 160-167 Wireframe Terrain, 168-175 Smoke, 176-183 Fluid, 184-191 Ink Clouds, 192-199 Flow Field, 200-207 Turbulence, 208-215 Organic Morph, 216-223 Glitch, 224-231 Strobe, 232-239 Gradient, 240-247 Text, 248-255 Logo / Image |
| 32 |  | `layer3.opacity` | Layer 3 Opacity | Effects3Adjust1 |  |
| 33 |  | `layer2.rotation` | Layer 2 Rotation | Effects2Adjust3 |  |
| 34 |  | `layer2.saturation` | Layer 2 Saturation | Effects2Adjust5 |  |
| 35 |  | `layer2.brightness` | Layer 2 Brightness | Effects2Adjust6 |  |
| 36 |  | `layer2.contrast` | Layer 2 Contrast | Effects2Adjust7 |  |
| 37 |  | `layer2.p1` | Layer 2 Param 1 | Effects2Adjust8 |  |
| 38 |  | `layer2.p2` | Layer 2 Param 2 | Effects2Adjust9 |  |
| 39 |  | `layer2.p3` | Layer 2 Param 3 | Effects2Adjust10 |  |
| 40 |  | `layer2.p4` | Layer 2 Param 4 | Effects2Adjust11 |  |
| 41 |  | `layer2.hue2` | Layer 2 Colour B hue | Effects2Adjust12 |  |
| 42 |  | `layer2.blend` | Layer 2 Blend mode | Effects2Adjust13 | 0-7 0, 8-15 1, 16-23 2, 24-31 3, 32-39 4 |
| 43 | 44 | `layer2.posx` | Layer 2 Position X | Effects2Adjust20 |  |
| 45 | 46 | `layer2.posy` | Layer 2 Position Y | Effects2Adjust21 |  |
| 47 |  | `layer3.speed` | Layer 3 Speed | Effects3Rate |  |
| 48 |  | `layer3.scale` | Layer 3 Scale | Effects3Adjust2 |  |
| 49 |  | `layer3.hue` | Layer 3 Hue shift | Effects3Adjust4 |  |
| 50 |  | `layer3.rotation` | Layer 3 Rotation | Effects3Adjust3 |  |
| 51 |  | `layer3.saturation` | Layer 3 Saturation | Effects3Adjust5 |  |
| 52 |  | `layer3.brightness` | Layer 3 Brightness | Effects3Adjust6 |  |
| 53 |  | `layer3.contrast` | Layer 3 Contrast | Effects3Adjust7 |  |
| 54 |  | `layer3.p1` | Layer 3 Param 1 | Effects3Adjust8 |  |
| 55 |  | `layer3.p2` | Layer 3 Param 2 | Effects3Adjust9 |  |
| 56 |  | `layer3.p3` | Layer 3 Param 3 | Effects3Adjust10 |  |
| 57 |  | `layer3.p4` | Layer 3 Param 4 | Effects3Adjust11 |  |
| 58 |  | `layer3.hue2` | Layer 3 Colour B hue | Effects3Adjust12 |  |
| 59 |  | `layer3.blend` | Layer 3 Blend mode | Effects3Adjust13 | 0-7 0, 8-15 1, 16-23 2, 24-31 3, 32-39 4 |
| 60 | 61 | `layer3.posx` | Layer 3 Position X | Effects3Adjust20 |  |
| 62 | 63 | `layer3.posy` | Layer 3 Position Y | Effects3Adjust21 |  |
| 64 |  | `layer4.speed` | Layer 4 Speed | Effects4Rate |  |
| 65 |  | `layer4.scale` | Layer 4 Scale | Effects4Adjust2 |  |
| 66 |  | `layer4.hue` | Layer 4 Hue shift | Effects4Adjust4 |  |
| 67 |  | `layer4.effect` | Layer 4 Effect | Effects4 | 0-7 Particle Cloud, 8-15 Particle Explosion, 16-23 Particle Storm, 24-31 Particle Vortex, 32-39 Particle Trails, 40-47 Particle Tunnel, 48-55 Attract / Repel, 56-63 Audio Particles, 64-71 Plasma, 72-79 Lightning, 80-87 Electric Arcs, 88-95 Energy Rings, 96-103 Pulsating Field, 104-111 Neon Lines, 112-119 Kaleidoscope, 120-127 Julia Fractal, 128-135 Mandala, 136-143 Geometric Tunnel, 144-151 Rotating Pattern, 152-159 Morphing Geometry, 160-167 Wireframe Terrain, 168-175 Smoke, 176-183 Fluid, 184-191 Ink Clouds, 192-199 Flow Field, 200-207 Turbulence, 208-215 Organic Morph, 216-223 Glitch, 224-231 Strobe, 232-239 Gradient, 240-247 Text, 248-255 Logo / Image |
| 68 |  | `layer4.opacity` | Layer 4 Opacity | Effects4Adjust1 |  |
| 69 |  | `layer4.rotation` | Layer 4 Rotation | Effects4Adjust3 |  |
| 70 |  | `layer4.saturation` | Layer 4 Saturation | Effects4Adjust5 |  |
| 71 |  | `layer4.brightness` | Layer 4 Brightness | Effects4Adjust6 |  |
| 72 |  | `layer4.contrast` | Layer 4 Contrast | Effects4Adjust7 |  |
| 73 |  | `layer4.p1` | Layer 4 Param 1 | Effects4Adjust8 |  |
| 74 |  | `layer4.p2` | Layer 4 Param 2 | Effects4Adjust9 |  |
| 75 |  | `layer4.p3` | Layer 4 Param 3 | Effects4Adjust10 |  |
| 76 |  | `layer4.p4` | Layer 4 Param 4 | Effects4Adjust11 |  |
| 77 |  | `layer4.hue2` | Layer 4 Colour B hue | Effects4Adjust12 |  |
| 78 |  | `layer4.blend` | Layer 4 Blend mode | Effects4Adjust13 | 0-7 0, 8-15 1, 16-23 2, 24-31 3, 32-39 4 |
| 79 | 80 | `layer4.posx` | Layer 4 Position X | Effects4Adjust20 |  |
| 81 | 82 | `layer4.posy` | Layer 4 Position Y | Effects4Adjust21 |  |
| 83 |  | `layer5.speed` | Layer 5 Speed | Effects5Rate |  |
| 84 |  | `layer5.scale` | Layer 5 Scale | Effects5Adjust2 |  |
| 85 |  | `layer5.hue` | Layer 5 Hue shift | Effects5Adjust4 |  |
| 86 |  | `layer5.rotation` | Layer 5 Rotation | Effects5Adjust3 |  |
| 87 |  | `layer5.saturation` | Layer 5 Saturation | Effects5Adjust5 |  |
| 88 |  | `layer5.brightness` | Layer 5 Brightness | Effects5Adjust6 |  |
| 89 |  | `layer5.contrast` | Layer 5 Contrast | Effects5Adjust7 |  |
| 90 |  | `layer5.p1` | Layer 5 Param 1 | Effects5Adjust8 |  |
| 91 |  | `layer5.p2` | Layer 5 Param 2 | Effects5Adjust9 |  |
| 92 |  | `layer5.p3` | Layer 5 Param 3 | Effects5Adjust10 |  |
| 93 |  | `layer5.p4` | Layer 5 Param 4 | Effects5Adjust11 |  |
| 94 |  | `layer5.hue2` | Layer 5 Colour B hue | Effects5Adjust12 |  |
| 95 |  | `layer5.blend` | Layer 5 Blend mode | Effects5Adjust13 | 0-7 0, 8-15 1, 16-23 2, 24-31 3, 32-39 4 |
| 96 | 97 | `layer5.posx` | Layer 5 Position X | Effects5Adjust20 |  |
| 98 | 99 | `layer5.posy` | Layer 5 Position Y | Effects5Adjust21 |  |
| 100 |  | `layer6.speed` | Layer 6 Speed | Effects6Rate |  |
| 101 |  | `layer6.scale` | Layer 6 Scale | Effects6Adjust2 |  |
| 102 |  | `layer6.hue` | Layer 6 Hue shift | Effects6Adjust4 |  |
| 103 |  | `layer6.rotation` | Layer 6 Rotation | Effects6Adjust3 |  |
| 104 |  | `layer6.saturation` | Layer 6 Saturation | Effects6Adjust5 |  |
| 105 |  | `layer6.brightness` | Layer 6 Brightness | Effects6Adjust6 |  |
| 106 |  | `layer6.contrast` | Layer 6 Contrast | Effects6Adjust7 |  |
| 107 |  | `layer6.p1` | Layer 6 Param 1 | Effects6Adjust8 |  |
| 108 |  | `layer6.p2` | Layer 6 Param 2 | Effects6Adjust9 |  |
| 109 |  | `layer6.p3` | Layer 6 Param 3 | Effects6Adjust10 |  |
| 110 |  | `layer6.p4` | Layer 6 Param 4 | Effects6Adjust11 |  |
| 111 |  | `layer6.hue2` | Layer 6 Colour B hue | Effects6Adjust12 |  |
| 112 |  | `layer6.blend` | Layer 6 Blend mode | Effects6Adjust13 | 0-7 0, 8-15 1, 16-23 2, 24-31 3, 32-39 4 |
| 113 | 114 | `layer6.posx` | Layer 6 Position X | Effects6Adjust20 |  |
| 115 | 116 | `layer6.posy` | Layer 6 Position Y | Effects6Adjust21 |  |
| 117 |  | `layer1.blend` | Layer 1 Blend mode | Effects1Adjust15 | 0-7 0, 8-15 1, 16-23 2, 24-31 3, 32-39 4 |
| 118 |  | `audio.gain` | Audio Audio gain | Effects9Adjust2 |  |
| 119 |  | `audio.smoothing` | Audio Audio smoothing | Effects9Adjust3 |  |
| 120 |  | `show.bpm` | Show BPM | Effects9Rate |  |
| 121 |  | `master.red` | Master Tint red | ColorRGB_Red |  |
| 122 |  | `master.green` | Master Tint green | ColorRGB_Green |  |
| 123 |  | `master.blue` | Master Tint blue | ColorRGB_Blue |  |
| 124 |  | `master.saturation` | Master Saturation | Effects9Adjust4 |  |
| 125 |  | `layer5.effect` | Layer 5 Effect | Effects5 | 0-7 Particle Cloud, 8-15 Particle Explosion, 16-23 Particle Storm, 24-31 Particle Vortex, 32-39 Particle Trails, 40-47 Particle Tunnel, 48-55 Attract / Repel, 56-63 Audio Particles, 64-71 Plasma, 72-79 Lightning, 80-87 Electric Arcs, 88-95 Energy Rings, 96-103 Pulsating Field, 104-111 Neon Lines, 112-119 Kaleidoscope, 120-127 Julia Fractal, 128-135 Mandala, 136-143 Geometric Tunnel, 144-151 Rotating Pattern, 152-159 Morphing Geometry, 160-167 Wireframe Terrain, 168-175 Smoke, 176-183 Fluid, 184-191 Ink Clouds, 192-199 Flow Field, 200-207 Turbulence, 208-215 Organic Morph, 216-223 Glitch, 224-231 Strobe, 232-239 Gradient, 240-247 Text, 248-255 Logo / Image |
| 126 |  | `layer5.opacity` | Layer 5 Opacity | Effects5Adjust1 |  |
| 127 |  | `layer5.speed` | Layer 5 Speed | Effects5Rate |  |
| 128 |  | `layer5.scale` | Layer 5 Scale | Effects5Adjust2 |  |
