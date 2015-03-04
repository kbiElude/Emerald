/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_FILE_PACKER_H
#define SYSTEM_FILE_PACKER_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API bool system_file_packer_add_file(__in __notnull system_file_packer        packer,
                                                    __in __notnull system_hashed_ansi_string filename);

/** TODO */
PUBLIC EMERALD_API system_file_packer system_file_packer_create();

/** TODO */
PUBLIC EMERALD_API void system_file_packer_release(__in __notnull __post_invalid system_file_packer packer);

/** TODO */
PUBLIC EMERALD_API bool system_file_packer_save(__in __notnull system_file_packer        packer,
                                                __in __notnull system_hashed_ansi_string target_packed_filename);

#endif /* SYSTEM_FILE_PACKER_H */