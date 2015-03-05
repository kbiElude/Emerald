/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_FILE_UNPACKER_H
#define SYSTEM_FILE_UNPACKER_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API system_file_unpacker system_file_unpacker_create(__in __notnull system_hashed_ansi_string packed_filename);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_release(__in __notnull __post_invalid system_file_unpacker unpacker);

#endif /* SYSTEM_FILE_UNPACKER_H */