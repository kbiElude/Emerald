/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef GFX_JPG_H
#define GFX_JPG_H

#include "gfx/gfx_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_jpg_load_from_file(__in __notnull system_hashed_ansi_string file_name);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_jpg_load_from_memory(__in __notnull const unsigned char* data_ptr,
                                                      __in           unsigned int         data_size);


#endif /* GFX_JPG_H */
