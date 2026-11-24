// LUMAFLOW effect: Particle Cloud (Particles)
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
}
