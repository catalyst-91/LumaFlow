// LUMAFLOW effect: Neon Lines (Energy)
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
}
