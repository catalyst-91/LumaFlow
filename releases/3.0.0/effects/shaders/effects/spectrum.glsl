// LUMAFLOW effect: Audio Spectrum (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    float bars = 8.0 + floor(uP.x * 24.0);
    float x = uv.x;
    if (uP.z > 0.5) x = abs(uv.x - 0.5) * 2.0;
    float b = floor(x * bars) / bars;
    float v = spectrumAt(b);
    float f = fract(x * bars);
    float bar = step(uP.y * 0.5, f) * step(f, 1.0 - uP.y * 0.5);
    float y = uP.z > 0.5 ? abs(uv.y - 0.5) * 2.0 : 1.0 - uv.y;
    float fill = step(y, v) * bar;
    vec3 c = mix(uColorA, uColorB, y) * fill + uColorB * smoothstep(0.03, 0.0, abs(y - v)) * bar * uP.w;
    return vec4(c, 1.0);
}
