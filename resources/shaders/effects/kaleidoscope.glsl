// LUMAFLOW effect: Kaleidoscope (Geometry)
vec4 effect(vec2 p, vec2 uv, float t) {
    float seg = floor(3.0 + uP.x * 13.0);
    float r = length(p), a = atan(p.y, p.x) + t * uP.w;
    float s = TAU / seg;
    a = mod(a, s); a = abs(a - s * 0.5);
    vec2 q = vec2(cos(a), sin(a)) * r * (1.0 + uP.y * 4.0);
    q += vec2(t * 0.1, t * 0.07);
    float v = fbm(q * (2.0 + uP.z * 6.0)) ;
    float lines = smoothstep(0.02, 0.0, abs(fract(v * 6.0) - 0.5) - 0.4);
    vec3 c = palette(v * 2.0 + t * 0.05) * (0.5 + v) + lines * 0.3;
    return vec4(c * smoothstep(1.2, 0.2, r), 1.0);
}
