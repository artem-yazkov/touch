#include <math.h>
#include <stdio.h>
#include "utl-galt.h"


const float backcolors[4] = {0.3, 0.1, 0.3, 1.0};

const float vertices[3][2][4] =
{
    {
        { 0.0f, -0.9f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f, 1.0f  }
    },
    {
        { -0.9f, 0.9f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f  }
    },
    {
        { 0.9f, 0.9f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f }
    }
};

typedef struct twist_s
{
    uint64_t cframes;
    double   angle;

    struct pipe_blend_state               blend;
    struct pipe_depth_stencil_alpha_state depthstencil;
    struct pipe_rasterizer_state          rasterizer;

    union  pipe_color_union     clear_color;
    struct pipe_resource       *vbuf;
    struct pipe_viewport_state  viewport;
    struct pipe_vertex_element  velem[2];
    void   *vs;
    void   *fs;

} twist_t;

int twist_init(twist_t *twist, galt_context_t *ctx)
{
    memcpy(&twist->clear_color.f, &backcolors, sizeof(backcolors));
    twist->vbuf = pipe_buffer_create(ctx->pipe->screen, PIPE_BIND_VERTEX_BUFFER,
                                     PIPE_USAGE_STATIC, sizeof(vertices));
    pipe_buffer_write(ctx->pipe, twist->vbuf, 0, sizeof(vertices), vertices);

    /* vertex elements state */
    twist->velem[0].src_offset = 0 * 4 * sizeof(float); /* offset 0, first element */
    twist->velem[0].instance_divisor = 0;
    twist->velem[0].vertex_buffer_index = 0;
    twist->velem[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

    twist->velem[1].src_offset = 1 * 4 * sizeof(float); /* offset 16, second element */
    twist->velem[1].instance_divisor = 0;
    twist->velem[1].vertex_buffer_index = 0;
    twist->velem[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

    cso_set_vertex_elements(ctx->cso, 2, twist->velem);

    /* vertex shader */
    {
            const uint semantic_names[] = { TGSI_SEMANTIC_POSITION,
                            TGSI_SEMANTIC_COLOR };
            const uint semantic_indexes[] = { 0, 0 };
            twist->vs = util_make_vertex_passthrough_shader(ctx->pipe, 2, semantic_names, semantic_indexes);

            cso_set_vertex_shader_handle(ctx->cso, twist->vs);
    }

    /* fragment shader */
    twist->fs = util_make_fragment_passthrough_shader(ctx->pipe,
                    TGSI_SEMANTIC_COLOR, TGSI_INTERPOLATE_PERSPECTIVE, TRUE);
    cso_set_fragment_shader_handle(ctx->cso, twist->fs);

    /* disabled blending/masking */
    twist->blend.rt[0].colormask = PIPE_MASK_RGBA;
    cso_set_blend(ctx->cso, &twist->blend);

    /* no-op depth/stencil/alpha */
    cso_set_depth_stencil_alpha(ctx->cso, &twist->depthstencil);

    /* rasterizer */
    twist->rasterizer.cull_face = PIPE_FACE_NONE;
    twist->rasterizer.half_pixel_center = 1;
    twist->rasterizer.bottom_edge_rule = 1;
    twist->rasterizer.depth_clip = 1;
    cso_set_rasterizer(ctx->cso, &twist->rasterizer);

    return 0;
}

int twist_draw(galt_handler_t *hdl,  galt_context_t *ctx, int32_t ww, int32_t wh)
{
    twist_t *twist = galt_get_udata(hdl);
    if (twist->cframes == 0)
    {
        twist_init(twist, ctx);
    }

    /* viewport, depth isn't really needed */
    {
        twist->viewport.scale[0] = (float)ww  * 0.1;
        twist->viewport.scale[1] = (float)wh * 0.1;
        twist->viewport.scale[2] = 0.0f;
        twist->viewport.scale[3] = 0.0f;

        twist->viewport.translate[0] = (float)ww * 0.5 + ((float)ww * 0.4 * cos(twist->angle));
        twist->viewport.translate[1] = (float)wh * 0.5 + ((float)wh * 0.4 * sin(twist->angle));
        twist->viewport.translate[2] = 0.0f;
        twist->viewport.translate[3] = 0.0f;
    }

    /* clear the render target */
    ctx->pipe->clear(ctx->pipe, PIPE_CLEAR_COLOR, &twist->clear_color, 0, 0);

    /* set misc state we care about */
    cso_set_viewport(ctx->cso, &twist->viewport);

    util_draw_vertex_buffer(ctx->pipe, ctx->cso,
                            twist->vbuf, 0, 0,
                            PIPE_PRIM_TRIANGLES,
                            3,  /* verts */
                            2); /* attribs/vert */

    ctx->pipe->flush(ctx->pipe, NULL, 0);

    twist->cframes++;

    return 0;
}

int twist_timer(galt_handler_t *hdl)
{
    twist_t *twist = galt_get_udata(hdl);
    twist->angle += 0.01;

    if (twist->angle >= 2 * M_PI)
        twist->angle -= 2 * M_PI;

    return galt_redraw(hdl);
}

int main()
{
    twist_t       *twist = calloc(1, sizeof(twist_t));
    galt_window_t *gwin  = calloc(1, sizeof(galt_window_t));

    gwin->w_height = 300;
    gwin->w_width  = 300;
    gwin->w_name   = "twist";
    gwin->event_window = twist_draw;
    gwin->udata    = twist;

    gwin->t_delay = 1.0 / 100;
    gwin->event_timer = twist_timer;

    galt_open_window(gwin);


    return 0;
}
