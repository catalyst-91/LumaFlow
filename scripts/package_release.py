#!/usr/bin/env python3
"""Assembles releases/3.0.0 with applications, console files, effects, documentation, video and QA, plus checksums."""
import os, shutil, hashlib, json, subprocess, zipfile, datetime
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); R = os.path.join(ROOT, "releases/3.0.0")
def cp(src, dst):
    d = os.path.join(R, dst); os.makedirs(os.path.dirname(d), exist_ok=True)
    s = os.path.join(ROOT, src)
    (shutil.copytree(s, d, dirs_exist_ok=True) if os.path.isdir(s) else shutil.copy2(s, d))
for sub in ["applications", "console-integration", "effects", "documentation", "video", "quality-assurance"]: os.makedirs(os.path.join(R, sub), exist_ok=True)
# applications
cp("build-release/lumaflow_3.0.0_amd64.deb", "applications/lumaflow_3.0.0_amd64.deb")
cp("build-release/lumaflow-3.0.0-Linux.tar.gz", "applications/lumaflow-3.0.0-Linux.tar.gz")
src = os.path.join(R, "applications/lumaflow-3.0.0-source.tar.gz")
subprocess.run(["git", "-C", ROOT, "archive", "--format=tar.gz", "--prefix=lumaflow-3.0.0/", "-o", src, "HEAD", "--", ".", ":!video/*.mp4", ":!releases"], check=True)
open(os.path.join(R, "applications/BUILD_INSTRUCTIONS.md"), "w").write(open(os.path.join(ROOT, "documentation/DEVELOPER_GUIDE.md")).read() +
    "\n\n## Installers not included\nWindows (NSIS/ZIP), macOS (DMG) and the AppImage are produced by `.github/workflows/build.yml`; they were not built in the release environment (progress/BLOCKERS.md B3/B4).\n")
# console integration
cp("fixture-profiles/grandma3", "console-integration/grandma3"); cp("fixture-profiles/magicq", "console-integration/magicq")
cp("fixture-profiles/gdtf", "console-integration/gdtf"); cp("fixture-profiles/definitions", "console-integration/definitions")
cp("examples/mappings_example.json", "console-integration/example_mappings.json")
for f in ["OSC_REFERENCE.md", "MIDI_REFERENCE.md", "DMX_CHANNEL_REFERENCE.md"]: cp("documentation/" + f, "console-integration/" + f)
# effects
cp("resources/effects.json", "effects/builtin_effects.json"); cp("resources/shaders", "effects/shaders")
cp("examples/node-graphs", "effects/node-graphs"); cp("examples/presets", "effects/presets"); cp("examples/projects", "effects/projects"); cp("examples/rigs", "effects/pixel-mapping-rigs")
cp("documentation/images/effects", "effects/previews")
# documentation
for f in ["LUMAFLOW_Professional_User_Manual.pdf", "LUMAFLOW_Quick_Start_Guide.pdf", "DEVELOPER_GUIDE.md", "TROUBLESHOOTING.md", "COMPATIBILITY_MATRIX.md", "RELEASE_NOTES.md"]: cp("documentation/" + f, "documentation/" + f)
cp("sdk", "documentation/plugin-sdk")
# video
for f in ["LUMAFLOW_Official_Demo.mp4", "LUMAFLOW_Official_Demo_web.mp4", "LUMAFLOW_Demo_Script.pdf", "LUMAFLOW_Demo_Subtitles.srt", "LUMAFLOW_Demo_Storyboard.pdf", "demo_script.json", "demo_segments.py", "build_video.py", "timing.json"]:
    if os.path.exists(os.path.join(ROOT, "video", f)): cp("video/" + f, "video/" + f)
open(os.path.join(R, "video/EDITABLE_PROJECT.md"), "w").write("The video is generated, not hand-edited. `demo_segments.py` (narration + timed UI actions) is the editable project: change it and run `build_video.py` on a display with LUMAFLOW built to re-record. `demo_script.json` is the resulting action script, `timing.json` the segment timing.\n")
# QA
for f in ["FEATURE_MANIFEST.json", "TEST_RESULTS.md", "BUILD_STATUS.md", "BLOCKERS.md"]: cp("progress/" + f, "quality-assurance/" + f)
for f in ["KNOWN_LIMITATIONS.md", "HARDWARE_VALIDATION_CHECKLIST.md", "benchmark_llvmpipe_1080p.json"]: cp("documentation/" + f, "quality-assurance/" + f)
# manifest + checksums
files = []
for dp, _, fn in os.walk(R):
    for f in sorted(fn):
        p = os.path.join(dp, f); rel = os.path.relpath(p, R)
        if rel in ("RELEASE_MANIFEST.json", "SHA256SUMS.txt", "RELEASE_MANIFEST.md"): continue
        h = hashlib.sha256(open(p, "rb").read()).hexdigest(); files.append({"path": rel, "bytes": os.path.getsize(p), "sha256": h})
files.sort(key=lambda x: x["path"])
status = {"applications/lumaflow_3.0.0_amd64.deb": "built, installed and run on Ubuntu 24.04", "applications/lumaflow-3.0.0-Linux.tar.gz": "built",
          "video/LUMAFLOW_Official_Demo.mp4": "master 1080p30, recorded from the real application; console control simulated (labelled); not in git (size)",
          "video/LUMAFLOW_Official_Demo_web.mp4": "web encode of the master (<100 MB)",
          "console-integration/gdtf/LUMAFLOW_MediaServer.gdtf": "structurally validated + re-import round trip; not tested on consoles"}
for f in files: f["verification"] = status.get(f["path"], "")
json.dump({"product": "LUMAFLOW", "version": "3.0.0", "date": datetime.date.today().isoformat(), "git": subprocess.run(["git", "-C", ROOT, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip(),
           "not_included": ["Windows installer (CI only)", "macOS DMG (CI only)", "AppImage (CI only)"], "files": files}, open(os.path.join(R, "RELEASE_MANIFEST.json"), "w"), indent=1)
open(os.path.join(R, "SHA256SUMS.txt"), "w").write("".join("%s  %s\n" % (f["sha256"], f["path"]) for f in files))
md = ["# LUMAFLOW 3.0.0 release manifest", "", "| File | Size | SHA-256 (first 16) | Verification |", "|---|---:|---|---|"]
for f in files:
    if f["path"].count("/") <= 2 and not f["path"].startswith(("effects/shaders", "effects/previews", "documentation/plugin-sdk")):
        md.append("| %s | %s | `%s` | %s |" % (f["path"], "{:,}".format(f["bytes"]), f["sha256"][:16], f["verification"]))
md += ["", "Not included: Windows installer, macOS DMG, AppImage – built by `.github/workflows/build.yml` (see quality-assurance/BLOCKERS.md).", "Full list with hashes: RELEASE_MANIFEST.json, SHA256SUMS.txt."]
open(os.path.join(R, "RELEASE_MANIFEST.md"), "w").write("\n".join(md))
print(len(files), "files,", sum(f["bytes"] for f in files) // 1024 // 1024, "MB")
