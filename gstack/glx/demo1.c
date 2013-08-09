/*
 *  gcc ./demo1.c  -o ./demo1 -lGL -lX11
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] )
{
   Display              *dpy = XOpenDisplay(NULL);
   Window                root;
   Window                win;
   GLXContext            ctx;
   XVisualInfo          *visinfo;
   XSetWindowAttributes  attr;

   const int width  = 300;
   const int height = 300; 
   
   int attrib[] = { 
                    GLX_RGBA,
		            GLX_RED_SIZE, 1,
		            GLX_GREEN_SIZE, 1,
		            GLX_BLUE_SIZE, 1,
		            GLX_DOUBLEBUFFER,
		            None 
		          };

   int scrnum;
   unsigned long mask;

   scrnum = DefaultScreen( dpy );
   root = RootWindow( dpy, scrnum );

   visinfo = glXChooseVisual( dpy, scrnum, attrib );
   //visinfo = DefaultVisual(dpy, scrnum);
   if (!visinfo) {
      printf("Error: couldn't get an RGB, Double-buffered visual\n");
      exit(1);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;

   attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask;
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( dpy, root, 0, 0, width, height,
		                0, visinfo->depth, InputOutput,
		                visinfo->visual, mask, &attr );


   ctx = glXCreateContext( dpy, visinfo, NULL, True );
   if (!ctx) {
      printf("Error: glXCreateContext failed\n");
      exit(1);
   }

   glXMakeCurrent( dpy, win, ctx );
   XMapWindow( dpy, win );



   XEvent event;
   while (1) {
      XNextEvent( dpy, &event );

      switch (event.type) {
	  case Expose:
	       //printf("swap byffers\n");
           //glClear( GL_COLOR_BUFFER_BIT );
	       glXSwapBuffers( dpy, event.xany.window );
	       break;
	  case ConfigureNotify:
	       break;
      }
   }

   //sleep(10);
   return 0;
}

