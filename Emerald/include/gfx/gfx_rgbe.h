/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef GFX_RGBE_H
#define GFX_RGBE_H

#include "gfx/gfx_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_rgbe_load_from_file(system_hashed_ansi_string file_name,
                                                     system_file_unpacker      file_unpacker);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_rgbe_load_from_memory(system_hashed_ansi_string name,
                                                       const char*               data_ptr);


#endif /* GFX_RGBE_H */
