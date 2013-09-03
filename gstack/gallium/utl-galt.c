#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/dri2.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xf86drm.h>

#include "utl-galt.h"

struct galt_handler_s
{
    struct galt_handler_xlib_s
    {
        Display* display;
        int      socket;
        int      nscreen;
        Window   win;
        XEvent   event;

        char    *w_name;
        int32_t  w_width;
        int32_t  w_height;
    } xlib;

    struct galt_handler_dri2_s
    {
        xcb_connection_t                  *conn;
        xcb_dri2_swap_buffers_cookie_t     swap_c;
        xcb_dri2_wait_sbc_cookie_t         wait_sbc_c;
        xcb_dri2_wait_msc_cookie_t         wait_msc_c;
        xcb_dri2_get_buffers_cookie_t      buffers_c;
        xcb_dri2_dri2_buffer_t            *buffers, *buffer_left;
        xcb_drawable_t                     drawable;

        struct galt_handler_dri2_cnt_s
        {
            uint64_t msc;
            uint64_t sbc;
            uint64_t ust;
        } cnt;

        int         drm_fd;
        drm_magic_t drm_magic;
    } dri2;

    struct galt_handler_event_s
    {
        galt_event_window  event_window;
        galt_event_key     event_key;
        galt_event_mouse   event_mouse;
        galt_event_timer   event_timer;
    } events;

    struct galt_handler_timer_s
    {
        timer_t  tmid;
        int      tmstatus;
        double   delay;
        uint64_t counter;
    } timer;

    struct galt_handler_gallium_s
    {
        struct pipe_loader_device *dev;
        struct pipe_screen        *screen;
        struct pipe_context       *pipe;
        struct cso_context        *cso;
    } gallium;

    void    *userdata;
};

static const uint32_t attachments[1] = { XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT };

int __handler_init(galt_handler_t **hdl, galt_window_t  *window)
{
    *hdl = calloc(1, sizeof(galt_handler_t));

    (*hdl)->userdata    = window->udata;
    (*hdl)->timer.delay = window->t_delay;

    (*hdl)->events.event_key    = window->event_key;
    (*hdl)->events.event_mouse  = window->event_mouse;
    (*hdl)->events.event_timer  = window->event_timer;
    (*hdl)->events.event_window = window->event_window;

    (*hdl)->xlib.w_height = window->w_height;
    (*hdl)->xlib.w_width  = window->w_width;

    if (window->w_name != NULL)
        (*hdl)->xlib.w_name   = strdup(window->w_name);

    return 0;
}

int __handler_free(galt_handler_t *hdl)
{
    if (hdl->xlib.w_name != NULL)
        free(hdl->xlib.w_name);
    free(hdl);

    return 0;
}

int __dri2_connect(struct galt_handler_dri2_s *dri2, Display *display, int nscreen)
{
    xcb_dri2_query_version_cookie_t    d2version_c;
    xcb_dri2_query_version_reply_t    *d2version_r;
    xcb_screen_iterator_t              xscreen_i;
    xcb_dri2_connect_cookie_t          d2conn_c;
    xcb_dri2_connect_reply_t          *d2conn_r;
    xcb_dri2_authenticate_cookie_t     d2auth_c;
    xcb_dri2_authenticate_reply_t     *d2auth_r;

    /* get xcb connection from display */
    if ((dri2->conn = XGetXCBConnection(display)) == NULL)
    {
        fprintf(stderr, "can't get xcb connection\n");
        return -1;
    }

    /* retrieve dri2 version */
    d2version_c = xcb_dri2_query_version_unchecked(dri2->conn, XCB_DRI2_MAJOR_VERSION, XCB_DRI2_MINOR_VERSION);
    d2version_r = xcb_dri2_query_version_reply (dri2->conn, d2version_c, NULL);
    if (d2version_r == NULL)
    {
        fprintf(stderr, "can't retrieve dri2 version\n");
        return -1;
    }
    if ((d2version_r->major_version != XCB_DRI2_MAJOR_VERSION) || (d2version_r->minor_version != XCB_DRI2_MINOR_VERSION))
    {
        fprintf(stderr, "warning: mismatch dri2 versions: %d.%d client, %d.%d server\n",
                XCB_DRI2_MAJOR_VERSION, XCB_DRI2_MINOR_VERSION,
                d2version_r->major_version, d2version_r->minor_version);
    }
    free(d2version_r);

    /* choose right screen */
    xscreen_i = xcb_setup_roots_iterator(xcb_get_setup(dri2->conn));
    while (nscreen--)
        xcb_screen_next(&xscreen_i);

    /* connect to dri2 */
    d2conn_c = xcb_dri2_connect_unchecked(dri2->conn, xscreen_i.data->root, XCB_DRI2_DRIVER_TYPE_DRI);
    d2conn_r = xcb_dri2_connect_reply(dri2->conn, d2conn_c, NULL);
    if (d2conn_r == NULL || d2conn_r->driver_name_length + d2conn_r->device_name_length == 0)
    {
        fprintf(stderr, "can't connect to dri2\n");
        if (d2conn_r != NULL)
            free(d2conn_r);
        return -1;
    }

    /* load drm */
    dri2->drm_fd = open(xcb_dri2_connect_device_name(d2conn_r), O_RDWR);
    if ((dri2->drm_fd < 0) || (drmGetMagic(dri2->drm_fd, &dri2->drm_magic) != 0))
    {
        fprintf(stderr, "can't retrieve drm magic from %s device\n", xcb_dri2_connect_device_name(d2conn_r));
        if (dri2->drm_fd < 0)
            close(dri2->drm_fd);
        free(d2conn_r);
        return -1;
    }
    free(d2conn_r);

    /* authentificate */
    d2auth_c = xcb_dri2_authenticate_unchecked(dri2->conn, xscreen_i.data->root, dri2->drm_magic);
    d2auth_r = xcb_dri2_authenticate_reply(dri2->conn, d2auth_c, NULL);
    if (d2auth_r == NULL || !d2auth_r->authenticated)
    {
       fprintf(stderr, "can't authenticate with dri2\n");
       if (d2auth_r != NULL)
           free(d2auth_r);
       return -1;
    }
    free(d2auth_r);

    return 0;
}

