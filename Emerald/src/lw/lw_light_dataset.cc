/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_light_dataset.h"
#include "scene/scene.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    float ambient[3];

    REFCOUNT_INSERT_VARIABLES
} _lw_light_dataset;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_light_dataset, lw_light_dataset, _lw_light_dataset);


/** TODO */
PRIVATE void _lw_light_dataset_release(void* arg)
{
    _lw_light_dataset* dataset_ptr = (_lw_light_dataset*) arg;

    /* Nothing to release */
}


/* Please see header for specification */
PUBLIC EMERALD_API void lw_light_dataset_apply_to_scene(__in __notnull lw_light_dataset dataset,
                                                        __in           scene            scene)
{
    _lw_light_dataset* dataset_ptr = (_lw_light_dataset*) dataset;

    // ..
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_light_dataset lw_light_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    _lw_light_dataset* dataset_ptr = new (std::nothrow) _lw_light_dataset;

    ASSERT_ALWAYS_SYNC(dataset_ptr != NULL, "Out of memory");
    if (dataset_ptr != NULL)
    {
        memset(dataset_ptr,
               0,
               sizeof(_lw_light_dataset) );

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(dataset_ptr,
                                                       _lw_light_dataset_release,
                                                       OBJECT_TYPE_LW_LIGHT_DATASET,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave light data-sets\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }

    return (lw_light_dataset) dataset_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_light_dataset lw_light_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                          __in __notnull system_file_serializer    serializer)
{
    lw_light_dataset dataset = lw_light_dataset_create(name);

    ASSERT_ALWAYS_SYNC(dataset != NULL,
                       "LW light dataset creation failed");
    if (dataset != NULL)
    {
        _lw_light_dataset* dataset_ptr = (_lw_light_dataset*) dataset;

        system_file_serializer_read(serializer,
                                    sizeof(dataset_ptr->ambient),
                                    dataset_ptr->ambient);
    } /* if (dataset != NULL) */

    return dataset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_light_dataset_save(__in __notnull lw_light_dataset       dataset,
                                              __in __notnull system_file_serializer serializer)
{
    _lw_light_dataset* dataset_ptr = (_lw_light_dataset*) dataset;

    system_file_serializer_write(serializer,
                                 sizeof(dataset_ptr->ambient),
                                 dataset_ptr->ambient);
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_light_dataset_set_property(__in __notnull lw_light_dataset          dataset,
                                                      __in           lw_light_dataset_property property,
                                                      __in __notnull void*                     data)
{
    _lw_light_dataset* dataset_ptr = (_lw_light_dataset*) dataset;

    switch (property)
    {
        case LW_LIGHT_DATASET_PROPERTY_AMBIENT_COLOR:
        {
            memcpy(dataset_ptr->ambient,
                   data,
                   sizeof(dataset_ptr->ambient) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_light_dataset_property value");
        }
    } /* switch (property) */
}
