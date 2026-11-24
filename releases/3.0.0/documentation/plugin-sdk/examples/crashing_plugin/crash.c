/* Test fixture: a deliberately broken plugin that crashes when queried. Used to
   verify that out-of-process probing isolates the host. Not installed. */
#include "lumaflow_plugin.h"
#include <stddef.h>
LF_PLUGIN_EXPORT const LfPluginInfo* lf_plugin_query(void) { volatile int* p = NULL; *p = 1; return NULL; }
