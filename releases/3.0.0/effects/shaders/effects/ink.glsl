// LUMAFLOW effect: Ink Clouds (Organic)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * 2.0;
    vec2 w = vec2(fbm(q + t * 0.1 * uP.w), fbm(q + vec2(5.2, 1.3) - t * 0.08));
    float n = fbm(q + w * (2.0 + uP.y * 4.0));
    float r = length(p);
    float mask = smoothstep(0.3 + uP.x * 0.8, 0.0, r - n * 0.4);
    vec3 c = mix(uColorA, uColorB, n) * mask * (1.0 - uP.z * 0.5 * n);
    return vec4(c, 1.0);
}
