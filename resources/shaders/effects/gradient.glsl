// LUMAFLOW effect: Gradient (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 d = vec2(cos(uP.x * TAU), sin(uP.x * TAU));
    float v = mix(dot(p, d) + 0.5, length(p) * 1.4, uP.w);
    v += t * uP.y * 0.2;
    if (uP.z > 0.0) v = floor(v * (2.0 + uP.z * 14.0)) / (2.0 + uP.z * 14.0);
    float k = 0.5 + 0.5 * sin(v * TAU);
    return vec4(mix(uColorA, uColorB, k), 1.0);
}
