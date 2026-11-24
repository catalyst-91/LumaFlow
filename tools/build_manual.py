#!/usr/bin/env python3
"""Builds documentation/LUMAFLOW_Professional_User_Manual.pdf from the real application assets."""
import os, sys, json, csv
sys.path.insert(0, os.path.dirname(__file__))
from manual_lib import *

DOCS = os.path.join(ROOT, "documentation"); SHOT = os.path.join(DOCS, "screenshots"); FX = os.path.join(DOCS, "images/effects")
def shot(n, cap): return image(os.path.join(SHOT, n + ".png"), 170, cap)
story = []
B = lambda t: story.extend(blocks_to_flowables(t))
def H1(t): story.append(NextPageTemplate("body")); story.append(PageBreak()); story.append(Paragraph(esc(t), S["h1"]))

# ------------------------------------------------------------------ front matter
story.append(Spacer(1, 1)); story.append(NextPageTemplate("body")); story.append(PageBreak())
story.append(Paragraph("Contents", ParagraphStyle("ct", parent=S["h1"]))); story.append(toc())
story.append(PageBreak())
story.append(Paragraph("About this manual", ParagraphStyle("ab", parent=S["h2"])))
B("""
This manual describes LUMAFLOW 3.0.0 as implemented and tested. Every screenshot is a capture of the running application (Linux build, rendered with Mesa on a virtual display). Where a feature is implemented but has not been validated on physical hardware, the text says so explicitly; a consolidated list is in chapter 41 (Technical reference → Known limitations).
Conventions: menu paths are written **Menu → Item**; parameter identifiers such as `layer1.speed` are the stable ids used by OSC, MIDI, DMX mappings, the timeline and remote control.
!note Photosensitivity warning: LUMAFLOW can produce strobing and rapidly flashing images and light. The safety limiter (chapter 32) caps flash rates at 3 Hz by default. Keep it enabled for public audiences.
""")

# ------------------------------------------------------------------ 1
H1("1. Product overview")
B("""
LUMAFLOW is a native desktop application that combines a GPU visual effects engine, a media server, pixel mapping, position-based spatial lighting effects and show automation. It runs fully offline: no account, no cloud service and no browser are required for local operation. Network features (Art-Net, sACN, OSC, remote touchscreens and multi-instance sync) are optional.
## What LUMAFLOW does
- Renders up to six composited layers of generative GPU effects, video, images, text and logos into a program canvas of any resolution (1080p, 4K, ultra-wide, custom LED resolutions).
- Samples the program canvas onto LED strips, matrices and patched fixtures (pixel mapping) and transmits DMX over Art-Net 4 and sACN.
- Drives fixtures from their real 3D positions with spatial effects (chases, waves, colour sweeps) that respect each fixture's capabilities.
- Acts as a fixture-style media server for grandMA3 and ChamSys MagicQ: the console patches a LUMAFLOW personality (16–128 channels) generated from the internal parameter registry.
- Accepts OSC, MIDI and DMX control with learnable mappings, and sends feedback.
- Plays shows from cue lists and a multi-track timeline chased to internal time, MTC, LTC or a sync primary.
""")
story.append(boxes_diagram([
    (2, 52, 34, 22, "Inputs\nconsole DMX · OSC\nMIDI · audio · TC", BLUE), (2, 26, 34, 22, "Content\neffects · nodes\nvideo · images · text", BLUE), (2, 0, 34, 22, "Show data\nscenes · cues\ntimeline · presets", BLUE),
    (50, 18, 44, 40, "Engine\nparameter registry\nmapping & ownership\nsnapshots", VIOLET),
    (108, 40, 30, 20, "GPU renderer\n6 layers + master", VIOLET), (108, 10, 30, 20, "Spatial engine\nfixture positions", VIOLET),
    (148, 52, 22, 20, "Video\noutputs", DARK), (148, 27, 22, 20, "Pixel\nmapping", DARK), (148, 2, 22, 20, "Art-Net\nsACN", DARK)],
    [((36, 63), (50, 48)), ((36, 37), (50, 38)), ((36, 11), (50, 28)), ((94, 44), (108, 50)), ((94, 32), (108, 20)), ((138, 52), (148, 62)), ((138, 46), (148, 37)), ((138, 20), (148, 12)), ((159, 27), (159, 22))], h=76))
story.append(Paragraph("Figure 1-1. Signal flow through LUMAFLOW", S["caption"]))
B("""
## Design principles
The renderer runs on its own thread and receives immutable snapshots of the show state, so user-interface activity, disk access and network traffic can never block rendering. Network I/O has its own thread with bounded, rate-limited transmission. Shader compile errors are isolated to the affected layer, a watchdog detects render stalls, and the project is autosaved with versioned backups.
""")

# ------------------------------------------------------------------ 2
H1("2. System requirements")
story.append(table([["Component", "Minimum", "Recommended"],
    ["Operating system", "Linux x86_64 (glibc 2.38, e.g. Ubuntu 24.04); Windows 10/11 x64*; macOS 12+*", "Same, 64-bit, current drivers"],
    ["GPU / API", "OpenGL 3.3 core (or OpenGL ES 3.0)", "Discrete GPU with current drivers (needed for 60 fps @ 1080p)"],
    ["CPU", "4 cores x86_64", "6–8 cores"], ["RAM", "8 GB", "16 GB (video caching)"],
    ["Network", "Optional; 100 Mbit Ethernet for Art-Net/sACN", "Dedicated lighting network, gigabit"],
    ["Libraries (Linux)", "Qt 6.4+, FFmpeg 6 libs, SQLite, zlib, ALSA", "Installed automatically by the .deb package"]], [38*mm, 66*mm, 66*mm]))
B("""
*Windows and macOS builds are defined in the automated pipeline but were not built or tested in the release environment (chapter 41).
!note Performance figures in this manual were measured with Mesa llvmpipe, a CPU software renderer, because the build machine had no GPU. They are a lower bound; validate frame rates on your show hardware with `lumaflow-cli benchmark`.
""")

# ------------------------------------------------------------------ 3-5
H1("3. Windows installation")
B("""
The Windows installer is produced by the CI job `windows` (MSVC x64, Qt 6, FFmpeg from vcpkg, `windeployqt`, CPack NSIS).
- Run **LUMAFLOW-3.0.0-win64.exe** and follow the wizard. Choose the installation folder (default `C:\\Program Files\\LUMAFLOW`).
- The installer adds a Start-menu entry and, optionally, a desktop shortcut. An uninstaller is registered in *Apps & features*.
- A portable ZIP is also produced; unzip and run `bin\\lumaflow.exe`.
- Allow LUMAFLOW through the Windows firewall for private networks if you use Art-Net (UDP 6454), sACN (UDP 5568), OSC (UDP 8000) or the remote server (TCP 8080).
!note Status: build recipe only. The Windows package was not built or tested in the release environment, and Windows MIDI/audio-capture device backends are not yet implemented.
""")
H1("4. macOS installation")
B("""
The CI job `macos` builds Apple Silicon (arm64) and Intel (x86_64) bundles, runs `macdeployqt`, signs with a Developer ID certificate, notarises with `notarytool` and staples the ticket (`installers/macos/package_macos.sh`). Without signing credentials an unsigned DMG is produced.
- Open **LUMAFLOW-3.0.0-macOS-arm64.dmg** (or x86_64) and drag LUMAFLOW to *Applications*.
- On first launch macOS asks for microphone access (audio reactivity) and local-network access (Art-Net/sACN/OSC). Grant both if needed.
- Unsigned builds: right-click → Open to bypass Gatekeeper once.
!note Status: build recipe only. Not built or tested in the release environment.
""")
H1("5. Linux installation")
B("""
### Debian / Ubuntu package (tested)
```
sudo apt install ./lumaflow_3.0.0_amd64.deb
lumaflow                # desktop application
lumaflow-cli info       # GPU / backend check
```
The package installs `/usr/bin/lumaflow`, `/usr/bin/lumaflow-cli`, a desktop launcher, the icon, AppStream metadata, example projects, fixture profiles, the SDK headers and the example plugin under `/usr/lib/lumaflow/plugins`. Dependencies (Qt 6, FFmpeg libraries, SQLite, ALSA) are resolved by apt.
### Tarball
Extract `lumaflow-3.0.0-Linux.tar.gz` and run `bin/lumaflow`; system Qt 6 and FFmpeg libraries are required.
### AppImage
`installers/linux/build_appimage.sh` creates a self-contained AppImage with linuxdeploy (run by CI).
### Building from source
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build && xvfb-run ctest --test-dir build
```
For MIDI, add your user to the `audio` group so ALSA sequencer ports can be created. For DMX over a dedicated interface, bind LUMAFLOW to that interface's address (Output Configuration).
""")

