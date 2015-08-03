/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "curve/curve_watchdog.h"
#include "system/system_global.h"
#include "system/system_resizable_vector.h"


/* Holds system global storage descriptor */
typedef struct _system_global
{
    system_resizable_vector                asset_paths;    /* system_hashed_ansi_string */
    system_global_curve_container_behavior curve_container_behavior;
    system_resizable_vector                file_unpackers; /* system_file_unpacker      */

    _system_global()
    {
        asset_paths              = system_resizable_vector_create(4,     /* capacity              */
                                                                  true); /* should_be_thread_safe */
        curve_container_behavior = SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_DO_NOT_SERIALIZE;
        file_unpackers          = system_resizable_vector_create(4,     /* capacity              */
                                                                 true); /* should_be_thread_safe */

        ASSERT_DEBUG_SYNC(asset_paths != NULL,
                          "Could not set up asset path storage.");
        ASSERT_DEBUG_SYNC(file_unpackers != NULL,
                          "Could not set up file unpacker storage.");
    }

    ~_system_global()
    {
        if (asset_paths != NULL)
        {
            system_resizable_vector_release(asset_paths);

            asset_paths = NULL;
        }

        if (curve_container_behavior == SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE)
        {
            curve_watchdog_deinit();
        }

        if (file_unpackers != NULL)
        {
            system_resizable_vector_release(file_unpackers);

            file_unpackers = NULL;
        }
    }
} _system_global;


PRIVATE _system_global* global_ptr = NULL;


/** Please see header for spec */
PUBLIC void system_global_deinit()
{
    if (global_ptr != NULL)
    {
        delete global_ptr;

        global_ptr = NULL;
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_add_asset_path(system_hashed_ansi_string asset_path)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage not initialized");
    ASSERT_DEBUG_SYNC(asset_path != NULL,
                      "Asset path is NULL");
    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(asset_path) > 0,
                      "Added asset path has a length of zero");

    /* Add the asset path */
    if (system_resizable_vector_find(global_ptr->asset_paths,
                                     asset_path) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(global_ptr->asset_paths,
                                     asset_path);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_add_file_unpacker(system_file_unpacker unpacker)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage not initialized");
    ASSERT_DEBUG_SYNC(unpacker != NULL,
                      "Input file unpacker instance is NULL");

    /* Add the asset path */
    if (system_resizable_vector_find(global_ptr->file_unpackers,
                                     unpacker) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(global_ptr->file_unpackers,
                                     unpacker);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_delete_file_unpacker(system_file_unpacker unpacker)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage not initialized");
    ASSERT_DEBUG_SYNC(unpacker != NULL,
                      "Input file unpacker instance is NULL");

    /* Add the asset path */
    size_t file_unpacker_index = system_resizable_vector_find(global_ptr->file_unpackers,
                                                              unpacker);

    ASSERT_DEBUG_SYNC(file_unpacker_index != ITEM_NOT_FOUND,
                      "Input file unpacker instance was not recognized.");

    if (file_unpacker_index != ITEM_NOT_FOUND)
    {
        system_resizable_vector_delete_element_at(global_ptr->file_unpackers,
                                                  file_unpacker_index);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_get_general_property(system_global_property property,
                                                           void*                  out_result)
{
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage not initialized");

    switch (property)
    {
        case SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR:
        {
            *(system_global_curve_container_behavior*) out_result = global_ptr->curve_container_behavior;

            break;
        }

        case SYSTEM_GLOBAL_PROPERTY_N_ASSET_PATHS:
        {
            system_resizable_vector_get_property(global_ptr->asset_paths,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        case SYSTEM_GLOBAL_PROPERTY_N_FILE_UNPACKERS:
        {
            system_resizable_vector_get_property(global_ptr->file_unpackers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "system_global_property value was not recognized");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_get_indexed_property(system_global_property property,
                                                           uint32_t               index,
                                                           void*                  out_result)
{
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage was not initialized.");

    switch (property)
    {
        case SYSTEM_GLOBAL_PROPERTY_ASSET_PATH:
        {
            if (!system_resizable_vector_get_element_at(global_ptr->asset_paths,
                                                        index,
                                                        out_result))
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve asset path at index [%d]",
                                  index);
            }

            break;
        }

        case SYSTEM_GLOBAL_PROPERTY_FILE_UNPACKER:
        {
            if (!system_resizable_vector_get_element_at(global_ptr->file_unpackers,
                                                        index,
                                                        out_result))
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve file unpacker at index [%d]",
                                  index);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "system_global_property value was not recognized");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void system_global_init()
{
    ASSERT_DEBUG_SYNC(global_ptr == NULL,
                      "Global storage already initialized");

    global_ptr = new (std::nothrow) _system_global;

    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                       "Out of memory");
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_set_general_property(system_global_property property,
                                                           const void*            data)
{
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage was not initialized.");

    switch (property)
    {
        case SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR:
        {
            system_global_curve_container_behavior new_behavior = *(system_global_curve_container_behavior*) data;

            if (new_behavior                         == SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE        &&
                global_ptr->curve_container_behavior == SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_DO_NOT_SERIALIZE)
            {
                global_ptr->curve_container_behavior = new_behavior;

                curve_watchdog_init();
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid setter call for the SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR property");
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_global_property value.");
        }
    } /* switch (property) */
}