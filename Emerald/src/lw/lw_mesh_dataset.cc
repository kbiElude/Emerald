/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_mesh_dataset.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{


    REFCOUNT_INSERT_VARIABLES
} _lw_mesh_dataset;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_mesh_dataset, lw_mesh_dataset, _lw_mesh_dataset);


/** TODO */
PRIVATE void _lw_mesh_dataset_release(void* arg)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) arg;

    // ..
}


/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_apply_to_scene(__in __notnull lw_mesh_dataset dataset,
                                                       __in           scene           scene)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;

    // ..
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_mesh_dataset lw_mesh_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    _lw_mesh_dataset* dataset_ptr = new (std::nothrow) _lw_mesh_dataset;

    ASSERT_ALWAYS_SYNC(dataset_ptr != NULL, "Out of memory");
    if (dataset_ptr != NULL)
    {
        memset(dataset_ptr,
               0,
               sizeof(_lw_mesh_dataset) );

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(dataset_ptr,
                                                       _lw_mesh_dataset_release,
                                                       OBJECT_TYPE_LW_MESH_DATASET,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave mesh data-sets\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }

    return (lw_mesh_dataset) dataset_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_get_property(__in  __notnull lw_mesh_dataset          dataset,
                                                     __in            lw_mesh_dataset_property property,
                                                     __out __notnull void*                    out_result)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;

    switch (property)
    {
        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_mesh_dataset_property value");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_mesh_dataset lw_mesh_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                        __in __notnull system_file_serializer    serializer)
{
    lw_mesh_dataset dataset = lw_mesh_dataset_create(name);

    ASSERT_ALWAYS_SYNC(dataset != NULL,
                       "LW mesh dataset creation failed");
    if (dataset != NULL)
    {
        _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;

        // ..
    }

    return dataset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_save(__in __notnull lw_mesh_dataset        dataset,
                                             __in __notnull system_file_serializer serializer)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;

    // ..
}
