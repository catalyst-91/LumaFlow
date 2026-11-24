#!/usr/bin/env python3
"""Produces video/LUMAFLOW_Official_Demo.mp4 from a live recording of the real application.
Steps: TTS narration (espeak-ng) -> timing -> demo_script.json + SRT -> record app (ffmpeg x11grab on DISPLAY) -> mux."""
import os, sys, json, subprocess, wave, time, struct, re
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from demo_segments import SEGMENTS
WORK = os.path.join(HERE, "work"); os.makedirs(WORK, exist_ok=True)
SR = 22050; PAD = 0.4; LEAD = 3.0
def tts(i, text):
    out = os.path.join(WORK, "seg%02d.wav" % i)
    subprocess.run(["espeak-ng", "-v", "en-gb+m3", "-s", "150", "-p", "40", "-g", "3", "-w", out, text], check=True)
    with wave.open(out) as w: return out, w.getnframes() / w.getframerate(), w.getframerate()
segs = []; t = LEAD
for i, s in enumerate(SEGMENTS):
    path, dur, sr = tts(i, s["text"])
    length = max(s.get("min", 0), dur + 1.4)
    segs.append(dict(s, start=t, tts=dur, wav=path, sr=sr, length=length)); t += length
total = t + 4.0
# demo script
steps = []
for s in segs:
    for off, a in s["actions"]: steps.append(dict(a, t=round(s["start"] + off, 2)))
    if not s.get("keepCaption"): steps.append({"t": round(s["start"] + 0.05, 2), "action": "caption", "text": s["text"]})
steps.append({"t": round(total - 3.5, 2), "action": "caption", "text": ""})
steps.append({"t": round(total - 3.0, 2), "action": "end", "after": 3.0})
steps.sort(key=lambda x: x["t"])
json.dump(steps, open(os.path.join(HERE, "demo_script.json"), "w"), indent=1)
# subtitles
def ts(x): h = int(x // 3600); m = int(x % 3600 // 60); sec = x % 60; return "%02d:%02d:%06.3f" % (h, m, sec)
srt = []; n = 1
for s in segs:
    words = s["text"].split(); chunks = []; cur = []
    for w in words:
        cur.append(w)
        if len(" ".join(cur)) > 80 or w.endswith((".", ",")) and len(" ".join(cur)) > 45: chunks.append(" ".join(cur)); cur = []
    if cur: chunks.append(" ".join(cur))
    total_chars = sum(len(c) for c in chunks); t0 = s["start"] + PAD
    for c in chunks:
        d = s["tts"] * len(c) / total_chars
        srt.append("%d\n%s --> %s\n%s\n" % (n, ts(t0).replace(".", ","), ts(t0 + d).replace(".", ","), c)); n += 1; t0 += d
open(os.path.join(HERE, "LUMAFLOW_Demo_Subtitles.srt"), "w").write("\n".join(srt))
# narration track
sr = segs[0]["sr"]; samples = bytearray(int(total * sr) * 2)
for s in segs:
    with wave.open(s["wav"]) as w: data = w.readframes(w.getnframes())
    o = int((s["start"] + PAD) * sr) * 2; samples[o:o + len(data)] = data[:len(samples) - o]
with wave.open(os.path.join(WORK, "narration.wav"), "wb") as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr); w.writeframes(bytes(samples))
json.dump([{k: v for k, v in s.items() if k not in ("actions",)} for s in segs], open(os.path.join(HERE, "timing.json"), "w"), indent=1)
print("planned duration %.1f s (%d:%02d), %d segments" % (total, total // 60, total % 60, len(segs)))
if "--plan-only" in sys.argv: sys.exit(0)
# record
env = dict(os.environ, QT_QPA_PLATFORM="xcb")
raw = os.path.join(WORK, "screen.mkv")
ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "x11grab", "-framerate", "30", "-video_size", "1920x1080", "-draw_mouse", "0", "-i", env.get("DISPLAY", ":99") + ".0",
                       "-c:v", "libx264", "-preset", "ultrafast", "-crf", "16", "-pix_fmt", "yuv420p", raw], stdin=subprocess.PIPE)
ff_start = time.time() + 0.15
time.sleep(1.0)
app = subprocess.Popen([os.path.join(ROOT, "build/src/lumaflow"), "--demo-script", os.path.join(HERE, "demo_script.json"), "--window-size", "1920x1080",
                        "--canvas", "1280x720", "--data-dir", "/tmp/lfgui2"], env=env, stderr=subprocess.PIPE, text=True)
demo_start = None
for line in app.stderr:
    m = re.match(r"DEMO_START (\d+)", line)
    if m and demo_start is None: demo_start = int(m.group(1)) / 1000.0
app.wait(); time.sleep(0.5)
ff.communicate(b"q"); ff.wait()
offset = max(0.0, demo_start - ff_start)
print("demo started %.2f s into the recording" % offset)
out = os.path.join(HERE, "LUMAFLOW_Official_Demo.mp4")
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-ss", "%.3f" % offset, "-i", raw, "-i", os.path.join(WORK, "narration.wav"), "-i", os.path.join(HERE, "LUMAFLOW_Demo_Subtitles.srt"),
                "-t", "%.2f" % total, "-map", "0:v", "-map", "1:a", "-map", "2:s", "-c:v", "libx264", "-preset", "medium", "-crf", "20", "-r", "30", "-pix_fmt", "yuv420p",
                "-c:a", "aac", "-b:a", "128k", "-ar", "44100", "-c:s", "mov_text", "-metadata:s:s:0", "language=eng", "-movflags", "+faststart", out], check=True)
print("wrote", out)
