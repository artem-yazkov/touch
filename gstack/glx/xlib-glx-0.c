/*
 * simple-window.c - demonstrate creation of a simple window.
 */

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
main(int argc, char* argv[])
{
  Display* display;    /* pointer to X Display structure.           */
  int screen_num;      /* number of screen to place the window on.  */
  Window win;          /* pointer to the newly created window.      */

  XVisualInfo *visinfo;
  GLXContext  glxctx;
  int visattr[] = { 
                    GLX_RGBA,
                    GLX_DOUBLEBUFFER,
                    None 
                  };

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


  visinfo = glXChooseVisual(display, screen_num, visattr);
  if (visinfo == NULL) {
    fprintf(stderr, "can't choose visual\n");
    exit(1);
  }

  glxctx = glXCreateContext(display, visinfo, NULL, True);
  if (!glxctx) {
    fprintf(stderr, "can't create glx context\n");
    exit(1);
  }
  
  glXMakeCurrent(display, win, glxctx);

  /* make the window actually appear on the screen. */
  XMapWindow(display, win);

  /* flush all pending requests to the X server, and wait until */
  /* they are processed by the X server.                        */
  XSync(display, False);
  
  glXSwapBuffers(display, win);

  /* make a delay for a short period. */
  pause();

  /* close the connection to the X server. */
  XCloseDisplay(display);
}

