/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_FILE_PACKER_H
#define SYSTEM_FILE_PACKER_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API bool system_file_packer_add_file(system_file_packer        packer,
                                                    system_hashed_ansi_string filename);

/** TODO */
PUBLIC EMERALD_API system_file_packer system_file_packer_create();

/** TODO */
PUBLIC EMERALD_API void system_file_packer_release(system_file_packer packer);

/** TODO */
PUBLIC EMERALD_API bool system_file_packer_save(system_file_packer        packer,
                                                system_hashed_ansi_string target_packed_filename);

#endif /* SYSTEM_FILE_PACKER_H */