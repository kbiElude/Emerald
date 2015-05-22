/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef SYSTEM_FILE_ENUMERATOR_H
#define SYSTEM_FILE_ENUMERATOR_H

#include "dll_exports.h"
#include "system_types.h"

typedef enum
{
    SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_LOAD,
    SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_SAVE
} system_file_enumerator_file_operation;

typedef enum
{
    SYSTEM_FILE_ENUMERATOR_FILE_PROPERTY_FILE_NAME /* not settable, system_hashed_ansi_string */
} system_file_enumerator_file_property;

typedef enum
{
    SYSTEM_FILE_ENUMERATOR_PROPERTY_N_FILES, /* not settable, uint32_t */
} system_file_enumerator_property;


/** TODO.
 *
 *  This function DOES NOT require a system_file_enumerator instance.
 *
 **/
PUBLIC EMERALD_API system_hashed_ansi_string system_file_enumerator_choose_file_via_ui(__in                                   system_file_enumerator_file_operation operation,
                                                                                       __in                                   unsigned int                          n_filters,
                                                                                       __in_ecount(n_filters) __notnull const system_hashed_ansi_string*            filter_descriptions,
                                                                                       __in_ecount(n_filters) __notnull const system_hashed_ansi_string*            filter_extensions,
                                                                                       __in                   __notnull       system_hashed_ansi_string             dialog_title);

/** TODO */
PUBLIC EMERALD_API system_file_enumerator system_file_enumerator_create(__in __notnull system_hashed_ansi_string file_pattern);

/** TODO */
PUBLIC EMERALD_API void system_file_enumerator_get_file_property(__in  __notnull system_file_enumerator               enumerator,
                                                                 __in            uint32_t                             n_file,
                                                                 __in            system_file_enumerator_file_property property,
                                                                 __out __notnull void*                                out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_enumerator_get_property(__in  __notnull system_file_enumerator          enumerator,
                                                            __in            system_file_enumerator_property property,
                                                            __out __notnull void*                           out_result);

/** Checks if user-specified file can be found by the system.
 *
 *  This function DOES NOT require a system_file_enumerator instance.
 *
 *  @param file_name Name of the file to look for.
 *
 *  @return true if the file was found, false otherwise.
 **/
PUBLIC EMERALD_API bool system_file_enumerator_is_file_present(__in __notnull system_hashed_ansi_string file_name);

/** Checks if user-specified file can be found in a system_file_unpacker instance.
 *
 *  This function DOES NOT require a system_file_enumerator instance.
 *
 *  @param file_unpacker  system_file_unpacker instance to use.
 *  @param file_name      Name of the file to look for.
 *  @param out_file_index If not NULL, *out_file_index will be set to the corresponding
 *                        file index within the @param file_unpacker instance, IF the
 *                        seeked file was found.
 *
 *  @return true if the file was found, false otherwise.
 */
PUBLIC EMERALD_API bool system_file_enumerator_is_file_present_in_system_file_unpacker(__in __notnull system_file_unpacker      file_unpacker,
                                                                                       __in __notnull system_hashed_ansi_string file_name,
                                                                                       __in           bool                      use_exact_match,
                                                                                       __out_opt      unsigned int*             out_file_index);

/** TODO */
PUBLIC EMERALD_API void system_file_enumerator_release(__in __notnull system_file_enumerator);

#endif /* SYSTEM_FILE_ENUMERATOR_H */