int __dri2_attach_drawable(struct galt_handler_dri2_s *dri2, Drawable drawable)
{
    xcb_get_geometry_cookie_t     xgeometry_c;
    xcb_get_geometry_reply_t     *xgeometry_r;
    xcb_dri2_wait_sbc_reply_t    *d2wait_r    = NULL;
    xcb_dri2_get_buffers_reply_t *d2buffers_r = NULL;

    int ibuffer;

    dri2->drawable = drawable;
    xcb_dri2_create_drawable(dri2->conn, dri2->drawable);

    d2buffers_r = xcb_dri2_get_buffers_reply(dri2->conn, dri2->buffers_c, NULL);

    if (!d2buffers_r)
    {
       xcb_dri2_get_buffers_cookie_t d2buffers_c;
       d2buffers_c = xcb_dri2_get_buffers_unchecked(dri2->conn, drawable, 1, 1, attachments);
       d2buffers_r = xcb_dri2_get_buffers_reply(dri2->conn, d2buffers_c, NULL);
    }
    if (!d2buffers_r)
    {
        fprintf(stderr, "can't get buffers reply\n");
        return -1;
    }

    dri2->buffers = xcb_dri2_get_buffers_buffers(d2buffers_r);
    if (!dri2->buffers)
    {
       fprintf(stderr, "can't get buffers\n");
       return -1;
    }

    for (ibuffer = 0; ibuffer < d2buffers_r->count; ibuffer++)
    {
       if (dri2->buffers[ibuffer].attachment == XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT)
       {
          dri2->buffer_left = &dri2->buffers[ibuffer];
          break;
       }
    }

    if (ibuffer == d2buffers_r->count)
    {
       fprintf(stderr, "can't find back left buffer\n");
       return -1;
    }

    xcb_dri2_get_msc_cookie_t  d2msc_c;
    xcb_dri2_get_msc_reply_t  *d2msc_r;

    d2msc_c = xcb_dri2_get_msc_unchecked(dri2->conn, dri2->drawable);
    d2msc_r = xcb_dri2_get_msc_reply(dri2->conn, d2msc_c, NULL);

    if (d2msc_r == NULL)
    {
        fprintf(stderr, "can't get msc\n");
        return -1;
    }

    dri2->cnt.msc = (uint64_t)d2msc_r->msc_hi << 32 | d2msc_r->msc_lo;
    dri2->cnt.sbc = (uint64_t)d2msc_r->sbc_hi << 32 | d2msc_r->sbc_lo;
    dri2->cnt.ust = (uint64_t)d2msc_r->ust_hi << 32 | d2msc_r->ust_lo;
    free(d2msc_r);

    return 0;
}

void __dri2_flush_frontbuffer(struct pipe_screen *screen,
                              struct pipe_resource *resource,
                              unsigned level, unsigned layer,
                              void *context_private)
{
    printf("__dri2_flush_frontbuffer");
}

int __gallium_init(struct galt_handler_gallium_s *ga)
{
    int ret;

    ret = pipe_loader_probe(&ga->dev, 1);
    ga->screen = pipe_loader_create_screen(ga->dev, PIPE_SEARCH_DIR);
    ga->screen->flush_frontbuffer = __dri2_flush_frontbuffer;
    ga->pipe = ga->screen->context_create(ga->screen, NULL);
    ga->cso = cso_create_context(ga->pipe);

    return 0;
}

