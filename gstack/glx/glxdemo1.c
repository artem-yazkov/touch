

/*
 * A demonstration of using the GLX functions.  This program is in the
 * public domain.
 *
 * Brian Paul
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static Display *dpy;
static Window   win;

float _x1 =  0,    _y1 = 0.25;
float _x2 = -0.2,  _y2 = -0.2;
float _x3 =  0.2,  _y3 = -0.2;

float angle = 0.0;

static void redraw( Display *dpy, Window w )
{
   glClear( GL_COLOR_BUFFER_BIT );

   glColor3f( 1.0, 1.0, 0.0 );
   
   glBegin(GL_TRIANGLES);
   glVertex3f( _x1 + 0.6*sin(angle), _y1 + 0.6*cos(angle), 0.0f);
   glVertex3f( _x2 + 0.6*sin(angle), _y2 + 0.6*cos(angle), 0.0f);
   glVertex3f( _x3 + 0.6*sin(angle), _y3 + 0.6*cos(angle), 0.0f);
   glEnd(); 
   
   glXSwapBuffers( dpy, w );
}



static void resize( unsigned int width, unsigned int height )
{
   glViewport( 0, 0, width, height );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glOrtho( -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 );
}



static Window make_rgb_db_window( Display *dpy,
				  unsigned int width, unsigned int height )
{
   int attrib[] = { GLX_RGBA,
		    GLX_RED_SIZE, 1,
		    GLX_GREEN_SIZE, 1,
		    GLX_BLUE_SIZE, 1,
		    GLX_DOUBLEBUFFER,
		    None };
   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   GLXContext ctx;
   XVisualInfo *visinfo;

   scrnum = DefaultScreen( dpy );
   root = RootWindow( dpy, scrnum );

   visinfo = glXChooseVisual( dpy, scrnum, attrib );
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

   return win;
}


static void event_loop( Display *dpy )
{
   XEvent event;

   while (XPending(dpy))
      XNextEvent( dpy, &event );
   while(1)
   {
       //float angle = 0;
       
       resize(900, 900 );
       redraw( dpy, win );
       struct timespec tm = { 0, 1000000000 / 60 }; // 1 sec
       nanosleep(&tm, NULL);
       angle += 0.01;

   }
   sleep(10);

}



int main( int argc, char *argv[] )
{

   dpy = XOpenDisplay(NULL);

   win = make_rgb_db_window( dpy, 900, 900 );

   glShadeModel( GL_FLAT );
   glClearColor( 0.5, 0.5, 0.5, 1.0 );

   XMapWindow( dpy, win );

   event_loop( dpy );
   return 0;
}
