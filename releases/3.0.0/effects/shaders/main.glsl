// LUMAFLOW shader epilogue - common transform and colour pipeline.
void main() {
    vec2 uv = vUV;
    float aspect = uResolution.x / max(uResolution.y, 1.0);
    vec2 p = uv - 0.5 - vec2(uPos.x, -uPos.y);
    p.x *= aspect;
    p = rot2(-uRotation * TAU) * p;
    p /= max(uScale, 0.01);
    float t = uTime;
    vec4 c = effect(p, uv, t);
    vec3 col = c.rgb;
    if (uHue != 0.0) { vec3 h = rgb2hsv(col); h.x = fract(h.x + uHue); col = hsv2rgb(h); }
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(vec3(l), col, uSaturation);
    col = (col - 0.5) * uContrast + 0.5;
    col *= uBrightness;
    fragColor = vec4(clamp(col, 0.0, 1.0), clamp(c.a, 0.0, 1.0));
}
