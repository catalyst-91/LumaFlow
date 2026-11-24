#!/usr/bin/env python3
"""Builds LUMAFLOW_Demo_Script.pdf and LUMAFLOW_Demo_Storyboard.pdf from timing.json (and frames of the final video)."""
import os, sys, json, subprocess
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "tools")); sys.path.insert(0, HERE)
from manual_lib import *
from demo_segments import SEGMENTS
timing = json.load(open(os.path.join(HERE, "timing.json")))
def mmss(t): return "%02d:%02d" % (int(t) // 60, int(t) % 60)
def describe(actions):
    out = []
    for off, a in actions:
        k = a["action"]
        d = {"workspace": "switch to %s workspace" % a.get("name"), "raise": "show %s panel" % a.get("dock"), "effect": "load effect '%s' on layer %d%s" % (a.get("id"), a.get("layer", 0) + 1, " (preview)" if a.get("preview") else ""),
             "scene": "recall scene %d (%s s)" % (a.get("index", 0) + 1, a.get("transition", "")), "take": "TAKE preview to program", "cue": "GO next cue", "ramp": "ramp %s %s→%s" % (a.get("id"), a.get("from"), a.get("to")),
             "param": "set %s = %s" % (a.get("id"), a.get("value")), "artnet": "simulated console sends ArtDmx %s" % a.get("channels"), "osc": "OSC %s %s" % (a.get("address"), a.get("args")),
             "pixelTest": "pixel test pattern %s" % a.get("pattern"), "spatial": "spatial effect %d: %s %s" % (a.get("index", 0) + 1, a.get("order", ""), a.get("waveform", "")),
             "brokenEffect": "load deliberately broken shader on layer %d" % (a.get("layer", 1) + 1), "testPattern": "calibration test pattern %s" % a.get("pattern"),
             "title": "title card", "demoShow": "load demo show", "audio": "enable audio input %s" % a.get("device"), "timeline": "timeline %s" % ("play" if a.get("play") else "pause"),
             "click": "press '%s' in %s" % (a.get("button"), a.get("dock")), "tab": "%s tab %d" % (a.get("dock"), a.get("index", 0)), "aiPrompt": "enter AI prompt '%s'" % a.get("text"),
             "console": "console link %d %s" % (a.get("index", 0) + 1, "enabled" if a.get("enabled") else "disabled"), "network": "enable Art-Net input", "layerSource": "layer %d mask %s" % (a.get("layer", 0) + 1, a.get("mask")),
             "layer": "select layer %d" % (a.get("index", 0) + 1), "caption": "on-screen label: " + a.get("text", ""), "resize": "window 1920×1080"}.get(k, k)
        out.append("+%.1fs %s" % (off, d))
    return "<br/>".join(esc(x) for x in out) if out else "—"
# ---------------- script
story = [Spacer(1, 1), NextPageTemplate("body"), PageBreak(), Paragraph("Narration script", S["h1"])]
story += blocks_to_flowables("""
Recorded live from LUMAFLOW 3.0 (Linux build, Mesa llvmpipe on a 1920×1080 virtual display) by the scripted demo director (`--demo-script video/demo_script.json`). Narration is synthesised with espeak-ng; captions show the same text. No console hardware was available: the console segment uses a software Art-Net sender and is labelled SIMULATED on screen.
""")
rows = [["Time", "Chapter", "Narration", "On-screen actions"]]
for s, t in zip(SEGMENTS, timing):
    rows.append([mmss(t["start"]) + "–" + mmss(t["start"] + t["length"]), s["chapter"], s["text"], describe(s["actions"])])
tab = Table([[Paragraph(md_inline(c) if i else c, S["cellb"] if r == 0 else S["cell"]) for i, c in enumerate(row)] if r else [Paragraph(c, S["cellb"]) for c in row] for r, row in enumerate(rows)],
            colWidths=[18*mm, 27*mm, 70*mm, 55*mm], repeatRows=1)
tab.setStyle(TableStyle([("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#c9cde3")), ("BACKGROUND", (0, 0), (-1, 0), BLUE), ("VALIGN", (0, 0), (-1, -1), "TOP")]))
story.append(tab)
d = Doc(os.path.join(HERE, "LUMAFLOW_Demo_Script.pdf"), "Demo Video Script", "LUMAFLOW demo script"); d.multiBuild(story); print("script pages", d.page)
# ---------------- storyboard (frames from the real video)
video = os.path.join(HERE, "LUMAFLOW_Official_Demo.mp4")
if os.path.exists(video):
    fdir = os.path.join(HERE, "work/frames"); os.makedirs(fdir, exist_ok=True)
    story = [Spacer(1, 1), NextPageTemplate("body"), PageBreak(), Paragraph("Storyboard", S["h1"]),
             Paragraph("One frame per segment, extracted from the final video at the middle of each segment.", S["body"])]
    cells = []
    for i, (s, t) in enumerate(zip(SEGMENTS, timing)):
        mid = t["start"] + t["length"] * 0.6; fp = os.path.join(fdir, "f%02d.jpg" % i)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-ss", "%.2f" % mid, "-i", video, "-frames:v", "1", "-vf", "scale=640:-1", fp], check=True)
        cells.append([Image(fp, 80*mm, 45*mm), Paragraph("<b>%s · %s</b><br/>%s" % (mmss(t["start"]), esc(s["chapter"]), esc(s["text"][:220] + ("…" if len(s["text"]) > 220 else ""))), S["cell"])])
    rows = [cells[i:i + 2] for i in range(0, len(cells), 2)]
    rows = [r + [""] * (2 - len(r)) if len(r) < 2 else r for r in rows]
    flat = [[c[0] if c else "", c[1] if c else ""] for r in rows for c in r]
    grid = []
    for r in rows:
        grid.append([r[0][0], r[1][0] if r[1] else ""]); grid.append([r[0][1], r[1][1] if r[1] else ""])
    g = Table(grid, colWidths=[85*mm, 85*mm]); g.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    story.append(g)
    d = Doc(os.path.join(HERE, "LUMAFLOW_Demo_Storyboard.pdf"), "Demo Video Storyboard", "LUMAFLOW storyboard"); d.multiBuild(story); print("storyboard pages", d.page)
