// LUMAFLOW effect: Attract / Repel (Particles)
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
}
