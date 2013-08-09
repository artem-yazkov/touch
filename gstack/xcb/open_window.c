/*******************************************************************************
= open_window(5) =
Ingo Krabbe <ingo.krabbe@eoa.de>
0.1, 21. October 2011

== NAME ==
open_window - functions and test routines to open a glX window.

== SYNOPSIS ==
 mesa.system gcc  -o open_window -lftgl -lev -lX11 -lGL -lxcb -lxcb-keysyms -lxcb-ewmh -lX11-xcb -I/usr/include/freetype2 ./open_window.c
 
== DESCRIPTION ==
A simple library and test code.

[source,c]
---------------
 ******************************************************************************/
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
extern long keysym2ucs(KeySym keysym);
#include "keysym2ucs.c"
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <FTGL/ftgl.h>

#ifndef USE_LIBEV
#define USE_LIBEV 1
#endif
#ifndef USE_POLL
#define USE_POLL 0
#endif
#ifndef DEBUG_WINDOW_GEOMETRY
#define DEBUG_WINDOW_GEOMETRY 1
#endif

#if USE_POLL
#include <poll.h>
#else
#include <sys/select.h>
#endif
#if USE_LIBEV
#include <ev.h>
#endif

/* DEFINES */
#ifndef DO_DEBUG_EVENT_LOOP
#define DO_DEBUG_EVENT_LOOP 1
#endif
#if (DO_DEBUG_EVENT_LOOP&1)
static int DRI2EventBase=0;
#endif

#define NUM_POLL_CHANNELS 5

/* PROTOTYPES */
struct list_of_windows; /* each display stores a double linked list of windows */
struct poll_channels {
#if USE_POLL
	struct pollfd channel[NUM_POLL_CHANNELS];
#else
	fd_set fds[3*NUM_POLL_CHANNELS];
#endif
	int nfds;
	int timeout;
};
struct display {
	Display* handle;
	xcb_connection_t* connection;
	xcb_screen_t* screen;
	xcb_generic_event_t* event;
	struct list_of_windows_el* current_window;
	struct list_of_windows* all_windows;
#if USE_LIBEV
	struct ev_loop* event_loop;
	struct watch_display_t {
		ev_io displayio;
		struct display* backref;
	} watch_display;
	ev_timer timer;
#endif
	int (*loop)(struct display*);
	int (*select_window)(struct display*, int win);
};
struct info {
	int version;
	GLXFBConfig config;
	XVisualInfo* visual;
	xcb_colormap_t colormap;
	struct poll_channels channels;
	xcb_key_symbols_t* keysymbols;
};
struct geometry {
	float x,y,w,h;
};
struct drawctxt {
	GLXContext context;
	GLXDrawable drawable;
	GLXWindow window;
};
struct context {
	struct drawctxt* gl;
	void* attached;
};
enum window_modes { wm_size_changed=1, wm_new_context=2 };
struct window {
	struct display* display;
	xcb_window_t handle;
	struct info* info;
	struct context ctxt;
	struct geometry geom;
	int mode;
	int keyflags;
	int (*open)(struct window* w);
	int (*draw)(struct window* w);
};

/*******************************************************************************
--------------------
=== Window Lists ===
We might want to be able to handle several windows for one connection.

[source,c]
---------------
 ******************************************************************************/
struct list_of_windows_el {
	struct list_of_windows_el *prev, *next;
	int mode;
	struct window window;
};
enum list_modes { lm_sorted=0, lm_list };
enum list_element_mode { lem_data_set=0
};
struct list_of_windows {
	struct list_of_windows_el *head;
	unsigned elsize, space, count;
	enum list_modes mode;
};
struct list_return { void* base; void* el; };
struct any_list { void* head; unsigned elsize; unsigned space; unsigned count;
	int mode; };
struct any_list_element { struct any_list_element* prev, *next;
	int mode;
};

#define COPY_LIST_ELEMENT(SRC,DEST,SIZE) do { \
	char* dp = (char*)&(DEST->mode); \
	char* sp = (char*)&(SRC->mode); \
	int ofs, len; \
	dp += sizeof(DEST->mode); \
	sp += sizeof(SRC->mode); \
	ofs = dp-((char*)DEST); \
	len = SIZE, len = (len>ofs) ?len-ofs :0; \
	DEST->mode |= (1<<lem_data_set); \
	/* printf("%p %p %d\n", dp, sp, len); */ \
	memcpy(dp,sp,len); \
} while(0)

