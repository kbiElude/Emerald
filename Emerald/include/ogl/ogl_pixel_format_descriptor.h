/**
 *
 * Emerald (kbi/elude @2015)
 *
 * NOTE: Internal use only.
 */
#ifndef OGL_PIXEL_FORMAT_DESCRIPTOR_H
#define OGL_PIXEL_FORMAT_DESCRIPTOR_H

#include "ogl/ogl_types.h"

typedef enum
{
    /* not settable, unsigned char */
    OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_ALPHA_BITS,

    /* not settable, unsigned char */
    OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_BLUE_BITS,

    /* not settable, unsigned char */
    OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_GREEN_BITS,

    /* not settable, unsigned char */
    OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_RED_BITS,

    /* not settable, unsigned char */
    OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_DEPTH_BITS,

#ifdef _WIN32
    /* not settable, const PIXELFORMATDESCRIPTOR*.
     *
     * Windows-specific */
    OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_DESCRIPTOR_PTR
#endif

} ogl_pixel_format_descriptor_property;


/** TODO */
PUBLIC ogl_pixel_format_descriptor ogl_pixel_format_descriptor_create(__in unsigned char color_buffer_red_bits,
                                                                      __in unsigned char color_buffer_green_bits,
                                                                      __in unsigned char color_buffer_blue_bits,
                                                                      __in unsigned char color_buffer_alpha_bits,
                                                                      __in unsigned char depth_buffer_bits);

/** TODO */
PUBLIC void ogl_pixel_format_descriptor_get_property(__in            ogl_pixel_format_descriptor          pfd,
                                                     __in            ogl_pixel_format_descriptor_property property,
                                                     __out __notnull void*                                out_result);

/** TODO */
PUBLIC void ogl_pixel_format_descriptor_release(__in __notnull ogl_pixel_format_descriptor pfd);

#endif /* OGL_PIXEL_FORMAT_DESCRIPTOR_H */
