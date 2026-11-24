// LUMAFLOW effect: Particle Vortex (Particles)
vec4 effect(vec2 p, vec2 uv, float t) {
    float r = length(p), a = atan(p.y, p.x);
    vec3 c = vec3(0.0);
    for (int L = 0; L < 3; L++) {
        float fl = float(L);
        float rr = log(r + 0.02) * (1.0 + uP.z * 2.0) + t * (0.6 + fl * 0.2);
        float aa = a + rr * (1.0 + uP.y * 4.0) + t * 0.3;
        vec2 q = vec2(aa / TAU * (12.0 + uP.x * 30.0), rr * (3.0 + uP.x * 6.0));
        vec2 id = floor(q), f = fract(q) - 0.5;
        float h = hash21(id + fl * 13.0);
        c += palette(h + r) * smoothstep(0.1 + uP.w * 0.3, 0.0, length(f)) * step(0.5, h) * smoothstep(0.0, 0.1, r);
    }
    c += uColorA * 0.15 / (r * 8.0 + 0.3);
    return vec4(c, 1.0);
}
