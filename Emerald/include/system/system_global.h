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
    /* Please see SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR documentation for more details */
    SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_DO_NOT_SERIALIZE,

    /* Please see SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR documentation for more details */
    SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE
} system_global_curve_container_behavior;

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

    /* general property; settable. system_global_curve_container_behavior.
     *
     * Affects curve container instance behavior as defined below:
     *
     * 1) SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_DO_NOT_SERIALIZE
     *
     * Serialization is disabled. Curve container is instantiated as requested on the API-level, and all
     * subsequent calls modifying the curve container state will be executed as expected. The instance's
     * state will not be serialized to a text file under "curves/curve_name.curve", nor will such file be
     * monitored for changes.
     * This is the default setting.
     *
     * 2) SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE
     *
     * If "curves/curve_name.curve" file is not present, the curve container is instantiated as requested
     * on the API-level. Furthermore, all subsequent calls that modify curve container configuration will
     * behave as expected. At the same time, the file will be re-generated every time the curve container
     * configuration is changed. Any changes applied to the file will be ignored.
     *
     * If "curves/curve_name.curve" file is present, the curve container is instantiated as requested
     * on the API_level, but its state will be configured as described in the text file. Should the text
     * file be malformed, the curve container's state will be reset and an assertion failure will be
     * generated.
     * For details on the text file structure, please see curve_container documentation.
     *
     */
     SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR,

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
PUBLIC EMERALD_API void system_global_add_asset_path(system_hashed_ansi_string asset_path);

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
PUBLIC EMERALD_API void system_global_add_file_unpacker(system_file_unpacker unpacker);

/** TODO */
PUBLIC EMERALD_API void system_global_delete_file_unpacker(system_file_unpacker unpacker);

/** Retrieves a general property value of the system global storage.
 *  Please check documentation of system_global_property for more
 *  details.
 *
 *  @param property   General property to use for the query.
 *  @param out_result Deref will be used to hold the reuslt value.
 *                    Must not be NULL.
 *
 **/
PUBLIC EMERALD_API void system_global_get_general_property(system_global_property property,
                                                           void*                  out_result);

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
PUBLIC EMERALD_API void system_global_get_indexed_property(system_global_property property,
                                                           uint32_t               index,
                                                           void*                  out_result);

/** Initializes system global storage. Must only be called once,
 *  preferably at Emerald's initialization time.
 **/
PUBLIC void system_global_init();

/** TODO */
PUBLIC EMERALD_API void system_global_set_general_property(system_global_property property,
                                                           const void*            data);

#endif /* SYSTEM_GLOBAL_H */
