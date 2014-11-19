/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_material_dataset.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definitions */
typedef struct _lw_material_dataset_material
{
    system_hashed_ansi_string name;
    float                     smoothing_angle;

    /* Constructor */
    explicit _lw_material_dataset_material(system_hashed_ansi_string in_name)
    {
        name            = in_name;
        smoothing_angle = -1.0f;
    }

} _lw_material_dataset_material;

typedef struct
{
    system_resizable_vector materials;

    REFCOUNT_INSERT_VARIABLES
} _lw_material_dataset;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_material_dataset, lw_material_dataset, _lw_material_dataset);


/** TODO */
PRIVATE void _lw_material_dataset_release(void* arg)
{
    _lw_material_dataset* dataset_ptr = (_lw_material_dataset*) arg;

    if (dataset_ptr->materials != NULL)
    {
        _lw_material_dataset_material* material_ptr = NULL;

        while (system_resizable_vector_pop(dataset_ptr->materials,
                                          &material_ptr) )
        {
            delete material_ptr;

            material_ptr = NULL;
        }

        system_resizable_vector_release(dataset_ptr->materials);

        dataset_ptr->materials = NULL;
    } /* if (dataset_ptr->materials != NULL) */
}


/* Please see header for specification */
PUBLIC EMERALD_API lw_material_dataset_material_id lw_material_dataset_add_material(__in __notnull lw_material_dataset       dataset,
                                                                                    __in __notnull system_hashed_ansi_string name)
{
    _lw_material_dataset*           dataset_ptr      = (_lw_material_dataset*) dataset;
    _lw_material_dataset_material*  new_material_ptr = NULL;
    lw_material_dataset_material_id result_id        = -1;

    new_material_ptr = new (std::nothrow) _lw_material_dataset_material(name);

    ASSERT_ALWAYS_SYNC(new_material_ptr != NULL,
                       "Out of memory");
    if (new_material_ptr != NULL)
    {
        /* Form the material ID */
        result_id = system_resizable_vector_get_amount_of_elements(dataset_ptr->materials);

        system_resizable_vector_push(dataset_ptr->materials,
                                     new_material_ptr);
    }

    return result_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_material_dataset_apply_to_scene(__in __notnull lw_material_dataset dataset,
                                                           __in           scene               scene)
{
    _lw_material_dataset* dataset_ptr = (_lw_material_dataset*) dataset;

    // ..
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_material_dataset lw_material_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    _lw_material_dataset* dataset_ptr = new (std::nothrow) _lw_material_dataset;

    ASSERT_ALWAYS_SYNC(dataset_ptr != NULL, "Out of memory");
    if (dataset_ptr != NULL)
    {
        memset(dataset_ptr,
               0,
               sizeof(_lw_material_dataset) );

        dataset_ptr->materials = system_resizable_vector_create(4, /* capacity */
                                                                sizeof(_lw_material_dataset*) );

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(dataset_ptr,
                                                       _lw_material_dataset_release,
                                                       OBJECT_TYPE_LW_DATASET,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave material data-sets\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }

    return (lw_material_dataset) dataset_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_material_dataset_get_material_property(__in  __notnull lw_material_dataset                   dataset,
                                                                  __in            lw_material_dataset_material_id       material_id,
                                                                  __in  __notnull lw_material_dataset_material_property property,
                                                                  __out __notnull void*                                 out_result)
{
    _lw_material_dataset*          dataset_ptr  = (_lw_material_dataset*) dataset;
    _lw_material_dataset_material* material_ptr = NULL;

    if (!system_resizable_vector_get_element_at(dataset_ptr->materials,
                                                material_id,
                                               &material_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Material ID was not recognized");

        goto end;
    }

    switch (property)
    {
        case LW_MATERIAL_DATASET_MATERIAL_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->name;

            break;
        }

        case LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE:
        {
            *(float*) out_result = material_ptr->smoothing_angle;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_material_dataset_material_property value");
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_material_dataset lw_material_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                                __in __notnull system_file_serializer    serializer)
{
    lw_material_dataset dataset = lw_material_dataset_create(name);

    ASSERT_ALWAYS_SYNC(dataset != NULL,
                       "LW material dataset creation failed");
    if (dataset != NULL)
    {
        _lw_material_dataset* dataset_ptr = (_lw_material_dataset*) dataset;
        uint32_t              n_materials = 0;

        system_file_serializer_read(serializer,
                                    sizeof(n_materials),
                                   &n_materials);

        for (uint32_t n_material = 0;
                      n_material < n_materials;
                    ++n_material)
        {
            system_hashed_ansi_string material_name            = NULL;
            float                     material_smoothing_angle = 0.0f;

            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &name);
            system_file_serializer_read                   (serializer,
                                                           sizeof(material_smoothing_angle),
                                                          &material_smoothing_angle);

            /* Add the material & configure it, using the data we've just read */
            lw_material_dataset_material_id new_material_id = lw_material_dataset_add_material(dataset,
                                                                                               name);

            lw_material_dataset_set_material_property(dataset,
                                                      new_material_id,
                                                      LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE,
                                                     &material_smoothing_angle);
        } /* for (all materials) */
    } /* if (dataset != NULL) */

    return dataset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_material_dataset_save(__in __notnull lw_material_dataset    dataset,
                                                 __in __notnull system_file_serializer serializer)
{
    _lw_material_dataset* dataset_ptr = (_lw_material_dataset*) dataset;
    const uint32_t        n_materials = system_resizable_vector_get_amount_of_elements(dataset_ptr->materials);

    system_file_serializer_write(serializer,
                                 sizeof(n_materials),
                                &n_materials);

    for (uint32_t n_material = 0;
                  n_material < n_materials;
                ++n_material)
    {
        _lw_material_dataset_material* material_ptr = NULL;

        if (!system_resizable_vector_get_element_at(dataset_ptr->materials,
                                                    n_material,
                                                   &material_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve material descriptor");

            continue;
        }

        system_file_serializer_write_hashed_ansi_string(serializer,
                                                        material_ptr->name);
        system_file_serializer_write                   (serializer,
                                                        sizeof(material_ptr->smoothing_angle),
                                                       &material_ptr->smoothing_angle);
    } /* for (all materials) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_material_dataset_set_material_property(__in __notnull lw_material_dataset                   dataset,
                                                                  __in           lw_material_dataset_material_id       material_id,
                                                                  __in           lw_material_dataset_material_property property,
                                                                  __in __notnull const void*                           data)
{
    _lw_material_dataset*          dataset_ptr  = (_lw_material_dataset*) dataset;
    _lw_material_dataset_material* material_ptr = NULL;

    if (!system_resizable_vector_get_element_at(dataset_ptr->materials,
                                                material_id,
                                               &material_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Material ID was not recognized");

        goto end;
    }

    switch (property)
    {
        case LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE:
        {
            material_ptr->smoothing_angle = *(float*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_material_dataset_material_property value");
        }
    } /* switch (property) */

end:
    ;
}