# ------------------------------------------------------------------ 6
H1("6. First launch")
B("""
On first launch LUMAFLOW starts the GPU renderer, loads the 37 built-in effects, scans plugin folders and opens the **Live** workspace. Effect thumbnails are rendered in the background and cached.
### Command-line options
""")
story.append(table([["Option", "Purpose"], ["`--demo`", "Load the demonstration show (fixtures, pixel map, scenes, cues, timeline)"], ["`--safe-mode`", "Start without plugins, custom effects, DMX output or remote control"],
    ["`--canvas WxH`", "Program canvas resolution"], ["`--data-dir DIR`", "Alternative settings/autosave folder"], ["`project.lumaflow`", "Open a project"], ["`--screenshots DIR`, `--demo-script FILE`", "Automation used for documentation and the demo video"]], [55*mm, 115*mm]))
B("""
### Crash recovery prompt
If the previous session did not shut down cleanly, LUMAFLOW offers to restore the autosave (chapter 38).
### Loading the demo show
**File → Load demo show** creates 24 fixtures (RGBW pars, moving washes, pixel tubes, LED battens, blinders), a 15-element pixel map, two spatial effects, six scenes, a six-cue list, a 60-second timeline and example mappings. All examples in this manual use it.
""")

# ------------------------------------------------------------------ 7
H1("7. Main interface")
story.append(shot("01_main_live", "Figure 7-1. Live workspace: effects library, layers, preview/program, show control, parameters and timeline"))
B("""
The window is built from dockable panels around the **Preview / Program** monitors. Every panel can be moved, tabbed, floated onto another monitor or closed; **View** lists all panels.
## Toolbar
Workspace buttons (Live, Programming, Mapping, Console, System), **GO** (next cue, Space), **TAKE** (preview → program, Enter), **TAP** (tap tempo, T), the **Master** fader and **BLACKOUT** (B).
## Workspaces
""")
story.append(table([["Workspace", "Panels"], ["Live", "Effects Library, Layers, Show Control, Parameters/Audio/Performance, Timeline/3D Stage/Pixel Mapper/Console/Recovery"],
    ["Programming", "Effects & Media Library, Layers, Parameters, AI Assistant, Node Editor, Timeline"], ["Mapping", "Pixel Mapper, Fixture Manager, 3D Stage, Spatial Effects, Output Configuration"],
    ["Console", "Console Mapping, Output Configuration, Remote & Sync, Show Control"], ["System", "Performance, Calibration, Audio, Recovery, Plugins, Remote & Sync"]], [35*mm, 135*mm]))
B("""
**View → Workspace → Save current layout as…** stores custom layouts; **Reset built-in layouts** restores the defaults.
## Status bar
Shows the last action, timecode (and REC while recording), DMX/OSC packet counters, the fault indicator (green OK / red count, click the Recovery panel for details) and the renderer frame rate.
## Undo / redo
**Edit → Undo / Redo** (Ctrl+Z / Ctrl+Shift+Z) covers parameter edits from sliders (merged per drag), effect changes, scene/preset/cue edits, patch, pixel-map and mapping changes.
## Touch operation
All buttons and sliders are sized for touch; monitors support pinch-to-zoom, double-tap to reset; the remote web UI (chapter 33) provides a tablet surface.
""")

# ------------------------------------------------------------------ 8
H1("8. Effects library")
story.append(shot("02_effects_library", "Figure 8-1. Programming workspace with the effects library and parameter editor"))
B("""
The library lists built-in, custom (node/shader), plugin and AI-generated effects with animated thumbnails. Use the search field and the category filter (All, Favourites, Particles, Energy, Geometry, Organic, Additional, Custom, Plugins, AI generated).
- **Double-click** loads the effect on the selected layer (or on the preview composition when *Edit preview* is enabled in Layers).
- **Right-click**: add to favourites, load to preview, export as `.lfeffect`, delete custom effects.
- **Import…** validates an `.lfeffect` file with an isolated GPU compile before adding it.
- Effects marked ⚠ produce flashing content and are subject to the safety limiter.
## Built-in effects
""")
rows = [["Effect", "Category", "Parameters p1–p4"]]
for line in open(os.path.join(FX, "effects.tsv")):
    parts = line.rstrip("\n").split("\t")
    if len(parts) >= 4: rows.append([parts[2] + (" ⚠" if "PHOTOSENSITIVE" in line else ""), parts[1], parts[3].strip("[]")])
story.append(table(rows, [45*mm, 28*mm, 97*mm]))
story.append(PageBreak()); story.append(Paragraph("Effect gallery", S["h2"]))
ids = [l.split("\t")[0] for l in open(os.path.join(FX, "effects.tsv")) if "\t" in l]
names = {l.split("\t")[0]: l.split("\t")[2] for l in open(os.path.join(FX, "effects.tsv")) if "\t" in l}
cells, row = [], []
for i, e in enumerate(ids):
    p = os.path.join(FX, e + ".png")
    if not os.path.exists(p): continue
    row.append([Image(p, 54*mm, 30.4*mm), Paragraph(esc(names[e]), S["caption"])])
    if len(row) == 3: cells.append(row); row = []
if row: cells.append(row + [""] * (3 - len(row)))
g = Table(cells, colWidths=[57*mm] * 3); g.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("ALIGN", (0, 0), (-1, -1), "CENTER")]))
story.append(g); story.append(Paragraph("Figure 8-2. All built-in effects rendered by lumaflow-cli render-all (default parameters, t = 1.5 s, no audio input)", S["caption"]))

# ------------------------------------------------------------------ 9
H1("9. GPU rendering")
B("""
LUMAFLOW renders with OpenGL 3.3 core profile (falling back to OpenGL ES 3.0) in an offscreen context on the dedicated `lf-render` thread.
## Render pipeline
- Each visible layer renders its effect into its own framebuffer at *canvas × render scale*.
- Layers are composited in order with their blend mode, opacity and mask.
- During a transition the outgoing and incoming compositions are rendered and crossfaded.
- A master pass applies intensity, tint, saturation, strobe (safety-limited), blackout, calibration gamma/gains/white point and the brightness limit.
- The final frame is read back once for the monitors, video outputs, pixel mapping and recording.
## Effect shaders
Every effect is a GLSL function `vec4 effect(vec2 p, vec2 uv, float t)`. `p` is centred and aspect-corrected after the layer's position, rotation and scale transform; `t` is the layer clock integrated with the layer speed (so speed changes never jump). Uniforms available to all effects include `uP` (p1–p4), `uColorA/uColorB`, `uAudio` (bass/mid/treble/level), `uBeat`, `uSpec[32]`, `uExt[8]`, `uTimecode`, `uPrev` (previous frame) and `uTex` (media/text texture).
## Shader caching and safety
Compiled programs are cached per effect and source hash; parameter changes are uniform updates only. If a shader fails to compile, the error is reported in **Recovery** and only that layer is skipped; if a previous version of the effect compiled, it keeps running. Unbounded `while` loops are rejected during validation. A watchdog reports stalls longer than 2 s while outputs hold the last frame.
## Quality and frame rate
**Performance Monitor** sets the target frame rate (default 60) and quality: Low (50 %), Medium (75 %), High (100 %) or Adaptive, which lowers the layer render scale when frames exceed the budget and raises it when there is headroom.
""")

# ------------------------------------------------------------------ 10
H1("10. Node editor")
story.append(shot("03_node_editor", "Figure 10-1. Node editor with the default graph compiled and validated on the GPU"))
B("""
The node editor builds effects without code. Graphs are type-checked, compiled into a single GLSL effect and validated with an isolated GPU compile before they are shown.
## Working with nodes
- **Add** a node by double-clicking it in the palette; it appears at the centre of the view.
- **Connect** by dragging from an output port (right) to an input port (left); dragging from an input disconnects it. Port colours: blue float, amber vec2, violet colour.
- **Edit** a node by double-clicking it: label, parameters, defaults for unconnected inputs, GLSL code for *Custom GLSL* nodes.
- **Right-click**: delete, copy, paste. Rubber-band select multiple nodes; **Group → Subgraph** collapses the selection into a reusable subgraph node.
- **Live preview** recompiles 0.7 s after a structural change and shows the result on the selected preview layer; **Apply to Program** sends it live; **Save as effect** adds it to the library (`.lfeffect` with the graph embedded).
## Validation
Invalid connections (e.g. vec2 into colour), cycles, multiple outputs or a missing Output node are reported in the log below the graph and never reach the GPU. Implicit conversions: float → vec2/colour, colour → float (luminance). Custom GLSL nodes accept a single expression (no `;`, braces or `discard`).
## External control in graphs
*Exposed Parameter* nodes read layer parameters p1–p4 (mappable from consoles and the timeline). *MIDI/OSC/DMX Input* nodes read external slots `ext.1`–`ext.8`, which you map to MIDI controls, OSC addresses or DMX channels in Console Mapping.
## Node types
""")
nodes = [l.rstrip("\n").split("\t") for l in open(os.path.join(DOCS, "images/nodes.tsv"))]
nodes += [[i, n, "Mathematics"] for i, n in [("add", "Add"), ("sub", "Subtract"), ("mul", "Multiply"), ("div", "Divide"), ("min", "Minimum"), ("max", "Maximum"), ("pow", "Power"), ("mod", "Modulo"), ("sin", "Sine"), ("cos", "Cosine"), ("abs", "Absolute"), ("fract", "Fraction"), ("sqrt", "Square Root"), ("sat", "Saturate")]]
bycat = {}
for i, n, c in nodes: bycat.setdefault(c, []).append(n)
story.append(table([["Category", "Nodes"]] + [[c, ", ".join(v)] for c, v in bycat.items()], [40*mm, 130*mm]))
B("Example graphs are provided in `examples/node-graphs/` (kaleido noise, audio rings, MIDI-controlled particles); `lumaflow-cli compile-graph FILE` prints the generated GLSL.")

