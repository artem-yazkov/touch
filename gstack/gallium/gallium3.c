/**************************************************************************
 *
 * Copyright © 2010 Jakob Bornecrantz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#define USE_TRACE 0
#define WIDTH 300
#define HEIGHT 300
#define NEAR 30
#define FAR 1000
#define FLIP 0

#include <stdio.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/dri2.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xf86drm.h>

/*
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
*/

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

#include "utl.h"

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

typedef struct dri2_s
{
    xcb_connection_t                  *xconn;
    xcb_extension_t                   *xext;
    const xcb_query_extension_reply_t *xext_r;
    xcb_screen_iterator_t              xscreen_i;
    xcb_generic_error_t               *xerror;

    xcb_dri2_query_version_cookie_t    d2version_c;
    xcb_dri2_query_version_reply_t    *d2version_r;
    xcb_dri2_connect_cookie_t          d2conn_c;
    xcb_dri2_connect_reply_t          *d2conn_r;
    xcb_dri2_authenticate_cookie_t     d2auth_c;
    xcb_dri2_authenticate_reply_t     *d2auth_r;

    xcb_dri2_swap_buffers_cookie_t     d2swap_c;
    xcb_dri2_wait_sbc_cookie_t         d2wait_c;
    xcb_dri2_get_buffers_cookie_t      d2buffers_c;

    xcb_dri2_dri2_buffer_t            *d2buffers, *d2buffer_left;

    int64_t last_ust, ns_frame, last_msc, next_msc;

    int         drm_fd;
    drm_magic_t drm_magic;

    Drawable    drawable;

} dri2_t;



static const unsigned int attachments[1] = { XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT };



static void
dri2_handle_stamps (dri2_t* dri2,
                      uint32_t ust_hi, uint32_t ust_lo,
                      uint32_t msc_hi, uint32_t msc_lo)
{
   int64_t ust = ((((uint64_t)ust_hi) << 32) | ust_lo) * 1000;
   int64_t msc = (((uint64_t)msc_hi) << 32) | msc_lo;

   if (dri2->last_ust && dri2->last_msc && (ust > dri2->last_ust) && (msc > dri2->last_msc))
      dri2->ns_frame = (ust - dri2->last_ust) / (msc - dri2->last_msc);

   dri2->last_ust = ust;
   dri2->last_msc = msc;
}

static xcb_dri2_get_buffers_reply_t*
dri2_get_flush_reply(dri2_t *dri2)
{
   xcb_dri2_wait_sbc_reply_t *d2wait_r;

   free(xcb_dri2_swap_buffers_reply(dri2->xconn, dri2->d2swap_c, NULL));

   d2wait_r = xcb_dri2_wait_sbc_reply(dri2->xconn, dri2->d2wait_c, NULL);
   if (!d2wait_r)
      return NULL;

   dri2_handle_stamps(dri2, d2wait_r->ust_hi, d2wait_r->ust_lo,
           d2wait_r->msc_hi, d2wait_r->msc_lo);
   free(d2wait_r);

   return xcb_dri2_get_buffers_reply(dri2->xconn, dri2->d2buffers_c, NULL);
}

static void
dri2_flush_frontbuffer(struct pipe_screen *screen,
                       struct pipe_resource *resource,
                       unsigned level, unsigned layer,
                       void *context_private)
{
   dri2_t *dri2 = (dri2_t *)context_private;
   uint32_t msc_hi, msc_lo;

   assert(screen);
   assert(resource);
   assert(context_private);

   free(dri2_get_flush_reply(dri2));

   msc_hi = dri2->next_msc >> 32;
   msc_lo = dri2->next_msc & 0xFFFFFFFF;

   dri2->d2swap_c    = xcb_dri2_swap_buffers_unchecked(dri2->xconn, dri2->drawable, msc_hi, msc_lo, 0, 0, 0, 0);
   dri2->d2wait_c    = xcb_dri2_wait_sbc_unchecked(dri2->xconn, dri2->drawable, 0, 0);
   dri2->d2buffers_c = xcb_dri2_get_buffers_unchecked(dri2->xconn, dri2->drawable, 1, 1, attachments);

   //dri2->flushed = true;
   //dri2->current_buffer = !scrn->current_buffer;
}


