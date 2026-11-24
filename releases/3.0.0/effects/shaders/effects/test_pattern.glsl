// LUMAFLOW effect: Test Pattern (Additional)
vec4 effect(vec2 p, vec2 uv, float t) {
    float mode = floor(uP.x * 3.99);
    vec3 c;
    if (mode < 1.0) {               // colour bars + grid + circle
        float bar = floor(uv.x * 8.0);
        vec3 bars[8] = vec3[8](vec3(1), vec3(1,1,0), vec3(0,1,1), vec3(0,1,0), vec3(1,0,1), vec3(1,0,0), vec3(0,0,1), vec3(0));
        c = bars[int(bar)] * 0.75;
        vec2 g = abs(fract(uv * uResolution / (40.0 + uP.y * 80.0)) - 0.5);
        c = mix(c, vec3(1.0), smoothstep(0.48, 0.5, max(g.x, g.y)));
        float r = length((uv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0));
        c = mix(c, vec3(1.0), smoothstep(0.006, 0.0, abs(r - 0.45)));
    } else if (mode < 2.0) { c = vec3(uv.x); }           // grey ramp
    else if (mode < 3.0) { c = vec3(1.0); }              // full white
    else { c = vec3(fract(t * 0.5) < 0.33 ? vec3(1,0,0) : fract(t * 0.5) < 0.66 ? vec3(0,1,0) : vec3(0,0,1)); }
    vec2 e = min(uv, 1.0 - uv) * uResolution;
    c = mix(c, vec3(1.0, 0.0, 0.0), step(min(e.x, e.y), 2.0));       // 2px red edge marks canvas boundary
    return vec4(c, 1.0);
}
