#ifndef __UTL_GALT_H__
#define __UTL_GALT_H__

#include <stdint.h>

#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "state_tracker/drm_driver.h"

#include "util/u_memory.h"
#include "util/u_hash.h"
#include "util/u_hash_table.h"
#include "util/u_inlines.h"

/* pipe_*_state structs */
#include "pipe/p_state.h"
/* pipe_context */
#include "pipe/p_context.h"
/* pipe_screen */
#include "pipe/p_screen.h"
/* PIPE_* */
#include "pipe/p_defines.h"
/* TGSI_SEMANTIC_{POSITION|GENERIC} */
#include "pipe/p_shader_tokens.h"
/* pipe_buffer_* helpers */
#include "util/u_inlines.h"

/* constant state object helper */
#include "cso_cache/cso_context.h"

/* debug_dump_surface_bmp */
#include "util/u_debug.h"
/* util_draw_vertex_buffer helper */
#include "util/u_draw_quad.h"
/* FREE & CALLOC_STRUCT */
#include "util/u_memory.h"
/* util_make_[fragment|vertex]_passthrough_shader */
#include "util/u_simple_shaders.h"
/* to get a hardware pipe driver */
#include "pipe-loader/pipe_loader.h"

typedef struct galt_handler_s galt_handler_t;
typedef struct galt_context_s
{
    struct cso_context  *cso;
    struct pipe_context *pipe;
} galt_context_t;

typedef int (*galt_event_window)(galt_handler_t *hdl,  galt_context_t *ctx, int32_t ww, int32_t wh);
typedef int (*galt_event_key)   (galt_handler_t *hdl,  uint32_t key,     int32_t wx, int32_t wy);
typedef int (*galt_event_mouse) (galt_handler_t *hdl,  uint32_t button,  int32_t wx, int32_t wy);
typedef int (*galt_event_timer) (galt_handler_t *hdl);

typedef struct galt_window_s
{
    char    *w_name;
    int32_t  w_width;
    int32_t  w_height;
    double   t_delay;

    galt_event_window  event_window;
    galt_event_key     event_key;
    galt_event_mouse   event_mouse;
    galt_event_timer   event_timer;

    void *udata;
} galt_window_t;

void *galt_get_udata  (galt_handler_t *hdl);
int   galt_set_timer  (galt_handler_t *hdl, double delay);
int   galt_redraw     (galt_handler_t *hdl);
int   galt_open_window(galt_window_t  *window);

#endif /* __UTL_GALT_H__ */
