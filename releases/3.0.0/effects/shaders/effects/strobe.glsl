// LUMAFLOW effect: Strobe (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    // Rate is limited by the global photosensitivity safety limiter (Output > Safety).
    float hz = 1.0 + uP.x * 11.0;
    float ph = fract(t * hz);
    float on = step(ph, max(0.05, uP.y));
    if (uP.z > 0.0) on *= step(1.0 - uP.z, hash11(floor(t * hz)));
    if (uP.w > 0.5) on = step(0.6, uBeat);
    return vec4(uColorA * on, 1.0);
}