int __gallium_free(struct galt_handler_gallium_s *ga)
{
    return 0;
}

void __timer_handle(int sig, siginfo_t *si, void *ucontext)
{
    galt_handler_t *hdl = si->si_ptr;
    hdl->timer.counter++;
}


void *galt_get_udata  (galt_handler_t *hdl)
{
    return hdl->userdata;
}

int   galt_set_timer  (galt_handler_t *hdl, double seconds)
{
    if (hdl->timer.tmstatus == 0)
    {
        struct sigevent  sigev;
        sigev.sigev_notify = SIGEV_SIGNAL;
        sigev.sigev_signo  = SIGALRM;
        sigev.sigev_value.sival_ptr = hdl;
        timer_create(CLOCK_REALTIME, &sigev, &hdl->timer.tmid);

        struct sigaction sigact;
        sigact.sa_flags = SA_SIGINFO;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_sigaction = __timer_handle;
        sigaction(SIGALRM, &sigact, NULL);

        hdl->timer.tmstatus = 1;
    }

    double ipart, fpart;
    struct itimerspec tmval;

    fpart = modf(seconds, &ipart);
    tmval.it_value.tv_sec  = (uint64_t)ipart;
    fpart = modf(fpart * 1000000000, &ipart);
    tmval.it_value.tv_nsec = (uint64_t)ipart;
    tmval.it_interval = tmval.it_value;

    timer_settime(hdl->timer.tmid, 0, &tmval, NULL);

    return 0;
}

int   galt_redraw  (galt_handler_t *hdl)
{
    Window     wnoop;
    static int w_width = -1, w_height = -1;
    int        noop;

    XGetGeometry(hdl->xlib.display, hdl->xlib.win,
                 &wnoop, &noop, &noop,
                 &hdl->xlib.w_width, &hdl->xlib.w_height,
                 &noop, &noop);

    if ((hdl->xlib.w_width != w_width) || (hdl->xlib.w_height != w_height))
    {
        static struct pipe_resource          *target;
        static struct pipe_framebuffer_state  framebuffer;
        static struct pipe_surface            surf_tmpl;

        /* attach a new drawable */
        __dri2_attach_drawable(&hdl->dri2, hdl->xlib.win);

        if (w_width != -1)
        {
            /* delete previous fb */
            pipe_surface_release(hdl->gallium.pipe, &framebuffer.cbufs[0]);
            pipe_resource_reference(&target, NULL);
        }

        /* render target texture */
        {
            struct winsys_handle dri2_handle;
            struct pipe_resource tmplt;

            memset(&dri2_handle, 0, sizeof(dri2_handle));
            dri2_handle.type = DRM_API_HANDLE_TYPE_SHARED;
            dri2_handle.handle = hdl->dri2.buffer_left->name;
            dri2_handle.stride = hdl->dri2.buffer_left->pitch;

            memset(&tmplt, 0, sizeof(tmplt));
            tmplt.target = PIPE_TEXTURE_2D;
            tmplt.format = PIPE_FORMAT_B8G8R8A8_UNORM; /* All drivers support this */
            tmplt.width0 = hdl->xlib.w_width;
            tmplt.height0 = hdl->xlib.w_height;
            tmplt.depth0 = 1;
            tmplt.array_size = 1;
            tmplt.last_level = 0;
            tmplt.bind = PIPE_BIND_RENDER_TARGET;

            target = hdl->gallium.screen->resource_from_handle(hdl->gallium.screen, &tmplt, &dri2_handle);
        }

        surf_tmpl.format = PIPE_FORMAT_B8G8R8A8_UNORM;
        surf_tmpl.u.tex.level = 0;
        surf_tmpl.u.tex.first_layer = 0;
        surf_tmpl.u.tex.last_layer = 0;

        /* drawing destination */
        memset(&framebuffer, 0, sizeof(framebuffer));
        framebuffer.width    = hdl->xlib.w_width;
        framebuffer.height   = hdl->xlib.w_height;
        framebuffer.nr_cbufs = 1;
        framebuffer.cbufs[0] = hdl->gallium.pipe->create_surface(hdl->gallium.pipe, target, &surf_tmpl);

        /* set the render target */
        cso_set_framebuffer(hdl->gallium.cso, &framebuffer);
    }

    galt_context_t ctx;
    ctx.cso  = hdl->gallium.cso;
    ctx.pipe = hdl->gallium.pipe;

    if (hdl->events.event_window != NULL)
        hdl->events.event_window(hdl, &ctx, hdl->xlib.w_width, hdl->xlib.w_height);


    if ((hdl->xlib.w_width != w_width) || (hdl->xlib.w_height != w_height))
    {
        w_width = hdl->xlib.w_width;
        w_height = hdl->xlib.w_height;
    }

    hdl->dri2.swap_c = xcb_dri2_swap_buffers_unchecked(hdl->dri2.conn,
                                                       hdl->dri2.drawable,
                                                       hdl->dri2.cnt.msc >> 32,
                                                       hdl->dri2.cnt.msc & 0xFFFFFFFF, 0, 0, 0, 0);
    hdl->dri2.wait_sbc_c = xcb_dri2_wait_sbc_unchecked(hdl->dri2.conn,
                                                       hdl->dri2.drawable,
                                                       hdl->dri2.cnt.sbc >> 32,
                                                       hdl->dri2.cnt.sbc & 0xFFFFFFFF);
    hdl->dri2.wait_msc_c = xcb_dri2_wait_msc_unchecked(hdl->dri2.conn,
                                                       hdl->dri2.drawable,
                                                       hdl->dri2.cnt.msc >> 32,
                                                       hdl->dri2.cnt.msc & 0xFFFFFFFF, 0, 0, 0, 0);

    xcb_dri2_swap_buffers_reply_t *d2swap_r     = xcb_dri2_swap_buffers_reply(hdl->dri2.conn, hdl->dri2.swap_c, NULL);
    xcb_dri2_wait_sbc_reply_t     *d2wait_sbc_r = xcb_dri2_wait_sbc_reply(hdl->dri2.conn, hdl->dri2.wait_sbc_c, NULL);
    xcb_dri2_wait_msc_reply_t     *d2wait_msc_r = xcb_dri2_wait_msc_reply(hdl->dri2.conn, hdl->dri2.wait_msc_c, NULL);
    hdl->dri2.cnt.msc = (uint64_t)d2wait_msc_r->msc_hi << 32 | d2wait_msc_r->msc_lo;
    hdl->dri2.cnt.sbc = (uint64_t)d2wait_msc_r->sbc_hi << 32 | d2wait_msc_r->sbc_lo;
    hdl->dri2.cnt.ust = (uint64_t)d2wait_msc_r->ust_hi << 32 | d2wait_msc_r->ust_lo;
    free(d2swap_r);
    free(d2wait_sbc_r);
    free(d2wait_msc_r);

    return 0;
}

