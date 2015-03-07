/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_FILE_UNPACKER_H
#define SYSTEM_FILE_UNPACKER_H

#include "system/system_types.h"


typedef enum
{
    SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER,
    SYSTEM_FILE_UNPACKER_FILE_PROPERTY_NAME
} system_file_unpacker_file_property;

typedef enum
{
    /* not settable, uint32_t. */
    SYSTEM_FILE_UNPACKER_PROPERTY_N_OF_EMBEDDED_FILES
} system_file_unpacker_property;

/** TODO */
PUBLIC EMERALD_API system_file_unpacker system_file_unpacker_create(__in __notnull system_hashed_ansi_string packed_filename);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_get_file_property(__in  __notnull system_file_unpacker               unpacker,
                                                               __in            uint32_t                           file_index,
                                                               __in            system_file_unpacker_file_property property,
                                                               __out __notnull void*                              out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_get_property(__in  __notnull system_file_unpacker          unpacker,
                                                          __in            system_file_unpacker_property property,
                                                          __out __notnull void*                         out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_release(__in __notnull __post_invalid system_file_unpacker unpacker);

#endif /* SYSTEM_FILE_UNPACKER_H */