/*  gcc ./xlib-dri2-0.c -o ./xlib-dri2-0 -lxcb -lxcb-dri2  -lX11 -lX11-xcb -ldrm -I/usr/include/libdrm
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/dri2.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xf86drm.h>

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

    int         drm_fd;
    drm_magic_t drm_magic;

} dri2_t;

static const unsigned int attachments[1] = { XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT };

int dri2_create(dri2_t *dri2, Display *display, int screen);

int dri2_hook_drawable(dri2_t *dri2, Drawable drawable);

void main(int argc, char* argv[])
{
  Display* display;    /* pointer to X Display structure.           */
  int screen_num;      /* number of screen to place the window on.  */
  Window win;          /* pointer to the newly created window.      */

  dri2_t dri2;

  char *display_name = getenv("DISPLAY");  /* address of the X display.      */

  display = XOpenDisplay(display_name);
  if (display == NULL) {
    fprintf(stderr, "%s: cannot connect to X server '%s'\n",
            argv[0], display_name);
    exit(1);
  }

  /* get the geometry of the default screen for our display. */
  screen_num = DefaultScreen(display);

  /* create a simple window, as a direct child of the screen's   */
  /* root window. Use the screen's white color as the background */
  /* color of the window. Place the new window's top-left corner */
  /* at the given 'x,y' coordinates.                             */
  win = XCreateSimpleWindow(display,                                       /* connection          */
                            RootWindow(display, screen_num),               /* parent window       */
                            0, 0,                                          /* x, y                */
                            300, 300,                                      /* width, height       */
                            10,                                            /* border width        */
                            BlackPixel(display, screen_num),
                            WhitePixel(display, screen_num));

  dri2_create(&dri2, display, screen_num);
  
  /* make the window actually appear on the screen. */
  XMapWindow(display, win);

  dri2_hook_drawable(&dri2, win);

  /* flush all pending requests to the X server, and wait until */
  /* they are processed by the X server.                        */
  XSync(display, False);

  /* make a delay for a short period. */
  pause();

  /* close the connection to the X server. */
  XCloseDisplay(display);
}

int dri2_create(dri2_t *dri2, Display *display, int screen)
{
    memset(dri2, 0, sizeof(dri2_t));

    /* get xcb connection from display */
    if ((dri2->xconn = XGetXCBConnection(display)) == NULL)
    {
        XCloseDisplay(display);
        fprintf(stderr, "Can't get xcb connection from display\n");
        return -1;
    }

#if 0
    /* check if dri2 extension is available */
    xcb_prefetch_extension_data(dri2->xconn, dri2->xext);
    dri2->xext_r = xcb_get_extension_data(dri2->xconn, dri2->xext);
    if (!(dri2->xext_r && dri2->xext_r->present))
    {
        fprintf(stderr, "Can't obtain xcb query extension data\n");
        return -1;
    }
#endif

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

int dri2_hook_drawable(dri2_t *dri2, Drawable drawable)
{
    xcb_get_geometry_cookie_t     xgeometry_c;
    xcb_get_geometry_reply_t     *xgeometry_r;

    xcb_dri2_wait_sbc_reply_t    *d2wait_r    = NULL;
    xcb_dri2_get_buffers_reply_t *d2buffers_r = NULL;
    xcb_dri2_dri2_buffer_t       *d2buffers, *d2buffer_left;
    int i;

    xcb_dri2_create_drawable(dri2->xconn, drawable);
#if 0
    xgeometry_c = xcb_get_geometry_unchecked (dri2->xconn, drawable);
    xgeometry_r = xcb_get_geometry_reply (dri2->xconn, xgeometry_c, NULL);
    if (!xgeometry_r)
    {
        fprintf(stderr, "can't get drawable geometry\n");
    } else
    {
        fprintf(stdout, "drawable geometry: %dx%d\n", xgeometry_r->width, xgeometry_r->height);
    }
#endif
#if 0
    free(xcb_dri2_swap_buffers_reply(dri2->xconn, dri2->d2swap_c, NULL));

    d2wait_r = xcb_dri2_wait_sbc_reply(dri2->xconn, dri2->d2wait_c, NULL);
    if (!d2wait_r)
    {
        fprintf(stderr, "can't wait sbc reply\n");
        //return -1;
    } else
        free(d2wait_r);
#endif

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
/*
    d2buffers = xcb_dri2_get_buffers_buffers(d2buffers_r);
    if (!d2buffers)
    {
       fprintf(stderr, "can't get buffers\n");
       return -1;
    }

    for (i = 0; i < d2buffers_r->count; ++i)
    {
       if (d2buffers[i].attachment == XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT)
       {
          d2buffer_left = &d2buffers[i];
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
*/
    dri2->d2swap_c = xcb_dri2_swap_buffers_unchecked(dri2->xconn, drawable, 0, 0, 0, 0, 0, 0);
    free(xcb_dri2_swap_buffers_reply(dri2->xconn, dri2->d2swap_c, NULL));


    return 0;
}