extern void* new_list(unsigned n_elements, unsigned size);
#define LIST_FUNC(N) struct list_return N(void* list,\
	void* element, int(*compar)(void*a,void*b))
extern LIST_FUNC(search_element);
extern LIST_FUNC(add_element);
extern LIST_FUNC(list_mode);
extern LIST_FUNC(array_mode);

static int compare_window_elements(void* a, void* b);
#define ADD_WINDOW(DISPLAY,WINDOW) do \
{ \
	struct list_of_windows_el W; \
	memset(&W,0,sizeof(W)); \
	W.window = WINDOW; \
	DISPLAY->current_window = add_element(DISPLAY->all_windows,&W,compare_window_elements).el;\
} \
while(0)

/*******************************************************************************
-----------------
=== Extensions Query ===
We might want to react on extension events. So we need the configuration.

[source,c]
-----------------
 ******************************************************************************/
struct query_extensions_t {
	xcb_query_extension_cookie_t ext;
	xcb_query_extension_reply_t* repl;
	const char* name; int nameLen;
};
static void init_extension(xcb_connection_t* c, struct query_extensions_t *qext);
void init_extension(xcb_connection_t* c, struct query_extensions_t *qext)
{
	xcb_generic_error_t* err;
	qext->repl = xcb_query_extension_reply(c, qext->ext,&err);
	if (err) {
		fputs("failed to query extension ",stdout);
		fwrite(qext->name,qext->nameLen,1,stdout);
		fputc('\n',stdout);
	}
	else {
		fputs("Extension ",stdout);
		fwrite(qext->name,qext->nameLen,1,stdout);
		fprintf(stdout," parameters (op=%3d, ev=%3d, err=%3d)\n",
			qext->repl->major_opcode,
			qext->repl->first_event,
			qext->repl->first_error);
#if (DO_DEBUG_EVENT_LOOP&1)
		if (0==(memcmp(qext->name,"DRI2",qext->nameLen<4?qext->nameLen:4)))
			DRI2EventBase=qext->repl->first_event;
#endif
	}
}
/*******************************************************************************
------------------
=== Functions ===
Some functions that are exported by this module.

[source,c]
---------------
 ******************************************************************************/
static int open_x11_window(struct window* win);
static int std_x11_loop(struct display* display);
#if USE_LIBEV
static void std_x11_event(struct ev_loop* L, struct ev_io* w, int revents);
typedef struct { unsigned char code[8]; } utf8_t;
static utf8_t get_utf8_from_ucs(uint32_t cp);
#endif
static int test_draw(struct window* win);

static int wait_for_event(struct display* d);

static int select_window(struct display* display, int win);
/*******************************************************************************
-------------------------
=== The Main function ===
This function contains the test code for the library functions and is not thought
to be exported if a library is build from this source.

[source,c]
---------------
 ******************************************************************************/
