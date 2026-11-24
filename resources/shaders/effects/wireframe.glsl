// LUMAFLOW effect: Wireframe Terrain (Geometry)
vec4 effect(vec2 p, vec2 uv, float t) {
    float hor = 0.05 + uP.w * 0.2;
    vec3 c = vec3(0.0);
    if (p.y < hor) {
        float z = (0.4) / (hor - p.y + 0.001);
        vec2 g = vec2(p.x * z, z + t * (0.5 + uP.z * 3.0)) * (2.0 + uP.x * 6.0);
        float h = fbm(g * 0.15) * uP.y * 3.0;
        g.y += h;
        vec2 f = abs(fract(g) - 0.5);
        float line = smoothstep(0.06 * z * 0.3, 0.0, min(f.x, f.y) - 0.0);
        c = palette(h * 0.3 + 0.2) * line * smoothstep(40.0, 2.0, z);
    } else {
        c = mix(uColorB * 0.3, vec3(0.0), clamp((p.y - hor) * 3.0, 0.0, 1.0));
        c += uColorA * smoothstep(0.25, 0.0, length(p - vec2(0.0, hor + 0.12))) * 0.6;
    }
    return vec4(c, 1.0);
}