# ------------------------------------------------------------------ 11
H1("11. Multi-layer compositing")
story.append(shot("18_layers_compositing", "Figure 11-1. Layers panel, parameter editor for layer 2 and the 3D stage"))
B("""
The program composition has six layers (layer 1 at the bottom). Each layer has a source (effect, video or image), its own clock, transform, colour correction, blend mode and mask.
""")
story.append(table([["Control", "Range / options", "Notes"], ["On / Solo", "checkbox", "Solo hides all non-solo layers"], ["Opacity", "0–1", "`layerN.opacity`"],
    ["Blend", "Alpha, Additive, Multiply, Screen, Masked", "Masked: layer shows where the layers below are bright"], ["Speed", "0–4", "integrates the layer clock"],
    ["Scale / Rotation / Position", "0.1–4 / ±1 turn / ±1", "position is 16-bit in console modes"], ["Hue shift, Colour B hue", "0–1", "hue rotation of output and of colour B"],
    ["Saturation, Brightness, Contrast", "0–2", ""], ["Param 1–4", "0–1", "effect-specific (names shown per effect)"], ["Audio reactivity", "0–1", "bass + beat modulate scale and brightness"],
    ["Mask", "None, Circle, Rectangle, Linear gradient, Luma of layer below", "size and feather"], ["Colour A / B", "colour pickers", "reset to effect defaults when the effect changes"]], [45*mm, 55*mm, 70*mm]))
B("""
## Preview and program
Enable **Edit preview** in the Layers panel to edit the preview composition without affecting the output, then press **TAKE**. The take crossfades over *Transition (s)*. **Program → Preview** copies the live state into preview for editing.
## Layer presets
**Parameters → Layer presets**: *Store* captures the layer's source, colours and values; *Recall* applies it to the selected layer; *Export* writes `.lfpreset`. **File → Import preset** loads preset files (examples in `examples/presets/`, including audio-reactive presets).
## Scenes
A scene is a snapshot of the whole program composition. Recall crossfades from the current output (chapter 30). The master fader is never overwritten by a scene.
""")

# ------------------------------------------------------------------ 12
H1("12. Media playback")
story.append(shot("17_media_library", "Figure 12-1. Media library"))
B("""
LUMAFLOW plays video through FFmpeg (libavformat/libavcodec/libswscale) and images through Qt image formats (PNG, JPEG, WebP, GIF, BMP, TIFF, SVG).
- **Import media…** adds files to the media library (stored in SQLite with metadata and a thumbnail).
- **Use on selected layer** (or double-click) makes the layer a media layer; **Use as logo** loads the image into the *Logo / Image* effect.
- Media layers have Loop, Reverse, In/Out points, Speed (layer speed) and the normal transform, colour, blend and mask controls. Transparency is preserved for formats with alpha.
- Video decoding runs on a background thread per clip with a bounded queue of 12 frames; late frames are dropped rather than stalling the renderer.
- Clips up to 900 frames (and < 768 MB decoded) are cached entirely, enabling exact reverse playback and seamless loops. Longer clips play forward only.
## Missing media
Missing files are marked ⚠ MISSING and reported in Recovery when a project is opened. **Relink missing…** searches a folder for files with the same names and updates both the library and the project.
""")

# ------------------------------------------------------------------ 13
H1("13. Pixel mapping")
story.append(shot("04_pixel_mapper", "Figure 13-1. Pixel mapper: program canvas with mapped elements, element table and statistics"))
B("""
The pixel mapper samples the program canvas at mapped points and writes DMX. Elements are drawn over the canvas (yellow = selected); the statistics line reports pixels, channels, universes and conflicts.
## Element types
- **Strip**: N pixels along a line, a channel order (RGB, RBG, GRB, GBR, BRG, BGR, RGBW, GRBW, W), a start universe/address and a split mode.
- **Matrix**: columns × rows with zigzag (snake) wiring, start corner, row- or column-major order and reversal.
- **Fixture**: a patched fixture, sampled per cell (multi-cell bars, matrices) or as one point. Fixture elements use the colour engine (virtual dimmer, white extraction, calibration).
**From patch (front/top)** creates fixture elements from the 3D positions of the patch, projected onto the canvas with preserved aspect ratio.
## Universe handling
*Pixel-aligned* split (default) never divides a pixel across universes: an RGB strip carries 170 pixels per universe (510 channels) and continues at channel 1 of the next universe. *Continuous* split packs channels across the boundary. Fixtures patched across a boundary are written correctly into both universes.
## Output processing
Global gamma (default 2.2), master limit, area or nearest sampling, per-element calibration (RGB/W gains, gamma, max brightness).
## Test patterns
Off (live), Full white, Identify (each element a different colour, first pixel white to show start and direction), Chase (single pixel running).
## Conflicts and safety
Overlapping channels between elements are listed. LUMAFLOW transmits only universes listed in *Output universes* (chapter 31), so a mapping error cannot leak onto unrelated universes; Output Configuration warns when the map uses universes that are not enabled.
""")

# ------------------------------------------------------------------ 14-16
H1("14. MVR import")
story.append(shot("05_fixture_manager", "Figure 14-1. Fixture manager with the demo patch and output configuration"))
B("""
**Fixture Manager → Import MVR…** reads `GeneralSceneDescription.xml` and every embedded `.gdtf` from an MVR (My Virtual Rig) file.
Extracted per fixture: name, UUID (preserved for re-export), FixtureID/UnitNumber, GDTF spec and mode, DMX address (absolute or universe.address form), layer, group objects and the full transform through nested layers, groups, trusses and scene objects. Positions are converted from millimetres to metres; rotations are decomposed to XYZ degrees.
## Warnings and correction
MVR exports are often incomplete. The import report lists, for example: fixtures without FixtureID (an ID is assigned), unpatched fixtures, GDTF files referenced but not embedded, unknown modes (first mode used), DMX breaks other than 0, and patch overlaps. All values remain editable in the fixture table (FID, name, universe, address, X/Y/Z, groups) and in the 3D stage.
## Export
**Export MVR…** writes the patch with positions, rotations, addresses, UUIDs and embedded GDTF descriptions of every fixture type used. The CLI equivalents are `lumaflow-cli import-mvr FILE` and `export-mvr`.
""")
H1("15. GDTF support")
B("""
**Import GDTF…** parses `description.xml` (GDTF DataVersion 1.x): fixture type, manufacturer, FixtureTypeID, DMX modes, channel offsets (8/16-bit; 24/32-bit channels use the top 16 bits with a warning), defaults, attributes and **multi-instance geometry**: channels attached to a geometry referenced by `GeometryReference` elements are instantiated per reference using the `Break DMXOffset`, giving per-cell channels and cell positions.
Unrecognised attributes are kept verbatim (raw) and reported. The export writes a self-consistent GDTF 1.2 description: attribute definitions with feature groups, geometry, modes, channel functions with defaults and channel sets for index ranges. Every export is checked by a structural validator (well-formed XML, required fixture-type attributes, GUID, unique offsets in range, attributes defined, geometry references) and by re-import.
!note The official GDTF XSD schema was not run and generated files have not yet been imported into console software. See chapter 41.
""")
H1("16. Fixture capability detection")
B("""
For every fixture mode LUMAFLOW derives a capability profile from its channel attributes: dimmer, colour model (RGB, RGBW, RGBA, RGBWA, RGBWAUV, CMY), pan/tilt, zoom, focus, iris, shutter, strobe, gobo, prism, colour wheel, CTO and the number of cells. The profile is shown in the fixture table and drives the colour engine, pixel mapping and spatial effects.
""")
story.append(table([["Built-in type", "Mode", "Capabilities"], ["Generic Dimmer", "1ch", "Dimmer"], ["RGB Par", "3ch", "RGB (virtual dimmer)"], ["RGBW Par", "4ch", "RGBW (virtual dimmer)"],
    ["RGBW Par Dim", "6ch", "Dimmer, RGBW, Strobe"], ["RGBWAUV Par", "6ch", "RGBWAUV (virtual dimmer)"], ["Strobe", "2ch", "Dimmer, Strobe"], ["LED Bar 8", "24ch", "RGB, 8 cells"],
    ["LED Batten 16 RGBW", "65ch", "Dimmer, RGBW, 16 cells"], ["Pixel Strip 170", "510ch", "RGB, 170 cells"], ["Pixel Tube 32", "96ch", "RGB, 32 cells"],
    ["Moving Spot", "16ch", "Pan/Tilt 16-bit, Dimmer 16-bit, Shutter, Colour wheel, Gobo, Prism, Focus, Zoom, Iris"], ["Moving Wash", "14ch", "Pan/Tilt 16-bit, Dimmer 16-bit, Strobe, RGBW, CTO, Zoom"],
    ["Matrix 8x8", "192ch", "RGB, 64 cells"]], [45*mm, 18*mm, 107*mm]))
