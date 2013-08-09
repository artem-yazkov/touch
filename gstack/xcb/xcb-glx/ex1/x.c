#include "x.h"
// makes all fbconfig lines to be printed out
//#define DUMP_FBCONFIG_LIST

/// prints error msg and exits
void fatal_error( const char* fmt, ...  )
{
    printf("fatal_error: ");
    va_list ap;
    va_start( ap, fmt );
    vprintf( fmt, ap );
    va_end( ap );
    exit( 1 );
}

/**---------------------------------------------------------------------------------------------------*\

    This function checks if glx is less than specified version  and exits if it is
    
\**---------------------------------------------------------------------------------------------------*/
void glx_check_version( xcb_connection_t* xconnection, uint major, uint minor )
{
    xcb_generic_error_t* xerror;
    xcb_glx_query_version_reply_t* glxver = 
    xcb_glx_query_version_reply ( xconnection, xcb_glx_query_version( xconnection, 0, 0 ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_query_version failed!\n");
    if( glxver->major_version < major && glxver->minor_version < minor ) fatal_error( "xcb_glx_query_version(): need at least %i.%i glx extention, found %i.%i\n", major, minor, glxver->major_version, glxver->minor_version );
    free( glxver );
}


/**---------------------------------------------------------------------------------------------------*\

    This function searches for an @param prop_name in the @param property list of properties
    of size @param prop. Prop is property count and not buffer size.

\**---------------------------------------------------------------------------------------------------*/
uint32_t glx_get_property( const uint32_t* property, const uint props, uint32_t prop_name )
{
    uint i = 0;
    while( i < props * 2 )
    {
        if( property[i] == prop_name ) return property[i+1];
        else i += 2;
    }
    fatal_error("glx_get_property: no matches found for property %u!\n", prop_name );
    return -1;
}

/**---------------------------------------------------------------------------------------------------*\

    This function chooses and returns specific fbconfig id depending on attributes specified in 
    @param attrib list. @param attribsz is the number of properties ( not list size )

\**---------------------------------------------------------------------------------------------------*/
int32_t glx_choose_fbconfig( xcb_connection_t* xconnection, uint32_t screen_num, uint32_t* attrib, uint32_t attribsz )
{
    xcb_generic_error_t* xerror;
    
    // getting fbconfig list
    xcb_glx_get_fb_configs_reply_t* fbconfigs =
    xcb_glx_get_fb_configs_reply( xconnection, xcb_glx_get_fb_configs( xconnection, screen_num ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_get_fb_configs failed!\n" );
    uint32_t* prop   = xcb_glx_get_fb_configs_property_list( fbconfigs );

    #ifdef DUMP_FBCONFIG_LIST
    uint i = 0;
    for( ; i < fbconfigs->length; i++ )
    {
        if( i > 0 && !(i%80) ) printf("|\n");
        printf("%u\t", prop[i] );
    }
    printf("-----------------------------------------------------------------------\n");
    #endif

    uint32_t* fbconfig_line   = prop;
    uint32_t  fbconfig_linesz = fbconfigs->num_properties * 2;

    // for each fbconfig line
    uint32_t i = 0;
    for( ; i < fbconfigs->num_FB_configs; i++ )
    {
        bool good_fbconfig = true;

        // for each attrib
	uint j = 0;
        for( ; j < attribsz*2; j += 2 )
        {
	    // if property found != property given
            if( glx_get_property( fbconfig_line, fbconfigs->num_properties, attrib[j] ) != attrib[j+1] ) 
            {
                good_fbconfig = false; // invalidate this fbconfig entry, sine one of the attribs doesn't match
                break;
            }
        }
	
        // if all attribs matched, return with fid
        if( good_fbconfig )
        {
            uint32_t fbconfig_id = glx_get_property( fbconfig_line, fbconfigs->num_properties , GLX_FBCONFIG_ID );
            free( fbconfigs );
            return fbconfig_id;
        }

        fbconfig_line += fbconfig_linesz; // next fbconfig line;
    }
    fatal_error( "glx_choose_fbconfig: no matching fbconfig line found!\n" );
    return -1;
}


/**---------------------------------------------------------------------------------------------------*\

    This function returns @param attrib value from a line containing GLX_FBCONFIG_ID of @param fid
    It kind of queries particular fbconfig line for a specific property.

\**---------------------------------------------------------------------------------------------------*/
uint32_t get_attrib_from_fbconfig( xcb_connection_t* xconnection, uint32_t screen_num, uint32_t fid, uint32_t attrib )
{
    xcb_generic_error_t* xerror;
    xcb_glx_get_fb_configs_reply_t* fbconfigs =
    xcb_glx_get_fb_configs_reply( xconnection, xcb_glx_get_fb_configs( xconnection, screen_num ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_get_fb_configs failed!\n" );
    uint32_t* prop   = xcb_glx_get_fb_configs_property_list( fbconfigs );
    
    uint i = 0;
    bool fid_found = false;
    while( i < fbconfigs->length )
    {
        if( prop[i] == GLX_FBCONFIG_ID ) 
        {
            if(  prop[i+1] == fid  )
            {
                fid_found = true;
                i -= i%( fbconfigs->num_properties * 2 ); // going to start of the fbconfig  line
                uint32_t attrib_value = glx_get_property( &prop[i], fbconfigs->num_properties, attrib );
                free( fbconfigs );
                return attrib_value;
            }
        }
        i+=2;
    }
    if( fid_found ) fatal_error("get_attrib_from_fbconfig: no attrib %u was found in a fbconfig with GLX_FBCONFIG_ID %u\n", attrib, fid);
    else fatal_error("get_attrib_from_fbconfig: GLX_FBCONFIG_ID %u was not found!\n", fid );
    return -1;
}


/// dumps glxcontext attributes
void glx_dump_query_context( xcb_connection_t* xconnection, xcb_glx_context_t glxcontext )
{
    xcb_generic_error_t* xerror;
    xcb_glx_query_context_reply_t* r = xcb_glx_query_context_reply( xconnection, xcb_glx_query_context( xconnection, glxcontext ), &xerror );
    if( xerror ) fatal_error( "xcb_glx_query_context: failed to query current glx context!\n" );

    // this function was wrongly documented in xcb in internet. Also in reply structure there should be "attrib" member instead 
    // of "attributes"
    uint32_t* vals = xcb_glx_query_context_attribs ( r );
    
    uint i = 0;
    for( ; i < r->length; i += 2 )
    {
        printf("Value 0x%X is 0x%X\t", vals[i], vals[i+1] );
    }
    printf("\n");
}
