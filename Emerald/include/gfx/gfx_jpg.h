/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef GFX_JPG_H
#define GFX_JPG_H

#include "gfx/gfx_types.h"
#include "system/system_types.h"

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_jpg_load_from_file(system_hashed_ansi_string file_name,
                                                    system_file_unpacker      file_unpacker);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_jpg_load_from_memory(const unsigned char* data_ptr,
                                                      unsigned int         data_size);


#endif /* GFX_JPG_H */
