/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_curve_dataset.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    lw_curve_dataset curves;

    REFCOUNT_INSERT_VARIABLES
} _lw_dataset;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_dataset, lw_dataset, _lw_dataset);


/** TODO */
PRIVATE void _lw_dataset_release(void* arg)
{
    _lw_dataset* dataset_ptr = (_lw_dataset*) arg;

    if (dataset_ptr->curves != NULL)
    {
        lw_curve_dataset_release(dataset_ptr->curves);

        dataset_ptr->curves = NULL;
    }
}


/* Please see header for specification */
PUBLIC EMERALD_API void lw_dataset_apply_to_scene(__in __notnull lw_dataset dataset,
                                                  __in           scene      scene)
{
    _lw_dataset* dataset_ptr = (_lw_dataset*) dataset;

    if (dataset_ptr->curves != NULL)
    {
        lw_curve_dataset_apply_to_scene(dataset_ptr->curves,
                                        scene);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_dataset lw_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    _lw_dataset* dataset_ptr = new (std::nothrow) _lw_dataset;

    ASSERT_ALWAYS_SYNC(dataset_ptr != NULL, "Out of memory");
    if (dataset_ptr != NULL)
    {
        memset(dataset_ptr,
               0,
               sizeof(_lw_dataset) );

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(dataset_ptr,
                                                       _lw_dataset_release,
                                                       OBJECT_TYPE_LW_DATASET,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave data-sets\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }

    return (lw_dataset) dataset_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_dataset_get_property(__in  __notnull lw_dataset          dataset,
                                                __in            lw_dataset_property property,
                                                __out __notnull void*               out_result)
{
    _lw_dataset* dataset_ptr = (_lw_dataset*) dataset;

    switch (property)
    {
        case LW_DATASET_PROPERTY_CURVE_DATASET:
        {
            *(lw_curve_dataset*) out_result = dataset_ptr->curves;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_dataset_property value");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_dataset lw_dataset_load(__in __notnull system_hashed_ansi_string name,
                                              __in __notnull system_file_serializer    serializer)
{
    lw_dataset dataset = lw_dataset_create(name);

    ASSERT_ALWAYS_SYNC(dataset != NULL,
                       "LW dataset creation failed");
    if (dataset != NULL)
    {
        _lw_dataset* dataset_ptr = (_lw_dataset*) dataset;

        /* Load the curve data-set */
        dataset_ptr->curves = lw_curve_dataset_load(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                            " curve data-set"),
                                                    serializer);
    }

    return dataset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_dataset_save(__in __notnull lw_dataset             dataset,
                                        __in __notnull system_file_serializer serializer)
{
    _lw_dataset* dataset_ptr = (_lw_dataset*) dataset;

    /* Store the curve data-set */
    ASSERT_ALWAYS_SYNC(dataset_ptr->curves != NULL,
                       "Curve data-set is NULL");

    lw_curve_dataset_save(dataset_ptr->curves,
                          serializer);
}
