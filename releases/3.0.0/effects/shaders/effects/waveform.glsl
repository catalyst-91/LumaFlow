// LUMAFLOW effect: Waveform (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    int n = 1 + int(uP.x * 6.0);
    for (int i = 0; i < 7; i++) {
        if (i >= n) break;
        float fi = float(i);
        float amp = (0.05 + uP.y * 0.3) * (0.3 + uP.w * (uAudio.w * 3.0 + spectrumAt(fract(uv.x * 0.5 + fi * 0.1))));
        float y = sin(p.x * (6.0 + fi * 3.0) + t * (2.0 + fi)) * amp + sin(p.x * 17.0 - t * 3.0) * amp * 0.3;
        y += (fi - float(n - 1) * 0.5) * 0.08;
        c += palette(fi * 0.2) * (0.002 + uP.z * 0.006) / (abs(p.y - y) + 0.002);
    }
    return vec4(c, 1.0);
}
