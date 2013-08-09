#include "utl.h"


int utl_window_init(utl_window_t *window)
{
    int blackColor,whiteColor;

    window->display  = XOpenDisplay(NULL);
    blackColor    = BlackPixel(window->display, DefaultScreen(window->display));
    whiteColor    = WhitePixel(window->display, DefaultScreen(window->display));
    window->screen   = DefaultScreen(window->display);

    window->window = XCreateSimpleWindow(
            window->display,
            DefaultRootWindow(window->display), 0, 0,
            window->width, window->height, 0, whiteColor, blackColor);

    XSelectInput(window->display, window->window, StructureNotifyMask);
    XMapWindow(window->display, window->window);

    return 0;
}


int utl_window_attach_surface(utl_window_t *window,
                              struct pipe_context *context,
                              struct pipe_surface *surface)
{
    struct pipe_transfer *transfer;
    void *ptr = pipe_transfer_map(
                    context,
                    surface->texture,
                    surface->u.tex.level,
                    surface->u.tex.first_layer,
                    PIPE_TRANSFER_READ,
                    0,
                    0,
                    surface->width,
                    surface->height,
                    &transfer
                );
    float *rgba;
    ubyte *image32, *img;
    int x, y;

    rgba = malloc(
                    transfer->box.width *
                    transfer->box.height *
                    transfer->box.depth *
                    4*sizeof(float));
    pipe_get_tile_rgba(
                        transfer, ptr, 0, 0,
                        transfer->box.width, transfer->box.height,
                        rgba
                    );
    image32=(ubyte *)malloc(transfer->box.width * transfer->box.height *4);

    img = image32;
    for(y = 0; y < transfer->box.height; y++)
    {
        float *ptr = rgba + (transfer->box.width * y * 4);
        for(x = 0; x < transfer->box.width; x++)
        {
            *img++ = float_to_ubyte(ptr[x*4 + 2]); // blue
            *img++ = float_to_ubyte(ptr[x*4 + 1]); // green
            *img++ = float_to_ubyte(ptr[x*4 + 0]); // red
            *img++ = float_to_ubyte(ptr[x*4 + 3]); // alpha
        }
    }
    free(rgba);
    context->transfer_unmap(context, transfer);

    window->image = XCreateImage
            (
                window->display,
                DefaultVisual(window->display, 0),
                24,
                ZPixmap,
                0,
                (char *)image32,
                transfer->box.width,
                transfer->box.height,
                32,
                0
            );

    return 0;
}

int utl_window_nextevent (utl_window_t *window)
{
    XNextEvent(window->display, &window->event);
    XPutImage(window->display,
              window->window,
              DefaultGC(window->display, 0),
              window->image, 0, 0, 0, 0,
              (unsigned int)window->image->width,
              (unsigned int)window->image->height);

    return 0;
}
