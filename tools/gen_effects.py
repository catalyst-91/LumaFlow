#!/usr/bin/env python3
"""Generates resources/shaders/effects/*.glsl and resources/effects.json.
Each effect implements: vec4 effect(vec2 p, vec2 uv, float t)
  p  : centred, aspect-corrected, transformed coordinates (approx -0.9..0.9 x -0.5..0.5)
  uv : raw 0..1 texture coordinates
  t  : time in seconds already multiplied by speed
"""
import json, os, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
E = []
def fx(id, name, cat, params, code, a=(0.1,0.5,1.0), b=(0.7,0.1,1.0), desc="", photosensitive=False):
    E.append(dict(id=id, name=name, category=cat, params=[dict(name=n, default=d) for n, d in params],
                  colorA=list(a), colorB=list(b), description=desc, photosensitive=photosensitive, code=code.strip()+"\n"))

# ---------------------------------------------------------------- Particles
PARTICLE_FIELD = """
vec3 particleLayer(vec2 p, float t, float density, float size, float layer) {
    vec3 acc = vec3(0.0);
    vec2 g = p * density;
    vec2 id = floor(g), f = fract(g) - 0.5;
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
        vec2 o = vec2(x, y);
        vec2 h = hash22(id + o + layer * 17.0);
        vec2 c = o + (h - 0.5) * 0.8 + 0.25 * vec2(sin(t * (0.5 + h.x) + h.y * TAU), cos(t * (0.4 + h.y) + h.x * TAU));
        float d = length(f - c);
        float tw = 0.6 + 0.4 * sin(t * 3.0 + h.x * 40.0);
        acc += palette(h.x + layer * 0.1) * smoothstep(size, 0.0, d) * tw * 1.8;
    }
    return acc;
}
"""
fx("particle_cloud", "Particle Cloud", "Particles", [("Count",0.5),("Size",0.4),("Drift",0.3),("Glow",0.5)], PARTICLE_FIELD + """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    float dens = 4.0 + uP.x * 20.0;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        vec2 q = p * (1.0 + fi * 0.6) + vec2(t * 0.05 * (1.0 + uP.z * 4.0) * (fi + 1.0), 0.0);
        c += particleLayer(q, t, dens, 0.05 + uP.y * 0.25, fi) * (1.0 - fi * 0.25);
    }
    c += c * c * uP.w * 2.0;
    return vec4(c, 1.0);
}""", desc="Layered drifting particle field with twinkle.")