B("GDTF attributes are mapped as: `Dimmer`; `ColorAdd_R/G/B/W/A/UV`, `ColorRGB_*`; `ColorSub_C/M/Y`; `Pan`, `Tilt`; `Zoom`, `Focus1`, `Iris`; `Shutter1`, `Shutter1Strobe`; `Gobo1`; `Prism1`; `Color1`; `CTO/CTC/CTB`; `Control*`.")

# ------------------------------------------------------------------ 17-20
H1("17. 3D stage view")
story.append(shot("06_stage_3d", "Figure 17-1. 3D stage with live fixture colours from the DMX output and the program on a virtual LED screen"))
B("""
The 3D stage shows the patch in metres (X stage-right +, Y upstage +, Z up) on a 20 × 12 m grid with the audience edge marked. Fixtures are drawn in their output colour (decoded from the DMX being transmitted), multi-cell fixtures per cell, with fixture IDs. Trusses are inferred from fixtures at equal height and depth, and the program output is shown on a virtual LED screen upstage.
- Cameras: Perspective, Top, Front, Side. Wheel zooms; middle-drag (Alt+drag) orbits; right-drag (Shift+drag) pans; **Orbit** auto-rotates.
- Click selects a fixture, Ctrl+click adds to the selection. The X/Y/Z and rX/rY/rZ fields edit the selection (multi-selection sets a common height and rotation).
- **Group from selection** and **Spatial group by** (left/right halves, front/back rows, height levels) create fixture groups for spatial effects.
""")
H1("18. Spatial effects")
story.append(shot("04_pixel_mapper", "Figure 18-1. Spatial Effects panel (right) in the Mapping workspace"))
B("""
Spatial effects compute a phase for every fixture (or cell) from its real position and evaluate a waveform over time. They output DMX directly to the patched fixtures, independent of the pixel map.
""")
story.append(table([["Control", "Description"], ["Order", "Left→right, Right→left, Front→back, Back→front, Top→bottom, Bottom→top, Centre outward, Outside inward, Radial clockwise/anticlockwise, Diagonal, Symmetrical, Mirrored, Random (seeded), Custom grid, User defined"],
    ["Waveform", "Sine, Square, Sawtooth, Ramp, Triangle, Pulse, Chase"], ["Speed / Phase spread / Width / Offset", "cycles per second, phase distributed over the ordering, duty cycle, phase offset"],
    ["Delay per step", "additional time offset per ordering step"], ["Wings / Blocks / Groups", "mirror the order into N wings; consecutive fixtures share a phase; repeat the pattern"],
    ["Random seed", "reproducible randomisation"], ["Intensity low/high, colours, hue sweep", "level range and colour at high/low, or hue rotating across the fixtures"],
    ["Fixture group / exclusion", "restrict to a group; fixtures marked excluded are skipped"], ["Reverse, Bounce, Output level", "direction, ping-pong, final scaling (also scaled by the master)"]], [45*mm, 125*mm]))
B("""
Positions are ranked rather than used as raw distances, so steps are evenly spaced; fixtures at equal distance (for example in symmetrical layouts) share a phase. Capability handling: colour fixtures without a dimmer use the virtual dimmer; fixtures with a dimmer receive intensity on the dimmer with full colour; intensity-only fixtures receive the level; multi-cell fixtures are evaluated per cell; moving heads are held at pan/tilt centre.
""")
H1("19. Virtual dimmers")
B("""
Many LED fixtures have colour channels but no intensity channel. LUMAFLOW's colour engine applies a **virtual dimmer** by scaling all colour emitters by the intensity while preserving the colour ratios. When a fixture has a physical dimmer, intensity goes to the dimmer and the colour mix stays at full — the engine never double-dims.
""")
story.append(table([["Fixture", "Intent: orange (1, 0.5, 0) at 50 %", "DMX result"], ["RGB par (no dimmer)", "virtual dimmer", "R 128, G 64, B 0"], ["RGBW par + dimmer", "physical dimmer", "Dim 128, R 255, G 128, B 0, W 0"],
    ["Dimmer only", "intensity of the intent", "Dim 128"], ["Multi-cell bar with master dimmer", "per-cell virtual dimming, master at full", "Dim 255, cells scaled"]], [50*mm, 60*mm, 60*mm]))
B("These results are verified by the automated tests `colourConversion`, `fixturePixelMapVirtualDimmer`, `fixturePixelMapPhysicalDimmer` and `spatialMixedCapabilities`.")
H1("20. Colour conversion")
B("""
- **RGB → RGBW**: white extraction moves the common component of R, G and B to the white emitter (*min extract*), or 80 % of it (*luminance* mode) to retain saturation.
- **Amber**: optional extraction from the red/green overlap for RGBA/RGBWA fixtures.
- **CMY**: subtractive conversion for CMY fixtures.
- **Colour temperature**: `fromKelvin` (1000–40000 K blackbody approximation) is used for the white-point calibration.
- **Saturation**, **gamma**, per-emitter **gains** and **maximum brightness** are applied per fixture or element (ColorCalibration).
!note Different LED emitters do not match colorimetrically. Without measured per-fixture calibration, the same RGB intent can look different on different fixture models. Use the per-element gains to balance fixtures visually.
""")

# ------------------------------------------------------------------ 21-24
H1("21. grandMA3 integration")
story.append(shot("07_console_links", "Figure 21-1. Console links: grandMA3 and MagicQ on independent universes"))
B(open(os.path.join(ROOT, "fixture-profiles/grandma3/LUMAFLOW_Standard_setup.md")).read().split("| Ch |")[0].replace("# ", "## "))
B("""
## Programming workflow (example)
- Patch LUMAFLOW (Standard 32ch) at universe 1 address 1 on the console and in **Console Mapping → Console links**.
- In LUMAFLOW store scenes (Show Control). The scene names appear as channel sets of the *MediaContent1* (scene) channel after re-exporting the personality.
- On the console select the LUMAFLOW fixture; set Dimmer (master), Effects1 (layer 1 effect) and Effects1Rate (speed); store cue 1. Repeat with other effects/scenes and transition times (Effects1Fade).
- Use the Control channel (GO, BACK, blackout, tap, take) for show-control triggers, or SendOSC macros.
## Status monitoring
Enter the console's IP and OSC port as the link's *OSC feedback host/port* to receive `/lumaflow/status/...` messages (chapter 25).
!note Not validated with a physical or onPC grandMA3 in this release. Menu paths follow grandMA3 documentation conventions and must be verified on your software version.
""")
H1("22. MagicQ integration")
B("""
- Copy `fixture-profiles/magicq/LUMAFLOW_MediaServer.gdtf` to the MagicQ show folder and import it with MagicQ's GDTF import (for MagicQ versions that support GDTF). Alternatively create the head in **MagicQ Head Editor** using `LUMAFLOW_<mode>_channels.csv`, which lists each channel, its 16-bit fine channel, a suggested MagicQ attribute type and the DMX ranges of index channels.
- Patch the head at the address set for the *MagicQ* console link (default universe 2, address 1); output Art-Net or sACN to LUMAFLOW.
- OSC: enable OSC in MagicQ network settings with LUMAFLOW's IP and port 8000. Example mappings for `/pb/1` (playback 1 level → layer 1 opacity) and `/pb/1/go` (cue GO) are in `examples/mappings_example.json`; verify the addresses for your MagicQ version.
!note No native .hed file is generated (the format is not publicly documented). Not validated with MagicQ software in this release.
""")
H1("23. Fixture personality generation")
story.append(shot("09_personality", "Figure 23-1. Fixture personality: active profile, pending changes, approval and rollback"))
B("""
One internal channel catalogue drives four modes: **Basic 16**, **Standard 32**, **Extended 64** and **Advanced 128** channels, plus **Custom** (a user-chosen parameter list, CLI/API). Lower modes are strict prefixes of higher modes, and the catalogue is append-only, so adding features never moves existing channels. 16-bit channels (master intensity, positions) use a coarse/fine pair.
## Versioning and approval
- **Check for updates** regenerates the personality from the current parameter registry, effect list and scene list, and compares it to the active profile.
- Differences are listed: added, removed, moved, resolution changed, renamed, index ranges changed. Changes that alter the meaning of an existing DMX channel are flagged **BREAKING**.
- Nothing is applied until you press **Approve pending**. Breaking changes need a second confirmation because consoles must update their fixture type.
- **Reject** discards the pending profile; **Rollback** restores the previous version (full history is stored in the project).
- **Export console files…** writes the GDTF (all four modes, re-imported for verification), grandMA3 setup notes, MagicQ channel sheets, JSON definitions and DMX tables.
LUMAFLOW never modifies the fixture type inside a console; you re-import the exported file.
""")
H1("24. Bidirectional mapping")
story.append(shot("08_console_mapping", "Figure 24-1. Control mapping table"))
B("""
Each mapping connects a source (OSC address pattern, MIDI key, DMX universe/channel[/fine]) to a parameter or an action (`action:cue.go`, `cue.back`, `scene.recall`, `take`, `blackout`, `tap`, `timeline.play/stop`). Mappings belong to a console set (generic, grandMA3, MagicQ) so several consoles can be connected with independent mappings.
""")
story.append(table([["Property", "Effect"], ["In min / In max", "input range (OSC raw value, DMX 0–1, MIDI 0–1)"], ["Out min / Out max", "normalised output range of the target"], ["Invert", "1 − value"],
    ["Deadband", "ignore changes smaller than the value"], ["Smooth", "one-pole smoothing 0–0.99"], ["Edge + threshold", "rising / falling / both: trigger once per crossing"],
    ["Soft takeover", "ignore the control until it passes through the current value (no jumps after external changes)"], ["Feedback", "send the value back (OSC / MIDI) when it is changed by another source"]], [40*mm, 130*mm]))
