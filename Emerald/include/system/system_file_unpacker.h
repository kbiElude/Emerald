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
    /* not settable, system_hashed_ansi_string */
    SYSTEM_FILE_UNPACKER_PROPERTY_PACKED_FILENAME,

    /* not settable, uint32_t. */
    SYSTEM_FILE_UNPACKER_PROPERTY_N_OF_EMBEDDED_FILES
} system_file_unpacker_property;

/** TODO.
 *
 *  The call does not leave until the unpacking process finishes!
 *
 **/
PUBLIC EMERALD_API system_file_unpacker system_file_unpacker_create(system_hashed_ansi_string packed_filename);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_get_file_property(system_file_unpacker               unpacker,
                                                               uint32_t                           file_index,
                                                               system_file_unpacker_file_property property,
                                                               void*                              out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_get_property(system_file_unpacker          unpacker,
                                                          system_file_unpacker_property property,
                                                          void*                         out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_unpacker_release(system_file_unpacker unpacker);

#endif /* SYSTEM_FILE_UNPACKER_H */