fx("particle_explosion", "Particle Explosion", "Particles", [("Count",0.6),("Burst rate",0.4),("Spread",0.6),("Trail",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float period = 3.5 - uP.y * 2.5;
    float cyc = floor(t / period); float lt = fract(t / period);
    vec3 c = vec3(0.0);
    int n = int(40.0 + uP.x * 160.0);
    for (int i = 0; i < 200; i++) {
        if (i >= n) break;
        float fi = float(i);
        vec2 h = hash22(vec2(fi, cyc));
        float ang = h.x * TAU, spd = (0.2 + h.y * 0.8) * (0.4 + uP.z);
        vec2 pos = vec2(cos(ang), sin(ang)) * spd * (1.0 - exp(-lt * 4.0));
        pos.y -= lt * lt * 0.25;
        float d = length(p - pos);
        float trail = smoothstep(0.012 + uP.w * 0.02, 0.0, d);
        c += palette(h.x) * trail * (1.0 - lt) * 1.5;
    }
    c += uColorA * exp(-length(p) * 10.0) * pow(1.0 - lt, 6.0) * 2.0;
    return vec4(c, 1.0);
}""", a=(1.0,0.6,0.1), b=(1.0,0.1,0.3))

fx("particle_storm", "Particle Storm", "Particles", [("Density",0.6),("Wind",0.6),("Turbulence",0.5),("Streak",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    for (int L = 0; L < 4; L++) {
        float fl = float(L);
        vec2 q = p * (3.0 + fl * 2.0 + uP.x * 6.0);
        q.x += t * (1.0 + uP.y * 6.0) * (1.0 + fl * 0.3);
        q.y += fbm(q * 0.3 + t * 0.2) * uP.z * 4.0;
        vec2 id = floor(q); vec2 f = fract(q) - 0.5;
        float h = hash21(id + fl * 31.0);
        if (h > 0.6) {
            vec2 o = (hash22(id) - 0.5) * 0.6;
            vec2 d = f - o; d.x *= 1.0 / (1.0 + uP.w * 6.0);
            c += palette(h + fl * 0.2) * smoothstep(0.08, 0.0, length(d)) * (0.5 + h);
        }
    }
    float flash = step(0.97, hash11(floor(t * 4.0))) * exp(-fract(t * 4.0) * 6.0);
    c += uColorB * flash * 0.6;
    return vec4(c, 1.0);
}""", a=(0.6,0.8,1.0), b=(0.9,0.9,1.0))

fx("particle_vortex", "Particle Vortex", "Particles", [("Count",0.6),("Twist",0.5),("Suction",0.5),("Size",0.4)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float r = length(p), a = atan(p.y, p.x);
    vec3 c = vec3(0.0);
    for (int L = 0; L < 3; L++) {
        float fl = float(L);
        float rr = log(r + 0.02) * (1.0 + uP.z * 2.0) + t * (0.6 + fl * 0.2);
        float aa = a + rr * (1.0 + uP.y * 4.0) + t * 0.3;
        vec2 q = vec2(aa / TAU * (12.0 + uP.x * 30.0), rr * (3.0 + uP.x * 6.0));
        vec2 id = floor(q), f = fract(q) - 0.5;
        float h = hash21(id + fl * 13.0);
        c += palette(h + r) * smoothstep(0.1 + uP.w * 0.3, 0.0, length(f)) * step(0.5, h) * smoothstep(0.0, 0.1, r);
    }
    c += uColorA * 0.15 / (r * 8.0 + 0.3);
    return vec4(c, 1.0);
}""")

fx("particle_trails", "Particle Trails", "Particles", [("Count",0.5),("Length",0.6),("Speed var",0.5),("Wave",0.4)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = int(10.0 + uP.x * 40.0);
    for (int i = 0; i < 50; i++) {
        if (i >= n) break;
        float fi = float(i); float h = hash11(fi * 7.13);
        float y = (h - 0.5) * 1.0 + sin(t * 0.7 + fi) * uP.w * 0.2;
        float sp = 0.3 + hash11(fi * 3.1) * uP.z * 1.5;
        float x = fract(t * sp * 0.2 + h) * 2.4 - 1.2;
        float dx = x - p.x;
        float trail = smoothstep(0.0, 0.02, dx) * exp(-dx * (12.0 - uP.y * 10.0));
        float head = smoothstep(0.015, 0.0, length(vec2(dx, p.y - y)));
        c += palette(h) * (head * 2.0 + trail * smoothstep(0.012, 0.0, abs(p.y - y)));
    }
    return vec4(c, 1.0);
}""")

fx("particle_tunnel", "Particle Tunnel", "Particles", [("Density",0.5),("Depth speed",0.5),("Twist",0.3),("Size",0.4)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float r = length(p), a = atan(p.y, p.x) / TAU;
    float z = 0.3 / (r + 0.01) + t * (0.5 + uP.y * 2.0);
    vec2 q = vec2(a * (16.0 + uP.x * 32.0) + z * uP.z * 4.0, z * (2.0 + uP.x * 4.0));
    vec2 id = floor(q), f = fract(q) - 0.5;
    float h = hash21(id);
    float d = length(f);
    vec3 c = palette(h + z * 0.05) * smoothstep(0.15 + uP.w * 0.3, 0.0, d) * step(0.55, h);
    c *= smoothstep(0.0, 0.25, r) * (1.0 + 1.0 / (r * 6.0 + 0.5));
    return vec4(c, 1.0);
}""", a=(0.2,0.6,1.0), b=(0.8,0.2,1.0))

fx("particle_attract", "Attract / Repel", "Particles", [("Count",0.6),("Force",0.5),("Repel",0.0),("Orbit",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 att = vec2(sin(t * 0.6), cos(t * 0.45)) * 0.35;
    vec3 c = vec3(0.0);
    int n = int(30.0 + uP.x * 150.0);
    for (int i = 0; i < 180; i++) {
        if (i >= n) break;
        float fi = float(i);
        vec2 h = hash22(vec2(fi, 3.7));
        vec2 home = (h - 0.5) * vec2(1.8, 1.0);
        vec2 toA = att - home;
        float k = (uP.z > 0.5 ? -1.0 : 1.0) * uP.y * 0.8;
        vec2 pos = home + toA * k * (0.5 + 0.5 * sin(t * (0.5 + h.x)));
        pos += vec2(-toA.y, toA.x) * uP.w * 0.3 * sin(t + fi);
        c += palette(h.y) * smoothstep(0.012, 0.0, length(p - pos)) * 1.5;
    }
    c += uColorA * smoothstep(0.05, 0.0, length(p - att)) * 0.5;
    return vec4(c, 1.0);
}""")

fx("audio_particles", "Audio Particles", "Particles", [("Count",0.6),("Bass kick",0.7),("Treble sparkle",0.6),("Size",0.4)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    float kick = uBeat * uP.y + uAudio.x * 0.5;
    vec2 g = p * (6.0 + uP.x * 14.0);
    vec2 id = floor(g), f = fract(g) - 0.5;
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
        vec2 o = vec2(x, y); vec2 h = hash22(id + o);
        vec2 cpos = o + (h - 0.5) * 0.7 * (1.0 + kick);
        float d = length(f - cpos);
        float s = 0.05 + uP.w * 0.2 + kick * 0.15;
        float spark = step(1.0 - uAudio.z * uP.z, h.y);
        c += palette(h.x + t * 0.05) * smoothstep(s, 0.0, d) * (0.6 + spark * 1.5);
    }
    c *= 0.5 + uAudio.w * 2.0 + uBeat;
    return vec4(c, 1.0);
}""", desc="Particles that expand on beats and sparkle with treble energy.")

# ---------------------------------------------------------------- Energy
fx("plasma", "Plasma", "Energy", [("Complexity",0.5),("Warp",0.4),("Contrast",0.5),("Colour cycle",0.3)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float k = 2.0 + uP.x * 8.0;
    vec2 q = p + uP.y * vec2(fbm(p * 2.0 + t * 0.2), fbm(p * 2.0 - t * 0.2));
    float v = sin(q.x * k + t) + sin(q.y * k * 0.8 - t * 1.3) + sin((q.x + q.y) * k * 0.6 + t * 0.7) + sin(length(q) * k * 1.4 - t * 1.1);
    v = v * 0.25;
    v = 0.5 + 0.5 * sign(v) * pow(abs(v), 1.0 - uP.z * 0.8);
    vec3 c = palette(v + t * uP.w * 0.2);
    return vec4(c, 1.0);
}""", a=(1.0,0.1,0.6), b=(0.1,0.4,1.0))

