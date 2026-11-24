// LUMAFLOW effect: Solid Colour (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    vec3 c = mix(uColorA, uColorB, uP.x);
    c *= 1.0 - uP.y * (0.5 + 0.5 * sin(t * TAU));
    return vec4(c, 1.0);
}
