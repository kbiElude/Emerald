/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Holds app-wide data.
 */
#ifndef SYSTEM_GLOBAL_H
#define SYSTEM_GLOBAL_H

#include "system_types.h"

typedef enum
{
    /* indexed property; not_settable system_hashed_ansi_string
     *
     * For any scene the scene_multiloader loads, an asset path is taken and added to the global
     * asset path storage. Later on, when gfx_image instances are created, all asset paths are
     * enumerated if the source image file cannot be found. This is necessary, since gfx_images
     * are initialized with file-paths as extracted on the machine, where the scene was exported on.
     * As long as the assets are located within the same directory as the scene blob, the asset paths
     * let Emerald locate the assets.
     */
    SYSTEM_GLOBAL_PROPERTY_ASSET_PATH,

    /* indexed property; not settable system_file_unpacker
     *
     * For any scene the scene_multiloader loads, one of the loops iterates over all available unpackers
     * in order to check if the file that needs to be loaded is not located within a recognized unpacker
     * instance.
     */
     SYSTEM_GLOBAL_PROPERTY_FILE_UNPACKER,

    /* general property; not_settable uint32_t
     *
     * Number of asset paths, available for extraction via SYSTEM_GLOBAL_PROPERTY_ASSET_PATH.
     */
    SYSTEM_GLOBAL_PROPERTY_N_ASSET_PATHS,

    /* general property; not settable uint32_t
     *
     * Number of active file unpackers. Each unpacker can be extracted via SYSTEM_GLOBAL_PROPERTY_FILE_UNPACKER
     */
     SYSTEM_GLOBAL_PROPERTY_N_FILE_UNPACKERS
} system_global_property;


/** Deinitializes global system storage. Should only be called once when
 *  Emerald is about to be unloaded from the process memory.
 */
PUBLIC void system_global_deinit();

/** Adds a new asset path to the global storage. The path will only
 *  be added, if it has not already been added in the past.
 *
 *  This function is MT-safe.
 *
 *  @param asset_path New path to remember. Must not be NULL.
 */
PUBLIC EMERALD_API void system_global_add_asset_path(__in __notnull system_hashed_ansi_string asset_path);

/** Adds a new file unpacker to the global storage. The unpacker will only
 *  be added, if it has not already been added in the past.
 *
 *  Right before the release time, the file unpacker should call
 *  system_global_delete_file_unpacker() to remove the instance from
 *  the global storage.
 *
 *  The global storage does not claim ownership of the unpacker.
 *  This function is MT-safe.
 *
 *  @param file_unpacker New file unpacker to remember. Must not be NULL.
 */
PUBLIC EMERALD_API void system_global_add_file_unpacker(__in __notnull system_file_unpacker unpacker);

/** TODO */
PUBLIC EMERALD_API void system_global_delete_file_unpacker(__in __notnull system_file_unpacker unpacker);

/** Retrieves a general property value of the system global storage.
 *  Please check documentation of system_global_property for more
 *  details.
 *
 *  @param property   General property to use for the query.
 *  @param out_result Deref will be used to hold the reuslt value.
 *                    Must not be NULL.
 *
 **/
PUBLIC EMERALD_API void system_global_get_general_property(__in            system_global_property property,
                                                           __out __notnull void*                  out_result);

/** Retrieves an indexed property value from the system global storage.
 *  Please check documentation of system_global_property for more
 *  details.
 *
 *  @param property   Indexed property to use for the query.
 *  @param index      Index to use for the query.
 *  @param out_result Deref will be used to hold the result value.
 *                    Must not be NULL.
 *
 **/
PUBLIC EMERALD_API void system_global_get_indexed_property(__in            system_global_property property,
                                                           __in            uint32_t               index,
                                                           __out __notnull void*                  out_result);

/** Initializes system global storage. Must only be called once,
 *  preferably at Emerald's initialization time.
 **/
PUBLIC void system_global_init();

#endif /* SYSTEM_GLOBAL_H */
