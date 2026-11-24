// LUMAFLOW effect: Audio Particles (Particles)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = vec3(0.0);
    float kick = uBeat * uP.y + uAudio.x * 0.5;
    vec2 g = p * (6.0 + uP.x * 14.0);
    vec2 id = floor(g), f = fract(g) - 0.5;
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
        vec2 o = vec2(x, y); vec2 h = hash22(id + o);
        vec2 cpos = o + (h - 0.5) * 0.7 * (1.0 + kick);
        float d = length(f - cpos);
        float s = 0.05 + uP.w * 0.2 + kick * 0.15;
        float spark = step(1.0 - uAudio.z * uP.z, h.y);
        c += palette(h.x + t * 0.05) * smoothstep(s, 0.0, d) * (0.6 + spark * 1.5);
    }
    c *= 0.5 + uAudio.w * 2.0 + uBeat;
    return vec4(c, 1.0);
}
