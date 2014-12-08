/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_curve_dataset.h"
#include "lw/lw_light_dataset.h"
#include "lw/lw_material_dataset.h"
#include "lw/lw_mesh_dataset.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    lw_curve_dataset    curves;
    lw_light_dataset    lights;
    lw_material_dataset materials;
    lw_mesh_dataset     meshes;

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

    if (dataset_ptr->lights != NULL)
    {
        lw_light_dataset_release(dataset_ptr->lights);

        dataset_ptr->lights = NULL;
    }

    if (dataset_ptr->materials != NULL)
    {
        lw_material_dataset_release(dataset_ptr->materials);

        dataset_ptr->materials = NULL;
    }

    if (dataset_ptr->meshes != NULL)
    {
        lw_mesh_dataset_release(dataset_ptr->meshes);

        dataset_ptr->meshes = NULL;
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

    if (dataset_ptr->lights != NULL)
    {
        lw_light_dataset_apply_to_scene(dataset_ptr->lights,
                                        scene);
    }

    if (dataset_ptr->materials != NULL)
    {
        lw_material_dataset_apply_to_scene(dataset_ptr->materials,
                                           scene);
    }

    if (dataset_ptr->meshes != NULL)
    {
        lw_mesh_dataset_apply_to_scene(dataset_ptr->meshes,
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

        /* Instantiate sub-datasets */
        dataset_ptr->curves    = lw_curve_dataset_create   (system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                    " curves") );
        dataset_ptr->lights    = lw_light_dataset_create   (system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                    " lights") );
        dataset_ptr->materials = lw_material_dataset_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                    " materials") );
        dataset_ptr->meshes    = lw_mesh_dataset_create    (system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                    " meshes") );

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

        case LW_DATASET_PROPERTY_LIGHT_DATASET:
        {
            *(lw_light_dataset*) out_result = dataset_ptr->lights;

            break;
        }

        case LW_DATASET_PROPERTY_MATERIAL_DATASET:
        {
            *(lw_material_dataset*) out_result = dataset_ptr->materials;

            break;
        }

        case LW_DATASET_PROPERTY_MESH_DATASET:
        {
            *(lw_mesh_dataset*) out_result = dataset_ptr->meshes;

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
        if (dataset_ptr->curves != NULL)
        {
            lw_curve_dataset_release(dataset_ptr->curves);

            dataset_ptr->curves = NULL;
        }

        dataset_ptr->curves = lw_curve_dataset_load(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                            " curve data-set"),
                                                    serializer);

        /* Load the light data-set */
        if (dataset_ptr->lights != NULL)
        {
            lw_light_dataset_release(dataset_ptr->lights);

            dataset_ptr->lights = NULL;
        }

        dataset_ptr->lights = lw_light_dataset_load(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                            " light data-set"),
                                                    serializer);

        /* Load the material data-set */
        if (dataset_ptr->materials != NULL)
        {
            lw_material_dataset_release(dataset_ptr->materials);

            dataset_ptr->materials = NULL;
        }

        dataset_ptr->materials = lw_material_dataset_load(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                  " material data-set"),
                                                          serializer);

        /* Load the mesh data-set */
        if (dataset_ptr->meshes != NULL)
        {
            lw_mesh_dataset_release(dataset_ptr->meshes);

            dataset_ptr->meshes = NULL;
        }

        dataset_ptr->meshes = lw_mesh_dataset_load(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                           " material data-set"),
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

    /* Store the light data-set */
    ASSERT_ALWAYS_SYNC(dataset_ptr->lights != NULL,
                       "Light data-set is NULL");

    lw_light_dataset_save(dataset_ptr->lights,
                          serializer);

    /* Store the material data-set */
    ASSERT_ALWAYS_SYNC(dataset_ptr->materials != NULL,
                       "Material data-set is NULL");

    lw_material_dataset_save(dataset_ptr->materials,
                             serializer);

    /* Store the mesh data-set */
    ASSERT_ALWAYS_SYNC(dataset_ptr->meshes != NULL,
                       "Mesh data-set is NULL");

    lw_mesh_dataset_save(dataset_ptr->meshes,
                         serializer);
}
