/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef GFX_BMP_H
#define GFX_BMP_H

#include "gfx/gfx_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_file(system_hashed_ansi_string file_name,
                                                    system_file_unpacker      file_unpacker);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_memory(const unsigned char* data_ptr);


#endif /* GFX_BMP_H */
