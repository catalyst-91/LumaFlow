// LUMAFLOW effect: Turbulence (Organic)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * (1.0 + uP.z * 5.0);
    float v = 0.0, a = 0.5;
    int oct = 2 + int(uP.x * 6.0);
    for (int i = 0; i < 8; i++) {
        if (i >= oct) break;
        v += a * abs(noise(q + t * 0.2 * float(i + 1)) * 2.0 - 1.0);
        q = rot2(0.6) * q * 2.0 + uP.y * v;
        a *= 0.5;
    }
    v = pow(1.0 - v, 1.0 + uP.w * 6.0);
    return vec4(palette(v) * v * 1.5, 1.0);
}