int   galt_open_window(galt_window_t  *window)
{
    galt_handler_t *hdl;
    int             ret;

    __handler_init(&hdl, window);

    hdl->xlib.display = XOpenDisplay(getenv("DISPLAY"));
    if (hdl->xlib.display == NULL)
    {
        //__handler_free(hdl);
        return -1; // can't open display
    }
    hdl->xlib.socket = ConnectionNumber(hdl->xlib.display);
    hdl->xlib.nscreen = DefaultScreen(hdl->xlib.display);
    hdl->xlib.win = XCreateSimpleWindow(
                        hdl->xlib.display,
                        RootWindow(hdl->xlib.display, hdl->xlib.nscreen),
                        0, 0,
                        hdl->xlib.w_width, hdl->xlib.w_height,
                        0,
                        BlackPixel(hdl->xlib.display, hdl->xlib.nscreen),
                        WhitePixel(hdl->xlib.display, hdl->xlib.nscreen));
    if (hdl->xlib.w_name != NULL)
        XStoreName(hdl->xlib.display, hdl->xlib.win, hdl->xlib.w_name);

    ret = __dri2_connect(&hdl->dri2, hdl->xlib.display, hdl->xlib.nscreen);
    if (ret != 0)
    {
        //__handler_free(hdl);
        return ret;
    }
    __gallium_init(&hdl->gallium);

    XMapWindow(hdl->xlib.display, hdl->xlib.win);
    XSelectInput (hdl->xlib.display, hdl->xlib.win, ExposureMask | KeyPressMask | ButtonPressMask);

    if (window->t_delay != 0)
        galt_set_timer(hdl, window->t_delay);

    while (1)
    {

        fd_set in_fds;
        FD_ZERO(&in_fds);
        FD_SET(hdl->xlib.socket, &in_fds);

        struct timeval tmval;
        tmval.tv_sec = 10;
        tmval.tv_usec = 0;

        while(XPending(hdl->xlib.display))
        {
            XNextEvent(hdl->xlib.display, &hdl->xlib.event);
            switch(hdl->xlib.event.type)
            {
            case Expose:
                printf("redraw\n");
                galt_redraw(hdl);
                break;
            case ButtonPress:
                printf("button press\n");
                break;
            case KeyPress:
                printf("key press\n");
                break;
            }
        }

        select(hdl->xlib.socket+1, &in_fds, 0, 0, &tmval);
        while (hdl->timer.counter > 0)
        {
            if (hdl->events.event_timer != NULL)
                hdl->events.event_timer(hdl);
            hdl->timer.counter=0;
        }

    }
    return 0;
}
