#include <stdio.h>
#include "utl-galt.h"


#define NEAR 30
#define FAR 1000
#define FLIP 0

float scale, bias;

int draw(galt_handler_t *hdl,  galt_context_t *ctx, int32_t ww, int32_t wh)
{
    struct pipe_blend_state               blend;
    struct pipe_depth_stencil_alpha_state depthstencil;
    struct pipe_rasterizer_state          rasterizer;

    union  pipe_color_union     clear_color;
    struct pipe_resource       *vbuf;
    struct pipe_viewport_state  viewport;
    struct pipe_vertex_element  velem[2];

    void   *vs;
    void   *fs;

    /* set clear color */
    clear_color.f[0] = 0.3;
    clear_color.f[1] = 0.1;
    clear_color.f[2] = 0.3;
    clear_color.f[3] = 1.0;

    /* vertex buffer */
    {
        float vertices[3][2][4] = {
                {
                    { 0.0f, -0.9f, 0.0f, 1.0f },
                    { 1.0f, 0.0f, 0.0f, 1.0f }
                },
                {
                    { -0.9f, 0.9f, 0.0f, 1.0f },
                    { 0.0f, 1.0f, 0.0f, 1.0f }
                },
                {
                    { 0.9f, 0.9f, 0.0f, 1.0f },
                    { 0.0f, 0.0f, 1.0f, 1.0f }
                }
        };

        vbuf = pipe_buffer_create(ctx->pipe->screen, PIPE_BIND_VERTEX_BUFFER,
                         PIPE_USAGE_STATIC, sizeof(vertices));
        pipe_buffer_write(ctx->pipe, vbuf, 0, sizeof(vertices), vertices);
    }


    /* viewport, depth isn't really needed */
    {
        float x = 0;
        float y = 0;
        float z = FAR;
        float half_width = (float)ww / 2.0f;
        float half_height = (float)wh / 2.0f;
        float half_depth = ((float)FAR - (float)NEAR) / 2.0f;

        //scale = 0.1;
        viewport.scale[0] = half_width  * scale;
        viewport.scale[1] = half_height * scale;
        viewport.scale[2] = half_depth  * scale;
        viewport.scale[3] = 1.0f;

        viewport.translate[0] = half_width + x;
        viewport.translate[1] = (half_height + y) * scale /*+ bias*/;
        viewport.translate[2] = half_depth + z;
        viewport.translate[3] = 0.0f;
    }

    /* vertex elements state */
    memset(velem, 0, sizeof(velem));
    velem[0].src_offset = 0 * 4 * sizeof(float); /* offset 0, first element */
    velem[0].instance_divisor = 0;
    velem[0].vertex_buffer_index = 0;
    velem[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

    velem[1].src_offset = 1 * 4 * sizeof(float); /* offset 16, second element */
    velem[1].instance_divisor = 0;
    velem[1].vertex_buffer_index = 0;
    velem[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

    /* vertex shader */
    {
            const uint semantic_names[] = { TGSI_SEMANTIC_POSITION,
                            TGSI_SEMANTIC_COLOR };
            const uint semantic_indexes[] = { 0, 0 };
            vs = util_make_vertex_passthrough_shader(ctx->pipe, 2, semantic_names, semantic_indexes);
    }

    /* fragment shader */
    fs = util_make_fragment_passthrough_shader(ctx->pipe,
                    TGSI_SEMANTIC_COLOR, TGSI_INTERPOLATE_PERSPECTIVE, TRUE);

    /* =========================================================================== */
    /* =========================================================================== */

    /* disabled blending/masking */
    memset(&blend, 0, sizeof(blend));
    blend.rt[0].colormask = PIPE_MASK_RGBA;
    cso_set_blend(ctx->cso, &blend);

    /* no-op depth/stencil/alpha */
    memset(&depthstencil, 0, sizeof(depthstencil));
    cso_set_depth_stencil_alpha(ctx->cso, &depthstencil);

    /* rasterizer */
    memset(&rasterizer, 0, sizeof(rasterizer));
    rasterizer.cull_face = PIPE_FACE_NONE;
    rasterizer.half_pixel_center = 1;
    rasterizer.bottom_edge_rule = 1;
    rasterizer.depth_clip = 1;
    cso_set_rasterizer(ctx->cso, &rasterizer);

    /* clear the render target */
    ctx->pipe->clear(ctx->pipe, PIPE_CLEAR_COLOR, &clear_color, 0, 0);

    /* set misc state we care about */
    cso_set_viewport(ctx->cso, &viewport);

    /* shaders */
    cso_set_fragment_shader_handle(ctx->cso, fs);
    cso_set_vertex_shader_handle(ctx->cso, vs);

    /* vertex element data */
    cso_set_vertex_elements(ctx->cso, 2, velem);

    util_draw_vertex_buffer(ctx->pipe, ctx->cso,
                            vbuf, 0, 0,
                            PIPE_PRIM_TRIANGLES,
                            3,  /* verts */
                            2); /* attribs/vert */


    ctx->pipe->flush(ctx->pipe, NULL, 0);

    //cso_release_all(ctx->cso);

    cso_delete_fragment_shader(ctx->cso, fs);
    cso_delete_vertex_shader(ctx->cso, vs);

    pipe_resource_reference(&vbuf, NULL);
}

int timer(galt_handler_t *hdl)
{
    if (scale >= 1.0)
        scale = 0.1;
    scale += 0.001;
    return galt_redraw(hdl);
}

int main()
{
    galt_window_t *gwin = calloc(1, sizeof(galt_window_t));

    gwin->w_height = 300;
    gwin->w_width  = 300;
    gwin->event_window = draw;

    gwin->t_delay = 1.0 / 60;
    gwin->event_timer = timer;

    galt_open_window(gwin);

    return 0;
}