/* static struct display* new_display(); */
/* static struct window* new_window(); */
static struct info* new_info();
static struct drawctxt* new_drawctxt();
int main(int argc, char** argv)
{
#if (TEST_LIST>0)
	struct window win;
	struct display* D = new_display();
	int i;

	D->all_windows = new_list(10,sizeof(struct window));
	memset(&win,0x0,sizeof(win));
	win.display = D;
	for (i=0;i<10;i++) {
		win.handle = i; ADD_WINDOW(D,win);
	}
	for (i=0; i<D->all_windows->count; ++i) {
		int handle = (int)(D->all_windows->head[i].window.handle);
		printf("%3d: %3d\n", i, handle);
	}
#else
	int i,n;
	int minKey, maxKey;
	Display* D;
	const char* dn;
	GLXFBConfig* cfgs;
	struct window win;
	struct display display;
	int defScreen;

	memset(&win,0,sizeof(win));
	memset(&display,0,sizeof(display));
	win.display = &display;
	win.display->select_window = select_window;
	win.display->all_windows = new_list(2,sizeof(struct list_of_windows_el));
	win.info = new_info();
	win.draw = test_draw;
	win.ctxt.gl = new_drawctxt();
	win.open = open_x11_window;
	win.display->loop = wait_for_event;
	D = win.display->handle = XOpenDisplay(dn=getenv("DISPLAY"));
	if (!D) {
		printf("cannot connect to display \"%s\"\n",
			dn ?dn :"(null)");
		exit(-1);
	}
	win.display->connection = XGetXCBConnection(D);
	XSetEventQueueOwner(D,XCBOwnsEventQueue);
	win.display->watch_display.backref = win.display;
	ev_io_init(
		&win.display->watch_display.displayio,
		std_x11_event,
		xcb_get_file_descriptor(win.display->connection),
		EV_READ);
	win.display->event_loop = ev_default_loop(0);
	ev_io_start(win.display->event_loop, &win.display->watch_display.displayio);
	XDisplayKeycodes(D, &minKey, &maxKey);
	win.info->keysymbols = xcb_key_symbols_alloc(win.display->connection);
	printf("Display Keycodes [%02x,%02x]\n", minKey, maxKey);

{
	enum {qext_num=32};
	uint32_t fill, num=qext_num, pos;
	struct query_extensions_t qext[qext_num];
	xcb_list_extensions_cookie_t list;
	xcb_list_extensions_reply_t *lr;
	xcb_generic_error_t *err;
	xcb_str_iterator_t name_i;
	int go = 1;
	list = xcb_list_extensions(win.display->connection);
	lr = xcb_list_extensions_reply(win.display->connection,
		list, &err);
	if (!err) go=1; else go=0;
	num=32;
	if (go) {
		name_i = xcb_list_extensions_names_iterator(lr);
	}
	fill=pos=0;
	while (go) {
		go = (name_i.data != xcb_str_end(name_i).data);
		if (go) {
			qext[fill].name = xcb_str_name(name_i.data);
			qext[fill].nameLen = xcb_str_name_length(name_i.data);
			qext[fill].ext = xcb_query_extension(
				win.display->connection,
				qext[fill].nameLen, qext[fill].name);
			xcb_str_next(&name_i);
			++fill;
		}
		if (fill==num) { fill=0; for(pos=0;pos<num;++pos) {
			init_extension(win.display->connection,&qext[pos]);
			if (qext[pos].repl) free(qext[pos].repl);
		}}
	}
	if (fill>0) for(pos=0;pos<fill;++pos){
		init_extension(win.display->connection,&qext[pos]);
		if (qext[pos].repl) free(qext[pos].repl);
	}
	free(lr);
}

	defScreen = DefaultScreen(D);
	xcb_screen_iterator_t screen_iter = 
		xcb_setup_roots_iterator(xcb_get_setup(win.display->connection));
	for(i = defScreen;
		screen_iter.rem && i > 0;
		--i, xcb_screen_next(&screen_iter));
	win.display->screen = screen_iter.data;
	win.geom.w = (float)win.display->screen->width_in_pixels;
	win.geom.h = (float)win.display->screen->height_in_pixels;
{
	int hi, lo;
	if ( False == (glXQueryVersion(D, &hi, &lo)) ) {
		printf("Failed to query GLX for Display: %p\n", D);
		return -1;
	}
	win.info->version = (hi<<4) | (lo&0xf);
	printf("GLX %d.%d\n", hi, lo);
}
	printf("GLX Version: %d\n", win.info->version);

{
	int attr[] = { GLX_DOUBLEBUFFER, True,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_X_RENDERABLE, True,
		None
	};
	cfgs = glXChooseFBConfig(D,0,attr,&n);
	if (n>0) {
		win.info->config = cfgs[0];
		win.info->visual = glXGetVisualFromFBConfig(D,win.info->config);
	}
	XFree(cfgs);
}
	printf("found %d configs\n", n);
	win.ctxt.gl->context = glXCreateNewContext(D,win.info->config, GLX_RGBA_TYPE, 0, True);
	win.mode |= wm_new_context;
	win.info->colormap = xcb_generate_id(win.display->connection);
	xcb_create_colormap(win.display->connection, XCB_COLORMAP_ALLOC_NONE,
		win.info->colormap, win.display->screen->root,
		win.info->visual->visualid);
	win.open(&win);
	ADD_WINDOW(win.display,win);
	win.ctxt.gl->window = win.handle;
	glXMakeContextCurrent(D,win.ctxt.gl->window,win.ctxt.gl->window,win.ctxt.gl->context);
	win.ctxt.gl->drawable = win.ctxt.gl->window;
	win.display->loop(win.display);
#endif	
	return 0;
}
/*******************************************************************************
---------------
=== Creating Types ===
In general a type can be created by +calloc(1,sizeof(T))+.

[source,c]
---------------
 ******************************************************************************/
