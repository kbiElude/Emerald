/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_PIXEL_FORMAT_DESCRIPTOR_H
#define OGL_PIXEL_FORMAT_DESCRIPTOR_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_pixel_format_descriptor,
                             ogl_pixel_format_descriptor)

/** TODO */
PUBLIC EMERALD_API ogl_pixel_format_descriptor ogl_pixel_format_descriptor_create(__in __notnull system_hashed_ansi_string name,
                                                                                  __in           unsigned char             color_buffer_red_bits,
                                                                                  __in           unsigned char             color_buffer_green_bits,
                                                                                  __in           unsigned char             color_buffer_blue_bits,
                                                                                  __in           unsigned char             color_buffer_alpha_bits,
                                                                                  __in           unsigned char             depth_buffer_bits);
                                      
/** TODO */
PUBLIC EMERALD_API bool ogl_pixel_format_descriptor_get(__in            ogl_pixel_format_descriptor_field, 
                                                        __in  __notnull ogl_pixel_format_descriptor, 
                                                        __out __notnull void*);

#ifdef _WIN32
    /** TODO */
    PUBLIC const PIXELFORMATDESCRIPTOR* _ogl_pixel_format_descriptor_get_descriptor(__in __notnull ogl_pixel_format_descriptor);
#endif /* _WIN32 */

#endif /* OGL_PIXEL_FORMAT_DESCRIPTOR_H */
