/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_global.h"
#include "system/system_resizable_vector.h"


/* Holds system global storage descriptor */
typedef struct _system_global
{
    system_resizable_vector asset_paths;

    _system_global()
    {
        asset_paths = system_resizable_vector_create(4, /* capacity */
                                                     sizeof(system_hashed_ansi_string),
                                                     true); /* should_be_thread_safe */

        ASSERT_DEBUG_SYNC(asset_paths != NULL,
                          "Could not set up asset path storage.");
    }

    ~_system_global()
    {
        if (asset_paths != NULL)
        {
            system_resizable_vector_release(asset_paths);

            asset_paths = NULL;
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
PUBLIC EMERALD_API void system_global_add_asset_path(__in __notnull system_hashed_ansi_string asset_path)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage not initialized");
    ASSERT_DEBUG_SYNC(asset_path != NULL,
                      "Asset path is NULL");
    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(asset_path) > 0,
                      "Added asset path has a length of zero");

    if (system_resizable_vector_find(global_ptr->asset_paths,
                                     asset_path) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(global_ptr->asset_paths,
                                     asset_path);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_global_get_general_property(__in            system_global_property property,
                                                           __out __notnull void*                  out_result)
{
    ASSERT_DEBUG_SYNC(global_ptr != NULL,
                      "Global storage not initialized");

    switch (property)
    {
        case SYSTEM_GLOBAL_PROPERTY_N_ASSET_PATHS:
        {
            *(uint32_t*) out_result = system_resizable_vector_get_amount_of_elements(global_ptr->asset_paths);

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
PUBLIC EMERALD_API void system_global_get_indexed_property(__in            system_global_property property,
                                                           __in            uint32_t               index,
                                                           __out __notnull void*                  out_result)
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