B("""
## Learn
Choose a target and console set, press **MIDI / OSC / DMX Learn** and move a control: the first MIDI message, OSC address or DMX channel that changes by more than 10 % creates the mapping.
## Ownership policies
**Ownership** sets per-parameter policies: *Last change wins*; *Console priority* (a console that changed a parameter holds it for 2 s against touch/remote); *Touch priority* (the reverse); *Locked* (only the chosen source). A default policy applies to all other parameters.
## Feedback loop prevention
A value is never echoed to the mapping that produced it, identical values are not re-sent, and soft takeover is re-armed whenever another source changes the parameter. DMX is never sent as feedback: transmitting DMX does not update a console's programmer or playbacks.
""")

# ------------------------------------------------------------------ 25-28 protocols
H1("25. OSC")
md = open(os.path.join(DOCS, "OSC_REFERENCE.md")).read()
story.append(table(md_table(md.split("## Feedback")[0]), [62*mm, 35*mm, 73*mm]))
story.append(Paragraph("Feedback messages", S["h2"]))
story.append(table(md_table(md.split("## Feedback")[1].split("Feedback is")[0]), [70*mm, 100*mm]))
B("Mapping address patterns support `*`, `?`, `[a-z]` and `{a,b}`. Bundles are unpacked recursively. Use `lumaflow-cli send-osc HOST PORT ADDRESS ARGS…` for testing.")
H1("26. MIDI")
B(open(os.path.join(DOCS, "MIDI_REFERENCE.md")).read().split("\n", 1)[1].replace("# ", "## "))
H1("27. Art-Net")
B("""
LUMAFLOW implements Art-Net 4 ArtDmx (send and receive), ArtPoll/ArtPollReply (replies as a media server node with up to four ports) and ArtSync encoding.
- **Output**: enable *Art-Net output*, set the target (directed broadcast such as 2.255.255.255 / 10.255.255.255 or a node's unicast IP) and list the output universes.
- **Universe numbering**: LUMAFLOW universes start at 1. With *Art-Net universe offset* 1 (default), LUMAFLOW universe 1 = Art-Net port address 0 (net 0, subnet 0, universe 0). Set the offset to 0 if your console counts from 1 on the wire.
- **Rate**: universes are sent when they change, limited to the configured refresh rate (default 40 Hz, maximum 44 Hz), plus a keep-alive refresh every second. Sequence numbers increment per universe.
- **Input**: enable *Art-Net input* and list input universes; packets from LUMAFLOW's own output socket are ignored (loop prevention). Multiple sources on a universe are merged HTP.
""")
H1("28. sACN")
B("""
ANSI E1.31-2018 data packets (638 bytes for 512 slots) are encoded and decoded with the component identifier (CID, unique per instance), source name, priority (default 100), sequence and options.
- **Output** uses multicast 239.255.<hi>.<lo> per universe or unicast to listed targets.
- **Input** joins the multicast groups of the input universes. Preview packets are ignored, *stream terminated* removes a source, and LUMAFLOW's own packets (same CID) are ignored.
- **Merging**: the highest priority among live sources wins (2.5 s source timeout); sources at equal priority are merged HTP.
## Output diagnostics
""")
story.append(shot("10_output_config", "Figure 28-1. Output configuration with per-universe diagnostics"))

# ------------------------------------------------------------------ 29-31
H1("29. Audio reactivity")
story.append(shot("12_audio", "Figure 29-1. Audio panel with band meters, spectrum and detected tempo (test signal at 128 BPM)"))
B("""
Inputs: ALSA capture devices (Linux), WAV files (`file:/path.wav`, looped in real time) and a built-in test signal (`test:<bpm>`) for rehearsals without audio.
Analysis: 1024-point FFT with 50 % overlap; six bands (sub 20–60 Hz, bass 60–250, low-mid 250–500, mid 500–2k, high-mid 2–6k, treble 6–16k) with automatic gain; RMS level with noise gate; spectral-flux onset detection weighted to low frequencies; BPM estimation from an inter-onset histogram folded into 70–180 BPM. Gain, gate, smoothing and beat sensitivity are adjustable.
Use: each layer's *Audio reactivity* scales the global *Audio reactivity*; bass and beat modulate the layer's scale and brightness. Effects such as Audio Particles, Spectrum, Waveform and Energy Rings read the bands directly; node graphs use Audio Band, Beat Pulse and Spectrum nodes. **Use detected BPM as show tempo** copies the estimate to the show BPM.
The test suite verifies 120 ± 3 BPM detection on a synthetic kick pattern.
""")
H1("30. Timeline and timecode")
story.append(shot("11_timeline", "Figure 30-1. Timeline with automation curves, scene events, OSC events and markers"))
B("""
## Cue list
Cues have a number, name, scene (index or name), fade time and follow mode (Manual, Auto-follow after the fade, Wait N seconds after the fade). GO / BACK / Go to selected; cues can also carry OSC or timeline actions. Cells are editable in place.
## Scenes and transitions
Recalling a scene crossfades the complete composition (effects, media and parameters) from the outgoing state over the scene's transition time or *Transition (s)*.
## Timeline
Tracks: parameter automation (any registry parameter, curves linear/step/ease/smooth), and event tracks for scenes, cues, OSC, MIDI, effects, blackout and actions. Markers, loop region, playback speed, BPM grid with snap. Editing: double-click adds a key (at the current parameter value) or an event; drag keys; right-click deletes; K keys at the playhead; M adds a marker; Ctrl+wheel zooms; drag the ruler to scrub.
## Timecode sources
*internal* (timeline clock), *mtc* (MIDI Time Code), *ltc* (Linear Timecode decoded from the audio input), *sync* (primary instance's clock). In chase modes the timeline follows the external position and fires events only on small forward steps, so relocating does not replay every cue. Rates 24, 25, 29.97 drop-frame and 30 fps are supported.
!note Synchronisation accuracy has not been measured against a reference clock. LUMAFLOW does not claim frame-accurate timecode chase.
""")
H1("31. Video output")
B("""
- **Canvas**: set the program resolution (presets 720p, 1080p, 4K, 3840×1080 ultra-wide, 1024×512 LED, or custom up to 16384).
- **Outputs**: each output has a screen index (−1 windowed, otherwise borderless fullscreen on that screen), a normalised crop of the canvas (multiple regions from one canvas), rotation 0/90/180/270, an optional fixed LED-processor size, an identify test pattern and blackout. **Open output windows** creates them; Shift+Esc closes a fullscreen output.
- **Recording**: records the program to H.264 MP4 through the `ffmpeg` executable (bounded write buffer). `lumaflow-cli render-video` renders a project offline at a fixed frame rate, independent of real time.
- NDI, Spout, Syphon and SDI are not bundled; they can be added as output-module plugins (chapter 35).
""")

