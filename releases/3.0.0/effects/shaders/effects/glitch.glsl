// LUMAFLOW effect: Glitch (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    float rate = floor(t * (2.0 + uP.w * 20.0));
    vec2 bs = vec2(8.0, 24.0) * (1.0 + uP.y * 4.0);
    vec2 blk = floor(uv * bs);
    float g = step(1.0 - uP.x * 0.6, hash21(blk + rate));
    vec2 q = uv + vec2((hash21(blk.yy + rate) - 0.5) * 0.2 * g, 0.0);
    float split = uP.z * 0.02 * (1.0 + g * 4.0);
    vec3 base;
    base.r = 0.5 + 0.5 * sin((q.x + split) * 20.0 + t + sin(q.y * 10.0));
    base.g = 0.5 + 0.5 * sin(q.x * 20.0 + t + sin(q.y * 10.0) + 2.0);
    base.b = 0.5 + 0.5 * sin((q.x - split) * 20.0 + t + sin(q.y * 10.0) + 4.0);
    vec3 c = mix(uColorA * base, uColorB, g * hash21(blk * 1.3 + rate) * 0.6);
    c *= 0.85 + 0.15 * sin(uv.y * uResolution.y * 1.5);
    return vec4(c, 1.0);
}
