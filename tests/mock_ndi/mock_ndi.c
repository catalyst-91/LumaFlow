/* Test double for the NDI runtime: exports the NDIlib_* send functions used by
   LUMAFLOW with the documented NDI SDK ABI and records every call. Not NDI. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#  define EXP __declspec(dllexport)
#else
#  define EXP __attribute__((visibility("default")))
#endif
typedef struct { const char* p_ndi_name; const char* p_groups; bool clock_video; bool clock_audio; } send_create_t;
typedef struct { int xres, yres; int32_t FourCC; int frame_rate_N, frame_rate_D; float picture_aspect_ratio; int32_t frame_format_type;
                 int64_t timecode; uint8_t* p_data; int line_stride_in_bytes; const char* p_metadata; int64_t timestamp; } video_frame_v2_t;
typedef struct { int initialized, destroyed, created, send_destroyed, frames; char name[128]; char groups[128];
                 int xres, yres, fourcc, stride, fps_n, fps_d, format; int64_t timecode; uint8_t first[8]; uint8_t last_row_first[4]; } mock_state_t;
static mock_state_t g;
EXP mock_state_t* mock_ndi_state(void) { return &g; }
EXP bool NDIlib_initialize(void) { g.initialized++; return true; }
EXP void NDIlib_destroy(void) { g.destroyed++; }
EXP void* NDIlib_send_create(const send_create_t* c) {
    g.created++;
    strncpy(g.name, c && c->p_ndi_name ? c->p_ndi_name : "", 127);
    strncpy(g.groups, c && c->p_groups ? c->p_groups : "", 127);
    return &g;
}
EXP void NDIlib_send_destroy(void* p) { (void)p; g.send_destroyed++; }
EXP void NDIlib_send_send_video_v2(void* p, const video_frame_v2_t* f) {
    (void)p; g.frames++; g.xres = f->xres; g.yres = f->yres; g.fourcc = f->FourCC; g.stride = f->line_stride_in_bytes;
    g.fps_n = f->frame_rate_N; g.fps_d = f->frame_rate_D; g.format = f->frame_format_type; g.timecode = f->timecode;
    memcpy(g.first, f->p_data, 8);
    memcpy(g.last_row_first, f->p_data + (size_t)(f->yres - 1) * f->line_stride_in_bytes, 4);
}
EXP int NDIlib_send_get_no_connections(void* p, uint32_t t) { (void)p; (void)t; return 2; }
