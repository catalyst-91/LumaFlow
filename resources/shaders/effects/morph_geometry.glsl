// LUMAFLOW effect: Morphing Geometry (Geometry)
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
}
