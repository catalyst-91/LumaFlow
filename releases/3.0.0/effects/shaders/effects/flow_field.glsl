// LUMAFLOW effect: Flow Field (Organic)
vec4 effect(vec2 p, vec2 uv, float t) {
    float ang = fbm(p * (1.0 + uP.y * 3.0) + t * 0.05) * TAU * 2.0;
    vec2 dir = vec2(cos(ang), sin(ang));
    float s = dot(p, vec2(-dir.y, dir.x)) * (30.0 + uP.x * 80.0);
    float along = dot(p, dir) * (2.0 + uP.z * 4.0) - t * (1.0 + uP.w * hash11(floor(s)));
    float line = smoothstep(0.35, 0.0, abs(fract(s) - 0.5)) * smoothstep(0.0, 1.0, fract(along));
    vec3 c = palette(ang / TAU) * line;
    return vec4(c, 1.0);
}
