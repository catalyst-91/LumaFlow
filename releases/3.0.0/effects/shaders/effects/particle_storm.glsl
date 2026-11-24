// LUMAFLOW effect: Particle Storm (Particles)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    for (int L = 0; L < 4; L++) {
        float fl = float(L);
        vec2 q = p * (3.0 + fl * 2.0 + uP.x * 6.0);
        q.x += t * (1.0 + uP.y * 6.0) * (1.0 + fl * 0.3);
        q.y += fbm(q * 0.3 + t * 0.2) * uP.z * 4.0;
        vec2 id = floor(q); vec2 f = fract(q) - 0.5;
        float h = hash21(id + fl * 31.0);
        if (h > 0.6) {
            vec2 o = (hash22(id) - 0.5) * 0.6;
            vec2 d = f - o; d.x *= 1.0 / (1.0 + uP.w * 6.0);
            c += palette(h + fl * 0.2) * smoothstep(0.08, 0.0, length(d)) * (0.5 + h);
        }
    }
    float flash = step(0.97, hash11(floor(t * 4.0))) * exp(-fract(t * 4.0) * 6.0);
    c += uColorB * flash * 0.6;
    return vec4(c, 1.0);
}
