/**
 *
 * Emerald (kbi/elude @2015)
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

    /* not settable, unsigned char */
    SYSTEM_PIXEL_FORMAT_PROPERTY_STENCIL_BITS,

#ifdef _WIN32
    /* not settable, const PIXELFORMATDESCRIPTOR*.
     *
     * Windows-specific. Not affected by n_samples property.
     **/
    SYSTEM_PIXEL_FORMAT_PROPERTY_DESCRIPTOR_PTR
#endif

} system_pixel_format_property;


static const unsigned char SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES = 0xFF;


/** TODO
 *
 *  @param color_buffer_red_bits   TODO.
 *  @param color_buffer_green_bits TODO.
 *  @param color_buffer_blue_bits  TODO.
 *  @param color_buffer_alpha_bits TODO.
 *  @param depth_buffer_bits       TODO.
 *  @param n_samples               TODO. Can also be equal to SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
 *                                 in which case a maximum number of samples reported by the driver to be supported
 *                                 for the requested pixel format configuration will be used at context initialization
 *                                 time.
 *                                 NOTE: the actual number of samples is NOT determined at creation time, but at context's
 *                                       init time. If you call get_property() for the n_samples enum, you will be returned
 *                                       the constant value, NOT the number of samples that will be used for rendering!
 *
 *  @param stencil_buffer_bits     TODO.
 *
 *  @return TODO
 */
PUBLIC EMERALD_API system_pixel_format system_pixel_format_create(unsigned char color_buffer_red_bits,
                                                                  unsigned char color_buffer_green_bits,
                                                                  unsigned char color_buffer_blue_bits,
                                                                  unsigned char color_buffer_alpha_bits,
                                                                  unsigned char depth_buffer_bits,
                                                                  unsigned char n_samples,
                                                                  unsigned char stencil_buffer_bits);

/** TODO */
PUBLIC system_pixel_format system_pixel_format_create_copy(system_pixel_format src_pf);

/** TODO */
PUBLIC void system_pixel_format_get_property(system_pixel_format          pf,
                                             system_pixel_format_property property,
                                             void*                        out_result);

/** TODO */
PUBLIC EMERALD_API void system_pixel_format_release(system_pixel_format pf);

#endif /* SYSTEM_PIXEL_FORMAT_H */
