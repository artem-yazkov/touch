#ifndef __UTL_H__
#define __UTL_H__

#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "util/u_inlines.h"
#include "util/u_tile.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

typedef struct utl_window_s
{
    int               width;
    int               height;

    Display          *display;
    int               screen;
    Window            window;
    XEvent            event;
    XImage           *image;

} utl_window_t;

int utl_window_init          (utl_window_t *window);
int utl_window_attach_surface(utl_window_t *window,
                              struct pipe_context *context,
                              struct pipe_surface *surface);

int utl_window_nextevent     (utl_window_t *window);

#endif /* __UTL_H__ */
