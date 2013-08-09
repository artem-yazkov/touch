#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

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

XFontSet getFont(Display *dpy, XrmDatabase db, char *name,
		char *cl, char *def){
	XrmValue v;
	char * type;
	XFontSet font = NULL;
	int nmissing;
	char **missing;
	char *def_string;

	if (XrmGetResource(db, name, cl, &type, &v)){
		if (v.addr)
			font = XCreateFontSet(dpy, v.addr, &missing, &nmissing, &def_string);
	}
	if (!font) {
		if (v.addr)
		fprintf(stderr, "unable to load preferred font: %s using fixed\n", v.addr);
		else 
		fprintf(stderr, "couldn't figure out preferred font\n");
		font = XCreateFontSet(dpy, def, &missing, &nmissing, &def_string);
	}
	XFreeStringList(missing);
	return font;
}


GC setup(Display * dpy, int argc, char ** argv, int *width_r, int *height_r,
		XFontSet *font_r){
	int width, height;
	unsigned long background, border;
	Window win;
	GC pen;
	XGCValues values;

	XFontSet font;
	XrmDatabase db;

	XrmInitialize();
	db = XrmGetDatabase(dpy);
	XrmParseCommand(&db, xrmTable, sizeof(xrmTable)/sizeof(xrmTable[0]),
		"xtut7", &argc, argv);

	font = getFont(dpy, db, "xtut7.font", "xtut7.Font", "fixed");
	background = getColour(dpy,  db, "xtut7.background", "xtut7.BackGround", "DarkGreen");
	border = getColour(dpy,  db, "xtut7.border", "xtut7.Border", "LightGreen");
	values.foreground = getColour(dpy,  db, "xtut7.foreground", "xtut7.ForeGround", "Red");


	width = 400;
	height = 400;

	win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), /* display, parent */
		0,0, /* x, y: the window manager will place the window elsewhere */
		width, height, /* width, height */
		2, border, /* border width & colour, unless you have a window manager */
		background); /* background colour */

	Xutf8SetWMProperties(dpy, win, "XTut7", "xtut7", argv, argc,
		NULL, NULL, NULL);

	/* create the pen to draw lines with */
	values.line_width = 1;
	values.line_style = LineSolid;
	/*values.font = font->fid; */
	pen = XCreateGC(dpy, win, GCForeground|GCLineWidth|GCLineStyle,&values);

	/* tell the display server what kind of events we would like to see */
	XSelectInput(dpy, win, ButtonPressMask|ButtonReleaseMask|StructureNotifyMask|ExposureMask);

	/* okay, put the window on the screen, please */
	XMapWindow(dpy, win);

	*width_r = width; *height_r = height;
	*font_r = font;

	return pen;
}

int main_loop(Display *dpy, XFontSet font, GC pen, int width, int height,
		 char *text){
	int text_width;
	int textx, texty;
	XEvent ev;
	int font_ascent;
	XFontStruct **fonts;
	char **font_names;
	int nfonts;
	int j;

	printf("%s:%d\n", text, strlen(text));
	text_width = Xutf8TextEscapement(font, text, strlen(text));
	font_ascent = 0;
	nfonts = XFontsOfFontSet(font, &fonts, &font_names);
	for(j = 0; j < nfonts; j += 1){
		if (font_ascent < fonts[j]->ascent) font_ascent = fonts[j]->ascent;
		printf("Font: %s\n", font_names[j]);
	}


	/* as each event that we asked about occurs, we respond. */
	while(1){
		XNextEvent(dpy, &ev);
		switch(ev.type){
		case Expose:
			if (ev.xexpose.count > 0) break;
			XDrawLine(dpy, ev.xany.window, pen, 0, 0, width/2-text_width/2, height/2);
			XDrawLine(dpy, ev.xany.window, pen, width, 0, width/2+text_width/2, height/2);
			XDrawLine(dpy, ev.xany.window, pen, 0, height, width/2-text_width/2, height/2);
			XDrawLine(dpy, ev.xany.window, pen, width, height, width/2+text_width/2, height/2);
   			textx = (width - text_width)/2;
   			texty = (height + font_ascent)/2;
   			Xutf8DrawString(dpy, ev.xany.window, font, pen, textx, texty, text, strlen(text));
			break;
		case ConfigureNotify:
			if (width != ev.xconfigure.width
					|| height != ev.xconfigure.height) {
				width = ev.xconfigure.width;
				height = ev.xconfigure.height;
				XClearWindow(dpy, ev.xany.window);
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
	XFontSet font;
	char *text = "Hello World ñ! ";
	setlocale(LC_ALL, getenv("LANG"));

	/* First connect to the display server */
	dpy = XOpenDisplay(NULL);
	if (!dpy) {fprintf(stderr, "unable to connect to display\n");return 7;}
	pen = setup(dpy, argc, argv, &width, &height, &font);
	if (argv[1] && argv[1][0]) text = argv[1];
	return main_loop(dpy, font, pen, width, height, text);
}
