# grandMA3 setup - LUMAFLOW Basic 16ch (v1)

1. Copy `LUMAFLOW_MediaServer.gdtf` to a USB stick under `grandMA3/shared/resource/fixturetypes/` or to the console's library folder.
2. Menu > Patch > Fixture Types > Import > select *LUMAFLOW Media Server* > mode **Basic 16ch**.
3. Insert one fixture at the DMX address configured in LUMAFLOW (Console Integration > DMX input), footprint 16 channels.
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
