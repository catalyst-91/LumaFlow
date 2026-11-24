// LUMAFLOW effect: Fluid (Organic)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * (1.0 + uP.z * 3.0);
    for (int i = 1; i < 6; i++) {
        float fi = float(i);
        q += uP.y * 0.6 / fi * vec2(sin(fi * q.y + t * (1.0 - uP.x * 0.8) + 0.3 * fi), cos(fi * q.x + t * 0.8 + 0.5 * fi));
    }
    float v = 0.5 + 0.5 * sin(q.x + q.y);
    v = pow(v, 1.0 + uP.w * 3.0);
    return vec4(palette(v + q.x * 0.05), 1.0);
}
