// LUMAFLOW effect: Smoke (Organic)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = p * (1.5 + uP.z * 3.0);
    q.y -= t * (0.1 + uP.y * 0.5);
    float n = fbm(q + fbm(q + t * 0.1) * 1.5);
    float d = smoothstep(0.3 - uP.x * 0.2, 0.9 - uP.w * 0.3, n);
    d *= smoothstep(0.6, -0.4, p.y + 0.0) * 1.2;
    vec3 c = mix(uColorB * 0.2, uColorA, d) * d;
    return vec4(c, 1.0);
}
