/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef GFX_BMP_H
#define GFX_BMP_H

#include "gfx/gfx_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_file(__in __notnull system_hashed_ansi_string file_name);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_memory(__in __notnull const unsigned char*  /* data_ptr */);


#endif /* GFX_BMP_H */
