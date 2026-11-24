// LUMAFLOW shader prelude - prepended to every effect / node-generated shader.
uniform float uTime;
uniform float uSpeed;
uniform float uScale;
uniform float uRotation;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uHue;
uniform float uHue2;
uniform vec2  uResolution;
uniform vec2  uPos;
uniform vec4  uP;          // effect-specific parameters p1..p4 (0..1)
uniform vec4  uAudio;      // x=bass y=mid z=treble w=level
uniform float uBeat;       // beat pulse 1 -> 0
uniform float uSpec[32];   // audio spectrum
uniform vec3  uColorA;
uniform vec3  uColorB;
uniform sampler2D uPrev;   // previous output frame (feedback effects)
uniform sampler2D uTex;    // text / logo / media texture
uniform float uTexAspect;
uniform float uExt[8];    // external control values (MIDI/OSC/DMX via ext.1..8 parameters)
uniform float uTimecode;  // timecode / timeline position in seconds
in vec2 vUV;
out vec4 fragColor;

#define PI 3.14159265359
#define TAU 6.28318530718

float hash11(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash21(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2  hash22(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), u.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) { float v = 0.0, a = 0.5; mat2 r = mat2(0.8, -0.6, 0.6, 0.8); for (int i = 0; i < 5; i++) { v += a * noise(p); p = r * p * 2.02; a *= 0.5; } return v; }
vec3  hsv2rgb(vec3 c) { vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0); return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y); }
vec3  rgb2hsv(vec3 c) { vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0); vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g)); vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r)); float d = q.x - min(q.w, q.y); float e = 1.0e-10; return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x); }
mat2  rot2(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }
vec3  palette(float t) { return mix(uColorA, uColorB, 0.5 + 0.5 * sin(TAU * t)); }
float spectrumAt(float x) { int i = int(clamp(x, 0.0, 0.999) * 32.0); return uSpec[i]; }
