#include "x.h"
// @todo free all replys
// mesa.system gcc ./main.c ./x.c -o ./test -lGL -lxcb -lxcb-glx

int main( int argc, char* argv[] )
{
    xcb_generic_error_t* xerror  = 0; // error placeholder

    // connecting to X server
    int32_t screen_number;
    xcb_connection_t* xconnection = xcb_connect( NULL, &screen_number );
    if( xcb_connection_has_error( xconnection )) fatal_error( "xcb_connect(): cannot open display\n");

    // getting the default screen
    xcb_screen_t* xscreen = xcb_setup_roots_iterator( xcb_get_setup( xconnection )).data; 

    // checking for glx version
    glx_check_version( xconnection, 1, 4 );

    // getting suitable fbconfig and a visual from it
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
                                GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PIXMAP_BIT | GLX_PBUFFER_BIT,
                                GLX_X_RENDERABLE, true
                            };

    xcb_glx_fbconfig_t fbconfig  = glx_choose_fbconfig( xconnection, screen_number, glxattribs, sizeof(glxattribs)/2/sizeof(uint32_t));
    xcb_visualid_t     glxvisual = get_attrib_from_fbconfig( xconnection, screen_number, fbconfig, GLX_VISUAL_ID );
    printf( "choosing fbconfig id 0x%X with visual id 0x%X\n", fbconfig, glxvisual );

    // generating id's for our objects
    xcb_glx_context_t glxcontext = xcb_generate_id( xconnection );
    xcb_colormap_t    xcolormap  = xcb_generate_id( xconnection );
    xcb_window_t      xwindow    = xcb_generate_id( xconnection );
    xcb_glx_window_t  glxwindow  = xcb_generate_id( xconnection );
    

    // creating glx context
    xcb_glx_create_new_context( xconnection, glxcontext, fbconfig, screen_number, GLX_RGBA_TYPE, 0, true);
    // alternative method:
    // xcb_glx_create_context( xconnection, glxcontext, glxvisual, 0, 0, true );

    // checking if we have direct rendering
    if(!( xcb_glx_is_direct_reply( xconnection, xcb_glx_is_direct( xconnection, glxcontext ), &xerror )->is_direct)) fatal_error( "glx context is not direct!\n" );
    if( xerror ) fatal_error( "xcb_glx_is_direct failed!\n" );

    // creating colormap
    xcb_create_colormap( xconnection , XCB_COLORMAP_ALLOC_NONE, xcolormap, xscreen->root, glxvisual );

    // creating an Window, using our new colormap
    uint32_t value_list[5], value_mask = XCB_CW_BACK_PIXEL|  XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP ;
    value_list[0] = xscreen->black_pixel;
    value_list[1] = 0;
    value_list[2] = XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION;
    value_list[3] = xcolormap;
    value_list[4] = 0;

    xcb_create_window( xconnection, xscreen->root_depth, xwindow, xscreen->root, 0, 0, 640, 480, 0,
                                                            XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                                            glxvisual,
                                                            value_mask,
                                                            value_list );
    xcb_map_window( xconnection, xwindow );
    xcb_glx_create_window( xconnection, screen_number, fbconfig, xwindow, glxwindow, 0, 0 );

    // making our glx context current
    xcb_glx_make_context_current_reply_t* reply_ctx = 
    xcb_glx_make_context_current_reply ( xconnection, xcb_glx_make_context_current( xconnection, 0, glxwindow, glxwindow, glxcontext ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_make_context_current failed!\n" );
    xcb_glx_context_tag_t glxcontext_tag = reply_ctx->context_tag;


    // alternative ?
    /*
    xcb_glx_make_current_reply_t* reply_mc =
    xcb_glx_make_current_reply( xconnection, xcb_glx_make_current( xconnection, glxwindow, glxcontext, 0 ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_make_current failed!\n" );
    xcb_glx_context_tag_t glxcontext_tag = reply_mc->context_tag;
    */

    xcb_flush( xconnection );

    glShadeModel( GL_SMOOTH );
    glHint      ( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
    glClearColor( 1.0f, 1.0f, 0.0f, 0.5f);  //Black Background
    glClearDepth( 1.0f );             //enables clearing of deapth buffer
    glEnable    ( GL_DEPTH_TEST );  //enables depth testing
    glDepthFunc ( GL_LEQUAL );      //type of depth test
    glEnable    ( GL_TEXTURE_2D );  //enable texture mapping
    glEnable    ( GL_BLEND);
    glBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glViewport( 0, 0, 640, 480 );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum( -1.0f , 1.0f , -1.0f, 1.0f, 1.0f, 1000.0f );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef( 0.0f, 0.0f, -2.0f ); // moving camera back, so i can see stuff i draw
    glFlush();

    // Uncommenting this causes my xserver to crash, why?
    /*
    xcb_glx_get_error_reply_t* err_reply =
    xcb_glx_get_error_reply( xconnection, xcb_glx_get_error( xconnection, glxcontext_tag ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_get_error failed, error code %i\n", xerror->error_code );
    */

    xcb_generic_event_t* xevent;
    bool running = true;
    while ( running && ( xevent = xcb_wait_for_event( xconnection ))) 
    {

        // drawing a triangle to test
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        glBegin( GL_TRIANGLES );
        glColor3f( 1.0f, 0.0f, 0.5f );
        glVertex3f( 1.0f, 1.0f, 0.0f );
        glVertex3f( 1.0f, 0.0f, 0.0f );
        glVertex3f( 0.0f, 1.0f, 0.0f );
        glEnd();

        switch ( xevent->response_type & ~0x80 ) 
        {
            case XCB_KEY_PRESS: // exit on key press
                running = false;
                break;
        }
        free( xevent );
        xcb_glx_swap_buffers( xconnection, glxcontext_tag, glxwindow );
    }

    xcb_disconnect( xconnection );
	return 0;
}
