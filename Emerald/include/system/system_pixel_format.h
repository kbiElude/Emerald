/**
 *
 * Emerald (kbi/elude @2015)
 *
 * NOTE: Internal use only.
 */
#ifndef SYSTEM_PIXEL_FORMAT_H
#define SYSTEM_PIXEL_FORMAT_H

#include "system/system_types.h"

typedef enum
{
    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS,

    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS,

    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS,

    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS,

    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,

    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_N_SAMPLES,

#ifdef _WIN32
    /* not settable, const PIXELFORMATDESCRIPTOR*.
     *
     * Windows-specific. Not affected by n_samples property.
     **/
    SYSTEM_PIXEL_FORMAT_PROPERTY_DESCRIPTOR_PTR
#endif

} system_pixel_format_property;


/** TODO */
PUBLIC EMERALD_API system_pixel_format system_pixel_format_create(__in unsigned char color_buffer_red_bits,
                                                                  __in unsigned char color_buffer_green_bits,
                                                                  __in unsigned char color_buffer_blue_bits,
                                                                  __in unsigned char color_buffer_alpha_bits,
                                                                  __in unsigned char depth_buffer_bits,
                                                                  __in unsigned char n_samples);

/** TODO */
PUBLIC system_pixel_format system_pixel_format_create_copy(__in system_pixel_format src_pf);

/** TODO */
PUBLIC void system_pixel_format_get_property(__in            system_pixel_format          pf,
                                             __in            system_pixel_format_property property,
                                             __out __notnull void*                        out_result);

/** TODO */
PUBLIC EMERALD_API void system_pixel_format_release(__in __notnull system_pixel_format pf);

#endif /* SYSTEM_PIXEL_FORMAT_H */
