/*
 * LUMAFLOW Plugin SDK - C ABI (stable across compilers).
 *
 * A plugin is a shared library (.so / .dll / .dylib) exporting:
 *     const LfPluginInfo* lf_plugin_query(void);
 * and optionally:
 *     int  lf_plugin_init(const LfHostApi* host);   // return 0 on success
 *     void lf_plugin_shutdown(void);
 *
 * Plugins are first loaded in a separate probe process (lumaflow-cli
 * plugin-probe) so that a plugin crashing on load cannot take down a running
 * show. Native code cannot be sandboxed by API validation alone: only install
 * plugins from sources you trust.
 */
#ifndef LUMAFLOW_PLUGIN_H
#define LUMAFLOW_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define LF_PLUGIN_API_VERSION 1

#if defined(_WIN32)
#  define LF_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define LF_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef enum LfPluginCategory {
    LF_PLUGIN_EFFECT = 1,          /* GLSL visual effects                 */
    LF_PLUGIN_SHADER_NODE = 2,     /* node-editor nodes                   */
    LF_PLUGIN_PROTOCOL = 3,        /* control protocols                   */
    LF_PLUGIN_FIXTURE_IMPORT = 4,
    LF_PLUGIN_FIXTURE_EXPORT = 5,
    LF_PLUGIN_OUTPUT = 6,          /* output modules (frame sinks)        */
    LF_PLUGIN_MEDIA_DECODER = 7,
    LF_PLUGIN_AUTOMATION = 8
} LfPluginCategory;

typedef struct LfEffectParam { const char* name; float default_value; } LfEffectParam;

typedef struct LfEffectDesc {
    const char* id;                /* unique, e.g. "acme.starfield"            */
    const char* name;
    const char* category;
    const char* glsl;              /* body defining: vec4 effect(vec2 p, vec2 uv, float t) */
    LfEffectParam params[4];
    float color_a[3];
    float color_b[3];
    int photosensitive;
} LfEffectDesc;

typedef struct LfNodeDesc {
    const char* id; const char* name; const char* category;
    const char* inputs;            /* "p:vec2,amount:float"   */
    const char* outputs;           /* "color:vec3"            */
    const char* expression;        /* GLSL using {in:name}    */
    const char* helper;            /* optional helper source  */
} LfNodeDesc;

/* Output module: receives each program frame (RGBA8, top-down). Called on the
   output thread; must not block. */
typedef struct LfOutputDesc {
    const char* id; const char* name;
    int  (*open)(int width, int height, const char* config_json);
    void (*frame)(const unsigned char* rgba, int width, int height, double time_seconds);
    void (*close)(void);
} LfOutputDesc;

typedef struct LfPluginInfo {
    int api_version;               /* must be LF_PLUGIN_API_VERSION          */
    const char* id;                /* reverse-DNS identifier                 */
    const char* name;
    const char* version;           /* plugin's own semantic version          */
    const char* vendor;
    const char* description;
    int category;                  /* LfPluginCategory (primary)             */
    const char* requires_csv;      /* other plugin ids required, or NULL     */
    int effect_count;  const LfEffectDesc* effects;
    int node_count;    const LfNodeDesc* nodes;
    int output_count;  const LfOutputDesc* outputs;
} LfPluginInfo;

typedef struct LfHostApi {
    int api_version;
    const char* host_version;
    void (*log)(int level, const char* message);   /* 0 debug .. 3 error */
    double (*get_param)(const char* id);
    int (*set_param)(const char* id, double value);
} LfHostApi;

typedef const LfPluginInfo* (*LfPluginQueryFn)(void);
typedef int (*LfPluginInitFn)(const LfHostApi*);
typedef void (*LfPluginShutdownFn)(void);

#ifdef __cplusplus
}
#endif
#endif
