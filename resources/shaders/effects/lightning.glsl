// LUMAFLOW effect: Lightning (Energy)
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
}
