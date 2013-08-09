/*  gcc ./xlib-dri2-0.c -o ./xlib-dri2-0 -lxcb -lxcb-dri2  -lX11 -lX11-xcb -ldrm -I/usr/include/libdrm
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xf86drm.h>

int dri2_screen_create(Display *display, int screen);

void main(int argc, char* argv[])
{
  Display* display;    /* pointer to X Display structure.           */
  int screen_num;      /* number of screen to place the window on.  */
  Window win;          /* pointer to the newly created window.      */

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

  dri2_screen_create(display, screen_num);
  
  /* make the window actually appear on the screen. */
  XMapWindow(display, win);

  /* flush all pending requests to the X server, and wait until */
  /* they are processed by the X server.                        */
  XSync(display, False);

  /* make a delay for a short period. */
  pause();

  /* close the connection to the X server. */
  XCloseDisplay(display);
}

int dri2_screen_create(Display *display, int screen)
{
    xcb_connection_t                  *xcb_conn;
    xcb_extension_t                   *xcb_ext;
    const xcb_query_extension_reply_t *xcb_query_ext;
    xcb_screen_iterator_t              xcb_screen_it;
    xcb_generic_error_t               *xcb_error = NULL;

    xcb_dri2_query_version_cookie_t    dri2_version_ck;
    xcb_dri2_query_version_reply_t    *dri2_version = NULL;
    xcb_dri2_connect_cookie_t          dri2_conn_ck;
    xcb_dri2_connect_reply_t          *dri2_conn = NULL;
    xcb_dri2_authenticate_cookie_t     dri2_auth_ck;
    xcb_dri2_authenticate_reply_t     *dri2_auth = NULL;

    int         drm_fd;
    drm_magic_t drm_magic;

    char *device_name;
    int fd, device_name_length;
    unsigned int driverType;

    drm_magic_t magic;

    /* get xcb connection from display */
    if ((xcb_conn = XGetXCBConnection(display)) == NULL)
    {
        XCloseDisplay(display);
        fprintf(stderr, "Can't get xcb connection from display\n");
        return -1;
    }

#if 0
    /* check if dri2 extension is available */
    xcb_prefetch_extension_data(xcb_conn, xcb_ext);
    xcb_query_ext = xcb_get_extension_data(xcb_conn, xcb_ext);
    if (!(xcb_query_ext && xcb_query_ext->present))
    {
        fprintf(stderr, "Can't obtain xcb query extension data\n");
        return -1;
    }
#endif

    /* retrieve dri2 version */
    dri2_version_ck = xcb_dri2_query_version (xcb_conn, XCB_DRI2_MAJOR_VERSION, XCB_DRI2_MINOR_VERSION);
    dri2_version = xcb_dri2_query_version_reply (xcb_conn, dri2_version_ck, &xcb_error);
    if (dri2_version == NULL || xcb_error != NULL)
    {
        fprintf(stderr, "can't retrieve dri2 version\n");
        return -1;
    } else
        fprintf(stdout, "dri2 version: %d.%d\n", dri2_version->major_version, dri2_version->minor_version);

    /* choose right screen */
    xcb_screen_it = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn));
    while (screen--)
        xcb_screen_next(&xcb_screen_it);

    /* connect to dri2 */
    dri2_conn_ck = xcb_dri2_connect_unchecked(xcb_conn, xcb_screen_it.data->root, XCB_DRI2_DRIVER_TYPE_DRI);
    dri2_conn = xcb_dri2_connect_reply(xcb_conn, dri2_conn_ck, NULL);
    if (dri2_conn == NULL || dri2_conn->driver_name_length + dri2_conn->device_name_length == 0)
    {
        fprintf(stderr, "can't connect to dri2\n");
        return -1;
    } else
        fprintf(stdout, "dri2 device: %s\n", xcb_dri2_connect_device_name(dri2_conn));

    /* load drm */
    drm_fd = open(xcb_dri2_connect_device_name(dri2_conn), O_RDWR);
    if ((drm_fd < 0) || (drmGetMagic(drm_fd, &drm_magic) != 0))
    {
        fprintf(stderr, "can't retrieve drm magic from %s device\n", xcb_dri2_connect_device_name(dri2_conn));
        return -1;
    }

    dri2_auth_ck = xcb_dri2_authenticate_unchecked(xcb_conn, xcb_screen_it.data->root, drm_magic);
    dri2_auth    = xcb_dri2_authenticate_reply(xcb_conn, dri2_auth_ck, NULL);
    if (dri2_auth == NULL || !dri2_auth->authenticated)
    {
       fprintf(stderr, "can't authenticate with dri2\n");
       return -1;
    }

    free(dri2_auth);
    free(dri2_conn);
#if 0
    free(xcb_conn);
#endif

    return 0;
}
