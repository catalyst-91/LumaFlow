// LUMAFLOW effect: Feedback Tunnel (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 c = uv - 0.5;
    c = rot2((uP.z - 0.5) * 0.1) * c * (0.96 + (0.5 - uP.y) * 0.06);
    vec3 prev = texture(uPrev, c + 0.5).rgb * (0.80 + uP.x * 0.19);
    float r = length(p - vec2(sin(t), cos(t * 1.3)) * 0.3);
    vec3 seed = palette(t * 0.1) * smoothstep(0.03 + uP.w * 0.05, 0.0, r);
    return vec4(max(prev, seed), 1.0);
}
