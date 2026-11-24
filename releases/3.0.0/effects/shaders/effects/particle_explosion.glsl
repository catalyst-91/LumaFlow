// LUMAFLOW effect: Particle Explosion (Particles)
vec4 effect(vec2 p, vec2 uv, float t) {
    float period = 3.5 - uP.y * 2.5;
    float cyc = floor(t / period); float lt = fract(t / period);
    vec3 c = vec3(0.0);
    int n = int(40.0 + uP.x * 160.0);
    for (int i = 0; i < 200; i++) {
        if (i >= n) break;
        float fi = float(i);
        vec2 h = hash22(vec2(fi, cyc));
        float ang = h.x * TAU, spd = (0.2 + h.y * 0.8) * (0.4 + uP.z);
        vec2 pos = vec2(cos(ang), sin(ang)) * spd * (1.0 - exp(-lt * 4.0));
        pos.y -= lt * lt * 0.25;
        float d = length(p - pos);
        float trail = smoothstep(0.012 + uP.w * 0.02, 0.0, d);
        c += palette(h.x) * trail * (1.0 - lt) * 1.5;
    }
    c += uColorA * exp(-length(p) * 10.0) * pow(1.0 - lt, 6.0) * 2.0;
    return vec4(c, 1.0);
}
