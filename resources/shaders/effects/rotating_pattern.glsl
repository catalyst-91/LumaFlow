// LUMAFLOW effect: Rotating Pattern (Geometry)
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
}
