// LUMAFLOW effect: Electric Arcs (Energy)
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
}
