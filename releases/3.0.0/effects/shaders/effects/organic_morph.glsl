// LUMAFLOW effect: Organic Morph (Organic)
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
}