#define new_F(N,TN) TN* new_ ## N() { \
	TN* obj = calloc(1,sizeof(TN)); return obj; }\

#define new_SF(N) new_F(N,struct N)

/* new_SF(display) */
new_SF(info)
/* new_SF(window) */
new_SF(drawctxt)
/*******************************************************************************
------------------------
=== X11 Implementation -- open ===
Window handle creation in an X11 System.

[source,c]
---------------
 ******************************************************************************/
#define GEOM(G) (int)(G.x+.5), (int)(G.y+.5),\
(unsigned int)(G.w+.5), (unsigned int)(G.h+.5)
int open_x11_window(struct window* win)
{
	uint32_t eventmask = XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_KEY_PRESS|\
		XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	uint32_t valuelist[] = { XCB_NONE, eventmask, win->info->colormap, 0 };
	uint32_t valuemask = XCB_CW_EVENT_MASK|XCB_CW_COLORMAP|\
		XCB_CW_BACK_PIXMAP;
	xcb_void_cookie_t cookies[2];
	xcb_connection_t* c = win->display->connection;
	xcb_generic_error_t *err;
	if (win->handle != 0x0) return win->handle;

	win->handle = xcb_generate_id(win->display->connection);
	cookies[0] = xcb_create_window(win->display->connection,
		XCB_COPY_FROM_PARENT, win->handle, win->display->screen->root,
		GEOM(win->geom), 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, win->info->visual->visualid,
		valuemask, valuelist);
	cookies[1] = xcb_map_window(win->display->connection, win->handle);
	if (0x0 != (err=xcb_request_check(c,cookies[0]))) {
		printf("request (%03d,%03d) failed with error %3d\n",
			err->pad0, err->major_code, err->error_code);
		return 1;
	}
	if (0x0 != (err=xcb_request_check(c,cookies[1]))) {
		printf("request (%03d,%03d) failed with error %3d\n",
			err->pad0, err->major_code, err->error_code);
		return 2;
	}
	return 0;
}
/*******************************************************************************
--------------------
=== Test implementation ===
The test implementation of the window draw function, also needs some special
context data, that is initialized on demand.
[source,c]
---------------
 ******************************************************************************/
enum test_context_numbers { tc_num_textures=1 };
struct test_context
{
	FTGLfont* font;
	char* font_path;
	GLuint texture[tc_num_textures];
	int status;
};
int test_draw(struct window* w)
{
	static int cmode=0;
	static FTGLfont *font=0x0;
	static const char* font_path="font.ttf";
	if ( font==0x0 ) {
		font = ftglCreateTextureFont(font_path);
	}
	switch (cmode) {
		case 0: glClearColor(.0f,.5f,0.8f,0.5f); break;
		case 1: glClearColor(.8f,.5f,.0f,.5f); break;
		case 2: glClearColor(.5f,.8f,.0f,.5f); break;
		case 3: glClearColor(.5f,.0f,.8f,.5f); break;
	}
	cmode = (cmode>2)?0:cmode+1;
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1000,1000,1000,-1000,0,1);
	glColor4f(.0f,.0f,.0f,.0f);
	glBegin(GL_QUADS);

	glVertex3f(10.0f,10.0f,.0f);
	glVertex3f(400.0f,10.0f,.0f);
	glVertex3f(400.0f,400.0f,.0f);
	glVertex3f(10.0f,400.0f,.0f);

	glEnd();
	glXSwapBuffers(w->display->handle, w->ctxt.gl->window);
	return 0;
}
/*******************************************************************************
--------------------
=== Standard loop ===
The standard loop is used when there are no other input sources. Later, this
might be modified to poll from other filedescriptors too.

[source,c]
---------------
 ******************************************************************************/