fx("lightning", "Lightning", "Energy", [("Bolts",0.4),("Branching",0.5),("Rate",0.5),("Glow",0.5)], """
float boltDist(vec2 p, float seed, float t) {
    float y = p.y;
    float x = 0.0; float amp = 0.25;
    for (int i = 0; i < 6; i++) { float fi = float(i); x += (noise(vec2(y * (3.0 + fi * 3.0), seed + fi * 10.0 + floor(t * 12.0))) - 0.5) * amp; amp *= 0.55; }
    return abs(p.x - x);
}
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = 1 + int(uP.x * 5.0);
    for (int i = 0; i < 6; i++) {
        if (i >= n) break;
        float fi = float(i);
        float ph = floor(t * (0.5 + uP.z * 3.0) + fi * 0.37);
        float on = step(0.4, hash11(ph + fi * 9.1));
        float flick = exp(-fract(t * (0.5 + uP.z * 3.0) + fi * 0.37) * 5.0);
        vec2 q = p - vec2((hash11(ph * 3.3 + fi) - 0.5) * 1.4, 0.0);
        float d = boltDist(q, ph + fi * 3.0, t);
        float br = boltDist(q * vec2(1.0, 1.0) + vec2(0.1, 0.0), ph + 77.0, t) + (1.0 - uP.y) * 0.2 + abs(q.y - 0.1) * 0.3;
        float core = 0.004 / (d + 0.002) + 0.002 / (br + 0.003) * uP.y;
        c += mix(uColorA, vec3(1.0), 0.5) * core * on * flick * (0.3 + uP.w);
    }
    return vec4(c, 1.0);
}""", a=(0.5,0.6,1.0), b=(0.9,0.9,1.0), photosensitive=True)

