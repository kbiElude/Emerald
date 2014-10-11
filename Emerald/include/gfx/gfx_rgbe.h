/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef GFX_RGBE_H
#define GFX_RGBE_H

#include "gfx/gfx_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_rgbe_load_from_file(__in __notnull system_hashed_ansi_string file_name);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_rgbe_load_from_memory(__in system_hashed_ansi_string name, __in __notnull const char*  /* data_ptr */);


#endif /* GFX_RGBE_H */
