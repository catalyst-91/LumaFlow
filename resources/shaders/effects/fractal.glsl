// LUMAFLOW effect: Julia Fractal (Geometry)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec2 z = p * (2.5 - uP.z * 2.0);
    vec2 cc = vec2(-0.8 + 0.2 * sin(t * 0.3 * (0.5 + uP.y)), 0.156 + 0.2 * cos(t * 0.23 * (0.5 + uP.y)));
    int maxIt = 32 + int(uP.x * 96.0);
    float it = 0.0;
    for (int i = 0; i < 128; i++) {
        if (i >= maxIt) break;
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + cc;
        if (dot(z, z) > 16.0) break;
        it += 1.0;
    }
    float sm = it - log2(max(log2(dot(z, z)), 1e-6)) + 4.0;
    float v = sm / float(maxIt);
    vec3 c = it >= float(maxIt) ? vec3(0.0) : palette(v * (1.0 + uP.w * 8.0) + t * 0.05) * sqrt(v * 2.0);
    return vec4(c, 1.0);
}