fx("electric_arcs", "Electric Arcs", "Energy", [("Arcs",0.5),("Jitter",0.6),("Thickness",0.4),("Hum",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = 2 + int(uP.x * 8.0);
    for (int i = 0; i < 10; i++) {
        if (i >= n) break;
        float fi = float(i);
        float ang = fi / float(n) * TAU + t * 0.2;
        vec2 a = vec2(cos(ang), sin(ang)) * 0.45;
        vec2 ba = -a; vec2 pa = p - a;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        vec2 perp = normalize(vec2(-ba.y, ba.x));
        float off = (noise(vec2(h * 12.0, t * 15.0 * uP.y + fi * 7.0)) - 0.5) * 0.25 * sin(h * PI);
        float d = length(pa - ba * h - perp * off);
        c += palette(fi * 0.1) * (0.002 + uP.z * 0.004) / (d + 0.001) * (0.7 + 0.3 * sin(t * 60.0 * uP.w + fi));
    }
    c += uColorA * 0.04 / (length(p) + 0.05);
    return vec4(c, 1.0);
}""", a=(0.3,0.7,1.0), b=(0.7,0.3,1.0))

fx("energy_rings", "Energy Rings", "Energy", [("Rings",0.5),("Width",0.3),("Spacing",0.5),("Audio",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float r = length(p);
    float k = 4.0 + uP.x * 20.0;
    float wave = fract(r * k * (0.5 + uP.z) - t * 0.8);
    float ring = smoothstep(0.02 + uP.y * 0.3, 0.0, abs(wave - 0.5) - 0.02) ;
    ring *= 1.0 + uBeat * uP.w * 2.0;
    vec3 c = palette(r * 2.0 - t * 0.1) * ring * exp(-r * 1.5);
    return vec4(c, 1.0);
}""", a=(0.0,0.9,1.0), b=(0.6,0.0,1.0))

fx("pulse_field", "Pulsating Field", "Energy", [("Cells",0.5),("Pulse",0.6),("Sharpness",0.5),("Audio",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 g = p * (4.0 + uP.x * 16.0);
    vec2 id = floor(g), f = fract(g) - 0.5;
    float h = hash21(id);
    float pulse = 0.5 + 0.5 * sin(t * (1.0 + uP.y * 4.0) + h * TAU + length(id) * 0.5);
    pulse = pow(pulse, 1.0 + uP.z * 8.0) + uAudio.x * uP.w;
    float d = length(f);
    vec3 c = palette(h + t * 0.05) * smoothstep(0.5, 0.0, d) * pulse;
    return vec4(c, 1.0);
}""")

fx("neon_lines", "Neon Lines", "Energy", [("Lines",0.5),("Wobble",0.4),("Glow",0.6),("Scroll",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = 3 + int(uP.x * 12.0);
    for (int i = 0; i < 15; i++) {
        if (i >= n) break;
        float fi = float(i);
        float y = (fi / float(n) - 0.5) * 1.0 + sin(p.x * (2.0 + fi) + t * (1.0 + fi * 0.1)) * uP.y * 0.15;
        y += fract(t * uP.w * 0.1) * 0.0;
        float d = abs(p.y - y);
        c += palette(fi / float(n) + t * 0.05) * (0.001 + uP.z * 0.004) / (d + 0.002);
    }
    return vec4(c, 1.0);
}""", a=(1.0,0.1,0.8), b=(0.1,0.9,1.0))

# ---------------------------------------------------------------- Geometry
fx("kaleidoscope", "Kaleidoscope", "Geometry", [("Segments",0.4),("Zoom",0.5),("Detail",0.5),("Spin",0.3)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float seg = floor(3.0 + uP.x * 13.0);
    float r = length(p), a = atan(p.y, p.x) + t * uP.w;
    float s = TAU / seg;
    a = mod(a, s); a = abs(a - s * 0.5);
    vec2 q = vec2(cos(a), sin(a)) * r * (1.0 + uP.y * 4.0);
    q += vec2(t * 0.1, t * 0.07);
    float v = fbm(q * (2.0 + uP.z * 6.0)) ;
    float lines = smoothstep(0.02, 0.0, abs(fract(v * 6.0) - 0.5) - 0.4);
    vec3 c = palette(v * 2.0 + t * 0.05) * (0.5 + v) + lines * 0.3;
    return vec4(c * smoothstep(1.2, 0.2, r), 1.0);
}""", a=(1.0,0.3,0.1), b=(0.1,0.3,1.0))

fx("fractal", "Julia Fractal", "Geometry", [("Iterations",0.6),("Morph",0.5),("Zoom",0.4),("Colour bands",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 z = p * (2.5 - uP.z * 2.0);
    vec2 cc = vec2(-0.8 + 0.2 * sin(t * 0.3 * (0.5 + uP.y)), 0.156 + 0.2 * cos(t * 0.23 * (0.5 + uP.y)));
    int maxIt = 32 + int(uP.x * 96.0);
    float it = 0.0;
    for (int i = 0; i < 128; i++) {
        if (i >= maxIt) break;
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + cc;
        if (dot(z, z) > 16.0) break;
        it += 1.0;
    }
    float sm = it - log2(max(log2(dot(z, z)), 1e-6)) + 4.0;
    float v = sm / float(maxIt);
    vec3 c = it >= float(maxIt) ? vec3(0.0) : palette(v * (1.0 + uP.w * 8.0) + t * 0.05) * sqrt(v * 2.0);
    return vec4(c, 1.0);
}""", a=(0.1,0.2,1.0), b=(1.0,0.5,0.0))

fx("mandala", "Mandala", "Geometry", [("Petals",0.5),("Layers",0.5),("Line width",0.3),("Breathe",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float r = length(p), a = atan(p.y, p.x);
    float petals = floor(4.0 + uP.x * 12.0);
    vec3 c = vec3(0.0);
    int layers = 2 + int(uP.y * 6.0);
    for (int i = 0; i < 8; i++) {
        if (i >= layers) break;
        float fi = float(i);
        float rr = 0.08 + fi * 0.07 * (1.0 + 0.1 * sin(t * uP.w * 2.0 + fi));
        float shape = rr + 0.04 * cos(a * petals * (fi + 1.0) * 0.5 + t * (fi + 1.0) * 0.2 * (mod(fi, 2.0) * 2.0 - 1.0));
        float d = abs(r - shape);
        c += palette(fi * 0.15 + t * 0.03) * smoothstep(0.004 + uP.z * 0.02, 0.0, d);
    }
    c += uColorA * 0.3 * smoothstep(0.05, 0.0, r);
    return vec4(c, 1.0);
}""", a=(1.0,0.7,0.2), b=(0.9,0.1,0.5))

fx("geo_tunnel", "Geometric Tunnel", "Geometry", [("Sides",0.3),("Speed depth",0.5),("Twist",0.4),("Line width",0.3)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float sides = floor(3.0 + uP.x * 6.0);
    float a = atan(p.y, p.x) + t * uP.z * 0.5;
    float s = TAU / sides;
    float r = length(p) * cos(mod(a, s) - s * 0.5);       // polygon distance
    float z = 0.2 / max(r, 0.001) + t * (0.5 + uP.y * 3.0);
    float lines = smoothstep(0.05 + uP.w * 0.2, 0.0, abs(fract(z) - 0.5) - 0.4);
    vec3 c = palette(floor(z) * 0.13) * lines * smoothstep(0.0, 0.3, r) * (1.0 + 0.5 / (r * 4.0 + 0.2));
    return vec4(c, 1.0);
}""", a=(0.0,1.0,0.7), b=(0.6,0.0,1.0))

fx("rotating_pattern", "Rotating Pattern", "Geometry", [("Density",0.5),("Counter-spin",0.5),("Shape",0.3),("Line width",0.4)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        vec2 q = rot2(t * (0.3 + fi * 0.2) * (mod(fi, 2.0) < 0.5 ? 1.0 : -uP.y * 2.0)) * p;
        q *= 3.0 + uP.x * 10.0 + fi * 2.0;
        vec2 f = fract(q) - 0.5;
        float d = mix(length(f), max(abs(f.x), abs(f.y)), uP.z);
        c += palette(fi * 0.3 + t * 0.05) * smoothstep(0.03 + uP.w * 0.1, 0.0, abs(d - 0.3));
    }
    return vec4(c * 0.8, 1.0);
}""")

fx("morph_geometry", "Morphing Geometry", "Geometry", [("Vertices",0.4),("Morph speed",0.5),("Fill",0.3),("Echoes",0.5)], """
float sdPoly(vec2 p, float n, float r) { float a = atan(p.y, p.x); float s = TAU / n; return length(p) * cos(mod(a, s) - s * 0.5) - r; }
vec4 effect(vec2 p, vec2 uv, float t) {
    float n1 = floor(3.0 + uP.x * 5.0), n2 = n1 + 2.0;
    float m = 0.5 + 0.5 * sin(t * (0.3 + uP.y * 2.0));
    vec3 c = vec3(0.0);
    int echoes = 1 + int(uP.w * 6.0);
    for (int i = 0; i < 7; i++) {
        if (i >= echoes) break;
        float fi = float(i);
        vec2 q = rot2(t * 0.2 + fi * 0.15) * p;
        float r = 0.35 - fi * 0.045;
        float d = mix(sdPoly(q, n1, r), length(q) - r, m) ;
        d = mix(d, sdPoly(q, n2, r), 0.5 - 0.5 * cos(t * 0.17));
        c += palette(fi * 0.12 + t * 0.04) * (smoothstep(0.006, 0.0, abs(d)) + uP.z * smoothstep(0.0, -0.02, d) * 0.15);
    }
    return vec4(c, 1.0);
}""", a=(0.2,1.0,0.6), b=(1.0,0.2,0.6))

fx("wireframe", "Wireframe Terrain", "Geometry", [("Grid density",0.5),("Height",0.5),("Scroll",0.5),("Horizon",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float hor = 0.05 + uP.w * 0.2;
    vec3 c = vec3(0.0);
    if (p.y < hor) {
        float z = (0.4) / (hor - p.y + 0.001);
        vec2 g = vec2(p.x * z, z + t * (0.5 + uP.z * 3.0)) * (2.0 + uP.x * 6.0);
        float h = fbm(g * 0.15) * uP.y * 3.0;
        g.y += h;
        vec2 f = abs(fract(g) - 0.5);
        float line = smoothstep(0.06 * z * 0.3, 0.0, min(f.x, f.y) - 0.0);
        c = palette(h * 0.3 + 0.2) * line * smoothstep(40.0, 2.0, z);
    } else {
        c = mix(uColorB * 0.3, vec3(0.0), clamp((p.y - hor) * 3.0, 0.0, 1.0));
        c += uColorA * smoothstep(0.25, 0.0, length(p - vec2(0.0, hor + 0.12))) * 0.6;
    }
    return vec4(c, 1.0);
}""", a=(1.0,0.2,0.8), b=(0.2,0.4,1.0))

# ---------------------------------------------------------------- Organic
fx("smoke", "Smoke", "Organic", [("Density",0.5),("Rise",0.5),("Detail",0.5),("Softness",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * (1.5 + uP.z * 3.0);
    q.y -= t * (0.1 + uP.y * 0.5);
    float n = fbm(q + fbm(q + t * 0.1) * 1.5);
    float d = smoothstep(0.3 - uP.x * 0.2, 0.9 - uP.w * 0.3, n);
    d *= smoothstep(0.6, -0.4, p.y + 0.0) * 1.2;
    vec3 c = mix(uColorB * 0.2, uColorA, d) * d;
    return vec4(c, 1.0);
}""", a=(0.8,0.85,0.95), b=(0.3,0.3,0.5))

fx("fluid", "Fluid", "Organic", [("Viscosity",0.5),("Swirl",0.6),("Scale",0.5),("Contrast",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * (1.0 + uP.z * 3.0);
    for (int i = 1; i < 6; i++) {
        float fi = float(i);
        q += uP.y * 0.6 / fi * vec2(sin(fi * q.y + t * (1.0 - uP.x * 0.8) + 0.3 * fi), cos(fi * q.x + t * 0.8 + 0.5 * fi));
    }
    float v = 0.5 + 0.5 * sin(q.x + q.y);
    v = pow(v, 1.0 + uP.w * 3.0);
    return vec4(palette(v + q.x * 0.05), 1.0);
}""", a=(0.0,0.4,1.0), b=(0.0,1.0,0.8))

fx("ink", "Ink Clouds", "Organic", [("Spread",0.5),("Billow",0.5),("Darkness",0.5),("Flow",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * 2.0;
    vec2 w = vec2(fbm(q + t * 0.1 * uP.w), fbm(q + vec2(5.2, 1.3) - t * 0.08));
    float n = fbm(q + w * (2.0 + uP.y * 4.0));
    float r = length(p);
    float mask = smoothstep(0.3 + uP.x * 0.8, 0.0, r - n * 0.4);
    vec3 c = mix(uColorA, uColorB, n) * mask * (1.0 - uP.z * 0.5 * n);
    return vec4(c, 1.0);
}""", a=(0.5,0.0,1.0), b=(0.0,0.2,0.6))

fx("flow_field", "Flow Field", "Organic", [("Lines",0.6),("Curl",0.5),("Length",0.5),("Speed var",0.4)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float ang = fbm(p * (1.0 + uP.y * 3.0) + t * 0.05) * TAU * 2.0;
    vec2 dir = vec2(cos(ang), sin(ang));
    float s = dot(p, vec2(-dir.y, dir.x)) * (30.0 + uP.x * 80.0);
    float along = dot(p, dir) * (2.0 + uP.z * 4.0) - t * (1.0 + uP.w * hash11(floor(s)));
    float line = smoothstep(0.35, 0.0, abs(fract(s) - 0.5)) * smoothstep(0.0, 1.0, fract(along));
    vec3 c = palette(ang / TAU) * line;
    return vec4(c, 1.0);
}""")

fx("turbulence", "Turbulence", "Organic", [("Octaves",0.6),("Distortion",0.5),("Scale",0.5),("Sharpness",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * (1.0 + uP.z * 5.0);
    float v = 0.0, a = 0.5;
    int oct = 2 + int(uP.x * 6.0);
    for (int i = 0; i < 8; i++) {
        if (i >= oct) break;
        v += a * abs(noise(q + t * 0.2 * float(i + 1)) * 2.0 - 1.0);
        q = rot2(0.6) * q * 2.0 + uP.y * v;
        a *= 0.5;
    }
    v = pow(1.0 - v, 1.0 + uP.w * 6.0);
    return vec4(palette(v) * v * 1.5, 1.0);
}""", a=(1.0,0.4,0.0), b=(0.2,0.0,0.6))

fx("organic_morph", "Organic Morph", "Organic", [("Blobs",0.5),("Gooeyness",0.5),("Speed var",0.5),("Rim",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float f = 0.0;
    int n = 3 + int(uP.x * 9.0);
    for (int i = 0; i < 12; i++) {
        if (i >= n) break;
        float fi = float(i);
        vec2 c = vec2(sin(t * (0.3 + hash11(fi) * uP.z) + fi * 1.7), cos(t * (0.25 + hash11(fi + 3.0) * uP.z) + fi * 2.3)) * 0.4;
        f += (0.01 + uP.y * 0.02) / dot(p - c, p - c);
    }
    float edge = smoothstep(0.9, 1.0, f) - smoothstep(1.0, 1.0 + uP.w * 2.0 + 0.1, f) * 0.5;
    vec3 col = palette(f * 0.1 + t * 0.03) * smoothstep(0.8, 1.2, f) + edge * 0.4;
    return vec4(col, 1.0);
}""", a=(0.0,1.0,0.5), b=(0.0,0.4,1.0))

# ---------------------------------------------------------------- Additional
fx("glitch", "Glitch", "Additional", [("Intensity",0.5),("Block size",0.5),("RGB split",0.5),("Rate",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float rate = floor(t * (2.0 + uP.w * 20.0));
    vec2 bs = vec2(8.0, 24.0) * (1.0 + uP.y * 4.0);
    vec2 blk = floor(uv * bs);
    float g = step(1.0 - uP.x * 0.6, hash21(blk + rate));
    vec2 q = uv + vec2((hash21(blk.yy + rate) - 0.5) * 0.2 * g, 0.0);
    float split = uP.z * 0.02 * (1.0 + g * 4.0);
    vec3 base;
    base.r = 0.5 + 0.5 * sin((q.x + split) * 20.0 + t + sin(q.y * 10.0));
    base.g = 0.5 + 0.5 * sin(q.x * 20.0 + t + sin(q.y * 10.0) + 2.0);
    base.b = 0.5 + 0.5 * sin((q.x - split) * 20.0 + t + sin(q.y * 10.0) + 4.0);
    vec3 c = mix(uColorA * base, uColorB, g * hash21(blk * 1.3 + rate) * 0.6);
    c *= 0.85 + 0.15 * sin(uv.y * uResolution.y * 1.5);
    return vec4(c, 1.0);
}""", a=(0.2,0.9,1.0), b=(1.0,0.0,0.5), photosensitive=True)

fx("strobe", "Strobe", "Additional", [("Rate",0.3),("Duty",0.2),("Random",0.0),("Beat sync",0.0)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    // Rate is limited by the global photosensitivity safety limiter (Output > Safety).
    float hz = 1.0 + uP.x * 11.0;
    float ph = fract(t * hz);
    float on = step(ph, max(0.05, uP.y));
    if (uP.z > 0.0) on *= step(1.0 - uP.z, hash11(floor(t * hz)));
    if (uP.w > 0.5) on = step(0.6, uBeat);
    return vec4(uColorA * on, 1.0);
}""", a=(1.0,1.0,1.0), b=(0.0,0.0,0.0), photosensitive=True, desc="Full-frame strobe. Rate is capped by the photosensitive safety limiter.")

fx("gradient", "Gradient", "Additional", [("Angle",0.0),("Scroll",0.3),("Bands",0.0),("Radial",0.0)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 d = vec2(cos(uP.x * TAU), sin(uP.x * TAU));
    float v = mix(dot(p, d) + 0.5, length(p) * 1.4, uP.w);
    v += t * uP.y * 0.2;
    if (uP.z > 0.0) v = floor(v * (2.0 + uP.z * 14.0)) / (2.0 + uP.z * 14.0);
    float k = 0.5 + 0.5 * sin(v * TAU);
    return vec4(mix(uColorA, uColorB, k), 1.0);
}""", a=(0.0,0.3,1.0), b=(0.8,0.0,1.0))

fx("text", "Text", "Additional", [("Scroll",0.0),("Outline glow",0.4),("Wave",0.0),("Size",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float aspect = uResolution.x / max(uResolution.y, 1.0);
    float tw = aspect * (0.4 + uP.w * 0.55);          // text width in canvas units
    float th = tw / max(uTexAspect, 0.01);
    vec2 q = p;
    q.x += t * uP.x * 0.3;
    if (uP.x > 0.0) q.x = mod(q.x + tw * 0.5 + aspect * 0.5, tw + aspect) - tw * 0.5 - aspect * 0.5;
    q.y += sin(q.x * 6.0 + t * 2.0) * uP.z * 0.05;
    vec2 tuv = vec2(q.x / tw + 0.5, 0.5 - q.y / th);
    float inside = step(0.0, tuv.x) * step(tuv.x, 1.0) * step(0.0, tuv.y) * step(tuv.y, 1.0);
    float a = texture(uTex, tuv).a * inside;
    float glow = 0.0;
    for (int i = 0; i < 8; i++) { float ang = float(i) / 8.0 * TAU; vec2 o = tuv + vec2(cos(ang), sin(ang) * uTexAspect) * 0.012; glow += texture(uTex, o).a * step(0.0, o.x) * step(o.x, 1.0) * step(0.0, o.y) * step(o.y, 1.0); }
    glow /= 8.0;
    vec3 c = uColorA * a + uColorB * max(glow - a, 0.0) * uP.y * 2.0;
    return vec4(c, max(a, glow * uP.y));
}""", a=(1.0,1.0,1.0), b=(0.2,0.5,1.0), desc="Renders the layer's text string (set in the Parameter Editor).")

fx("logo", "Logo / Image", "Additional", [("Pulse",0.0),("Spin",0.0),("Glow",0.3),("Tint",0.0)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = rot2(t * uP.y) * p;
    q /= 1.0 + uP.x * 0.1 * sin(t * 4.0) + uBeat * uP.x * 0.2;
    vec2 tuv = vec2(q.x / (0.8 * uTexAspect) + 0.5, 0.5 - q.y / 0.8);
    float inside = step(0.0, tuv.x) * step(tuv.x, 1.0) * step(0.0, tuv.y) * step(tuv.y, 1.0);
    vec4 tx = texture(uTex, tuv) * inside;
    vec3 col = mix(tx.rgb, uColorA * dot(tx.rgb, vec3(0.333)), uP.w);
    col += uColorB * uP.z * 0.3 * smoothstep(0.6, 0.0, length(q));
    return vec4(col, max(tx.a, uP.z * 0.3 * smoothstep(0.6, 0.0, length(q))));
}""", a=(1.0,1.0,1.0), b=(0.3,0.5,1.0))

fx("spectrum", "Audio Spectrum", "Additional", [("Bars",0.5),("Gap",0.3),("Mirror",1.0),("Peak glow",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float bars = 8.0 + floor(uP.x * 24.0);
    float x = uv.x;
    if (uP.z > 0.5) x = abs(uv.x - 0.5) * 2.0;
    float b = floor(x * bars) / bars;
    float v = spectrumAt(b);
    float f = fract(x * bars);
    float bar = step(uP.y * 0.5, f) * step(f, 1.0 - uP.y * 0.5);
    float y = uP.z > 0.5 ? abs(uv.y - 0.5) * 2.0 : 1.0 - uv.y;
    float fill = step(y, v) * bar;
    vec3 c = mix(uColorA, uColorB, y) * fill + uColorB * smoothstep(0.03, 0.0, abs(y - v)) * bar * uP.w;
    return vec4(c, 1.0);
}""", a=(0.0,1.0,0.6), b=(1.0,0.1,0.5), desc="Live FFT spectrum bars from the audio input.")

fx("waveform", "Waveform", "Additional", [("Lines",0.3),("Amplitude",0.5),("Thickness",0.3),("Audio drive",0.8)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = 1 + int(uP.x * 6.0);
    for (int i = 0; i < 7; i++) {
        if (i >= n) break;
        float fi = float(i);
        float amp = (0.05 + uP.y * 0.3) * (0.3 + uP.w * (uAudio.w * 3.0 + spectrumAt(fract(uv.x * 0.5 + fi * 0.1))));
        float y = sin(p.x * (6.0 + fi * 3.0) + t * (2.0 + fi)) * amp + sin(p.x * 17.0 - t * 3.0) * amp * 0.3;
        y += (fi - float(n - 1) * 0.5) * 0.08;
        c += palette(fi * 0.2) * (0.002 + uP.z * 0.006) / (abs(p.y - y) + 0.002);
    }
    return vec4(c, 1.0);
}""", a=(0.2,1.0,1.0), b=(1.0,0.2,1.0))

fx("feedback", "Feedback Tunnel", "Additional", [("Decay",0.8),("Zoom",0.5),("Rotate",0.5),("Seed",0.5)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 c = uv - 0.5;
    c = rot2((uP.z - 0.5) * 0.1) * c * (0.96 + (0.5 - uP.y) * 0.06);
    vec3 prev = texture(uPrev, c + 0.5).rgb * (0.80 + uP.x * 0.19);
    float r = length(p - vec2(sin(t), cos(t * 1.3)) * 0.3);
    vec3 seed = palette(t * 0.1) * smoothstep(0.03 + uP.w * 0.05, 0.0, r);
    return vec4(max(prev, seed), 1.0);
}""", desc="Feeds the previous frame back with zoom/rotation for trails.")

fx("solid", "Solid Colour", "Additional", [("Mix B",0.0),("Pulse",0.0),("-",0.0),("-",0.0)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = mix(uColorA, uColorB, uP.x);
    c *= 1.0 - uP.y * (0.5 + 0.5 * sin(t * TAU));
    return vec4(c, 1.0);
}""", a=(1.0,1.0,1.0), b=(0.0,0.0,1.0))

fx("test_pattern", "Test Pattern", "Additional", [("Mode",0.0),("Grid",0.5),("-",0.0),("-",0.0)], """
vec4 effect(vec2 p, vec2 uv, float t) {
    float mode = floor(uP.x * 3.99);
    vec3 c;
    if (mode < 1.0) {               // colour bars + grid + circle
        float bar = floor(uv.x * 8.0);
        vec3 bars[8] = vec3[8](vec3(1), vec3(1,1,0), vec3(0,1,1), vec3(0,1,0), vec3(1,0,1), vec3(1,0,0), vec3(0,0,1), vec3(0));
        c = bars[int(bar)] * 0.75;
        vec2 g = abs(fract(uv * uResolution / (40.0 + uP.y * 80.0)) - 0.5);
        c = mix(c, vec3(1.0), smoothstep(0.48, 0.5, max(g.x, g.y)));
        float r = length((uv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0));
        c = mix(c, vec3(1.0), smoothstep(0.006, 0.0, abs(r - 0.45)));
    } else if (mode < 2.0) { c = vec3(uv.x); }           // grey ramp
    else if (mode < 3.0) { c = vec3(1.0); }              // full white
    else { c = vec3(fract(t * 0.5) < 0.33 ? vec3(1,0,0) : fract(t * 0.5) < 0.66 ? vec3(0,1,0) : vec3(0,0,1)); }
    vec2 e = min(uv, 1.0 - uv) * uResolution;
    c = mix(c, vec3(1.0, 0.0, 0.0), step(min(e.x, e.y), 2.0));       // 2px red edge marks canvas boundary
    return vec4(c, 1.0);
}""", desc="Colour bars, grey ramp, white field or RGB cycle, with canvas edge markers.")

out_dir = os.path.join(root, "resources", "shaders", "effects")
os.makedirs(out_dir, exist_ok=True)
manifest = []
for e in E:
    with open(os.path.join(out_dir, e["id"] + ".glsl"), "w") as f:
        f.write("// LUMAFLOW effect: %s (%s)\n" % (e["name"], e["category"]))
        f.write(e["code"])
    m = {k: v for k, v in e.items() if k != "code"}
    m["shader"] = "effects/%s.glsl" % e["id"]
    manifest.append(m)
with open(os.path.join(root, "resources", "effects.json"), "w") as f:
    json.dump({"version": 1, "effects": manifest}, f, indent=1)
# qrc
files = ["shaders/prelude.glsl", "shaders/main.glsl", "effects.json"] + ["shaders/" + m["shader"] for m in manifest]
extra = ["icons/lumaflow.png", "icons/lumaflow.svg", "themes/dark.qss", "remote/index.html", "presets/factory.lfpreset", "fonts/README"]
with open(os.path.join(root, "resources", "resources.qrc"), "w") as f:
    f.write('<RCC>\n  <qresource prefix="/lumaflow">\n')
    for x in files + extra:
        if os.path.exists(os.path.join(root, "resources", x)):
            f.write("    <file>%s</file>\n" % x)
    f.write("  </qresource>\n</RCC>\n")
print("generated %d effects" % len(E))