# ------------------------------------------------------------------ 32-36
H1("32. Colour calibration")
story.append(shot("14_calibration", "Figure 32-1. Calibration and safety limits"))
B("""
- **Safety limits**: limiter on/off (disabling requires confirmation), maximum flash rate (default 3 Hz, applied to the master strobe and the Strobe effect), maximum output brightness.
- **Video output calibration**: gamma, red/green/blue gain, brightness and white point (2000–12000 K, relative to 6500 K).
- **Master colour**: tint (RGB) and saturation.
- **Test patterns**: colour bars with grid and circle, grey ramp, full white, RGB cycle. A two-pixel red border marks the canvas edge.
- **Calibration presets** save and load the calibration as JSON.
Pixel-mapped fixtures have their own per-element calibration (chapter 13).
""")
H1("33. Remote control")
story.append(shot("15_remote_sync", "Figure 33-1. Remote server users, connected clients and sync status"))
B("""
**Start server** runs an HTTP server on the local network (default port 8080). Open the shown address on a tablet or phone and enter an access token. The page offers GO/BACK/TAKE/TAP/BLACKOUT, the master fader, scene buttons and per-layer effect, opacity, speed, scale and hue.
## Roles
""")
story.append(table([["Role", "Allowed"], ["viewer", "read state, effect and preset lists"], ["operator", "parameters, effects, scenes, presets, cues, take, tap"], ["admin", "everything including blackout and output settings"]], [35*mm, 135*mm]))
B("""
Tokens are generated randomly at startup (shown in the panel); requests are limited to 1 MB; responses are not cached. Remote writes pass through the ownership policies with the source *remote*. The server runs on its own thread; stopping it or losing a client never affects rendering or output.
## REST API
`GET /api/state`, `/api/effects`, `/api/presets`, `/api/whoami`; `POST /api/param {id, value}`, `/api/effect {layer, effect}`, `/api/scene/recall {index}`, `/api/preset/recall {index, layer}`, `/api/cue/go`, `/api/cue/back`, `/api/take`, `/api/tap`, `/api/blackout {on}`. Authenticate with `Authorization: Bearer <token>` or `?token=`.
!note The remote server uses plain HTTP on the local network. Use it on a trusted lighting network or behind a firewall; it is not intended for exposure to the internet.
""")
H1("34. Multi-instance synchronisation")
B("""
Several LUMAFLOW machines can share show state over UDP multicast (239.255.77.77, port 7700) or unicast.
- The **primary** sends heartbeats with its show clock, parameter changes with sequence numbers, and events (scene recall, take, blackout).
- A **secondary** applies them, requests a full snapshot when it joins, after sequence gaps and after reconnecting, and estimates round-trip time and clock offset with ping/pong.
- If two primaries are detected, the instance with the lower ID stays primary and the other becomes secondary (deterministic conflict resolution).
- When the primary disappears, secondaries keep running with the last state and re-synchronise automatically.
The timeline on a secondary can chase the primary clock (TC source *sync*).
!note Alignment depends on network latency and jitter (typically a few milliseconds on a LAN). This is not frame-accurate genlock; use genlock/house sync hardware where frame accuracy is required.
""")
H1("35. Plugin SDK")
B("""
Plugins are shared libraries with a C ABI defined in `lumaflow_plugin.h` (API version 1). A plugin exports `lf_plugin_query()` returning an `LfPluginInfo` (id, name, version, vendor, category, dependencies and arrays of effects, shader nodes and output modules), and optionally `lf_plugin_init(const LfHostApi*)` / `lf_plugin_shutdown()`.
Categories: visual effects, shader nodes, control protocols, fixture importers/exporters, output modules (frame callbacks), media decoders, automation.
## Safety
Every plugin library is first loaded by `lumaflow-cli plugin-probe` in a separate process. A crash, hang (5 s) or invalid response marks the plugin as failed without affecting LUMAFLOW (verified by the `crashingPluginIsolated` test). API version and dependencies are checked; plugins can be disabled, and safe mode loads none.
!note After a plugin passes the probe it runs inside the LUMAFLOW process. Native code cannot be sandboxed by API validation alone. Install only trusted plugins.
## Example
```
#include "lumaflow_plugin.h"
static const LfEffectDesc kEffects[] = {{ "example.starfield", "Starfield (plugin)", "Plugins",
   "vec4 effect(vec2 p, vec2 uv, float t) { ... }", {{"Speed", .3f}, ...}, {.8f,.9f,1}, {.4f,.6f,1}, 0 }};
static const LfPluginInfo kInfo = { LF_PLUGIN_API_VERSION, "io.lumaflow.example.starfield", "Starfield Example",
   "1.0.0", "LUMAFLOW", "Example", LF_PLUGIN_EFFECT, NULL, 1, kEffects, 0, NULL, 0, NULL };
LF_PLUGIN_EXPORT const LfPluginInfo* lf_plugin_query(void) { return &kInfo; }
```
Build it as a MODULE library (see `sdk/examples/effect_starfield`) and install it with **Plugin Manager → Install plugin…**.
""")
H1("36. AI effect generation")
story.append(shot("16_ai_assistant", "Figure 36-1. AI Effect Assistant: local generation of a blue particle vortex"))
B("""
The assistant turns a description into a layer preset, a node graph, a GLSL shader or a spatial effect.
- **Local generator** (default, offline, deterministic): recognises effect families (vortex, plasma, lightning, smoke, kaleidoscope…), colours, speed words, audio reactivity, density/scale/spin and spatial terms (chase, centre-out, left to right, symmetrical, radial, wings, colour wave). Combinations of families produce a node graph.
- **External model** (optional, off by default): sends the prompt to the configured Anthropic Messages API endpoint and model; the API key is read from an environment variable and never stored in projects. The model must return one `effect()` function.
- **Validate & apply to preview** compiles generated shaders and graphs in an isolated validation program. Failures are reported and not activated; common mistakes (GLSL ES precision lines, `texture2D`, `gl_FragColor`, integer literals) are repaired automatically, and the external model can be asked to repair its own error. Valid results go to the preview layer; **TAKE** sends them live.
""")
story.append(table([["Prompt", "Result"], ["Create a blue particle vortex", "layer preset: Particle Vortex, colour A blue"], ["Generate a red and white centre-out chase", "spatial: Centre outward, Chase, red/white"],
    ["Create an audio-reactive plasma effect", "layer preset: Plasma, audio reactivity 0.8"], ["Generate a symmetrical effect using fixture positions", "spatial: Symmetrical, Sine"],
    ["Create a slow colour wave across LED strips", "spatial: hue sweep, speed 0.2"], ["Combine plasma with particle rings and music", "node graph: warp → particles + rings × audio → palette"]], [80*mm, 90*mm]))
B("The external backend was implemented against the documented API but not exercised in the release environment (no API key).")

# ------------------------------------------------------------------ 37-39
H1("37. Project management")
B("""
""")
story.append(table([["File", "Content"], [".lumaflow", "project: compositions, scenes, presets, cue list, timeline, patch and show fixture types, pixel map, spatial effects, mappings & policies, personality profiles, console links, network, video outputs, calibration, safety, audio, custom effects, plugin references"],
    [".lfpreset", "one layer preset"], [".lfscene", "one scene"], [".lfeffect", "custom effect: GLSL, parameters, colours, optional node graph"], [".lfgraph", "node graph"]], [30*mm, 140*mm]))
B("""
All files are JSON with `format` and `schemaVersion`. Saving is atomic (temporary file, rename) and verified by reading the file back. Projects from older schemas (v1, v2) are migrated in memory and the original file is untouched until you save; files from a newer LUMAFLOW are refused rather than risk data loss. On load, missing media, unresolved fixture types, missing plugins and missing effects are reported.
**File → New / Open / Open recent / Save / Save as / Import scene / Import preset / Load demo show**.
""")
H1("38. Backup and recovery")
story.append(shot("13_performance_recovery", "Figure 38-1. Performance monitor and recovery dashboard"))
B("""
- **Autosave** every 30 s when there are changes (`autosave.lumaflow` in the data folder).
- **Versioned backups**: each save copies the previous file to `backups/<name>_<timestamp>.lumaflow`; the newest 20 are kept.
- **Crash recovery**: a session lock file detects an unclean shutdown and the next launch offers the autosave.
- **Last-known-good**: after 60 s of running without critical faults, the state is saved as `last-known-good.lumaflow`.
- **Recovery dashboard**: active faults with severity, subsystem, message, repetition count and recommended action; backups list with restore (the current file is kept as `.before-restore`); autosave/LKG loading; live log.
- **Safe mode** (`--safe-mode`): no plugins, custom effects, DMX output or remote control, low render quality.
- **Watchdog and isolation**: render stalls are reported and recovered; shader failures affect one layer; network bind errors and missing media are reported without stopping the renderer.
""")
H1("39. Performance optimisation")
B("""
- Use **Adaptive** quality for shows; it keeps the frame budget by lowering the layer render scale.
- Set layers you are not using to opacity 0: invisible layers are not rendered.
- Prefer a canvas that matches your outputs; pixel mapping samples a bounded number of taps per pixel.
- Particle effects with many particles (Explosion, Attract/Repel) are the most expensive; reduce *Count*.
- Use the **Performance Monitor** for frame time, submit/GPU/readback breakdown, texture memory and GPU memory (NVIDIA/AMD extensions).
- Benchmark with `lumaflow-cli benchmark --width 1920 --height 1080 -o results.json` and record the hardware.
""")
bench = json.load(open(os.path.join(DOCS, "benchmark_llvmpipe_1080p.json")))
rows = [["Effect", "ms/frame", "fps"]] + [[r["effect"], "%.1f" % r["msPerFrame"], "%.1f" % r["fps"]] for r in bench["results"]]
half = (len(rows) + 1) // 2
t1 = table(rows[:half], [36*mm, 22*mm, 18*mm]); t2 = table([rows[0]] + rows[half:], [36*mm, 22*mm, 18*mm])
story.append(Table([[t1, t2]], colWidths=[85*mm, 85*mm], style=[("VALIGN", (0, 0), (-1, -1), "TOP")]))
story.append(Paragraph("Table 39-1. Single-layer benchmark at 1920×1080 on " + esc(bench["gpu"].split("|")[0]) + " (CPU software rendering, 4 cores). Hardware GPUs are expected to be one to two orders of magnitude faster; measure on target hardware.", S["caption"]))

