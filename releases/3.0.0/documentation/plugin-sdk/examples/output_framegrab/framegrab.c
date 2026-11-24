/* Example LUMAFLOW output-module plugin: a frame grabber. Every N frames it writes
   the received program frame as a binary PPM to $LUMAFLOW_FRAMEGRAB_PATH (default
   ./lumaflow_framegrab.ppm). Demonstrates the LfOutputDesc interface used for
   custom outputs (capture cards, streaming encoders, LED processors, ...). */
#include "lumaflow_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_w, g_h; static long g_frames; static const char* g_path;
static int every(void) { const char* e = getenv("LUMAFLOW_FRAMEGRAB_EVERY"); int n = e ? atoi(e) : 60; return n > 0 ? n : 60; }

static int fg_open(int width, int height, const char* config_json) {
    (void)config_json;
    g_w = width; g_h = height; g_frames = 0;
    g_path = getenv("LUMAFLOW_FRAMEGRAB_PATH"); if (!g_path) g_path = "lumaflow_framegrab.ppm";
    return 0;
}
static void fg_frame(const unsigned char* rgba, int width, int height, double t) {
    (void)t;
    if (g_frames++ % every() != 0) return;
    FILE* f = fopen(g_path, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) fwrite(rgba + i * 4, 1, 3, f);
    fclose(f);
}
static void fg_close(void) { }

static const LfOutputDesc kOutputs[] = { { "framegrab", "Frame grabber (PPM)", fg_open, fg_frame, fg_close } };
static const LfPluginInfo kInfo = { LF_PLUGIN_API_VERSION, "io.lumaflow.example.framegrab", "Frame Grabber Example", "1.0.0", "LUMAFLOW",
    "Example output module writing PPM snapshots of the program output.", LF_PLUGIN_OUTPUT, NULL, 0, NULL, 0, NULL, 1, kOutputs };
LF_PLUGIN_EXPORT const LfPluginInfo* lf_plugin_query(void) { return &kInfo; }
