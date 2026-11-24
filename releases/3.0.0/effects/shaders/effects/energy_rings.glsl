// LUMAFLOW effect: Energy Rings (Energy)
vec4 effect(vec2 p, vec2 uv, float t) {
    float r = length(p);
    float k = 4.0 + uP.x * 20.0;
    float wave = fract(r * k * (0.5 + uP.z) - t * 0.8);
    float ring = smoothstep(0.02 + uP.y * 0.3, 0.0, abs(wave - 0.5) - 0.02) ;
    ring *= 1.0 + uBeat * uP.w * 2.0;
    vec3 c = palette(r * 2.0 - t * 0.1) * ring * exp(-r * 1.5);
    return vec4(c, 1.0);
}
