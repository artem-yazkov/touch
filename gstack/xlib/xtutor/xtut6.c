#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XrmOptionDescRec xrmTable[] = {
	{"-bg", "*background", XrmoptionSepArg, NULL},
	{"-fg", "*foreground", XrmoptionSepArg, NULL},
	{"-bc", "*bordercolour", XrmoptionSepArg, NULL},
	{"-font", "*font", XrmoptionSepArg, NULL},
};

unsigned long getColour(Display *dpy,  XrmDatabase db, char *name,
			char *cl, char *def){
	XrmValue v;
	XColor col1, col2;
	Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
	char * type;

	if (XrmGetResource(db, name, cl, &type, &v)
			&& XAllocNamedColor(dpy, cmap, v.addr, &col1, &col2)) {
	} else {
		XAllocNamedColor(dpy, cmap, def, &col1, &col2);
	}
	return col2.pixel;
}

XFontStruct *getFont(Display *dpy, XrmDatabase db, char *name,
		char *cl, char *def){
	XrmValue v;
	char * type;
	XFontStruct *font = NULL;

	if (XrmGetResource(db, name, cl, &type, &v)){
		if (v.addr)
			font = XLoadQueryFont(dpy, v.addr);
	}
	if (!font) {
		if (v.addr)
		fprintf(stderr, "unable to load preferred font: %s using fixed\n", v.addr);
		else 
		fprintf(stderr, "unable to find preferred font\n");
		font = XLoadQueryFont(dpy, def);
	}
	return font;
}


GC setup(Display * dpy, int argc, char ** argv, int *width_r, int *height_r,
		XFontStruct **font_r){
	int width, height;
	unsigned long background, border;
	Window win;
	GC pen;
	XGCValues values;

	XFontStruct *font;
	XrmDatabase db;

	XrmInitialize();
	db = XrmGetDatabase(dpy);
	XrmParseCommand(&db, xrmTable, sizeof(xrmTable)/sizeof(xrmTable[0]),
		"xtut6", &argc, argv);

	background = getColour(dpy,  db, "xtut6.background", "xtut6.BackGround", "DarkGreen");
	border = getColour(dpy,  db, "xtut6.border", "xtut6.Border", "LightGreen");
	values.foreground = getColour(dpy,  db, "xtut6.foreground", "xtut6.ForeGround", "Red");
	font = getFont(dpy, db, "xtut6.font", "xtut6.Font", "fixed");


	width = 400;
	height = 400;

	win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), /* display, parent */
		0,0, /* x, y: the window manager will place the window elsewhere */
		width, height, /* width, height */
		2, border, /* border width & colour, unless you have a window manager */
		background); /* background colour */

	/* create the pen to draw lines with */
	values.line_width = 1;
	values.line_style = LineSolid;
	values.font = font->fid;
	pen = XCreateGC(dpy, win, GCForeground|GCLineWidth|GCLineStyle|GCFont,&values);

	/* tell the display server what kind of events we would like to see */
	XSelectInput(dpy, win, ButtonPressMask|ButtonReleaseMask|StructureNotifyMask|ExposureMask);

	/* okay, put the window on the screen, please */
	XMapWindow(dpy, win);

	*width_r = width; *height_r = height;
	*font_r = font;

	return pen;
}

int main_loop(Display *dpy, XFontStruct *font, GC pen, int width, int height,
		 char *text){
	int text_width;
	int textx, texty;
	XEvent ev;

	text_width = XTextWidth(font, text, strlen(text));

	/* as each event that we asked about occurs, we respond. */
	while(1){
		XNextEvent(dpy, &ev);
		switch(ev.type){
		case Expose:
			XDrawLine(dpy, ev.xany.window, pen, 0, 0, width/2-text_width/2, height/2);
			XDrawLine(dpy, ev.xany.window, pen, width, 0, width/2+text_width/2, height/2);
			XDrawLine(dpy, ev.xany.window, pen, 0, height, width/2-text_width/2, height/2);
			XDrawLine(dpy, ev.xany.window, pen, width, height, width/2+text_width/2, height/2);
   			textx = (width - text_width)/2;
   			texty = (height + font->ascent)/2;
   			XDrawString(dpy, ev.xany.window, pen, textx, texty, text, strlen(text));
			break;
		case ConfigureNotify:
			if (width != ev.xconfigure.width
					|| height != ev.xconfigure.height) {
				width = ev.xconfigure.width;
				height = ev.xconfigure.height;
				XClearWindow(dpy, ev.xany.window);
				printf("Size changed to: %d by %d\n", width, height);
			}
			break;
		case ButtonRelease:
			XCloseDisplay(dpy);
			return 0;
		}
	}
}

int main(int argc, char ** argv){
	int width, height;
	Display *dpy;
	GC pen;
	XFontStruct *font;

	/* First connect to the display server */
	dpy = XOpenDisplay(NULL);
	if (!dpy) {fprintf(stderr, "unable to connect to display\n");return 7;}
	pen = setup(dpy, argc, argv, &width, &height, &font);
	return main_loop(dpy, font, pen, width, height, "Hi!");
}
