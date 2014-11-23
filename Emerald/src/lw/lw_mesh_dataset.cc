/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_mesh_dataset.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definitions */
typedef struct _lw_mesh_dataset_mesh
{
    system_hashed_ansi_string filename;
    bool                      is_shadow_caster;
    bool                      is_shadow_receiver;
    system_hashed_ansi_string name;

    explicit _lw_mesh_dataset_mesh(__in __notnull system_hashed_ansi_string in_name)
    {
        filename           = NULL;
        is_shadow_caster   = false;
        is_shadow_receiver = false;
        name               = in_name;
    }
} _lw_mesh_dataset_mesh;

typedef struct
{
    system_resizable_vector meshes;

    REFCOUNT_INSERT_VARIABLES
} _lw_mesh_dataset;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_mesh_dataset, lw_mesh_dataset, _lw_mesh_dataset);


/** TODO */
PRIVATE void _lw_mesh_dataset_release(void* arg)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) arg;

    if (dataset_ptr->meshes != NULL)
    {
        _lw_mesh_dataset_mesh* mesh_ptr = NULL;

        while (system_resizable_vector_pop(dataset_ptr->meshes,
                                          &mesh_ptr) )
        {
            delete mesh_ptr;

            mesh_ptr = NULL;
        }

        system_resizable_vector_release(dataset_ptr->meshes);
        dataset_ptr->meshes = NULL;
    } /* if (dataset_ptr->meshes != NULL) */
}


/* Please see header for specification */
PUBLIC EMERALD_API lw_mesh_dataset_mesh_id lw_mesh_dataset_add_mesh(__in __notnull lw_mesh_dataset           dataset,
                                                                    __in __notnull system_hashed_ansi_string name)
{
    _lw_mesh_dataset*       dataset_ptr  = (_lw_mesh_dataset*) dataset;
    _lw_mesh_dataset_mesh*  new_mesh_ptr = NULL;
    lw_mesh_dataset_mesh_id result_id    = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);

    new_mesh_ptr = new (std::nothrow) _lw_mesh_dataset_mesh(name);

    ASSERT_ALWAYS_SYNC(new_mesh_ptr != NULL,
                       "Out of memory");
    if (new_mesh_ptr != NULL)
    {
        system_resizable_vector_push(dataset_ptr->meshes,
                                     new_mesh_ptr);
    }
    else
    {
        result_id = -1;
    }

    return result_id;

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

        dataset_ptr->meshes = system_resizable_vector_create(4, /* capacity */
                                                             sizeof(_lw_mesh_dataset_mesh*) );

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
PUBLIC EMERALD_API void lw_mesh_dataset_get_mesh_property(__in  __notnull lw_mesh_dataset               dataset,
                                                          __in            lw_mesh_dataset_mesh_id       mesh_id,
                                                          __in            lw_mesh_dataset_mesh_property property,
                                                          __out __notnull void*                         out_result)
{
    _lw_mesh_dataset*      dataset_ptr = (_lw_mesh_dataset*) dataset;
    _lw_mesh_dataset_mesh* mesh_ptr    = NULL;

    if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                mesh_id,
                                               &mesh_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized mesh ID requested");

        goto end;
    }

    switch (property)
    {
        case LW_MESH_DATASET_MESH_PROPERTY_FILENAME:
        {
            *(system_hashed_ansi_string*) out_result = mesh_ptr->filename;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER:
        {
            *(bool*) out_result = mesh_ptr->is_shadow_caster;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER:
        {
            *(bool*) out_result = mesh_ptr->is_shadow_receiver;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = mesh_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_mesh_dataset_mesh_property value");
        }
    } /* switch (property) */

end:
    ;
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
        uint32_t          n_meshes    = 0;

        system_file_serializer_read(serializer,
                                    sizeof(n_meshes),
                                   &n_meshes);

        for (uint32_t n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
        {
            system_hashed_ansi_string mesh_filename           = NULL;
            lw_mesh_dataset_mesh_id   mesh_id                 = -1;
            bool                      mesh_is_shadow_caster   = false;
            bool                      mesh_is_shadow_receiver = false;
            system_hashed_ansi_string mesh_name               = NULL;

            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &mesh_filename);
            system_file_serializer_read                   (serializer,
                                                           sizeof(mesh_is_shadow_caster),
                                                          &mesh_is_shadow_caster);
            system_file_serializer_read                   (serializer,
                                                           sizeof(mesh_is_shadow_receiver),
                                                          &mesh_is_shadow_receiver);
            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &mesh_name);

            mesh_id = lw_mesh_dataset_add_mesh(dataset,
                                               mesh_name);

            lw_mesh_dataset_set_mesh_property(dataset,
                                              mesh_id,
                                              LW_MESH_DATASET_MESH_PROPERTY_FILENAME,
                                             &mesh_filename);
            lw_mesh_dataset_set_mesh_property(dataset,
                                              mesh_id,
                                              LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER,
                                             &mesh_is_shadow_caster);
            lw_mesh_dataset_set_mesh_property(dataset,
                                              mesh_id,
                                              LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER,
                                             &mesh_is_shadow_receiver);
        } /* for (all meshes) */
    } /* if (dataset != NULL) */

    return dataset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_save(__in __notnull lw_mesh_dataset        dataset,
                                             __in __notnull system_file_serializer serializer)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;
    const uint32_t    n_meshes    = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);

    system_file_serializer_write(serializer,
                                 sizeof(n_meshes),
                                &n_meshes);

    for (uint32_t n_mesh = 0;
                  n_mesh < n_meshes;
                ++n_mesh)
    {
        _lw_mesh_dataset_mesh* mesh_ptr = NULL;

        if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                    n_mesh,
                                                   &mesh_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh descriptor");

            continue;
        }

        system_file_serializer_write_hashed_ansi_string(serializer,
                                                        mesh_ptr->filename);
        system_file_serializer_write                   (serializer,
                                                        sizeof(mesh_ptr->is_shadow_caster),
                                                       &mesh_ptr->is_shadow_caster);
        system_file_serializer_write                   (serializer,
                                                        sizeof(mesh_ptr->is_shadow_receiver),
                                                       &mesh_ptr->is_shadow_receiver);
        system_file_serializer_write_hashed_ansi_string(serializer,
                                                        mesh_ptr->name);
    } /* for (all meshes) */
}

/* Please see header for specification */;
PUBLIC EMERALD_API void lw_mesh_dataset_set_mesh_property(__in __notnull lw_mesh_dataset               dataset,
                                                          __in           lw_mesh_dataset_mesh_id       mesh_id,
                                                          __in           lw_mesh_dataset_mesh_property property,
                                                          __in __notnull const void*                   data)
{
    _lw_mesh_dataset*      dataset_ptr = (_lw_mesh_dataset*) dataset;
    _lw_mesh_dataset_mesh* mesh_ptr    = NULL;

    if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                mesh_id,
                                               &mesh_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized mesh ID requested");

        goto end;
    }

    switch (property)
    {
        case LW_MESH_DATASET_MESH_PROPERTY_FILENAME:
        {
            mesh_ptr->filename = *(system_hashed_ansi_string*) data;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER:
        {
            mesh_ptr->is_shadow_caster = *(bool*) data;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER:
        {
            mesh_ptr->is_shadow_receiver = *(bool*) data;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_NAME:
        {
            mesh_ptr->name = *(system_hashed_ansi_string*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_mesh_dataset_mesh_property value");
        }
    } /* switch (property) */

end:
    ;
}