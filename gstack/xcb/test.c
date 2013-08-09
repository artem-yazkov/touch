// compile with: mesa.system gcc ./test.c -o ./test -std=c99 -lGL -lX11 -lX11-xcb -lxcb

#include <GL/glx.h>
#include <stdlib.h>
#include <X11/Xlib-xcb.h>

#define GLXATTRIBS                     \
  GLX_RED_SIZE,        8,              \
    GLX_GREEN_SIZE,    8,              \
    GLX_BLUE_SIZE,     8,              \
    GLX_ALPHA_SIZE,    8,              \
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,   \
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, \
    GLX_X_RENDERABLE,  True,           \
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, \
    None

int main(int argc, char* argv[])
{
  // open display
  Display *display = XOpenDisplay(0);
  if(!display) exit(1);

  // get xcb connection and make it the event queue owner
  xcb_connection_t *connection = XGetXCBConnection(display);
  if(!connection) exit(1);
  XSetEventQueueOwner(display, XCBOwnsEventQueue);

  // get default screen
  int default_screen = DefaultScreen(display);
  xcb_screen_iterator_t iter
    = xcb_setup_roots_iterator(xcb_get_setup(connection));
  for(int c = 0; c < default_screen; c++)
    xcb_screen_next(&iter);
  xcb_screen_t *screen = iter.data;

  // choose appropriete framebuffer configuration
  int count;
  int doubleBufferAttributes[] = { GLX_DOUBLEBUFFER, True, GLXATTRIBS };
  GLXFBConfig *configs = glXChooseFBConfig(display, default_screen,
                                           doubleBufferAttributes, &count);
  if(!configs)
    {
      int singleBufferAttributess[] = { GLX_DOUBLEBUFFER, False, GLXATTRIBS };
      configs = glXChooseFBConfig(display, default_screen,
                                  singleBufferAttributess, &count);
      if(!configs) exit(1);
    }
  
  // get visual id
  int visualid = screen->root_visual;
  glXGetFBConfigAttrib(display, configs[0], GLX_VISUAL_ID, &visualid);
  
  // create glx context
  GLXContext context
    = glXCreateNewContext(display, configs[0], GLX_RGBA_TYPE, 0, True);
  if(!context) exit(1);

  // create colormap
  xcb_colormap_t colormap = xcb_generate_id(connection);
  xcb_void_cookie_t cookie =
    xcb_create_colormap_checked(connection, XCB_COLORMAP_ALLOC_NONE, colormap,
                                screen->root, visualid);
  xcb_generic_error_t *error = xcb_request_check(connection, cookie);
  if(error) exit(1);

  // create window
  xcb_window_t win = xcb_generate_id(connection);
  int width = 640;
  int height = 480;
  uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
  uint32_t eventmask = XCB_EVENT_MASK_EXPOSURE
    | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
  uint32_t values[] = { eventmask, colormap, 0 };
  cookie = xcb_create_window_checked(connection, XCB_COPY_FROM_PARENT, win,
                                     screen->root, 0, 0, width, height, 0,
                                     XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                     visualid, mask, values);
  error = xcb_request_check(connection, cookie);
  if(error) exit(1);

  // make context current
  if(!glXMakeCurrent(display, win, context)) exit(1);

  // configure OpenGL for 2D drawing
  glDepthFunc(GL_ALWAYS);
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);

  // show window
  cookie = xcb_map_window_checked(connection, win);
  error = xcb_request_check(connection, cookie);
  if(error) exit(1);
  xcb_flush(connection);

  // event loop
  xcb_generic_event_t *e;
  while((e = xcb_wait_for_event(connection)))
    {
      switch(e->response_type & ~0x80)
        {
        case XCB_EXPOSE: // redraw
          {
            glClearColor(1.0, 1.0, 1.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glColor3f(0.0, 0.0, 0.0);
            glBegin(GL_LINE_LOOP);
            glVertex2f(10, 10);
            glVertex2f(width-10, 10);
            glVertex2f(width-10, height-10);
            glVertex2f(10, height-10);
            glEnd();
            glXSwapBuffers(display, win);
            break;
          }
        case XCB_CONFIGURE_NOTIFY: // resize
          {
            xcb_configure_notify_event_t *event
              = (xcb_configure_notify_event_t*)e;
            width = event->width;
            height = event->height;
            glViewport(0, 0, width, height);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, width, height, 0, -1, 1);
            break;
          }
        default:
          break;
        }
      free(e);
    }

  // clean up
  glXDestroyContext(display, context);
  cookie = xcb_destroy_window_checked(connection, win);
  error = xcb_request_check(connection, cookie);
  if(error) exit(1);
  XCloseDisplay(display);

  return 0;
}
