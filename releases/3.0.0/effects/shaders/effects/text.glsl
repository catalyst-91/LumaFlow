// LUMAFLOW effect: Text (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    float aspect = uResolution.x / max(uResolution.y, 1.0);
    float tw = aspect * (0.4 + uP.w * 0.55);          // text width in canvas units
    float th = tw / max(uTexAspect, 0.01);
    vec2 q = p;
    q.x += t * uP.x * 0.3;
    if (uP.x > 0.0) q.x = mod(q.x + tw * 0.5 + aspect * 0.5, tw + aspect) - tw * 0.5 - aspect * 0.5;
    q.y += sin(q.x * 6.0 + t * 2.0) * uP.z * 0.05;
    vec2 tuv = vec2(q.x / tw + 0.5, 0.5 - q.y / th);
    float inside = step(0.0, tuv.x) * step(tuv.x, 1.0) * step(0.0, tuv.y) * step(tuv.y, 1.0);
    float a = texture(uTex, tuv).a * inside;
    float glow = 0.0;
    for (int i = 0; i < 8; i++) { float ang = float(i) / 8.0 * TAU; vec2 o = tuv + vec2(cos(ang), sin(ang) * uTexAspect) * 0.012; glow += texture(uTex, o).a * step(0.0, o.x) * step(o.x, 1.0) * step(0.0, o.y) * step(o.y, 1.0); }
    glow /= 8.0;
    vec3 c = uColorA * a + uColorB * max(glow - a, 0.0) * uP.y * 2.0;
    return vec4(c, max(a, glow * uP.y));
}