# ------------------------------------------------------------------ 40-42
H1("40. Troubleshooting")
story.append(table(md_table(open(os.path.join(DOCS, "TROUBLESHOOTING.md")).read()), [52*mm, 45*mm, 73*mm]))
H1("41. Technical reference")
B("""
## Parameter registry
Global: `master.intensity` (16-bit), `master.strobe`, `master.red/green/blue`, `master.saturation`, `master.blackout`, `show.scene`, `show.transition`, `show.control`, `show.bpm`, `audio.reactivity`, `audio.gain`, `audio.smoothing`, `ext.1`–`ext.8`.
Per layer (N = 1–6): `effect`, `opacity`, `speed`, `scale`, `rotation`, `posx`, `posy`, `hue`, `hue2`, `saturation`, `brightness`, `contrast`, `p1`–`p4`, `audio`, `blend`.
## Network ports
Art-Net UDP 6454 · sACN UDP 5568 · OSC UDP 8000 (configurable) · Remote TCP 8080 (configurable) · Sync UDP 7700.
## DMX personality – Standard 32ch
""")
md = open(os.path.join(ROOT, "fixture-profiles/definitions/LUMAFLOW_Standard_DMX.md")).read()
rows = md_table(md)
story.append(table([r[:5] for r in rows], [12*mm, 12*mm, 40*mm, 50*mm, 56*mm]))
B("Full tables for all four modes: `documentation/DMX_CHANNEL_REFERENCE.md` and `fixture-profiles/definitions/`.")
story.append(Paragraph("Known limitations", S["h2"]))
B(open(os.path.join(DOCS, "KNOWN_LIMITATIONS.md")).read().split("\n", 2)[2].replace("## ", "### "))
story.append(Paragraph("Command-line tools", S["h2"]))
story.append(table([["lumaflow-cli command", "Purpose"], ["info", "version, GPU, FFmpeg, MIDI and audio backends"], ["effects", "list effects and parameters"], ["render-frame / render-all", "render effects to PNG"],
    ["render-video", "offline render of a project to MP4"], ["benchmark", "per-effect frame times (JSON output)"], ["export-profiles", "GDTF + console files with round-trip check"], ["import-gdtf / import-mvr / export-mvr", "fixture data tools"],
    ["validate-shader / compile-graph", "check custom shaders and graphs"], ["demo-project / project-info", "create / inspect projects"], ["send-osc / send-artnet / artnet-monitor", "protocol test tools"], ["ai PROMPT", "local AI generator"], ["plugin-probe", "isolated plugin validation (used by the host)"]], [60*mm, 110*mm]))
H1("42. Glossary")
story.append(table([["Term", "Meaning"], ["Art-Net", "UDP protocol carrying DMX512 universes over Ethernet (Artistic Licence)"], ["sACN / E1.31", "ANSI standard for streaming DMX over IP multicast"], ["DMX512", "lighting control protocol, 512 channels (slots) per universe"],
    ["Universe", "a set of 512 DMX channels"], ["Footprint", "number of DMX channels a fixture mode uses"], ["Personality / fixture type", "description of a fixture's channels for a console"], ["GDTF", "General Device Type Format (DIN SPEC 15800) fixture description"],
    ["MVR", "My Virtual Rig (DIN SPEC 15801) scene exchange format"], ["Virtual dimmer", "intensity applied by scaling colour channels on fixtures without a dimmer"], ["HTP / LTP", "highest-takes-precedence / latest-takes-precedence merging"],
    ["OSC", "Open Sound Control, UDP message protocol"], ["MTC / LTC", "MIDI Time Code / Linear (audio) Time Code"], ["Pixel mapping", "sampling video onto individually controlled light sources"], ["Soft takeover", "a physical control only takes effect after passing the current value"],
    ["Program / Preview", "live output / composition being prepared"], ["Take", "timed transition from preview to program"], ["Watchdog", "monitor that detects a stalled render thread"], ["Safe mode", "start-up with optional components disabled"], ["llvmpipe", "Mesa CPU software OpenGL renderer"]], [40*mm, 130*mm]))


# ------------------------------------------------------------------ appendices
H1("Appendix A. Tutorials")
B("""
## A.1 Build a first show in ten minutes
- Start LUMAFLOW and choose **File → Load demo show**, or start empty with **File → New project**.
- In **Layers** select L1. In the **Effects Library** double-click *Plasma*. The program monitor shows it immediately.
- Select L2, double-click *Particle Cloud*, set its blend to **Additive** and opacity to 0.6 in the Layers table.
- In **Parameters** adjust *Speed*, *Scale* and *Colour A/B*. Every change can be undone with Ctrl+Z.
- In **Show Control** press **Store** and name the scene *Opening*. Change the look and store a second scene.
- Press **Add cue** twice and set the *Scene* column to 1 and 2, the *Fade* to 3 s. Press **GO** (Space) to run the cues.
- Save with **File → Save** (Ctrl+S). Each later save creates a versioned backup.
## A.2 Prepare looks safely in preview
- In **Layers** enable **Edit preview**. Library double-clicks and parameter edits now go to the preview monitor only.
- Build the next look while the program keeps running.
- Set **Transition (s)** and press **TAKE** (Enter). The output crossfades to the new look.
## A.3 Pixel-map an LED strip
- Patch nothing: strips do not need fixtures. In **Pixel Mapper** press **+ Strip** and enter 200 pixels.
- Edit the *Univ* and *Addr* cells (for example universe 10, address 1) and the *Order* (e.g. GRB for WS2812-type controllers).
- Choose **Test: identify**: the first pixel is white and the rest a colour, so you can check start and direction on the real strip.
- Drag nothing: the strip line is drawn over the program canvas; place it with the generator or edit the project JSON for exact coordinates.
- In **Output Configuration** enable **Art-Net output**, set the node's IP or broadcast address and list universes 10-11 (200 RGB pixels need two universes with pixel-aligned splitting). Press **Apply network settings** and return the test pattern to *off (live)*.
## A.4 Import a rig from MVR and create a spatial chase
- **Fixture Manager → Import MVR…** and review the import warnings. Correct addresses or positions in the table if needed.
- Open **3D Stage**, use the Top camera, select fixtures and **Group from selection** (e.g. *Truss 1*).
- In **Spatial Effects** press **+ Spatial effect**, select the group, order *Centre outward*, waveform *Chase*, width 0.3, colours red/white, and tick **Enabled**.
- Watch the chase in the 3D stage; enable the output universes in Output Configuration to send it to the fixtures.
## A.5 Connect grandMA3 over sACN
- **Console Mapping → Fixture personality**: choose *Standard 32ch*, press **Export console files…** and copy `LUMAFLOW_MediaServer.gdtf` to the console.
- Import the fixture type on the console and patch it at universe 1 address 1 (the default for the *grandMA3* link).
- In LUMAFLOW **Console links** tick **Enabled** for grandMA3 (this enables DMX input for that universe).
- Output sACN universe 1 from the console to LUMAFLOW's network. Move the Dimmer: the master follows. Select effects with Effects1.
- Optional OSC: set the link's feedback host to the console IP and add `SendOSC` macros (chapter 21).
## A.6 Make a custom effect in the node editor
- Switch to the **Programming** workspace. The node editor opens with an example graph.
- Double-click *Kaleidoscope* in the palette; connect *Coordinates.p* → *Kaleidoscope.p* and *Kaleidoscope.p* → *Noise Warp.p*.
- Watch the preview update (Live preview). Double-click the Kaleidoscope node and set segments to 8.
- Add an *Exposed Parameter* node, label it *Warp*, and connect it to *Noise Warp.amount*; it appears as Param 1 on the layer and can be mapped to a console.
- Name the graph and press **Save as effect**. It is now in the library under *Custom* and saved as `.lfeffect`.
## A.7 Audio-reactive show
- **Audio** panel: choose an input (or `test:128` to rehearse), tick **Enable audio input**, adjust gain until the Level meter moves and beats are counted.
- Load *Audio Particles* or *Energy Rings*; set each layer's **Audio reactivity** (Parameters → Colour group) to 0.6–1.0.
- Press **Use detected BPM as show tempo** to set the show BPM for the timeline grid.
## A.8 Recover from a problem during a show
- If a custom shader fails, only that layer stops and the previous version keeps playing; read the message in **Recovery**.
- If the application closes unexpectedly, restart it: accept the autosave recovery prompt.
- To roll back a project, select a backup in **Recovery → Backups** and press **Restore selected backup**.
- If a plugin prevents starting, launch with `--safe-mode` and disable it in **Plugin Manager**.
""")

