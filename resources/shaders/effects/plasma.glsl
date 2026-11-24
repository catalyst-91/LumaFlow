// LUMAFLOW effect: Plasma (Energy)
vec4 effect(vec2 p, vec2 uv, float t) {
    float k = 2.0 + uP.x * 8.0;
    vec2 q = p + uP.y * vec2(fbm(p * 2.0 + t * 0.2), fbm(p * 2.0 - t * 0.2));
    float v = sin(q.x * k + t) + sin(q.y * k * 0.8 - t * 1.3) + sin((q.x + q.y) * k * 0.6 + t * 0.7) + sin(length(q) * k * 1.4 - t * 1.1);
    v = v * 0.25;
    v = 0.5 + 0.5 * sign(v) * pow(abs(v), 1.0 - uP.z * 0.8);
    vec3 c = palette(v + t * uP.w * 0.2);
    return vec4(c, 1.0);
}
