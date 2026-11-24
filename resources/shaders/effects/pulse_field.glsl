// LUMAFLOW effect: Pulsating Field (Energy)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 g = p * (4.0 + uP.x * 16.0);
    vec2 id = floor(g), f = fract(g) - 0.5;
    float h = hash21(id);
    float pulse = 0.5 + 0.5 * sin(t * (1.0 + uP.y * 4.0) + h * TAU + length(id) * 0.5);
    pulse = pow(pulse, 1.0 + uP.z * 8.0) + uAudio.x * uP.w;
    float d = length(f);
    vec3 c = palette(h + t * 0.05) * smoothstep(0.5, 0.0, d) * pulse;
    return vec4(c, 1.0);
}
