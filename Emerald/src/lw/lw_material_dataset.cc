/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_material_dataset.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definitions */
typedef struct _lw_material_dataset_material
{
    lw_material_dataset_material_id id;
    system_hashed_ansi_string       name;
    float                           smoothing_angle;

    /* Constructor */
    explicit _lw_material_dataset_material(system_hashed_ansi_string in_name)
    {
        id              = -1;
        name            = in_name;
        smoothing_angle = 0.0f;
    }

} _lw_material_dataset_material;

typedef struct
{
    system_resizable_vector materials;
    system_hash64map        materials_by_name;

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

    if (dataset_ptr->materials_by_name != NULL)
    {
        system_hash64map_release(dataset_ptr->materials_by_name);

        dataset_ptr->materials_by_name = NULL;
    }
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

        new_material_ptr->id = result_id;

        system_resizable_vector_push(dataset_ptr->materials,
                                     new_material_ptr);

        /* Store the material by name */
        system_hash64map_insert(dataset_ptr->materials_by_name,
                                system_hashed_ansi_string_get_hash(name),
                                new_material_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }

    return result_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_material_dataset_apply_to_scene(__in __notnull lw_material_dataset dataset,
                                                           __in           scene               scene)
{
    _lw_material_dataset* dataset_ptr = (_lw_material_dataset*) dataset;

    /* Iterate over all objects in the scene. Materials are defined on the layer pass level
     * so for each traversed object, go over all the layer passes available.
     */
    uint32_t n_mesh_instances = 0;

    scene_get_property(scene,
                       SCENE_PROPERTY_N_MESH_INSTANCES,
                      &n_mesh_instances);

    for (uint32_t n_mesh_instance = 0;
                  n_mesh_instance < n_mesh_instances;
                ++n_mesh_instance)
    {
        scene_mesh                mesh_instance      = NULL;
        mesh                      mesh_instance_mesh = NULL;
        system_hashed_ansi_string mesh_instance_name = NULL;
        uint32_t                  n_mesh_layers      = 0;

        if ((mesh_instance = scene_get_mesh_instance_by_index(scene,
                                                              n_mesh_instance)) == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh instance at index [%d]",
                              n_mesh_instance);

            continue;
        }

        scene_mesh_get_property(mesh_instance,
                                SCENE_MESH_PROPERTY_MESH,
                               &mesh_instance_mesh);
        scene_mesh_get_property(mesh_instance,
                                SCENE_MESH_PROPERTY_NAME,
                               &mesh_instance_name);

        mesh_get_property(mesh_instance_mesh,
                          MESH_PROPERTY_N_LAYERS,
                         &n_mesh_layers);

        for (uint32_t n_mesh_layer = 0;
                      n_mesh_layer < n_mesh_layers;
                    ++n_mesh_layer)
        {
            uint32_t n_mesh_layer_passes = mesh_get_amount_of_layer_passes(mesh_instance_mesh,
                                                                           n_mesh_layer);

            for (uint32_t n_mesh_layer_pass = 0;
                          n_mesh_layer_pass < n_mesh_layer_passes;
                        ++n_mesh_layer_pass)
            {
                mesh_material             mesh_layer_pass_material             = NULL;
                system_hashed_ansi_string mesh_layer_pass_material_uv_map_name = NULL;

                if (!mesh_get_layer_pass_property(mesh_instance_mesh,
                                                  n_mesh_layer,
                                                  n_mesh_layer_pass,
                                                  MESH_LAYER_PROPERTY_MATERIAL,
                                                 &mesh_layer_pass_material) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve material for a mesh layer pass");

                    continue;
                }

                /* Retrieve the mesh material name */
                mesh_material_get_property(mesh_layer_pass_material,
                                           MESH_MATERIAL_PROPERTY_UV_MAP_NAME,
                                          &mesh_layer_pass_material_uv_map_name);

                /* Is this a material that is described by the LW material dataset? */
                _lw_material_dataset_material* lw_material_ptr = NULL;

                if (system_hash64map_get(dataset_ptr->materials_by_name,
                                         system_hashed_ansi_string_get_hash(mesh_layer_pass_material_uv_map_name),
                                        &lw_material_ptr) )
                {
                    /* It is. Update the smoothing angle. */
                    mesh_material_set_property(mesh_layer_pass_material,
                                               MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                                              &lw_material_ptr->smoothing_angle);
                }
                else
                {
                    /* TODO: The matching mechanism does not always work correctly. It needs a smarter approach,
                     *       but for now we'll have to live with what we have.
                     */
                    LOG_FATAL("Mesh layer pass material for mesh [%s] does not define a recognized UV map name [%s]",
                              system_hashed_ansi_string_get_buffer(mesh_instance_name),
                              system_hashed_ansi_string_get_buffer(mesh_layer_pass_material_uv_map_name) );
                }
            } /* for (all mesh layer passes) */
        } /* for (all mesh layers) */
    } /* for (all mesh instances) */
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

        dataset_ptr->materials         = system_resizable_vector_create(4, /* capacity */
                                                                        sizeof(_lw_material_dataset*) );
        dataset_ptr->materials_by_name = system_hash64map_create       (sizeof(_lw_material_dataset*) );

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
PUBLIC EMERALD_API bool lw_material_dataset_get_material_by_name(__in  __notnull lw_material_dataset              dataset,
                                                                 __in  __notnull system_hashed_ansi_string        name,
                                                                 __out __notnull lw_material_dataset_material_id* out_result_id)
{
    _lw_material_dataset*          dataset_ptr  = (_lw_material_dataset*) dataset;
    _lw_material_dataset_material* material_ptr = NULL;
    bool                           result       = false;

    if (system_hash64map_contains(dataset_ptr->materials_by_name,
                                  system_hashed_ansi_string_get_hash(name) ))
    {
        result = system_hash64map_get(dataset_ptr->materials_by_name,
                                      system_hashed_ansi_string_get_hash(name),
                                     &material_ptr);

        if (result && out_result_id != NULL)
        {
            *out_result_id = material_ptr->id;
        }
    }

    return result;
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
