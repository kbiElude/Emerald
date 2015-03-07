/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef GFX_PNG_H
#define GFX_PNG_H

#include "gfx/gfx_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_png_load_from_file(__in __notnull system_hashed_ansi_string file_name,
                                                    __in __notnull system_file_unpacker      file_unpacker);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_png_load_from_memory(__in __notnull const unsigned char* data_ptr);


#endif /* OGL_PIXEL_FORMAT_DESCRIPTOR_H */
