/* Example LUMAFLOW plugin: provides a "Starfield" GLSL effect and a "Hyperspace" variant. */
#include "lumaflow_plugin.h"
#include <stddef.h>

static const LfEffectDesc kEffects[] = {
    { "example.starfield", "Starfield (plugin)", "Plugins",
      "vec4 effect(vec2 p, vec2 uv, float t) {\n"
      "    vec3 c = vec3(0.0);\n"
      "    for (int i = 0; i < 4; i++) {\n"
      "        float depth = fract(float(i) * 0.25 + t * (0.05 + uP.x * 0.3));\n"
      "        float scale = mix(20.0, 0.5, depth);\n"
      "        vec2 q = p * scale + float(i) * 13.1;\n"
      "        vec2 id = floor(q); vec2 f = fract(q) - 0.5;\n"
      "        vec2 h = hash22(id);\n"
      "        float d = length(f - (h - 0.5) * 0.7);\n"
      "        float star = smoothstep(0.08 * (0.3 + uP.y), 0.0, d) * step(0.6, h.x);\n"
      "        c += mix(uColorA, uColorB, h.y) * star * depth * smoothstep(1.0, 0.8, depth) * 2.0;\n"
      "    }\n"
      "    return vec4(c, 1.0);\n"
      "}\n",
      { {"Speed", 0.3f}, {"Star size", 0.5f}, {"-", 0.0f}, {"-", 0.0f} },
      {0.8f, 0.9f, 1.0f}, {0.4f, 0.6f, 1.0f}, 0 },
    { "example.hyperspace", "Hyperspace (plugin)", "Plugins",
      "vec4 effect(vec2 p, vec2 uv, float t) {\n"
      "    float a = atan(p.y, p.x); float r = length(p);\n"
      "    float lines = step(0.85, hash11(floor(a * 60.0)));\n"
      "    float z = fract(1.0 / (r + 0.05) * 0.2 - t * (0.5 + uP.x * 2.0) + hash11(floor(a * 60.0)));\n"
      "    vec3 c = mix(uColorA, uColorB, z) * lines * smoothstep(0.0, 0.3, z) * smoothstep(1.0, 0.7, z) * smoothstep(0.02, 0.2, r);\n"
      "    return vec4(c * 1.5, 1.0);\n"
      "}\n",
      { {"Warp speed", 0.5f}, {"-", 0.0f}, {"-", 0.0f}, {"-", 0.0f} },
      {1.0f, 1.0f, 1.0f}, {0.3f, 0.5f, 1.0f}, 0 },
};

static const LfPluginInfo kInfo = {
    LF_PLUGIN_API_VERSION, "io.lumaflow.example.starfield", "Starfield Example", "1.0.0", "LUMAFLOW",
    "Example effect plugin demonstrating the LUMAFLOW C plugin ABI.", LF_PLUGIN_EFFECT, NULL,
    2, kEffects, 0, NULL, 0, NULL
};

static const LfHostApi* g_host = NULL;

LF_PLUGIN_EXPORT const LfPluginInfo* lf_plugin_query(void) { return &kInfo; }
LF_PLUGIN_EXPORT int lf_plugin_init(const LfHostApi* host) {
    g_host = host;
    if (host && host->log) host->log(1, "Starfield example plugin initialised");
    return host && host->api_version == LF_PLUGIN_API_VERSION ? 0 : 1;
}
LF_PLUGIN_EXPORT void lf_plugin_shutdown(void) { g_host = NULL; }
