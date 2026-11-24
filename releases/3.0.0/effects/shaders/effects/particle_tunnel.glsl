// LUMAFLOW effect: Particle Tunnel (Particles)
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
}