#if (DO_DEBUG_EVENT_LOOP&1)
#define DEBUG_EVENT_LOOP_EVENT(E,display) do \
	{ \
		static const char* event_name_table[0x7f] = { \
			0x0, 0x0 \
		}; \
		const char* event_name = event_name_table[0]; \
		if ( event_name_table[0]==event_name_table[1] ) { \
			int s,n,l; \
			event_name_table[0] = "unknown"; \
			n=(sizeof(event_name_table)/sizeof(const char*)); \
			for (l=0,s=1; s<n; s+=l) { \
				if (l==0)l=1; \
				else { \
					l*=2; \
					if (s+l>n) l=n-s; \
				} \
				memcpy(event_name_table+s,event_name_table,l*sizeof(const char*)); \
			} \
			event_name_table[0] = "error"; \
			event_name_table[1] = "reply"; \
			event_name_table[0xc] = "expose"; \
			event_name_table[0x2] = "key-press"; \
			event_name_table[MapNotify] = "map"; \
			event_name_table[UnmapNotify] = "unmap"; \
			event_name_table[ConfigureNotify] = "configure"; \
			if (DRI2EventBase!=0) { \
				event_name_table[DRI2EventBase] = "DRI2 Swap Complete"; \
				event_name_table[DRI2EventBase+1] = "DRI2 Invalidate Buffers"; \
			} \
		} \
		event_name = event_name_table[E->response_type & ~0x80]; \
		fprintf(stderr, "event:%20s(%02d) %d\n", event_name?event_name:"*FATAL*", E->response_type, \
			xcb_connection_has_error(display->connection)); \
	} while (0)
#else
#define DEBUG_EVENT_LOOP_EVENT(E,display) do { } while(0)
#endif

#if USE_LIBEV
void std_x11_event(struct ev_loop* L, struct ev_io* w, int revents)
{
	struct watch_display_t *x = (struct watch_display_t*)w;
	struct display* d = x->backref;
	std_x11_loop(d);
}
#endif
#define STDX11LOOP_SelectWindow(WIN) do { \
	if(cwh!=WIN) { \
		if (0!=change_window(display,WIN)) { \
			printf("the window %d not found\n", WIN); \
			return -1; \
		} \
		cw = display->current_window; \
	} \
} while(0)
#define SQR(X) ((X)*(X))
#define GEOM_DELTA(A,B)  \
	(SQR(A.x-B.x)+SQR(A.y-B.y)+SQR(A.w-B.w)+SQR(A.h-B.h))
int std_x11_loop(struct display* display)
{
	int running = 1;
	struct list_of_windows_el* cw;
	struct window* win;
	int cwh;
	xcb_generic_event_t*E;
	int (*change_window)(struct display* display, int w);

	if (!display) { printf("no display structure to work on\n"); return -1; }
	cw = display->current_window;
	win = &cw->window;
	cwh = (cw) ?cw->window.handle:0x0;
	change_window = display->select_window;

	while(running) {
		E = display->event = xcb_poll_for_event(display->connection);
		if (!E) break;
		DEBUG_EVENT_LOOP_EVENT(E,display);
		switch (E->response_type & ~0x80) {
			case XCB_KEY_PRESS:{
				xcb_key_press_event_t* key =(xcb_key_press_event_t*)E;
				xcb_keysym_t sym = xcb_key_symbols_get_keysym(
					win->info->keysymbols, key->detail, win->keyflags );
				long ucs = keysym2ucs((KeySym)sym);
				utf8_t utf8 = get_utf8_from_ucs(ucs);
				utf8.code[0] = 0x0;
/*
				printf("found ucs code x0%04x, %s, from symbol 0x%04x\n", (uint32_t)ucs, utf8.code, sym); 
				printf("DETAIL = 0x%04x\n", key->detail); */
			} break;
			case XCB_EXPOSE:{
				struct geometry G;
				float H;
				struct window* W;
				Display* D;
				xcb_window_t win;
				GLXContext ctxt;
				xcb_expose_event_t* expose = (xcb_expose_event_t*)E;
				int (*draw)(struct window*);
				STDX11LOOP_SelectWindow(expose->window);
				W = &cw->window;
				D = W->display->handle;
				win = W->handle;
				ctxt = W->ctxt.gl->context;
				G = W->geom;
				draw = cw->window.draw;
				printf("drawing (%d,%d %dx%d)...\n", GEOM(G));
				H = W->display->screen->height_in_pixels;
				if (G.h>H)G.h=H;
				G.y = H - G.h;
				G.x = .0f;
				printf("viewport (%d,%d %dx%d)...\n", GEOM(G));
				glViewport(G.x,G.y,G.w,G.h);
				if (draw) draw(&cw->window);
				printf("...done.\n");
			} break;
			case XCB_CONFIGURE_NOTIFY:{
				Display* D;
				GLXContext ctxt;
				xcb_connection_t* c;
				xcb_window_t win;
				uint16_t mask;
				uint32_t values[4];
				struct geometry G,O;
				xcb_configure_notify_event_t* cfg =
					(xcb_configure_notify_event_t*)E;
				STDX11LOOP_SelectWindow(cfg->window);
				win = cw->window.handle;
				c = cw->window.display->connection;
				D = cw->window.display->handle;
				ctxt = cw->window.ctxt.gl->context;
				mask = XCB_CONFIG_WINDOW_X|
					XCB_CONFIG_WINDOW_Y|
					XCB_CONFIG_WINDOW_WIDTH|
					XCB_CONFIG_WINDOW_HEIGHT;
				G=cw->window.geom; O.x=(float)cfg->x;
				O.y=(float)cfg->y; O.w=(float)cfg->width;
				O.h=(float)cfg->height;
				if(GEOM_DELTA(G,O)>.25f) {
#if DEBUG_WINDOW_GEOMETRY
					printf("configured window "
						"[(%d,%d %dx%d) - (%d,%d %dx%d)]² ="
						"%f\n", GEOM(G), GEOM(O), GEOM_DELTA(G,O));
#endif
					G=O; values[0]=(uint32_t)(G.x+.5f);
					values[1]=(uint32_t)(G.y+.5f);
					values[2]=(uint32_t)(G.w+.5f);
					values[3]=(uint32_t)(G.h+.5f);
					// xcb_configure_window(c,cw->window.ctxt.gl->window,mask,values);
					cw->window.geom = G;
					cw->window.mode|= wm_size_changed;
				}
#if DEBUG_WINDOW_GEOMETRY&2
				else {
					printf("not configured window "
						"[(%d,%d %dx%d) - (%d,%d %dx%d)]² ="
						"%f\n", GEOM(G), GEOM(O), GEOM_DELTA(G,O));
				}
#endif
			} break;
			case 0: {
				xcb_generic_error_t* err = (xcb_generic_error_t*)E;
/*
				fprintf(stderr, "got error code %03d\n"
					"error label: %-30s\n", err->error_code,
					xcb_event_get_error_label(err->error_code)
				);
*/
			} break;
		}
		free(E); display->event = 0x0;
	}
	return 0;
}