H1("Appendix B. Effect reference")
DESC = {
 "particle_cloud": "Layered field of drifting, twinkling particles in palette colours. Count sets density, Size the particle radius, Drift the lateral motion, Glow adds bloom.",
 "particle_explosion": "Repeating bursts of particles from the centre with gravity and trails. Burst rate sets the period, Spread the velocity, Trail the particle width.",
 "particle_storm": "Wind-driven streaks with turbulence and occasional flashes. Density, Wind speed, Turbulence and Streak length.",
 "particle_vortex": "Particles spiralling into a logarithmic vortex. Twist sets the spiral tightness, Suction the inward speed.",
 "particle_trails": "Horizontal comets with fading trails. Count, Length, speed variation and vertical Wave.",
 "particle_tunnel": "Particles streaming towards the viewer through a cylindrical tunnel.",
 "particle_attract": "Particles pulled towards (or pushed from) a moving attractor; Repel above 0.5 inverts the force, Orbit adds tangential motion.",
 "audio_particles": "Particle cells that expand on beats and sparkle with treble energy; strongest with an audio input.",
 "plasma": "Classic sine plasma with optional noise warp, contrast shaping and colour cycling.",
 "lightning": "Random branching bolts with flicker. Photosensitive: the flash rate follows Rate.",
 "electric_arcs": "Jittering arcs between points on a circle with mains hum flicker.",
 "energy_rings": "Concentric rings travelling outward; Audio makes them pulse on beats.",
 "pulse_field": "Grid of pulsing cells with individual phases; Sharpness narrows the pulse.",
 "neon_lines": "Glowing horizontal neon lines that wobble.",
 "kaleidoscope": "Mirrored segments of an evolving noise texture. Segments 3–16, Zoom, Detail and Spin.",
 "fractal": "Animated Julia set with smooth colouring; Iterations controls detail and cost.",
 "mandala": "Layered petal rings rotating in alternate directions with breathing radius.",
 "geo_tunnel": "Polygonal tunnel with depth lines; Sides 3–9.",
 "rotating_pattern": "Counter-rotating tiled circles/squares.",
 "morph_geometry": "Polygons morphing between vertex counts and a circle with echoes.",
 "wireframe": "Retro scrolling wireframe terrain with horizon glow.",
 "smoke": "Rising fBm smoke in colour A over colour B.",
 "fluid": "Domain-warped flowing colour bands suggesting liquid motion.",
 "ink": "Ink clouds spreading from the centre with billowing edges.",
 "flow_field": "Lines following a noise flow field.",
 "turbulence": "Ridged turbulence noise with adjustable octaves and sharpness.",
 "organic_morph": "Metaball blobs merging and separating with rim highlights.",
 "glitch": "Block displacement, RGB split and scan lines. Photosensitive at high rates.",
 "strobe": "Full-frame strobe; Rate is capped by the safety limiter. Beat sync above 0.5 follows audio beats.",
 "gradient": "Linear or radial gradient between colour A and B with scrolling and banding.",
 "text": "Renders the layer's text string with scroll, wave and outline glow; Size sets the width.",
 "logo": "Displays an image (Logo / Image) with pulse, spin, glow and tint.",
 "spectrum": "Live FFT spectrum bars, optionally mirrored.",
 "waveform": "Glowing wave lines driven by audio level and spectrum.",
 "feedback": "Feeds the previous frame back with zoom and rotation, seeded by a moving dot, creating trails.",
 "solid": "Solid colour (mix of A and B) with optional pulse; useful for wash looks and tests.",
 "test_pattern": "Colour bars + grid + circle, grey ramp, full white or RGB cycle, with canvas edge markers.",
}
rows = [["Effect", "Description"]]
for line in open(os.path.join(FX, "effects.tsv")):
    pr = line.rstrip("\n").split("\t")
    if len(pr) >= 3: rows.append([pr[2] + "\n(" + pr[0] + ")", DESC.get(pr[0], "")])
story.append(table(rows, [45*mm, 125*mm]))
for cat in ["Particles", "Energy", "Geometry", "Organic", "Additional"]:
    story.append(PageBreak()); story.append(Paragraph(cat + " effects", S["h2"]))
    cells, row = [], []
    for line in open(os.path.join(FX, "effects.tsv")):
        pr = line.rstrip("\n").split("\t")
        if len(pr) < 3 or pr[1] != cat: continue
        pth = os.path.join(FX, pr[0] + ".png")
        row.append([Image(pth, 80*mm, 45*mm), Paragraph("<b>" + esc(pr[2]) + "</b>: " + esc(DESC.get(pr[0], "")), S["cell"])])
        if len(row) == 2: cells.append(row); row = []
    if row: cells.append(row + [""])
    g = Table(cells, colWidths=[85*mm] * 2); g.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BOTTOMPADDING", (0, 0), (-1, -1), 8)]))
    story.append(g)

H1("Appendix C. DMX personality tables")
B("All four modes are generated from the same catalogue. Index channels use fixed 8-value ranges per entry (first 32 entries).")
for mode in ["Basic", "Standard", "Extended", "Advanced"]:
    md = open(os.path.join(ROOT, "fixture-profiles/definitions/LUMAFLOW_%s_DMX.md" % mode)).read()
    story.append(Paragraph(mode + " mode", S["h2"]))
    story.append(table([r[:5] for r in md_table(md)], [12*mm, 12*mm, 40*mm, 50*mm, 56*mm]))

H1("Appendix D. Control channel and OSC quick reference")
B(open(os.path.join(DOCS, "DMX_CHANNEL_REFERENCE.md")).read().split("## LUMAFLOW")[0].split("\n", 2)[2].replace("## ", "### "))
story.append(Paragraph("Example mappings file", S["h2"]))
mp = json.load(open(os.path.join(ROOT, "examples/mappings_example.json")))
story.append(table([["Console", "Source", "Address", "Target"]] + [[m["console"], m["source"], m["address"], m["target"]] for m in mp["mappings"]], [28*mm, 18*mm, 60*mm, 64*mm]))
story.append(Paragraph("Hardware validation checklist", S["h2"]))
story.append(table(md_table(open(os.path.join(DOCS, "HARDWARE_VALIDATION_CHECKLIST.md")).read()), [8*mm, 62*mm, 35*mm, 50*mm, 15*mm]))
story.append(Paragraph("Compatibility matrix", S["h2"]))
story.append(table(md_table(open(os.path.join(DOCS, "COMPATIBILITY_MATRIX.md")).read()), [28*mm, 40*mm, 34*mm, 34*mm, 34*mm]))

H1("Appendix E. Keyboard shortcuts and panel reference")
story.append(table([["Key", "Action"], ["Space", "GO (next cue)"], ["Enter", "TAKE preview → program"], ["B", "Blackout toggle"], ["T", "Tap tempo"], ["Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S", "New / open / save / save as"],
    ["Ctrl+Z / Ctrl+Shift+Z", "Undo / redo"], ["F11 (platform full-screen key)", "Full-screen main window"], ["K (timeline focused)", "Keyframe at playhead"], ["M (timeline focused)", "Marker at playhead"],
    ["Ctrl+wheel (timeline)", "Zoom timeline"], ["Wheel / double-click (monitors)", "Zoom / reset monitor"], ["Shift+Esc (output window)", "Close fullscreen output"]], [70*mm, 100*mm]))
story.append(Paragraph("Panel reference", S["h2"]))
story.append(table([["Panel", "Purpose", "Chapter"], ["Effects Library", "browse, search, favourite, import/export effects", "8"], ["Layers", "layer on/solo/blend/opacity, preview editing", "11"], ["Parameters", "transform, effect, colour, mask, presets for the selected layer", "11"],
    ["Show Control", "GO/BACK/TAKE, master, transition, tempo, scenes, cue list", "30"], ["Timeline", "automation and event tracks, timecode source", "30"], ["Node Editor", "visual effect programming", "10"],
    ["Pixel Mapper", "strips, matrices, fixtures, test patterns, conflicts", "13"], ["3D Stage", "fixture positions, cameras, groups, live colours", "17"], ["Spatial Effects", "position-based fixture effects", "18"], ["Fixture Manager", "patch, MVR/GDTF import/export, validation", "14–16"],
    ["Console Mapping", "console links, mappings, learn, ownership, personality", "21–24"], ["Output Configuration", "Art-Net/sACN/OSC settings, diagnostics, video outputs, recording", "27, 28, 31"], ["Media Library", "import, assign, relink media", "12"],
    ["Audio", "input, analysis, BPM", "29"], ["Calibration", "safety limits, output calibration, test patterns", "32"], ["Remote & Sync", "web remote server, users, clients, sync role", "33, 34"], ["Plugin Manager", "install, enable, remove plugins", "35"],
    ["AI Effect Assistant", "generate and validate effects from text", "36"], ["Performance Monitor", "frame rate, timing, quality, memory", "39"], ["Recovery", "faults, backups, autosave, log", "38"]], [40*mm, 105*mm, 25*mm]))
story.append(PageBreak()); story.append(Paragraph("Plugin manager and calibration", S["h2"]))
story.append(shot("14_calibration", "Figure E-1. System workspace: calibration panel with the plugin manager tab (the example Starfield plugin is listed when installed)"))

doc = Doc(os.path.join(DOCS, "LUMAFLOW_Professional_User_Manual.pdf"), "Professional User Manual", "LUMAFLOW 3.0 user manual")
doc.multiBuild(story)
print("pages:", doc.page)
