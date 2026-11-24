// LUMAFLOW effect: Logo / Image (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 q = rot2(t * uP.y) * p;
    q /= 1.0 + uP.x * 0.1 * sin(t * 4.0) + uBeat * uP.x * 0.2;
    vec2 tuv = vec2(q.x / (0.8 * uTexAspect) + 0.5, 0.5 - q.y / 0.8);
    float inside = step(0.0, tuv.x) * step(tuv.x, 1.0) * step(0.0, tuv.y) * step(tuv.y, 1.0);
    vec4 tx = texture(uTex, tuv) * inside;
    vec3 col = mix(tx.rgb, uColorA * dot(tx.rgb, vec3(0.333)), uP.w);
    col += uColorB * uP.z * 0.3 * smoothstep(0.6, 0.0, length(q));
    return vec4(col, max(tx.a, uP.z * 0.3 * smoothstep(0.6, 0.0, length(q))));
}