/*******************************************************************************
---------------
=== UCS to UTF8 Converter ===
Convert a UCS Character to a UTF8 code.
[source,c]
---------------
 ******************************************************************************/
utf8_t get_utf8_from_ucs(uint32_t cp) 
{
	utf8_t ret={ {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff} };
	if (cp<=0x7f) {
		ret.code[0] = (unsigned char)cp;
		ret.code[1] = 0x0;
	}
	else if (cp<=0x7ff) {
		ret.code[0] = (0xc0|(cp>>6));
		ret.code[1] = (0x80|(cp&0x03f));
		ret.code[2] = 0x0;
	}
	else if (cp<=0xffff) {
		ret.code[0] = (0xe0|(cp>>12));
		ret.code[1] = (0x80|((cp>>6)&0x3f));
		ret.code[2] = (0x80|(cp&0x3f));
		ret.code[3] = 0x0;
	}
	else if (cp<=0x1fffff) {
		ret.code[0] = (0xf0|(cp>>18));
		ret.code[1] = (0x80|((cp>>12)&0x3f));
		ret.code[2] = (0x80|((cp>>6)&0x3f));
		ret.code[3] = (0x80|(cp&0x3f));
		ret.code[4] = 0x0;
	}
	else if (cp<=0x3ffffff) {
		ret.code[0] = (0xf8|(cp>>24));
		ret.code[1] = (0x80|(cp>>18));
		ret.code[2] = (0x80|((cp>>12)&0x3f));
		ret.code[3] = (0x80|((cp>>6)&0x3f));
		ret.code[4] = (0x80|(cp&0x3f));
		ret.code[5] = 0x0;
	}
	else if (cp<=0x7fffffff) {
		ret.code[0] = (0xfc|(cp>>30));
		ret.code[1] = (0x80|((cp>>24)&0x3f));
		ret.code[2] = (0x80|((cp>>18)&0x3f));
		ret.code[3] = (0x80|((cp>>12)&0x3f));
		ret.code[4] = (0x80|((cp>>6)&0x3f));
		ret.code[5] = (0x80|(cp&0x3f));
		ret.code[6] = 0x0;
	}
	return ret;
}

/*******************************************************************************
---------------
=== Waiting for Window Events and Timeouts ===
Most events have input sources, which can be polled from a set of file handles.

[source,c]
---------------
 ******************************************************************************/
