#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from manual_lib import *
DOCS = os.path.join(ROOT, "documentation"); SHOT = os.path.join(DOCS, "screenshots")
story = [Spacer(1, 1), NextPageTemplate("body"), PageBreak()]
def H1(t, brk=True):
    if brk: story.append(PageBreak())
    story.append(Paragraph(esc(t), S["h1"]))
H1("1. Install", False)
story += blocks_to_flowables("""
- **Linux**: `sudo apt install ./lumaflow_3.0.0_amd64.deb`, then start *LUMAFLOW* from the application menu or run `lumaflow`.
- **Windows / macOS**: run the installer / open the DMG produced by the build pipeline (see the full manual, chapters 3–4).
- Check the GPU: `lumaflow-cli info` must report a GPU with OpenGL 3.3 or newer.
!note Photosensitivity warning: LUMAFLOW can produce flashing content. The safety limiter is on by default (3 Hz).
""")
H1("2. The screen", False)
story.append(image(os.path.join(SHOT, "01_main_live.png"), 170, "Live workspace"))
story.append(table([["Area", "Use"], ["Effects Library (left)", "double-click an effect to load it on the selected layer"], ["Layers (left, bottom)", "select layer, on/solo, blend, opacity, Edit preview"],
    ["Preview / Program (centre)", "green = preparing, red = live output"], ["Show Control (right)", "GO, BACK, TAKE, BLACKOUT, master, scenes, cues"], ["Parameters (right)", "speed, scale, colours, effect parameters, mask, presets"], ["Bottom tabs", "Timeline, 3D Stage, Pixel Mapper, Console Mapping, Recovery"]], [55*mm, 115*mm]))
H1("3. Five-minute show")
story += blocks_to_flowables("""
- **File → Load demo show** (24 fixtures, pixel map, 6 scenes, cues, timeline) – or start with your own effects.
- Select layer 1 and double-click *Plasma*; select layer 2, double-click *Particle Cloud*, set blend *Additive*.
- Tune *Speed*, *Scale*, *Colour A/B* in Parameters. Ctrl+Z undoes any change.
- **Store** a scene; change the look; store another.
- **Add cue** for each scene; press **GO** (Space) to run them with crossfades.
- **File → Save** (Ctrl+S).
""")
H1("4. Send light to fixtures")
story += blocks_to_flowables("""
- **Mapping** workspace → **Fixture Manager**: add fixtures (type, mode, count, universe) or **Import MVR…**.
- **Pixel Mapper**: **+ From patch (front)** maps fixtures onto the canvas; **+ Strip / + Matrix** for LED pixels. Use *Test: identify* to check wiring.
- **Spatial Effects**: position-based chases and waves on fixture groups (enable to output).
- **Output Configuration**: tick *Art-Net output* or *sACN output*, set target, list **Output universes** (only these are sent), **Apply**.
""")
story.append(image(os.path.join(SHOT, "04_pixel_mapper.png"), 150, "Mapping workspace"))
H1("5. Connect a console")
story += blocks_to_flowables("""
- **Console Mapping → Fixture personality → Export console files…** writes `LUMAFLOW_MediaServer.gdtf` (Basic 16 / Standard 32 / Extended 64 / Advanced 128 channels).
- Import the GDTF on grandMA3 (or on MagicQ versions with GDTF import; otherwise build the head from the CSV sheet) and patch it.
- **Console links**: enable *grandMA3* and/or *MagicQ* with the same universe/address; send Art-Net or sACN to this computer.
- OSC: send to UDP port 8000, e.g. `/lumaflow/cue/go`, `/lumaflow/scene/recall 2`, `/lumaflow/param/layer1.speed 0.5`.
- MIDI: **Control mapping → Learn**, then move a control.
!note Console integration was tested with simulated consoles only; verify on your console before a show.
""")
H1("6. If something goes wrong")
story.append(table([["Problem", "Do this"], ["Black output", "check Master, BLACKOUT, layer opacity; Recovery panel lists shader errors"], ["No light on fixtures", "enable the universe in Output universes; check Art-Net offset"],
    ["Crash", "restart and accept the autosave recovery"], ["Bad change", "Recovery → Backups → Restore, or Ctrl+Z"], ["Plugin problem", "start with --safe-mode"]], [45*mm, 125*mm]))
story += blocks_to_flowables("Full documentation: *LUMAFLOW_Professional_User_Manual.pdf*. Known limitations: chapter 41.")
doc = Doc(os.path.join(DOCS, "LUMAFLOW_Quick_Start_Guide.pdf"), "Quick Start Guide", "LUMAFLOW quick start")
doc.multiBuild(story); print("pages:", doc.page)
