/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_pixel_format_descriptor.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

/** Internal types */
typedef struct
{
    #ifdef _WIN32
        PIXELFORMATDESCRIPTOR pfd;
    #endif

    unsigned char color_red_bits;
    unsigned char color_green_bits;
    unsigned char color_blue_bits;
    unsigned char color_alpha_bits;
    unsigned char depth_bits;

    REFCOUNT_INSERT_VARIABLES

} _ogl_pixel_format_descriptor;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_pixel_format_descriptor, ogl_pixel_format_descriptor, _ogl_pixel_format_descriptor);

/** Please see header for specification */
PRIVATE void _ogl_pixel_format_descriptor_release(__in __notnull __deallocate(mem) void* descriptor)
{
    /* Nothing to do - refcount impl automatically releases the descriptor after this func finishes executing */
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_pixel_format_descriptor ogl_pixel_format_descriptor_create(__in __notnull system_hashed_ansi_string name, __in unsigned char color_buffer_red_bits, __in unsigned char color_buffer_green_bits, __in unsigned char color_buffer_blue_bits, __in unsigned char color_buffer_alpha_bits, __in unsigned char depth_buffer_bits)
{
    _ogl_pixel_format_descriptor* descriptor = new (std::nothrow) _ogl_pixel_format_descriptor;

    ASSERT_ALWAYS_SYNC(descriptor != NULL, "Out of memory while allocating pixel format descriptor.");
    if (descriptor != NULL)
    {
        descriptor->color_red_bits   = color_buffer_red_bits;
        descriptor->color_green_bits = color_buffer_green_bits;
        descriptor->color_blue_bits  = color_buffer_blue_bits;
        descriptor->color_alpha_bits = color_buffer_alpha_bits;
        descriptor->depth_bits       = depth_buffer_bits;

        #ifdef _WIN32
            memset(&descriptor->pfd, 0, sizeof(PIXELFORMATDESCRIPTOR) );

            descriptor->pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
            descriptor->pfd.nVersion   = 1;
            descriptor->pfd.dwFlags    = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            descriptor->pfd.iPixelType = PFD_TYPE_RGBA;
            descriptor->pfd.cColorBits = color_buffer_red_bits + color_buffer_green_bits + color_buffer_blue_bits;
            descriptor->pfd.cRedBits   = color_buffer_red_bits;
            descriptor->pfd.cGreenBits = color_buffer_green_bits;
            descriptor->pfd.cBlueBits  = color_buffer_blue_bits;
            descriptor->pfd.cAlphaBits = color_buffer_alpha_bits;
            descriptor->pfd.cDepthBits = depth_buffer_bits;
        #endif

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(descriptor,
                                                       _ogl_pixel_format_descriptor_release, 
                                                       OBJECT_TYPE_OGL_PIXEL_FORMAT_DESCRIPTOR, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Pixel Format Descriptors\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_pixel_format_descriptor) descriptor;
}
                                      
/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_pixel_format_descriptor_get(__in ogl_pixel_format_descriptor_field field, __in __notnull ogl_pixel_format_descriptor descriptor, __out __notnull void* out_result)
{
    _ogl_pixel_format_descriptor* pfd_descriptor = (_ogl_pixel_format_descriptor*) descriptor;

    switch (field)
    {
        case OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_RED_BITS:   *(unsigned char*)out_result = pfd_descriptor->color_red_bits;   return true;
        case OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_GREEN_BITS: *(unsigned char*)out_result = pfd_descriptor->color_green_bits; return true;
        case OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_BLUE_BITS:  *(unsigned char*)out_result = pfd_descriptor->color_blue_bits;  return true;
        case OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_ALPHA_BITS: *(unsigned char*)out_result = pfd_descriptor->color_alpha_bits; return true;
        case OGL_PIXEL_FORMAT_DESCRIPTOR_DEPTH_BITS:              *(unsigned char*)out_result = pfd_descriptor->depth_bits;       return true;

        default: ASSERT_DEBUG_SYNC(false, "Unrecognized field [%d] requested", field); return false;
    }
}

#ifdef _WIN32
    /** Please see header for specification. */
    PUBLIC const PIXELFORMATDESCRIPTOR* _ogl_pixel_format_descriptor_get_descriptor(__in __notnull ogl_pixel_format_descriptor descriptor)
    {
        return &((_ogl_pixel_format_descriptor*)descriptor)->pfd;
    }
#endif /* _WIN32 */