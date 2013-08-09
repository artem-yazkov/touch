#include <stdio.h>
#include <stdlib.h>

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
/* */
#include "util/u_tile.h"
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

struct program
{
    struct pipe_loader_device *dev;
    struct pipe_screen *screen;
    struct pipe_context *pipe;
    struct cso_context *cso;

    struct pipe_blend_state blend;
    struct pipe_depth_stencil_alpha_state depthstencil;
    struct pipe_rasterizer_state rasterizer;
    struct pipe_viewport_state viewport;
    struct pipe_framebuffer_state framebuffer;
    struct pipe_vertex_element velem[2];

    void *vs;
    void *fs;

    union pipe_color_union clear_color;

    struct pipe_resource *vbuf;
    struct pipe_resource *target;
};

int main()
{
    struct program *program = calloc(1, sizeof(struct program));

    struct pipe_loader_device **devs = NULL;
    const int max_devices = 32;

    int idev, ndev;

    /* find & list graphic devices */
    ndev = pipe_loader_probe(&program->dev, max_devices);

    devs = &program->dev;
    for (idev = 0; idev < ndev; idev++)
    {
        printf("driver name: %s; type: %d\n", devs[idev]->driver_name, devs[idev]->type);
    }

    /* init a pipe screen */
    program->screen = pipe_loader_create_screen(devs[0], PIPE_SEARCH_DIR);

    /* create the pipe driver context and cso context */
    program->pipe = program->screen->context_create(program->screen, NULL);
    program->cso = cso_create_context(program->pipe);

    /* destroy */
    cso_destroy_context(program->cso);
    program->pipe->destroy(program->pipe);
    program->screen->destroy(program->screen);

    return 0;
}
