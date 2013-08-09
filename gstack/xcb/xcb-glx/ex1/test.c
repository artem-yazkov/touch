#include "GL/gl.h"
#include "x.h"

static int AttributeList[] = { GLX_RGBA, 0 };

xcb_glx_context_t glxcontext;
xcb_colormap_t    xcolormap;
xcb_window_t      xwindow;
xcb_glx_window_t  glxwindow;
xcb_glx_context_tag_t glxcontext_tag;
int32_t screen_number;
xcb_connection_t* xconnection;
xcb_screen_t* xscreen;

void setup_glx13() {
	/* Find a FBConfig that uses RGBA.  Note that no attribute list is */
	/* needed since GLX_RGBA_BIT is a default attribute.*/
	    uint32_t glxattribs[] = {
                                GLX_DOUBLEBUFFER, 1,
                                GLX_RED_SIZE, 8,
                                GLX_GREEN_SIZE, 8,
                                GLX_BLUE_SIZE, 8,
                                GLX_ALPHA_SIZE, 8,
                                GLX_STENCIL_SIZE, 8,
                                GLX_DEPTH_SIZE, 24,
                                GLX_BUFFER_SIZE, 32,
                                GLX_RENDER_TYPE, GLX_RGBA_BIT,
                                GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT |
GLX_PIXMAP_BIT | GLX_PBUFFER_BIT,
                                GLX_X_RENDERABLE, true
                            };

	xcb_glx_fbconfig_t fbconfig = glx_choose_fbconfig( xconnection,
screen_number, glxattribs, sizeof(glxattribs)/2/sizeof(uint32_t));
	xcb_visualid_t glxvisual = get_attrib_from_fbconfig( xconnection,
screen_number, fbconfig, GLX_VISUAL_ID );

	/* Create a GLX context using the first FBConfig in the list. */
	xcb_glx_create_new_context( xconnection, glxcontext, fbconfig, screen_number, GLX_RGBA_TYPE, 0, true);

	/* Create a colormap */
	xcb_create_colormap( xconnection , XCB_COLORMAP_ALLOC_NONE, xcolormap,
xscreen->root, glxvisual );

	/* Create a window */
	uint32_t value_list[2], value_mask = XCB_CW_BACK_PIXEL | XCB_CW_COLORMAP
;
    value_list[0] = xscreen->white_pixel;
    value_list[1] = xcolormap;

    xcb_create_window( xconnection, xscreen->root_depth, xwindow,
xscreen->root, 0, 0, 100, 100, 0,
                                                           
XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                                            glxvisual,
                                                             value_mask,
                                                            value_list );
    xcb_map_window( xconnection, xwindow );
	/* Create a GLX window using the same FBConfig that we used for the */
	/* the GLX context.                                                 */
	xcb_glx_create_window( xconnection, screen_number, fbconfig, xwindow,
glxwindow, 0, 0);
	/* Connect the context to the window for read and write */
    xcb_glx_make_context_current_reply_t* reply_ctx =
xcb_glx_make_context_current_reply ( xconnection,
 xcb_glx_make_context_current( xconnection, 0, glxwindow, glxwindow,
 glxcontext ), 0);
 	glxcontext_tag = reply_ctx->context_tag;
}

 int main(int argc, char **argv) {

     xconnection = xcb_connect( NULL, &screen_number );

    // getting the default screen
    xscreen = xcb_setup_roots_iterator( xcb_get_setup( xconnection)).data;

	glxcontext = xcb_generate_id( xconnection );
	xcolormap  = xcb_generate_id( xconnection );
	xwindow    = xcb_generate_id( xconnection );
	glxwindow  = xcb_generate_id( xconnection );

	setup_glx13();
	xcb_flush(xconnection);
	/* clear the buffer */
	glClearColor(1,1,0,1);
	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();

	//This kills xserver
	xcb_glx_finish_reply_t* reply = xcb_glx_finish_reply(xconnection, xcb_glx_finish (xconnection, glxcontext_tag), 0);

	xcb_glx_swap_buffers( xconnection, glxcontext_tag, glxwindow );

	/* wait a while */
	sleep(10);
}


