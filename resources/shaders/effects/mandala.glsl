// LUMAFLOW effect: Mandala (Geometry)
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
}