static void init_prog(struct program *p, dri2_t *dri2)
{
	struct pipe_surface surf_tmpl;
	int ret;

	/* find a hardware device */
	ret = pipe_loader_probe(&p->dev, 1);
	assert(ret);

	/* init a pipe screen */
	p->screen = pipe_loader_create_screen(p->dev, PIPE_SEARCH_DIR);
	assert(p->screen);
	p->screen->flush_frontbuffer = dri2_flush_frontbuffer;

	/* create the pipe driver context and cso context */
	p->pipe = p->screen->context_create(p->screen, NULL);
	p->cso = cso_create_context(p->pipe);

	/* set clear color */
	p->clear_color.f[0] = 0.3;
	p->clear_color.f[1] = 0.1;
	p->clear_color.f[2] = 0.3;
	p->clear_color.f[3] = 1.0;

	/* vertex buffer */
	{
		float vertices[4][2][4] = {
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

		p->vbuf = pipe_buffer_create(p->screen, PIPE_BIND_VERTEX_BUFFER,
					     PIPE_USAGE_STATIC, sizeof(vertices));
		pipe_buffer_write(p->pipe, p->vbuf, 0, sizeof(vertices), vertices);
	}

	/* render target texture */
	{
	    struct winsys_handle dri2_handle;
	    struct pipe_resource tmplt;

	    memset(&dri2_handle, 0, sizeof(dri2_handle));
	    dri2_handle.type = DRM_API_HANDLE_TYPE_SHARED;
	    dri2_handle.handle = dri2->d2buffer_left->name;
	    dri2_handle.stride = dri2->d2buffer_left->pitch;

	    memset(&tmplt, 0, sizeof(tmplt));
		tmplt.target = PIPE_TEXTURE_2D;
		tmplt.format = PIPE_FORMAT_B8G8R8A8_UNORM; /* All drivers support this */
		tmplt.width0 = WIDTH;
		tmplt.height0 = HEIGHT;
		tmplt.depth0 = 1;
		tmplt.array_size = 1;
		tmplt.last_level = 0;
		tmplt.bind = PIPE_BIND_RENDER_TARGET;

		p->target = p->screen->resource_from_handle(p->screen, &tmplt, &dri2_handle);
	}

	/* disabled blending/masking */
	memset(&p->blend, 0, sizeof(p->blend));
	p->blend.rt[0].colormask = PIPE_MASK_RGBA;

	/* no-op depth/stencil/alpha */
	memset(&p->depthstencil, 0, sizeof(p->depthstencil));

	/* rasterizer */
	memset(&p->rasterizer, 0, sizeof(p->rasterizer));
	p->rasterizer.cull_face = PIPE_FACE_NONE;
	p->rasterizer.half_pixel_center = 1;
	p->rasterizer.bottom_edge_rule = 1;
	p->rasterizer.depth_clip = 1;

	surf_tmpl.format = PIPE_FORMAT_B8G8R8A8_UNORM;
	surf_tmpl.u.tex.level = 0;
	surf_tmpl.u.tex.first_layer = 0;
	surf_tmpl.u.tex.last_layer = 0;
	/* drawing destination */
	memset(&p->framebuffer, 0, sizeof(p->framebuffer));
	p->framebuffer.width = WIDTH;
	p->framebuffer.height = HEIGHT;
	p->framebuffer.nr_cbufs = 1;
	p->framebuffer.cbufs[0] = p->pipe->create_surface(p->pipe, p->target, &surf_tmpl);

	/* viewport, depth isn't really needed */
	{
		float x = 0;
		float y = 0;
		float z = FAR;
		float half_width = (float)WIDTH / 2.0f;
		float half_height = (float)HEIGHT / 2.0f;
		float half_depth = ((float)FAR - (float)NEAR) / 2.0f;
		float scale, bias;

		if (FLIP) {
			scale = -1.0f;
			bias = (float)HEIGHT;
		} else {
			scale = 1.0f;
			bias = 0.0f;
		}

		p->viewport.scale[0] = half_width;
		p->viewport.scale[1] = half_height * scale;
		p->viewport.scale[2] = half_depth;
		p->viewport.scale[3] = 1.0f;

		p->viewport.translate[0] = half_width + x;
		p->viewport.translate[1] = (half_height + y) * scale + bias;
		p->viewport.translate[2] = half_depth + z;
		p->viewport.translate[3] = 0.0f;
	}

	/* vertex elements state */
	memset(p->velem, 0, sizeof(p->velem));
	p->velem[0].src_offset = 0 * 4 * sizeof(float); /* offset 0, first element */
	p->velem[0].instance_divisor = 0;
	p->velem[0].vertex_buffer_index = 0;
	p->velem[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

	p->velem[1].src_offset = 1 * 4 * sizeof(float); /* offset 16, second element */
	p->velem[1].instance_divisor = 0;
	p->velem[1].vertex_buffer_index = 0;
	p->velem[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

	/* vertex shader */
	{
			const uint semantic_names[] = { TGSI_SEMANTIC_POSITION,
							TGSI_SEMANTIC_COLOR };
			const uint semantic_indexes[] = { 0, 0 };
			p->vs = util_make_vertex_passthrough_shader(p->pipe, 2, semantic_names, semantic_indexes);
	}

	/* fragment shader */
	p->fs = util_make_fragment_passthrough_shader(p->pipe,
                    TGSI_SEMANTIC_COLOR, TGSI_INTERPOLATE_PERSPECTIVE, TRUE);
}

static void close_prog(struct program *p)
{
	/* unset all state */
	cso_release_all(p->cso);

	p->pipe->delete_vs_state(p->pipe, p->vs);
	p->pipe->delete_fs_state(p->pipe, p->fs);

	pipe_surface_reference(&p->framebuffer.cbufs[0], NULL);
	pipe_resource_reference(&p->target, NULL);
	pipe_resource_reference(&p->vbuf, NULL);

	cso_destroy_context(p->cso);
	p->pipe->destroy(p->pipe);
	p->screen->destroy(p->screen);
	pipe_loader_release(&p->dev, 1);

	FREE(p);
}

static void draw(struct program *p)
{
	/* set the render target */
	cso_set_framebuffer(p->cso, &p->framebuffer);

	/* clear the render target */
	p->pipe->clear(p->pipe, PIPE_CLEAR_COLOR, &p->clear_color, 0, 0);

	/* set misc state we care about */
	cso_set_blend(p->cso, &p->blend);
	cso_set_depth_stencil_alpha(p->cso, &p->depthstencil);
	cso_set_rasterizer(p->cso, &p->rasterizer);
	cso_set_viewport(p->cso, &p->viewport);

	/* shaders */
	cso_set_fragment_shader_handle(p->cso, p->fs);
	cso_set_vertex_shader_handle(p->cso, p->vs);

	/* vertex element data */
	cso_set_vertex_elements(p->cso, 2, p->velem);

	util_draw_vertex_buffer(p->pipe, p->cso,
	                        p->vbuf, 0, 0,
	                        PIPE_PRIM_TRIANGLES,
	                        3,  /* verts */
	                        2); /* attribs/vert */

    p->pipe->flush(p->pipe, NULL, 0);

	//debug_dump_surface_bmp(p->pipe, "result.bmp", p->framebuffer.cbufs[0]);
}



int dri2_create(dri2_t *dri2, struct program *prog, Display *display, int screen)
{
    memset(dri2, 0, sizeof(dri2_t));

    /* get xcb connection from display */
    if ((dri2->xconn = XGetXCBConnection(display)) == NULL)
    {
        XCloseDisplay(display);
        fprintf(stderr, "Can't get xcb connection from display\n");
        return -1;
    }

    /* retrieve dri2 version */
    dri2->d2version_c = xcb_dri2_query_version (dri2->xconn, XCB_DRI2_MAJOR_VERSION, XCB_DRI2_MINOR_VERSION);
    dri2->d2version_r = xcb_dri2_query_version_reply (dri2->xconn, dri2->d2version_c, &dri2->xerror);
    if (dri2->d2version_r == NULL || dri2->xerror != NULL)
    {
        fprintf(stderr, "can't retrieve dri2 version\n");
        return -1;
    } else
        fprintf(stdout, "dri2 version: %d.%d\n", dri2->d2version_r->major_version, dri2->d2version_r->minor_version);

    /* choose right screen */
    dri2->xscreen_i = xcb_setup_roots_iterator(xcb_get_setup(dri2->xconn));
    while (screen--)
        xcb_screen_next(&dri2->xscreen_i);

    /* connect to dri2 */
    dri2->d2conn_c = xcb_dri2_connect_unchecked(dri2->xconn, dri2->xscreen_i.data->root, XCB_DRI2_DRIVER_TYPE_DRI);
    dri2->d2conn_r = xcb_dri2_connect_reply(dri2->xconn, dri2->d2conn_c, NULL);
    if (dri2->d2conn_r == NULL || dri2->d2conn_r->driver_name_length + dri2->d2conn_r->device_name_length == 0)
    {
        fprintf(stderr, "can't connect to dri2\n");
        return -1;
    } else
        fprintf(stdout, "dri2 device: %s\n", xcb_dri2_connect_device_name(dri2->d2conn_r));

    /* load drm */
    dri2->drm_fd = open(xcb_dri2_connect_device_name(dri2->d2conn_r), O_RDWR);
    if ((dri2->drm_fd < 0) || (drmGetMagic(dri2->drm_fd, &dri2->drm_magic) != 0))
    {
        fprintf(stderr, "can't retrieve drm magic from %s device\n", xcb_dri2_connect_device_name(dri2->d2conn_r));
        return -1;
    }

    dri2->d2auth_c = xcb_dri2_authenticate_unchecked(dri2->xconn, dri2->xscreen_i.data->root, dri2->drm_magic);
    dri2->d2auth_r = xcb_dri2_authenticate_reply(dri2->xconn, dri2->d2auth_c, NULL);
    if (dri2->d2auth_r == NULL || !dri2->d2auth_r->authenticated)
    {
       fprintf(stderr, "can't authenticate with dri2\n");
       return -1;
    }

#if 0
    free(dri2_auth);
    free(dri2_conn);
    free(xcb_conn);
#endif

    return 0;
}

int dri2_hook_drawable(dri2_t *dri2, struct program *prog, Drawable drawable)
{
    xcb_get_geometry_cookie_t     xgeometry_c;
    xcb_get_geometry_reply_t     *xgeometry_r;

    xcb_dri2_wait_sbc_reply_t    *d2wait_r    = NULL;
    xcb_dri2_get_buffers_reply_t *d2buffers_r = NULL;

    int i;

    xcb_dri2_create_drawable(dri2->xconn, drawable);


    d2buffers_r = xcb_dri2_get_buffers_reply(dri2->xconn, dri2->d2buffers_c, NULL);

    if (!d2buffers_r)
    {
       xcb_dri2_get_buffers_cookie_t d2buffers_c;
       d2buffers_c = xcb_dri2_get_buffers_unchecked(dri2->xconn, drawable, 1, 1, attachments);
       d2buffers_r = xcb_dri2_get_buffers_reply(dri2->xconn, d2buffers_c, NULL);
    }
    if (!d2buffers_r)
    {
        fprintf(stderr, "can't get buffers\n");
        return -1;
    }

    dri2->d2buffers = xcb_dri2_get_buffers_buffers(d2buffers_r);
    if (!dri2->d2buffers)
    {
       fprintf(stderr, "can't get buffers\n");
       return -1;
    }

    for (i = 0; i < d2buffers_r->count; ++i)
    {
       if (dri2->d2buffers[i].attachment == XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT)
       {
          dri2->d2buffer_left = &dri2->d2buffers[i];
          break;
       }
    }

    if (i == d2buffers_r->count)
    {
       fprintf(stderr, "can't find back left buffer\n");
       return -1;
    } else
    {
        fprintf(stderr, "back left buffer: %d from %d\n", i, d2buffers_r->count);
    }


    dri2->d2swap_c = xcb_dri2_swap_buffers_unchecked(dri2->xconn, drawable, 0, 0, 0, 0, 0, 0);
    free(xcb_dri2_swap_buffers_reply(dri2->xconn, dri2->d2swap_c, NULL));

    return 0;
}




int main(int argc, char** argv)
{
    Display* display;
    int screen_num;
    Window win;

    dri2_t          dri2;
    struct program *prog      = CALLOC_STRUCT(program);

    display = XOpenDisplay(NULL);

    if (display == NULL)
    {
        exit(1);
    }

    screen_num = DefaultScreen(display);
    win = XCreateSimpleWindow(display,                                       /* connection          */
                              RootWindow(display, screen_num),               /* parent window       */
                              0, 0,                                          /* x, y                */
                              300, 300,                                      /* width, height       */
                              10,                                            /* border width        */
                              BlackPixel(display, screen_num),
                              WhitePixel(display, screen_num));

    dri2_create(&dri2, prog, display, screen_num);

    /* make the window actually appear on the screen. */
    XMapWindow(display, win);

    dri2_hook_drawable(&dri2, prog, win);
    init_prog(prog, &dri2);
    draw(prog);

    dri2.d2swap_c = xcb_dri2_swap_buffers_unchecked(dri2.xconn, win, 0, 0, 0, 0, 0, 0);
    free(xcb_dri2_swap_buffers_reply(dri2.xconn, dri2.d2swap_c, NULL));

    /* flush all pending requests to the X server, and wait until */
    /* they are processed by the X server.                        */
    XSync(display, False);

    /* make a delay for a short period. */
    pause();

    /* close the connection to the X server. */
    XCloseDisplay(display);

#if 0
	struct program *p      = CALLOC_STRUCT(program);
	utl_window_t   *window = calloc(1, sizeof(utl_window_t));

	window->width  = WIDTH;
	window->height = HEIGHT;
	utl_window_init(window);

	init_prog(p);

	draw(p);

	utl_window_attach_surface(window, p->pipe, p->framebuffer.cbufs[0]);

	for (;;)
	{
	    utl_window_nextevent(window);
	    if (window->event.type == KeyPress)
	        break;
	}
	close_prog(p);
#endif
	return 0;
}