int wait_for_event(struct display* d)
{
	int ret = -1;
	static struct ev_loop* L = 0x0;
	if (!L) L=EV_DEFAULT;
	ev_run(L,0);
	ret = (int)ev_depth(L);
	return ret;
}

/*******************************************************************************
---------------
=== General List Operations ===
Some simple list operations on opaque list elements. Acutally we use an array of
lists, to be able to convert the list into a sorted array, which can be
searched by binary search.

[source,c]
---------------
 ******************************************************************************/
void* new_list(unsigned n, unsigned l)
{
	enum { AnyList=sizeof(struct any_list), AnyEl=sizeof(struct any_list_element) };
	unsigned E = l+AnyEl;
	unsigned L = n*E + AnyList;
	void* space = calloc(1,L);
	struct any_list* List=space, ListObj= { ((char*)space)+sizeof(struct any_list), E, n*E, 0, 0 };
	*List = ListObj;
	return space;
}
LIST_FUNC(add_element)
{
	struct list_return ret = {0x0,0x0};
	char*ip,*dp,*ep;
	struct any_list *_l = list;
	struct any_list_element *_e;

	ip=dp=0x0; ep=((char*)_l->head)+_l->count*_l->elsize;
	if ( ((char*)_l->head+_l->space)<(ep+_l->elsize) )
		return ret;
	ret = search_element(list,element,compar);
	if (ret.el!=0x0) {ret.el=0x0; return ret;}
	if (ret.base == 0x0) {
		if (_l->head==0x0) return ret; /* {0,0} => can't add to void */
		_e = _l->head;
		if ((_e->mode & (1<<lem_data_set)) == 0) {
			COPY_LIST_ELEMENT(((struct any_list_element*)element),_e,_l->elsize);
			ret.el=_e;
			_l->count++;
		}
		else {
			ip=_l->head; dp=ip+_l->elsize;
		}
	}
	else {
		ip=ret.base; ip+=_l->elsize;
		dp=ip; dp+=_l->elsize;
		if ( ((char*)_l->head)+_l->space < ip ) ip = 0x0;
	}
	if (ip!=0x0) {
		_e = ((struct any_list_element*)ip);
		if (ep>ip) { memcpy(dp,ip,ep-ip); }
		COPY_LIST_ELEMENT(((struct any_list_element*)element),_e,_l->elsize);
		_l->count++;
	}
	return ret;
}

LIST_FUNC(search_element)
{
	struct list_return ret = {0x0,0x0};
	char* bp;
	unsigned m,l,r;
	int c;
	struct any_list* L=list;

	if (L->count==0) return ret;
	for (bp=L->head,l=0,r=L->count-1; 0<((r+1)-l); ) {
		m = (l+r)/2;
		ret.base=(L->head+m*L->elsize);
		c = compar(element,ret.base);
		if (c==0) { ret.el=ret.base; return ret; }
		else if (c<0) r=m-1;
		else l=m+1;
	}
	if (c<0) ret.base=(L->head+l*L->elsize);
	return ret;
}

int compare_window_elements(void* a, void* b)
{
	struct list_of_windows_el* A = a;
	struct list_of_windows_el* B = b;
	int i = A ?A->window.handle: 0;
	int j = B ?B->window.handle: 0;

	return (i<j) ?-1 :(i==j)? 0: +1;
}

/*******************************************************************************
---------------
=== LIST MODES ===
Depending on the usage of a data space, we might store the collection as a sorted
array or as a double linked list. The transformation functions might be used
to assure that wanted mode is enabled.

The list modes are

  - +lm_list+ ― a double linked list.
  - +lm_array+ — a sorted array.

[source,c]
----------------
 ******************************************************************************/
LIST_FUNC(list_mode)
{
	struct list_return ret = {0x0,0x0};
	return ret;
}
LIST_FUNC(array_mode)
{
	struct list_return ret = {0x0,0x0};
	return ret;
}
/*******************************************************************************
---------------
=== SELECTING A WINDOW ===
Most applications will run only one window, but this code is a good use case
for the list handler.

[source,c]
------------------
 ******************************************************************************/
int select_window(struct display* d, int win)
{
	struct list_of_windows_el W;
	struct list_return res;
	W.window.handle=(xcb_window_t)win;
	res = search_element(d->all_windows, &W,compare_window_elements);
	d->current_window = res.el;
	return (res.el ?0 :1);
}
/*******************************************************************************
---------------
 ******************************************************************************/
