/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"

/** Internal types */
typedef struct _system_pixel_format
{
    #ifdef _WIN32
        PIXELFORMATDESCRIPTOR pf;
    #endif

    unsigned char color_alpha_bits;
    unsigned char color_blue_bits;
    unsigned char color_green_bits;
    unsigned char color_red_bits;
    unsigned char depth_bits;
    unsigned char n_samples;

    _system_pixel_format()
    {
        #ifdef _WIN32
        {
            memset(&pf,
                   0,
                   sizeof(pf) );
        }
        #endif

        color_alpha_bits = 0;
        color_blue_bits  = 0;
        color_green_bits = 0;
        color_red_bits   = 0;
        depth_bits       = 0;
        n_samples        = 1;
    }
} _system_pixel_format;

/** Please see header for specification */
PRIVATE void _system_pixel_format_release(__in __notnull __deallocate(mem) void* descriptor)
{
    /* Nothing to do - refcount impl automatically releases the descriptor after this func finishes executing */
}

/** Please see header for specification */
PUBLIC EMERALD_API system_pixel_format system_pixel_format_create(__in unsigned char color_buffer_red_bits,
                                                                  __in unsigned char color_buffer_green_bits,
                                                                  __in unsigned char color_buffer_blue_bits,
                                                                  __in unsigned char color_buffer_alpha_bits,
                                                                  __in unsigned char depth_buffer_bits,
                                                                  __in unsigned char n_samples)
{
    _system_pixel_format* pf_ptr = new (std::nothrow) _system_pixel_format;

    ASSERT_ALWAYS_SYNC(pf_ptr != NULL,
                       "Out of memory while allocating pixel format descriptor.");

    if (pf_ptr != NULL)
    {
        pf_ptr->color_red_bits   = color_buffer_red_bits;
        pf_ptr->color_green_bits = color_buffer_green_bits;
        pf_ptr->color_blue_bits  = color_buffer_blue_bits;
        pf_ptr->color_alpha_bits = color_buffer_alpha_bits;
        pf_ptr->depth_bits       = depth_buffer_bits;
        pf_ptr->n_samples        = n_samples;

        #ifdef _WIN32
        {
            memset(&pf_ptr->pf,
                   0,
                   sizeof(PIXELFORMATDESCRIPTOR) );

            pf_ptr->pf.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
            pf_ptr->pf.nVersion   = 1;
            pf_ptr->pf.dwFlags    = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pf_ptr->pf.iPixelType = PFD_TYPE_RGBA;
            pf_ptr->pf.cColorBits = color_buffer_red_bits + color_buffer_green_bits + color_buffer_blue_bits;
            pf_ptr->pf.cRedBits   = color_buffer_red_bits;
            pf_ptr->pf.cGreenBits = color_buffer_green_bits;
            pf_ptr->pf.cBlueBits  = color_buffer_blue_bits;
            pf_ptr->pf.cAlphaBits = color_buffer_alpha_bits;
            pf_ptr->pf.cDepthBits = depth_buffer_bits;
        }
        #endif
    }

    return (system_pixel_format) pf_ptr;
}

/** Please see header for specification */
PUBLIC system_pixel_format system_pixel_format_create_copy(__in system_pixel_format src_pf)
{
    _system_pixel_format* new_pf_ptr = new (std::nothrow) _system_pixel_format( *(_system_pixel_format*) src_pf);

    ASSERT_ALWAYS_SYNC(new_pf_ptr != NULL,
                       "Out of memory");

    return (system_pixel_format) new_pf_ptr;
}

/** Please see header for specification */
PUBLIC void system_pixel_format_get_property(__in            system_pixel_format          pf,
                                             __in            system_pixel_format_property property,
                                             __out __notnull void*                        out_result)
{
    _system_pixel_format* pf_ptr = (_system_pixel_format*) pf;

    switch (property)
    {
        case SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS:
        {
            *(unsigned char*) out_result = pf_ptr->color_red_bits;

            break;
        }

        case SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS:
        {
            *(unsigned char*) out_result = pf_ptr->color_green_bits;

            break;
        }

        case SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS:
        {
            *(unsigned char*) out_result = pf_ptr->color_blue_bits;

            break;
        }

        case SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS:
        {
            *(unsigned char*) out_result = pf_ptr->color_alpha_bits;

            break;
        }

        case SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS:
        {
            *(unsigned char*) out_result = pf_ptr->depth_bits;

            break;
        }

#ifdef _WIN32
        case SYSTEM_PIXEL_FORMAT_PROPERTY_DESCRIPTOR_PTR:
        {
            *(PIXELFORMATDESCRIPTOR**) out_result = &pf_ptr->pf;

            break;
        }
#endif

        case SYSTEM_PIXEL_FORMAT_PROPERTY_N_SAMPLES:
        {
            *(unsigned char*) out_result = pf_ptr->n_samples;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized PFD property [%d] requested",
                              property);
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_pixel_format_release(__in __notnull system_pixel_format pf)
{
    delete (_system_pixel_format*) pf;

    pf = NULL;
}