// LUMAFLOW effect: Particle Trails (Particles)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = int(10.0 + uP.x * 40.0);
    for (int i = 0; i < 50; i++) {
        if (i >= n) break;
        float fi = float(i); float h = hash11(fi * 7.13);
        float y = (h - 0.5) * 1.0 + sin(t * 0.7 + fi) * uP.w * 0.2;
        float sp = 0.3 + hash11(fi * 3.1) * uP.z * 1.5;
        float x = fract(t * sp * 0.2 + h) * 2.4 - 1.2;
        float dx = x - p.x;
        float trail = smoothstep(0.0, 0.02, dx) * exp(-dx * (12.0 - uP.y * 10.0));
        float head = smoothstep(0.015, 0.0, length(vec2(dx, p.y - y)));
        c += palette(h) * (head * 2.0 + trail * smoothstep(0.012, 0.0, abs(p.y - y)));
    }
    return vec4(c, 1.0);
}
