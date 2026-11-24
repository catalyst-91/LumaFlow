"""LUMAFLOW demo video: narration segments and the UI actions performed during each one.
Each action is (offset_seconds_within_segment, {action...}). Timings are derived from the
synthesised narration length so picture and voice stay aligned."""
SIM = "SIMULATED CONSOLE: "
SEGMENTS = [
 # 00 Introduction
 dict(chapter="Introduction", text="Welcome to LUMAFLOW, a native application for real-time visual effects, media playback, pixel mapping, spatial lighting effects and show control. Everything you are about to see is recorded live from the running application.",
      actions=[(0, {"action": "resize", "w": 1920, "h": 1080}), (0, {"action": "title", "text": "LUMAFLOW 3.0\nReal-time visuals · Pixel mapping · Show control", "height": 260})], min=9),
 dict(chapter="Introduction", text="LUMAFLOW runs completely offline. It needs no account, no browser and no cloud service. In this recording it renders with a software OpenGL driver on a virtual display, so frame rates are lower than on a real graphics card.",
      actions=[(0, {"action": "title", "text": ""}), (0.5, {"action": "demoShow"})], min=8),
 # Main interface
 dict(chapter="Main interface", text="This is the Live workspace. The effects library is on the left, with animated thumbnails. In the centre are the preview monitor, in green, and the program monitor, in red. Show control and the parameter editor are on the right, and the timeline is at the bottom.",
      actions=[(0, {"action": "workspace", "name": "Live"}), (2, {"action": "scene", "index": 0, "transition": 1})], min=10),
 dict(chapter="Main interface", text="Panels can be docked, tabbed or floated onto another monitor. Five built-in workspaces arrange the panels for live operation, programming, mapping, console work and system monitoring.",
      actions=[(1, {"action": "workspace", "name": "Programming"}), (3.5, {"action": "workspace", "name": "Mapping"}), (6, {"action": "workspace", "name": "Console"}), (8.5, {"action": "workspace", "name": "Live"})], min=11),
 # Generative effects
 dict(chapter="Generative effects", text="LUMAFLOW includes thirty-seven GPU effects in five families: particles, energy, geometry, organic and additional effects such as text, logos, audio spectrum and feedback.",
      actions=[(0, {"action": "layer", "index": 0}), (1, {"action": "effect", "layer": 0, "id": "particle_vortex", "opacity": 1}), (4, {"action": "effect", "layer": 0, "id": "lightning"}), (7, {"action": "effect", "layer": 0, "id": "kaleidoscope"})], min=10),
 dict(chapter="Generative effects", text="Selecting an effect loads it on the current layer. Every effect has speed, scale, rotation, position and colour controls, plus four effect-specific parameters, such as segments, zoom and spin for the kaleidoscope.",
      actions=[(0, {"action": "raise", "dock": "Parameters"}), (2, {"action": "ramp", "id": "layer1.p1", "from": 0.4, "to": 0.9, "duration": 4}), (6, {"action": "ramp", "id": "layer1.speed", "from": 1, "to": 2.5, "duration": 3})], min=11),
 dict(chapter="Generative effects", text="Here are a few more: smoke, fluid, a Julia fractal, a wireframe terrain and the energy ring effect. Shaders are compiled once and cached, so changing parameters never recompiles anything.",
      actions=[(0, {"action": "param", "id": "layer1.speed", "value": 1}), (0.2, {"action": "effect", "layer": 0, "id": "smoke"}), (2.5, {"action": "effect", "layer": 0, "id": "fluid"}), (5, {"action": "effect", "layer": 0, "id": "fractal"}), (7.5, {"action": "effect", "layer": 0, "id": "wireframe"}), (10, {"action": "effect", "layer": 0, "id": "energy_rings"})], min=12),
 # Node editor
 dict(chapter="Node-based effects editor", text="The node editor builds new effects without code. Nodes for coordinates, noise, warping, colour and maths are connected with typed ports, and the graph is compiled into a single shader.",
      actions=[(0, {"action": "workspace", "name": "Programming"}), (0.5, {"action": "raise", "dock": "Node Editor"}), (1, {"action": "click", "dock": "Node Editor", "button": "Compile"})], min=10),
 dict(chapter="Node-based effects editor", text="Invalid connections and cycles are rejected before anything reaches the GPU. Valid graphs are validated with an isolated test compile, then shown live on the preview monitor. Saving adds the result to the library as a custom effect.",
      actions=[(1, {"action": "click", "dock": "Node Editor", "button": "Save as effect"})], min=11),
 # Compositing
 dict(chapter="Multi-layer compositing", text="Six layers are composited on the GPU with alpha, additive, multiply, screen and masked blending. Here a particle layer is added over plasma, and a circular mask cuts a window into the top layer.",
      actions=[(0, {"action": "workspace", "name": "Live"}), (0.5, {"action": "effect", "layer": 0, "id": "plasma", "opacity": 1}), (2, {"action": "effect", "layer": 1, "id": "particle_cloud", "opacity": 1}), (2.1, {"action": "param", "id": "layer2.blend", "value": 1}), (5, {"action": "layerSource", "layer": 1, "mask": 1}), (9, {"action": "layerSource", "layer": 1, "mask": 0})], min=12),
 dict(chapter="Multi-layer compositing", text="Looks can be prepared on the preview composition while the program keeps running. Pressing TAKE crossfades the preview into the program over the transition time.",
      actions=[(0, {"action": "effect", "layer": 0, "id": "geo_tunnel", "preview": True, "opacity": 1}), (1, {"action": "effect", "layer": 1, "id": "neon_lines", "preview": True, "opacity": 0.7}), (5, {"action": "take", "transition": 3})], min=10),
 dict(chapter="Multi-layer compositing", text="A scene stores the complete composition. Scenes can be recalled with their own transition time, from the show control panel, a cue list, the timeline, OSC, MIDI or a lighting console.",
      actions=[(1, {"action": "scene", "index": 1, "transition": 2}), (5, {"action": "scene", "index": 3, "transition": 2}), (9, {"action": "scene", "index": 2, "transition": 2})], min=12),
 # Media
 dict(chapter="Video and image playback", text="Video and images play through FFmpeg on background threads with a bounded frame queue, so disk and decoder activity never block rendering. Media layers support looping, in and out points, speed, reverse for short clips, and the same transform, colour and mask controls as effects.",
      actions=[(0, {"action": "workspace", "name": "Programming"}), (0.5, {"action": "raise", "dock": "Media Library"}), (2, {"action": "effect", "layer": 2, "id": "text", "opacity": 1}), (2.1, {"action": "param", "id": "layer3.blend", "value": 3})], min=14),
 dict(chapter="Video and image playback", text="Text and logo layers render on top of any content. Missing media files are detected when a project opens and can be relinked from a folder.",
      actions=[(6, {"action": "param", "id": "layer3.opacity", "value": 0})], min=8),
 # Pixel mapping
 dict(chapter="Pixel mapping", text="The pixel mapper samples the program canvas onto LED strips, matrices and patched fixtures. In the demonstration rig, LED tubes, battens, RGBW pars and a sixteen by eight matrix are mapped from their real positions.",
      actions=[(0, {"action": "workspace", "name": "Mapping"}), (0.5, {"action": "raise", "dock": "Pixel Mapper"}), (1, {"action": "scene", "index": 0, "transition": 1})], min=11),
 dict(chapter="Pixel mapping", text="LUMAFLOW handles channel order, gamma, brightness limiting and universe boundaries. Pixels are never split across universes, conflicts are listed, and only universes you explicitly enable are transmitted. The identify test pattern shows each element's start and direction.",
      actions=[(3, {"action": "pixelTest", "pattern": 2}), (9, {"action": "pixelTest", "pattern": 3}), (13, {"action": "pixelTest", "pattern": 0})], min=15),
 # 3D stage & spatial
 dict(chapter="3D stage and spatial effects", text="The 3D stage shows every fixture at its real position, coloured by the DMX that is actually being sent. Fixture layouts can be imported from MVR files with their GDTF fixture types.",
      actions=[(0, {"action": "raise", "dock": "3D Stage"}), (0.3, {"action": "raise", "dock": "Spatial Effects"})], min=10),
 dict(chapter="3D stage and spatial effects", text="Spatial effects order fixtures by position: left to right, centre outward, radial, symmetrical, random with a reproducible seed and more. Here a chase runs along the blinders, and the washes breathe from the centre outward.",
      actions=[(1, {"action": "spatial", "index": 1, "enabled": True}), (6, {"action": "spatial", "index": 0, "order": "Radial clockwise", "waveform": "Chase"}), (11, {"action": "spatial", "index": 0, "order": "Centre outward", "waveform": "Sine"})], min=14),
 dict(chapter="3D stage and spatial effects", text="Fixtures with colour but no dimmer use a virtual dimmer, fixtures with a dimmer get intensity on the dimmer without double dimming, and intensity-only fixtures receive the level.",
      actions=[], min=9),
 dict(chapter="3D stage and spatial effects", text="The fixture manager holds the patch: fixture types, modes, universes, addresses, positions and groups. Capabilities such as dimmer, colour mixing, pan and tilt or multiple cells are detected automatically from GDTF data, and the patch is checked for overlaps and universe boundary errors.",
      actions=[(0, {"action": "raise", "dock": "Fixture Manager"})], min=13),
 # Console integration
 dict(chapter="grandMA3 and MagicQ integration", text="LUMAFLOW works as a fixture-style media server for grandMA3 and ChamSys MagicQ. Both consoles can be connected at the same time on separate addresses, each with its own mapping set.",
      actions=[(0, {"action": "workspace", "name": "Console"}), (0.5, {"action": "raise", "dock": "Console Mapping"}), (0.6, {"action": "tab", "dock": "Console Mapping", "index": 0}),
               (1, {"action": "network", "artnetIn": True}), (1.5, {"action": "console", "index": 0, "enabled": True, "universe": 1, "address": 101})], min=11),
 dict(chapter="grandMA3 and MagicQ integration", text="No console hardware was available for this recording, so the following control is a simulated console sending real Art-Net packets to LUMAFLOW. The master intensity and the layer one effect are being driven over DMX through the personality.",
      actions=[(0, {"action": "caption", "text": SIM + "Art-Net packets from a software sender on this computer, not a real console"}),
               (3, {"action": "artnet", "channels": {"101": 255, "102": 255, "103": 64, "104": 255}}), (6, {"action": "artnet", "channels": {"101": 128, "102": 0, "103": 64, "104": 255}}),
               (9, {"action": "artnet", "channels": {"101": 255, "102": 255, "103": 120, "104": 255}}), (12, {"action": "artnet", "channels": {"101": 255, "102": 255, "103": 8, "104": 255}})], min=15, keepCaption=True),
 # Personality generation
 dict(chapter="Fixture profile generation", text="The fixture personality is generated from the internal parameter registry in four modes: sixteen, thirty-two, sixty-four and one hundred and twenty-eight channels. Lower modes are exact prefixes of higher ones, and channels never move when new parameters are added.",
      actions=[(0, {"action": "console", "index": 0, "enabled": False}), (0.5, {"action": "tab", "dock": "Console Mapping", "index": 3})], min=13),
 dict(chapter="Fixture profile generation", text="When effects or scenes change, LUMAFLOW proposes an updated profile and shows exactly what changed. Breaking changes are flagged, nothing is applied without operator approval, and earlier versions can be rolled back. Export writes a GDTF file for the consoles, verified by re-importing it.",
      actions=[(2, {"action": "click", "dock": "Console Mapping", "button": "Check for updates"})], min=14),
 # Bidirectional
 dict(chapter="Fixture profile generation", text="This is the Standard thirty-two channel mode: master intensity in sixteen bit, effect selection, opacity, speed, scale, rotation and colour for layer one, scene selection, transition time, a second layer, strobe and a control channel for go, back, blackout and tap tempo.",
      actions=[], min=12),
 dict(chapter="Bidirectional control", text="The control mapping table connects OSC addresses, MIDI controls and DMX channels to any parameter or action, with scaling, inversion, deadband, smoothing, trigger edges and soft takeover. A learn button creates mappings by moving a control.",
      actions=[(0, {"action": "tab", "dock": "Console Mapping", "index": 1})], min=12),
 dict(chapter="Bidirectional control", text="Ownership policies decide who wins when a console and a touchscreen change the same value. Feedback is sent back to consoles and controllers without creating loops. Here OSC messages recall a scene and change the master.",
      actions=[(0, {"action": "tab", "dock": "Console Mapping", "index": 2}), (4, {"action": "osc", "address": "/lumaflow/scene/recall", "args": [5]}), (8, {"action": "osc", "address": "/lumaflow/master/intensity", "args": [0.6]}), (11, {"action": "osc", "address": "/lumaflow/master/intensity", "args": [1.0]})], min=13),
 # Timeline
 dict(chapter="Timeline and automation", text="Shows can run from cue lists with manual, auto-follow and wait cues, or from the multi-track timeline. Automation tracks animate any parameter with curves, and event tracks trigger scenes, cues and OSC messages.",
      actions=[(0, {"action": "workspace", "name": "Live"}), (0.5, {"action": "raise", "dock": "Timeline"}), (1, {"action": "timeline", "play": True, "seek": 4})], min=12),
 dict(chapter="Timeline and automation", text="The timeline can follow its own clock, MIDI time code, linear time code decoded from audio, or the clock of another LUMAFLOW instance. The cue list GO button runs the next cue.",
      actions=[(3, {"action": "cue"}), (7, {"action": "cue"})], min=11),
 # Audio
 dict(chapter="Audio reactivity and output", text="Audio analysis splits the input into six frequency bands, detects beats and estimates the tempo. Here a built-in test signal at one hundred and twenty-eight beats per minute drives an audio-reactive particle layer.",
      actions=[(0, {"action": "timeline", "play": False}), (0.3, {"action": "audio", "device": "test:128", "enabled": True}), (0.5, {"action": "raise", "dock": "Audio"}), (1, {"action": "effect", "layer": 0, "id": "audio_particles", "opacity": 1}), (1.2, {"action": "param", "id": "layer1.audio", "value": 1}), (6, {"action": "effect", "layer": 1, "id": "spectrum", "opacity": 0.8})], min=13),
 dict(chapter="Audio reactivity and output", text="The program can be sent to multiple displays and LED processors, with cropping, rotation and scaling per output, and recorded to video. Calibration sets gamma, colour balance and white point, and a safety limiter caps flash rates for photosensitive audiences.",
      actions=[(0, {"action": "workspace", "name": "System"}), (0.5, {"action": "raise", "dock": "Calibration"})], min=13),
 dict(chapter="Reliability", text="For reliability, LUMAFLOW autosaves, keeps versioned backups, detects crashes and offers recovery on the next start. A shader error only affects its own layer, a watchdog monitors the render thread, and the recovery dashboard lists every fault with a recommended action.",
      actions=[(0, {"action": "raise", "dock": "Recovery"}), (0.2, {"action": "raise", "dock": "Performance Monitor"})], min=13),
 dict(chapter="Reliability", text="Here is a deliberate fault. A custom effect with a shader error is loaded on layer two. The compile error is reported in the recovery panel, but layer one and every output keep running without interruption.",
      actions=[(0, {"action": "workspace", "name": "System"}), (0.2, {"action": "raise", "dock": "Recovery"}), (2, {"action": "brokenEffect", "layer": 1}), (12, {"action": "effect", "layer": 1, "id": "particle_cloud"})], min=14),
 dict(chapter="Audio reactivity and output", text="Calibration test patterns help line up projectors, LED walls and monitors: colour bars with a grid and circle, a grey ramp for gamma, and full white. The red border marks the edge of the canvas.",
      actions=[(0, {"action": "raise", "dock": "Calibration"}), (1, {"action": "testPattern", "pattern": 0}), (5, {"action": "testPattern", "pattern": 1}), (8, {"action": "testPattern", "pattern": 2}), (10.5, {"action": "testPattern", "pattern": -1})], min=12),
 dict(chapter="Remote, sync, plugins and AI", text="Tablets on the local network can control the show through a web interface with token based roles. Several LUMAFLOW machines can share their state. Plugins extend the effect library through a C plugin interface, and the AI assistant turns a sentence into an effect.",
      actions=[(0, {"action": "workspace", "name": "Programming"}), (0.5, {"action": "raise", "dock": "AI Effect Assistant"}), (6, {"action": "aiPrompt", "text": "Create a blue particle vortex"}), (9, {"action": "click", "dock": "AI Effect Assistant", "button": "Validate"})], min=14),
 dict(chapter="Remote, sync, plugins and AI", text="Generated effects are validated before they can be activated, and go to the preview first. The AI assistant works locally by default; an external model is optional.",
      actions=[(4, {"action": "take", "transition": 2})], min=9),
 # Installation and conclusion
 dict(chapter="Installation and conclusion", text="On Linux, LUMAFLOW installs from a Debian package. Windows and macOS installers are produced by the automated build pipeline. The professional user manual, quick start guide, console fixture profiles and protocol references are included with the release.",
      actions=[(0, {"action": "workspace", "name": "Live"}), (1, {"action": "scene", "index": 4, "transition": 3})], min=13),
 dict(chapter="Installation and conclusion", text="Console integration was demonstrated with simulated control only and must be validated on real consoles before a production. Thank you for watching LUMAFLOW.",
      actions=[(1, {"action": "scene", "index": 2, "transition": 3}), (8, {"action": "title", "text": "LUMAFLOW 3.0\nThank you for watching", "height": 240})], min=11, final=True),
]
