/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Multiunpacker is an utility object which allows you to load multiple system_file_unpacker
 * instances in parallel. Each load op is submitted to the thread pool at creation time.
 * The object supports a wait operation which waits for all load ops to finish before returning
 * execution flow to the caller.
 *
 */
#ifndef SYSTEM_FILE_MULTIUNPACKER_H
#define SYSTEM_FILE_MULTIUNPACKER_H

#include "system/system_types.h"


typedef enum _system_file_multiunpacker_property
{
    /* not settable; system_file_unpacker */
    SYSTEM_FILE_MULTIUNPACKER_PROPERTY_FILE_UNPACKER,

} _system_file_multiunpacker_property;

/** TODO.
 *
 *  Leaves BEFORE all unpackers are set up.
 */
PUBLIC EMERALD_API system_file_multiunpacker system_file_multiunpacker_create(__in_ecount(n_packed_filenames) __notnull const system_hashed_ansi_string* packed_filenames,
                                                                              __in                                      unsigned int                     n_packed_filenames);

/** TODO */
PUBLIC EMERALD_API void system_file_multiunpacker_get_indexed_property(__in  __notnull system_file_multiunpacker           multiunpacker,
                                                                       __in            unsigned int                        n_unpacker,
                                                                       __in  __notnull _system_file_multiunpacker_property property,
                                                                       __out __notnull void*                               out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_multiunpacker_wait_till_ready(__in __notnull system_file_multiunpacker multiunpacker);

/** TODO */
PUBLIC EMERALD_API void system_file_multiunpacker_release(__in __notnull __post_invalid system_file_multiunpacker multiunpacker);

#endif /* SYSTEM_FILE_MULTIUNPACKER_H */