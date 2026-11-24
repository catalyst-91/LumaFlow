// LUMAFLOW effect: Geometric Tunnel (Geometry)
vec4 effect(vec2 p, vec2 uv, float t) {
    float sides = floor(3.0 + uP.x * 6.0);
    float a = atan(p.y, p.x) + t * uP.z * 0.5;
    float s = TAU / sides;
    float r = length(p) * cos(mod(a, s) - s * 0.5);       // polygon distance
    float z = 0.2 / max(r, 0.001) + t * (0.5 + uP.y * 3.0);
    float lines = smoothstep(0.05 + uP.w * 0.2, 0.0, abs(fract(z) - 0.5) - 0.4);
    vec3 c = palette(floor(z) * 0.13) * lines * smoothstep(0.0, 0.3, r) * (1.0 + 0.5 / (r * 4.0 + 0.2));
    return vec4(c, 1.0);
}
