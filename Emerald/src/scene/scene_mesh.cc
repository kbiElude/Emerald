
/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "scene/scene_mesh.h"
#include "system/system_file_serializer.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"


/* Private declarations */
typedef struct
{
    mesh                      geometry;
    uint32_t                  id;
    bool                      is_shadow_caster;
    bool                      is_shadow_receiver;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _scene_mesh;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_mesh,
                               scene_mesh,
                              _scene_mesh);

/** TODO */
PRIVATE void _scene_mesh_init(_scene_mesh*              data_ptr,
                              system_hashed_ansi_string name,
                              mesh                      geometry)
{
    data_ptr->geometry           = geometry;
    data_ptr->id                 = -1;
    data_ptr->is_shadow_caster   = true;
    data_ptr->is_shadow_receiver = true;
    data_ptr->name               = name;

    mesh_retain(geometry);
}

/** TODO */
PRIVATE void _scene_mesh_release(void* data_ptr)
{
    _scene_mesh* mesh_ptr = (_scene_mesh*) data_ptr;

    mesh_release(mesh_ptr->geometry);
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_mesh scene_mesh_create(system_hashed_ansi_string name,
                                                system_hashed_ansi_string object_manager_path,
                                                mesh                      geometry)
{
    _scene_mesh* new_scene_mesh = new (std::nothrow) _scene_mesh;

    ASSERT_DEBUG_SYNC(new_scene_mesh != NULL,
                      "Out of memory");

    if (new_scene_mesh != NULL)
    {
        _scene_mesh_init(new_scene_mesh,
                         name,
                         geometry);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_mesh,
                                                       _scene_mesh_release,
                                                       OBJECT_TYPE_SCENE_MESH,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_SCENE_MESH,
                                                                       object_manager_path) );
    }

    return (scene_mesh) new_scene_mesh;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_mesh_get_property(scene_mesh          mesh_instance,
                                                scene_mesh_property property,
                                                void*               out_result)
{
    _scene_mesh* mesh_ptr = (_scene_mesh*) mesh_instance;

    switch (property)
    {
        case SCENE_MESH_PROPERTY_ID:
        {
            *((uint32_t*) out_result) = mesh_ptr->id;

            break;
        }

        case SCENE_MESH_PROPERTY_IS_SHADOW_CASTER:
        {
            *(bool*) out_result = mesh_ptr->is_shadow_caster;

            break;
        }

        case SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER:
        {
            *(bool*) out_result = mesh_ptr->is_shadow_receiver;

            break;
        }

        case SCENE_MESH_PROPERTY_MESH:
        {
            *((mesh*) out_result) = mesh_ptr->geometry;

            break;
        }

        case SCENE_MESH_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = mesh_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_mesh property requested");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC scene_mesh scene_mesh_load(system_file_serializer    serializer,
                                  system_hashed_ansi_string object_manager_path,
                                  system_hash64map          id_to_gpu_mesh_map)
{
    system_hashed_ansi_string name                           = NULL;
    mesh                      mesh_gpu                       = NULL;
    uint32_t                  mesh_map_id                    = -1;
    bool                      result                         = true;
    scene_mesh                result_mesh                    = NULL;
    uint32_t                  result_mesh_id                 = -1;
    bool                      result_mesh_is_shadow_caster   = false;
    bool                      result_mesh_is_shadow_receiver = false;

    /* Retrieve mesh instance properties. */
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(result_mesh_id),
                                                            &result_mesh_id);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(result_mesh_is_shadow_caster),
                                                            &result_mesh_is_shadow_caster);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(result_mesh_is_shadow_receiver),
                                                            &result_mesh_is_shadow_receiver);
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &name);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(mesh_map_id),
                                                            &mesh_map_id);

    if (!result)
    {
        goto end_error;
    }

    /* Retrieve GPU mesh instance */
    if (!system_hash64map_get(id_to_gpu_mesh_map,
                              mesh_map_id,
                             &mesh_gpu) )
    {
        goto end_error;
    }

    /* Spawn the instance */
    result_mesh = scene_mesh_create(name,
                                    object_manager_path,
                                    mesh_gpu);

    ASSERT_DEBUG_SYNC(result_mesh != NULL,
                      "Could not spawn GPU mesh instance");

    if (result_mesh == NULL)
    {
        goto end_error;
    }

    scene_mesh_set_property(result_mesh,
                            SCENE_MESH_PROPERTY_IS_SHADOW_CASTER,
                           &result_mesh_is_shadow_caster);
    scene_mesh_set_property(result_mesh,
                            SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER,
                           &result_mesh_is_shadow_receiver);
    scene_mesh_set_property(result_mesh,
                            SCENE_MESH_PROPERTY_ID,
                           &result_mesh_id);

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false, "SCene mesh serialization failed.");

    if (result_mesh != NULL)
    {
        scene_mesh_release(result_mesh);

        result_mesh = NULL;
    }

end:
    return result_mesh;
}

/* Please see header for spec */
PUBLIC bool scene_mesh_save(system_file_serializer serializer,
                            const scene_mesh       mesh,
                            system_hash64map       gpu_mesh_to_id_map)
{
    const _scene_mesh* mesh_ptr = (const _scene_mesh*) mesh;
    bool               result;

    result  = system_file_serializer_write                   (serializer,
                                                              sizeof(mesh_ptr->id),
                                                             &mesh_ptr->id);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(mesh_ptr->is_shadow_caster),
                                                             &mesh_ptr->is_shadow_caster);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(mesh_ptr->is_shadow_receiver),
                                                             &mesh_ptr->is_shadow_receiver);
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              mesh_ptr->name);

    /* Retrieve GPU mesh instance's ID and store it. */
    uint32_t mesh_id     = -1;
    void*    mesh_id_ptr = NULL;

    if (!system_hash64map_get(gpu_mesh_to_id_map,
                              (system_hash64) (intptr_t) mesh_ptr->geometry,
                             &mesh_id_ptr) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve mesh ID");

        result = false;
        goto end;
    }

    mesh_id = (uint32_t) (intptr_t) mesh_id_ptr;

    result &= system_file_serializer_write(serializer,
                                           sizeof(mesh_id),
                                          &mesh_id);

end:
    return result;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_mesh_set_property(scene_mesh          mesh,
                                                scene_mesh_property property,
                                                const void*         data)
{
    _scene_mesh* mesh_ptr = (_scene_mesh*) mesh;

    switch (property)
    {
        case SCENE_MESH_PROPERTY_ID:
        {
            mesh_ptr->id = *((uint32_t*) data);

            break;
        }

        case SCENE_MESH_PROPERTY_IS_SHADOW_CASTER:
        {
            mesh_ptr->is_shadow_caster = *(bool*) data;

            break;
        }

        case SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER:
        {
            mesh_ptr->is_shadow_receiver = *(bool*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_mesh property requested");
        }
    } /* switch (property)*/
}
