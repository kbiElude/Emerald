/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_pixel_format_descriptor.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

/** Internal types */
typedef struct _ogl_pixel_format_descriptor
{
    #ifdef _WIN32
        PIXELFORMATDESCRIPTOR pfd;
    #endif

    unsigned char color_alpha_bits;
    unsigned char color_blue_bits;
    unsigned char color_green_bits;
    unsigned char color_red_bits;
    unsigned char depth_bits;

    _ogl_pixel_format_descriptor()
    {
        #ifdef _WIN32
        {
            memset(&pfd,
                   0,
                   sizeof(pfd) );
        }
        #endif

        color_alpha_bits = 0;
        color_blue_bits  = 0;
        color_green_bits = 0;
        color_red_bits   = 0;
        depth_bits       = 0;
    }
} _ogl_pixel_format_descriptor;

/** Please see header for specification */
PRIVATE void _ogl_pixel_format_descriptor_release(__in __notnull __deallocate(mem) void* descriptor)
{
    /* Nothing to do - refcount impl automatically releases the descriptor after this func finishes executing */
}

/** Please see header for specification */
PUBLIC ogl_pixel_format_descriptor ogl_pixel_format_descriptor_create(__in unsigned char color_buffer_red_bits,
                                                                      __in unsigned char color_buffer_green_bits,
                                                                      __in unsigned char color_buffer_blue_bits,
                                                                      __in unsigned char color_buffer_alpha_bits,
                                                                      __in unsigned char depth_buffer_bits)
{
    _ogl_pixel_format_descriptor* pfd_ptr = new (std::nothrow) _ogl_pixel_format_descriptor;

    ASSERT_ALWAYS_SYNC(pfd_ptr != NULL,
                       "Out of memory while allocating pixel format descriptor.");

    if (pfd_ptr != NULL)
    {
        pfd_ptr->color_red_bits   = color_buffer_red_bits;
        pfd_ptr->color_green_bits = color_buffer_green_bits;
        pfd_ptr->color_blue_bits  = color_buffer_blue_bits;
        pfd_ptr->color_alpha_bits = color_buffer_alpha_bits;
        pfd_ptr->depth_bits       = depth_buffer_bits;

        #ifdef _WIN32
        {
            memset(&pfd_ptr->pfd,
                   0,
                   sizeof(PIXELFORMATDESCRIPTOR) );

            pfd_ptr->pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
            pfd_ptr->pfd.nVersion   = 1;
            pfd_ptr->pfd.dwFlags    = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pfd_ptr->pfd.iPixelType = PFD_TYPE_RGBA;
            pfd_ptr->pfd.cColorBits = color_buffer_red_bits + color_buffer_green_bits + color_buffer_blue_bits;
            pfd_ptr->pfd.cRedBits   = color_buffer_red_bits;
            pfd_ptr->pfd.cGreenBits = color_buffer_green_bits;
            pfd_ptr->pfd.cBlueBits  = color_buffer_blue_bits;
            pfd_ptr->pfd.cAlphaBits = color_buffer_alpha_bits;
            pfd_ptr->pfd.cDepthBits = depth_buffer_bits;
        }
        #endif
    }

    return (ogl_pixel_format_descriptor) pfd_ptr;
}

/** Please see header for specification */
PUBLIC void ogl_pixel_format_descriptor_get_property(__in            ogl_pixel_format_descriptor          pfd,
                                                     __in            ogl_pixel_format_descriptor_property property,
                                                     __out __notnull void*                                out_result)
{
    _ogl_pixel_format_descriptor* pfd_ptr = (_ogl_pixel_format_descriptor*) pfd;

    switch (property)
    {
        case OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_RED_BITS:
        {
            *(unsigned char*) out_result = pfd_ptr->color_red_bits;

            break;
        }

        case OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_GREEN_BITS:
        {
            *(unsigned char*) out_result = pfd_ptr->color_green_bits;

            break;
        }

        case OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_BLUE_BITS:
        {
            *(unsigned char*) out_result = pfd_ptr->color_blue_bits;

            break;
        }

        case OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_ALPHA_BITS:
        {
            *(unsigned char*) out_result = pfd_ptr->color_alpha_bits;

            break;
        }

        case OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_DEPTH_BITS:
        {
            *(unsigned char*) out_result = pfd_ptr->depth_bits;

            break;
        }

#ifdef _WIN32
        case OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_DESCRIPTOR_PTR:
        {
            *(PIXELFORMATDESCRIPTOR**) out_result = &pfd_ptr->pfd;

            break;
        }
#endif

        default: 
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized PFD property [%d] requested",
                              property);
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC void ogl_pixel_format_descriptor_release(__in __notnull ogl_pixel_format_descriptor pfd)
{
    delete (_ogl_pixel_format_descriptor*) pfd;

    pfd = NULL;
}