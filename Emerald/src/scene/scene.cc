
/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_texture.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_material.h"
#include "scene/scene_mesh.h"
#include "scene/scene_multiloader.h"
#include "scene/scene_texture.h"
#include "system/system_callback_manager.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

#define BASE_OBJECT_STORAGE_CAPACITY (4)


/* Private declarations */
typedef struct
{
    ogl_context               context;

    system_resizable_vector   cameras;        /* TODO: make this a hasy64 map hashed by camera name */
    system_hash64map          curves_map;
    system_resizable_vector   lights;         /* TODO: make this a hash64 map hashed by light name */
    system_resizable_vector   materials;      /* TODO: make this a hash64 map hashed by material name */
    system_resizable_vector   mesh_instances; /* holds scene_mesh instances */
    system_resizable_vector   textures;       /* TODO: make this a hash64 map hashed by texture_id */
    system_resizable_vector   unique_meshes;  /* holds mesh instances */

    float                     fps;
    scene_graph               graph;
    float                     max_animation_duration;
    system_hashed_ansi_string name;
    bool                      shadow_mapping_enabled;

    system_callback_manager   callback_manager;

    REFCOUNT_INSERT_VARIABLES
} _scene;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene,
                               scene,
                              _scene);

/* Please see header for specification */
PRIVATE void _scene_release(void* arg)
{
    _scene* scene_ptr = (_scene*) arg;

    if (scene_ptr->callback_manager != NULL)
    {
        system_callback_manager_release(scene_ptr->callback_manager);

        scene_ptr->callback_manager = NULL;
    }

    if (scene_ptr->cameras != NULL)
    {
        scene_camera camera = NULL;

        while (system_resizable_vector_pop(scene_ptr->cameras,
                                          &camera) )
        {
            scene_camera_release(camera);
        }
        system_resizable_vector_release(scene_ptr->cameras);

        scene_ptr->cameras = NULL;
    }

    if (scene_ptr->curves_map != NULL)
    {
        scene_curve curve_ptr = NULL;
        uint32_t    n_curves  = 0;

        system_hash64map_get_property(scene_ptr->curves_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_curves);

        for (uint32_t n_curve = 0;
                      n_curve < n_curves;
                    ++n_curve)
        {
            if (!system_hash64map_get_element_at(scene_ptr->curves_map,
                                                 n_curve,
                                                &curve_ptr,
                                                 NULL) ) /* outHash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve curve descriptor at index [%d]",
                                  n_curve);

                continue;
            }

            scene_curve_release(curve_ptr);
            curve_ptr = NULL;
        } /* for (all curves) */

        system_hash64map_release(scene_ptr->curves_map);
        scene_ptr->curves_map = NULL;
    }

    if (scene_ptr->graph != NULL)
    {
        scene_graph_release(scene_ptr->graph);

        scene_ptr->graph = NULL;
    }

    if (scene_ptr->lights != NULL)
    {
        scene_light light = NULL;

        while (system_resizable_vector_pop(scene_ptr->lights,
                                          &light) )
        {
            scene_light_release(light);

            light = NULL;
        }
        system_resizable_vector_release(scene_ptr->lights);

        scene_ptr->lights = NULL;
    }

    if (scene_ptr->materials != NULL)
    {
        scene_material material = NULL;

        while (system_resizable_vector_pop(scene_ptr->materials,
                                          &material) )
        {
            scene_material_release(material);

            material = NULL;
        }
        system_resizable_vector(scene_ptr->materials);

        scene_ptr->materials = NULL;
    }

    if (scene_ptr->mesh_instances != NULL)
    {
        scene_mesh mesh_instance_ptr = NULL;

        while (system_resizable_vector_pop(scene_ptr->mesh_instances,
                                          &mesh_instance_ptr) )
        {
            scene_mesh_release(mesh_instance_ptr);
        }
        system_resizable_vector_release(scene_ptr->mesh_instances);

        scene_ptr->mesh_instances = NULL;
    }

    if (scene_ptr->textures != NULL)
    {
        scene_texture texture_ptr = NULL;

        while (system_resizable_vector_pop(scene_ptr->textures,
                                          &texture_ptr) )
        {
            scene_texture_release(texture_ptr);
        }

        system_resizable_vector_release(scene_ptr->textures);
        scene_ptr->textures = NULL;
    }

    if (scene_ptr->unique_meshes != NULL)
    {
        mesh mesh_gpu = NULL;

        while (system_resizable_vector_pop(scene_ptr->unique_meshes,
                                          &mesh_gpu) )
        {
            mesh_release(mesh_gpu);

            mesh_gpu = NULL;
        }
        system_resizable_vector_release(scene_ptr->unique_meshes);

        scene_ptr->unique_meshes = NULL;
    }

    if (scene_ptr->context != NULL)
    {
        ogl_context_release(scene_ptr->context);
    }
}


/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_camera(scene        scene_instance,
                                         scene_camera new_camera)

{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (new_camera != NULL)
    {
        system_resizable_vector_push(scene_ptr->cameras,
                                     new_camera);

        scene_camera_retain(new_camera);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_curve(scene       scene_instance,
                                        scene_curve curve_instance)
{
    scene_curve_id curve_id  = -1;
    bool           result    = false;
    _scene*        scene_ptr = (_scene*) scene_instance;

    scene_curve_get(curve_instance,
                    SCENE_CURVE_PROPERTY_ID,
                   &curve_id);

    if (!system_hash64map_contains(scene_ptr->curves_map,
                                   (system_hash64) curve_id) )
    {
        system_hash64map_insert(scene_ptr->curves_map,
                                (system_hash64) curve_id,
                                curve_instance,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */

        scene_curve_retain(curve_instance);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_light(scene       scene_instance,
                                        scene_light light_instance)

{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (system_resizable_vector_find(scene_ptr->lights,
                                     light_instance) == ITEM_NOT_FOUND)
    {
        /* If this is an ambient light that is about to be added, make sure
         * one has not already been added */
        scene_light_type added_light_type = SCENE_LIGHT_TYPE_UNKNOWN;

        scene_light_get_property(light_instance,
                                 SCENE_LIGHT_PROPERTY_TYPE,
                                &added_light_type);

        if (added_light_type == SCENE_LIGHT_TYPE_AMBIENT)
        {
            scene_light_type current_light_type = SCENE_LIGHT_TYPE_UNKNOWN;
            uint32_t         n_lights           = 0;

            system_resizable_vector_get_property(scene_ptr->lights,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_lights);

            for (uint32_t n_light = 0;
                          n_light < n_lights;
                        ++n_light)
            {
                scene_light current_light = NULL;

                if (!system_resizable_vector_get_element_at(scene_ptr->lights,
                                                            n_light,
                                                           &current_light) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve light descriptor at index [%d]",
                                      n_light);

                    result = false;
                    goto end;
                }

                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_TYPE,
                                        &current_light_type);

                if (current_light_type == SCENE_LIGHT_TYPE_AMBIENT)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Cannot add an ambient light - ambient light already added to the scene!");

                    result = false;
                    goto end;
                }
            } /* for (all known lights) */
        } /* if (added_light_type == SCENE_LIGHT_TYPE_AMBIENT) */

        /* OK to store the light */
        system_resizable_vector_push(scene_ptr->lights,
                                     light_instance);

        scene_light_retain(light_instance);

        result = true;

        /* Call back any subscribers */
        system_callback_manager_call_back(scene_ptr->callback_manager,
                                          SCENE_CALLBACK_ID_LIGHT_ADDED,
                                          light_instance);
    }

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_material(scene          scene_instance,
                                           scene_material material)
{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (system_resizable_vector_find(scene_ptr->materials,
                                     material) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(scene_ptr->materials,
                                     material);

        scene_material_retain(material);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_mesh_instance_defined(scene      scene,
                                                        scene_mesh mesh_instance)
{
    bool    result    = true;
    _scene* scene_ptr = (_scene*) scene;

    system_resizable_vector_push(scene_ptr->mesh_instances,
                                 mesh_instance);
    scene_mesh_retain           (mesh_instance);

    /* If the GPU mesh object is unknown, add it to the unique_meshes vector, but only if it
     * does not have an instantiation parent.
     */
    mesh mesh_gpu = NULL;

    scene_mesh_get_property(mesh_instance,
                            SCENE_MESH_PROPERTY_MESH,
                           &mesh_gpu);

    if (system_resizable_vector_find(scene_ptr->unique_meshes,
                                     mesh_gpu) == ITEM_NOT_FOUND)
    {
        mesh mesh_gpu_instantiation_parent = NULL;

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_INSTANTIATION_PARENT,
                         &mesh_gpu_instantiation_parent);

        if (mesh_gpu_instantiation_parent == NULL)
        {
            system_resizable_vector_push(scene_ptr->unique_meshes,
                                         mesh_gpu);

            mesh_retain(mesh_gpu);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_mesh_instance(scene                     scene,
                                                mesh                      mesh_data,
                                                system_hashed_ansi_string name,
                                                scene_mesh*               out_opt_result_mesh_ptr)
{
    mesh_type                 mesh_type;
    scene_mesh                new_instance     = NULL;
    bool                      result           = true;
    system_hashed_ansi_string scene_name       = NULL;
    _scene*                   scene_ptr        = (_scene*) scene;
    uint32_t                  n_mesh_instances = 0;

    system_resizable_vector_get_property(scene_ptr->mesh_instances,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_mesh_instances);

    mesh_get_property (mesh_data,
                       MESH_PROPERTY_TYPE,
                      &mesh_type);
    scene_get_property(scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    new_instance = scene_mesh_create(name,
                                     scene_name,
                                     mesh_data); /* scene_mesh retains mesh_data */

    scene_mesh_set_property     (new_instance,
                                 SCENE_MESH_PROPERTY_ID,
                                &n_mesh_instances);
    system_resizable_vector_push(scene_ptr->mesh_instances,
                                 new_instance);

    /* Store the GPU mesh representation in unique_meshes vector, but only
     * if its instantiation parent is NULL.
     *
     * NOTE: This is only needed for regular meshes! */

    if (mesh_type == MESH_TYPE_REGULAR                             &&
        system_resizable_vector_find(scene_ptr->unique_meshes,
                                     mesh_data) == ITEM_NOT_FOUND)
    {
        mesh mesh_gpu_instantiation_parent = NULL;

        mesh_get_property(mesh_data,
                          MESH_PROPERTY_INSTANTIATION_PARENT,
                         &mesh_gpu_instantiation_parent);

        if (mesh_gpu_instantiation_parent == NULL)
        {
            system_resizable_vector_push(scene_ptr->unique_meshes,
                                         mesh_data);

            mesh_retain(mesh_data);
        }
    }

    if (result)
    {
        if (out_opt_result_mesh_ptr != NULL)
        {
            *out_opt_result_mesh_ptr = new_instance;
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_texture(scene         scene_instance,
                                          scene_texture texture_instance)
{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (system_resizable_vector_find(scene_ptr->textures,
                                     texture_instance) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(scene_ptr->textures,
                                     texture_instance);

        scene_texture_retain(texture_instance);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene scene_create(ogl_context               context,
                                      system_hashed_ansi_string name)
{
    _scene* new_scene = new (std::nothrow) _scene;

    ASSERT_DEBUG_SYNC(new_scene != NULL,
                      "Out of memory");

    if (new_scene != NULL)
    {
        memset(new_scene,
               0,
               sizeof(_scene) );

        if (context != NULL)
        {
            ogl_context_retain(context);
        }

        new_scene->context                = context;
        new_scene->callback_manager       = system_callback_manager_create( (_callback_id) SCENE_CALLBACK_ID_LIGHT_ADDED);
        new_scene->shadow_mapping_enabled = true;

        new_scene->cameras                = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY);
        new_scene->curves_map             = system_hash64map_create       (sizeof(scene_curve) );
        new_scene->fps                    = 0;
        new_scene->graph                  = scene_graph_create( (scene) new_scene,
                                                                name);
        new_scene->lights                 = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY);
        new_scene->materials              = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY);
        new_scene->max_animation_duration = 0.0f;
        new_scene->mesh_instances         = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY);
        new_scene->name                   = name;
        new_scene->textures               = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY);
        new_scene->unique_meshes          = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY);

        if (new_scene->cameras        == NULL || new_scene->curves_map == NULL ||
            new_scene->lights         == NULL || new_scene->materials  == NULL ||
            new_scene->mesh_instances == NULL || new_scene->textures   == NULL ||
            new_scene->unique_meshes  == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            goto end_with_failure;
        }

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene,
                                                       _scene_release,
                                                       OBJECT_TYPE_SCENE,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_SCENE,
                                                                       NULL) ); /* scene_name */
    }

    return (scene) new_scene;

end_with_failure:
    if (new_scene != NULL)
    {
        _scene_release(new_scene);

        delete new_scene;

        new_scene = NULL;
    }

    return NULL;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_index(scene        scene,
                                                          unsigned int index)
{
    scene_camera result    = NULL;
    _scene*      scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->cameras,
                                           index,
                                          &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_name(scene                     scene,
                                                         system_hashed_ansi_string name)
{
    _scene*      scene_ptr = (_scene*) scene;
    unsigned int n_cameras = 0;
    scene_camera  result   = NULL;

    system_resizable_vector_get_property(scene_ptr->cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_cameras);

    LOG_INFO("scene_get_camera_by_name(): slow code-path call");

    for (unsigned int n_camera = 0;
                      n_camera < n_cameras;
                    ++n_camera)
    {
        scene_camera              camera      = NULL;
        system_hashed_ansi_string camera_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->cameras,
                                                   n_camera,
                                                  &camera) )
        {
            scene_camera_get_property(camera,
                                      SCENE_CAMERA_PROPERTY_NAME,
                                      0, /* time - irrelevant for camera name */
                                     &camera_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(camera_name,
                                                                  name) )
            {
                result = camera;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve camera at index [%d]",
                              n_camera);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_get_curve_by_container(scene           instance,
                                                            curve_container curve)
{
    /* TODO: A curve_container->scene_curve map would fit in here just perfectly. */
    _scene*     scene_ptr = (_scene*) instance;
    uint32_t    n_curves  = 0;
    scene_curve result    = NULL;

    system_hash64map_get_property(scene_ptr->curves_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_curves);

    for (uint32_t n_curve = 0;
                  n_curve < n_curves;
                ++n_curve)
    {
        curve_container current_curve_container = NULL;
        scene_curve     current_curve           = NULL;

        if (!system_hash64map_get_element_at(scene_ptr->curves_map,
                                             n_curve,
                                            &current_curve,
                                             NULL) ) /* pOutHash */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve scene_curve instance");

            continue;
        }

        /* Is this a wrapper for curve_container we're looking for? */
        scene_curve_get(current_curve,
                        SCENE_CURVE_PROPERTY_INSTANCE,
                       &current_curve_container);

        if (current_curve_container == curve)
        {
            result = current_curve;

            break;
        }
    } /* for (all curves) */

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_get_curve_by_id(scene          instance,
                                                     scene_curve_id id)
{
    scene_curve result    = NULL;
    _scene*     scene_ptr = (_scene*) instance;

    system_hash64map_get(scene_ptr->curves_map,
                         (system_hash64) id,
                        &result);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not retrieve requested curve");
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_get_light_by_index(scene        scene,
                                                        unsigned int index)
{
    scene_light result    = NULL;
    _scene*     scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->lights,
                                           index,
                                          &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_get_light_by_name(scene                     scene,
                                                       system_hashed_ansi_string name)
{
    _scene*      scene_ptr = (_scene*) scene;
    unsigned int n_lights  = 0;
    scene_light  result    = NULL;

    system_resizable_vector_get_property(scene_ptr->lights,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_lights);

    LOG_INFO("scene_get_light_by_name(): slow code-path call");

    for (unsigned int n_light = 0;
                      n_light < n_lights;
                    ++n_light)
    {
        scene_light               light      = NULL;
        system_hashed_ansi_string light_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->lights,
                                                   n_light,
                                                  &light) )
        {
            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_NAME,
                                    &light_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(light_name,
                                                                  name) )
            {
                result = light;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                             "Could not retrieve light at index [%d]",
                             n_light);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_index(scene        scene,
                                                               unsigned int index)
{
    scene_mesh result    = NULL;
    _scene*    scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->mesh_instances,
                                           index,
                                          &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_material scene_get_material_by_index(scene        scene,
                                                              unsigned int index)
{
    scene_material result    = NULL;
    _scene*        scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->materials,
                                           index,
                                          &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_material scene_get_material_by_name(scene                     scene,
                                                             system_hashed_ansi_string name)
{
    _scene*        scene_ptr   = (_scene*) scene;
    unsigned int   n_materials = 0;
    scene_material result      = NULL;

    system_resizable_vector_get_property(scene_ptr->materials,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_materials);

    LOG_INFO("scene_get_material_by_name(): slow code-path call");

    for (unsigned int n_material = 0;
                      n_material < n_materials;
                    ++n_material)
    {
        scene_material            material      = NULL;
        system_hashed_ansi_string material_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->materials,
                                                   n_material,
                                                  &material) )
        {
            scene_material_get_property(material,
                                        SCENE_MATERIAL_PROPERTY_NAME,
                                       &material_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(material_name,
                                                                  name) )
            {
                result = material;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve material at index [%d]",
                              n_material);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_name(scene                     scene,
                                                              system_hashed_ansi_string name)
{
    _scene*      scene_ptr        = (_scene*) scene;
    unsigned int n_mesh_instances = 0;
    scene_mesh   result           = NULL;

    system_resizable_vector_get_property(scene_ptr->mesh_instances,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_mesh_instances);

    LOG_INFO("scene_get_mesh_instance_by_name(): slow code-path call");

    for (unsigned int n_mesh_instance = 0;
                      n_mesh_instance < n_mesh_instances;
                    ++n_mesh_instance)
    {
        scene_mesh                mesh_instance      = NULL;
        system_hashed_ansi_string mesh_instance_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->mesh_instances,
                                                   n_mesh_instance,
                                                  &mesh_instance) )
        {
            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_NAME,
                                   &mesh_instance_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(mesh_instance_name,
                                                                  name) )
            {
                result = mesh_instance;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve mesh instance at index [%d]", n_mesh_instance);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_get_property(scene          scene,
                                           scene_property property,
                                           void*          out_result)
{
    const _scene* scene_ptr = (_scene*) scene;

    switch (property)
    {
        case SCENE_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result = scene_ptr->callback_manager;

            break;
        }

        case SCENE_PROPERTY_FPS:
        {
            *(float*) out_result = scene_ptr->fps;

            break;
        }

        case SCENE_PROPERTY_GRAPH:
        {
            *((scene_graph*) out_result) = scene_ptr->graph;

            break;
        }

        case SCENE_PROPERTY_MAX_ANIMATION_DURATION:
        {
            *((float*) out_result) = scene_ptr->max_animation_duration;

            break;
        }

        case SCENE_PROPERTY_N_CAMERAS:
        {
            system_resizable_vector_get_property(scene_ptr->cameras,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        case SCENE_PROPERTY_N_LIGHTS:
        {
            system_resizable_vector_get_property(scene_ptr->lights,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        case SCENE_PROPERTY_N_MATERIALS:
        {
            system_resizable_vector_get_property(scene_ptr->materials,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        case SCENE_PROPERTY_N_MESH_INSTANCES:
        {
            system_resizable_vector_get_property(scene_ptr->mesh_instances,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        case SCENE_PROPERTY_N_UNIQUE_MESHES:
        {
            system_resizable_vector_get_property(scene_ptr->unique_meshes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            break;
        }

        case SCENE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = scene_ptr->name;

            break;
        }

        case SCENE_PROPERTY_SHADOW_MAPPING_ENABLED:
        {
            *(bool*) out_result = scene_ptr->shadow_mapping_enabled;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_texture scene_get_texture_by_index(scene        scene,
                                                            unsigned int index)
{
    scene_texture result    = NULL;
    _scene*       scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->textures,
                                           index,
                                          &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_texture scene_get_texture_by_name(scene                     instance,
                                                           system_hashed_ansi_string name)
{
    /* Todo: OPTIMISE THIS! Use hashed strings! */
    scene_texture result     = NULL;
    _scene*       scene_ptr  = (_scene*) instance;
    size_t        n_textures = 0;

    system_resizable_vector_get_property(scene_ptr->textures,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_textures);


    for (size_t n_texture = 0;
                 n_texture < n_textures;
               ++n_texture)
    {
        scene_texture             texture      = NULL;
        system_hashed_ansi_string texture_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->textures,
                                                   n_texture,
                                                  &texture) )
        {
            scene_texture_get(texture, SCENE_TEXTURE_PROPERTY_NAME,
                             &name);

            ASSERT_DEBUG_SYNC(texture_name != NULL,
                              "Invalid texture name encountered");
            ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(texture_name) != 0,
                              "Empty texture name encountered");

            if (system_hashed_ansi_string_is_equal_to_hash_string(name,
                                                                  texture_name) )
            {
                result = texture;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve %dth texture",
                              n_texture);
        }
    }

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not retrieve texture with name [%s]",
                      system_hashed_ansi_string_get_buffer(name) );

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh scene_get_unique_mesh_by_index(scene        scene,
                                                       unsigned int index)
{
    _scene*      scene_ptr       = (_scene*) scene;
    unsigned int n_unique_meshes = 0;
    mesh         result          = NULL;

    system_resizable_vector_get_property(scene_ptr->unique_meshes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_unique_meshes);

    if (n_unique_meshes > index)
    {
        system_resizable_vector_get_element_at(scene_ptr->unique_meshes,
                                               index,
                                              &result);
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid unique mesh index [%d]",
                          index);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene scene_load(ogl_context               context,
                                    system_hashed_ansi_string full_file_name_with_path)
{
    /* Use multi-loader to load the scene */
    scene_multiloader loader = NULL;
    scene             result = NULL;

    loader = scene_multiloader_create_from_filenames(context,
                                                     1, /* n_scenes */
                                                    &full_file_name_with_path);

    scene_multiloader_load_async         (loader);
    scene_multiloader_wait_until_finished(loader);

    scene_multiloader_get_loaded_scene(loader,
                                       0, /* n_scene */
                                      &result);

    scene_retain             (result);
    scene_multiloader_release(loader);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene scene_load_with_serializer(ogl_context            context,
                                                    system_file_serializer serializer)
{
    /* Use multi-loader to load the scene */
    scene_multiloader loader = NULL;
    scene             result = NULL;

    loader = scene_multiloader_create_from_system_file_serializers(context,
                                                                   1, /* n_scenes */
                                                                  &serializer);

    scene_multiloader_load_async         (loader);
    scene_multiloader_wait_until_finished(loader);

    scene_multiloader_get_loaded_scene(loader,
                                       0, /* n_scene */
                                      &result);

    scene_retain             (result);
    scene_multiloader_release(loader);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_save(scene                     instance,
                                   system_hashed_ansi_string full_file_name_with_path)
{
    bool                   result     = false;
    _scene*                scene_ptr  = (_scene*) instance;
    system_file_serializer serializer = system_file_serializer_create_for_writing(full_file_name_with_path);

    if (serializer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not save scene [%s] - serializer could not have been instantiated",
                          system_hashed_ansi_string_get_buffer(full_file_name_with_path) );

        goto end;
    }

    result = scene_save_with_serializer(instance,
                                        serializer);

    ASSERT_DEBUG_SYNC(result,
                      "Could not save scene [%s] - serialization failed",
                      system_hashed_ansi_string_get_buffer(full_file_name_with_path) );

    system_file_serializer_release(serializer);
end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_save_with_serializer(scene                  instance,
                                                   system_file_serializer serializer)
{
    bool    result    = true;
    _scene* scene_ptr = (_scene*) instance;

    system_hash64map camera_ptr_to_id_map         = system_hash64map_create(sizeof(void*) );
    system_hash64map gpu_mesh_to_id_map           = system_hash64map_create(sizeof(void*) );
    system_hash64map light_ptr_to_id_map          = system_hash64map_create(sizeof(void*) );
    system_hash64map mesh_instance_ptr_to_id_map  = system_hash64map_create(sizeof(void*) );
    system_hash64map mesh_material_ptr_to_id_map  = system_hash64map_create(sizeof(void*) );
    system_hash64map scene_material_ptr_to_id_map = system_hash64map_create(sizeof(unsigned int) );
    system_hash64map unique_meshes_to_id_map      = system_hash64map_create(sizeof(void*) );

    /* Retrieve unique meshes that we will need to serialize */
    uint32_t n_mesh_instances = 0;

    system_resizable_vector_get_property(scene_ptr->mesh_instances,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_mesh_instances);

    for (unsigned int n_mesh_instance = 0;
                      n_mesh_instance < n_mesh_instances;
                    ++n_mesh_instance)
    {
        scene_mesh mesh_instance = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->mesh_instances,
                                                   n_mesh_instance,
                                                  &mesh_instance) )
        {
            mesh mesh_gpu = NULL;

            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_MESH,
                                   &mesh_gpu);

            ASSERT_DEBUG_SYNC(mesh_gpu != NULL, "Mesh instance uses a NULL mesh");

            if (mesh_gpu != NULL &&
                !system_hash64map_contains(unique_meshes_to_id_map,
                                           (system_hash64) mesh_gpu) )
            {
                system_hash64map_insert(unique_meshes_to_id_map,
                                        (system_hash64) mesh_gpu,
                                        (void*)         (intptr_t) n_mesh_instance,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            }
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve mesh instance at index [%d]",
                               n_mesh_instance);
        }
    } /* for (all mesh instances) */

    /* Retrieve unique mesh material instances */
    uint32_t n_unique_materials = 0;
    uint32_t n_unique_meshes    = 0;

    system_hash64map_get_property(unique_meshes_to_id_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_unique_meshes);

    for (uint32_t n_unique_mesh = 0;
                  n_unique_mesh < n_unique_meshes;
                ++n_unique_mesh)
    {
        /* Retrieve materials vector for the mesh */
        mesh                    mesh_gpu       = NULL;
        system_hash64           mesh_gpu_ptr   = 0;
        void*                   mesh_id        = 0;
        system_resizable_vector mesh_materials = NULL;

        if (!system_hash64map_get_element_at(unique_meshes_to_id_map,
                                             (size_t) n_unique_mesh,
                                            &mesh_id,
                                            &mesh_gpu_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve unique mesh at index [%d]",
                              n_unique_mesh);

            continue;
        }

        mesh_gpu = (mesh) (void*) mesh_gpu_ptr;

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_MATERIALS,
                         &mesh_materials);

        /* Iterate through assigned materials and store the ones that we
         * have been unaware of.
         */
        unsigned int n_mesh_materials = 0;

        system_resizable_vector_get_property(mesh_materials,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_mesh_materials);

        for (uint32_t n_mesh_material = 0;
                      n_mesh_material < n_mesh_materials;
                    ++n_mesh_material)
        {
            mesh_material  current_mesh_material    = NULL;
            unsigned int   current_mesh_material_id = -1;
            scene_material current_scene_material   = NULL;

            if (!system_resizable_vector_get_element_at(mesh_materials,
                                                        n_mesh_material,
                                                       &current_mesh_material) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh material at index [%d]",
                                  n_mesh_material);

                continue;
            }

            mesh_material_get_property(current_mesh_material,
                                       MESH_MATERIAL_PROPERTY_SOURCE_SCENE_MATERIAL,
                                      &current_scene_material);

            if (!system_hash64map_contains(mesh_material_ptr_to_id_map,
                                           (system_hash64) current_mesh_material) )
            {
                if (!system_hash64map_get(scene_material_ptr_to_id_map,
                                          (system_hash64) current_scene_material,
                                         &current_mesh_material_id) )
                {
                    /* This mesh_material instance is linked to a scene_material instance we have
                     * not come across yet. Assign a new ID */
                    current_mesh_material_id = n_unique_materials++;

                    system_hash64map_insert(scene_material_ptr_to_id_map,
                                            (system_hash64) current_scene_material,
                                            (void*)         (intptr_t) current_mesh_material_id,
                                            NULL,  /* on_remove_callback          */
                                            NULL); /* on_remove_callback_user_arg */
                }
                else
                {
                    /* This mesh_material instance is linked to an already recognized scene_material
                     * instance. Map it to the ID we're already using */
                }

                system_hash64map_insert(mesh_material_ptr_to_id_map,
                                        (system_hash64) current_mesh_material,
                                        (void*)         (intptr_t) current_mesh_material_id,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            } /* if (we have not already processed current mesh_material instance) */
        } /* for (all mesh materials) */
    } /* for (all unique meshes) */

    /* Store basic stuff */
    system_file_serializer_write_hashed_ansi_string(serializer,
                                                    scene_ptr->name);
    system_file_serializer_write                   (serializer,
                                                    sizeof(scene_ptr->fps),
                                                   &scene_ptr->fps);
    system_file_serializer_write                   (serializer,
                                                    sizeof(scene_ptr->max_animation_duration),
                                                   &scene_ptr->max_animation_duration);

    /* Store the number of owned objects */
    uint32_t n_cameras   = 0;
    uint32_t n_curves    = 0;
    uint32_t n_lights    = 0;
    uint32_t n_materials = 0;
    uint32_t n_textures  = 0;

    system_hash64map_get_property       (scene_ptr->curves_map,
                                         SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                        &n_curves);
    system_resizable_vector_get_property(scene_ptr->cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_cameras);
    system_resizable_vector_get_property(scene_ptr->lights,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_lights);
    system_resizable_vector_get_property(scene_ptr->materials,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_materials);
    system_resizable_vector_get_property(scene_ptr->textures,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_textures);

    system_file_serializer_write(serializer,
                                 sizeof(n_cameras),
                                &n_cameras);
    system_file_serializer_write(serializer,
                                 sizeof(n_curves),
                                &n_curves);
    system_file_serializer_write(serializer,
                                 sizeof(n_lights),
                                &n_lights);
    system_file_serializer_write(serializer,
                                 sizeof(n_materials),
                                &n_materials);
    system_file_serializer_write(serializer,
                                 sizeof(n_mesh_instances),
                                &n_mesh_instances);
    system_file_serializer_write(serializer,
                                 sizeof(n_textures),
                                &n_textures);

    /* Serialize curves */
    for (uint32_t n_curve = 0;
                  n_curve < n_curves;
                ++n_curve)
    {
        scene_curve current_curve = NULL;

        if (system_hash64map_get_element_at(scene_ptr->curves_map,
                                            n_curve,
                                            &current_curve,
                                            NULL) ) /* outHash */
        {
            if (!scene_curve_save(serializer,
                                  current_curve) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Curve serialization failed");

                result = false;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve curve at index [%d]",
                              n_curve);

            result = false;
        }
    } /* for (all curves) */

    /* Serialize cameras.
     *
     * Note that each object is assigned an ID. This ID is later used during scene graph serialization.
     */
    for (uint32_t n_camera = 0;
                  n_camera < n_cameras;
                ++n_camera)
    {
        scene_camera current_camera = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->cameras,
                                                   n_camera,
                                                  &current_camera) )
        {
            if (!scene_camera_save(serializer,
                                   current_camera,
                                   instance) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Camera serialization failed");

                result = false;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve camera at index [%d]",
                              n_camera);

            result = false;
        }

        /* Store the ID */
        if (!system_hash64map_insert(camera_ptr_to_id_map,
                                     (system_hash64) current_camera,
                                     (void*)         (intptr_t) n_camera,
                                     NULL,  /* on_remove_callback_proc */
                                     NULL) ) /* on_remove_callback_proc_user_arg */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to associate camera instance with an ID");
        }
    } /* for (all cameras) */

    /* Serialize lights */
    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light current_light = NULL;

        if (!system_resizable_vector_get_element_at(scene_ptr->lights,
                                                   n_light,
                                                  &current_light) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve light at index [%d]",
                              n_light);

            result = false;
        }

        if (result)
        {
            scene_light_save(serializer,
                             current_light,
                             instance);
        }

        /* Store the ID */
        if (!system_hash64map_insert(light_ptr_to_id_map,
                                     (system_hash64) current_light,
                                     (void*)         (intptr_t) n_light,
                                     NULL,  /* on_remove_callback_proc */
                                     NULL) ) /* on_remove_callback_proc_user_arg */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to associate light instance with an ID");
        }
    } /* for (all lights) */

    /* Serialize materials */
    for (uint32_t n_material = 0;
                  n_material < n_materials;
                ++n_material)
    {
        scene_material current_material = NULL;

        if (!system_resizable_vector_get_element_at(scene_ptr->materials,
                                                    n_material,
                                                   &current_material) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve material at index [%d]",
                              n_material);

            result = false;
        }

        if (result)
        {
            scene_material_save(serializer,
                                current_material,
                                instance);
        }

        /* Assign an ID to the material */
        unsigned int current_material_id = 0;

        if (!system_hash64map_get(scene_material_ptr_to_id_map,
                                  (system_hash64) current_material,
                                 &current_material_id) )
        {
            /* TODO: This could occur if a scene_material is added to the scene instance
             *       but is not referred to by any of the layer passes of any of the
             *       meshes assigned to the scene owning the scene_material instance.
             */
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve an ID for scene_material instance");
        }
        else
        {
            system_file_serializer_write(serializer,
                                         sizeof(current_material_id),
                                        &current_material_id);
        }
    } /* for (all materials) */

    /* Serialize textures */
    for (uint32_t n_texture = 0;
                  n_texture < n_textures;
                ++n_texture)
    {
        scene_texture current_texture = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->textures,
                                                   n_texture,
                                                  &current_texture) )
        {
            if (!scene_texture_save(serializer,
                                    current_texture) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Texture serialization failed");

                result = false;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve texture at index [%d]",
                              n_texture);

            result = false;
        }
    } /* for (all textures) */

    /* Serialize meshes: first store the instantiation parent meshes,
     *                   the follow with instanced meshes.
     */
    unsigned int n_meshes = 0;

    system_hash64map_get_property(unique_meshes_to_id_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_meshes);

    system_file_serializer_write(serializer,
                                 sizeof(n_meshes),
                                &n_meshes);

    for (unsigned int n_iteration = 0;
                      n_iteration < 2; /* instantiation parents, then instanced meshes */
                    ++n_iteration)
    {
        for (unsigned int n_mesh = 0;
                          n_mesh < n_meshes;
                        ++n_mesh)
        {
            system_hash64 mesh_gpu_hash   = 0;
            void*         mesh_gpu_id_ptr = 0;

            if (system_hash64map_get_element_at(unique_meshes_to_id_map,
                                                n_mesh,
                                                &mesh_gpu_id_ptr,
                                                &mesh_gpu_hash) )
            {
                mesh         mesh_gpu    = (mesh)         mesh_gpu_hash;
                unsigned int mesh_gpu_id = (unsigned int) (intptr_t) mesh_gpu_id_ptr;

                /* Is this an instanced mesh? */
                mesh mesh_instantiation_parent_gpu = NULL;

                mesh_get_property(mesh_gpu,
                                  MESH_PROPERTY_INSTANTIATION_PARENT,
                                 &mesh_instantiation_parent_gpu);

                if (mesh_instantiation_parent_gpu == NULL && (n_iteration == 0) ||
                    mesh_instantiation_parent_gpu != NULL && (n_iteration == 1) )
                {
                    system_file_serializer_write(serializer,
                                                 sizeof(mesh_gpu_id),
                                                &mesh_gpu_id);

                    result &= mesh_save_with_serializer(mesh_gpu,
                                                        serializer,
                                                        mesh_material_ptr_to_id_map);

                    system_hash64map_insert(gpu_mesh_to_id_map,
                                            (system_hash64) mesh_gpu,
                                            (void*)         (intptr_t) mesh_gpu_id,
                                            NULL,  /* on_remove_callback */
                                            NULL); /* on_remove_callback_user_arg */
                }
            }
        } /* for (all meshes) */
    } /* for (both iterations) */

    /* Serialize mesh instances */
    for (uint32_t n_mesh_instance = 0;
                  n_mesh_instance < n_mesh_instances;
                ++n_mesh_instance)
    {
        scene_mesh current_mesh_instance = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->mesh_instances,
                                                   n_mesh_instance,
                                                  &current_mesh_instance) )
        {
            if (!scene_mesh_save(serializer,
                                 current_mesh_instance,
                                 gpu_mesh_to_id_map) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Mesh instance serialization failed");

                result = false;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh instance at index [%d]",
                              n_mesh_instance);

            result = false;
        }

        /* Sanity check: make sure order is determined by ID. This is an assumption
         *               the loader is built around.
         */
        #ifdef _DEBUG
        {
            uint32_t mesh_instance_id = 0;

            scene_mesh_get_property(current_mesh_instance,
                                    SCENE_MESH_PROPERTY_ID,
                                   &mesh_instance_id);

            ASSERT_DEBUG_SYNC(mesh_instance_id == n_mesh_instance,
                              "Mesh instance ID & order mismatch detected");
        }
        #endif

        /* Store the ID */
        if (!system_hash64map_insert(mesh_instance_ptr_to_id_map,
                                     (system_hash64) current_mesh_instance,
                                     (void*)         (intptr_t) n_mesh_instance,
                                     NULL,  /* on_remove_callback_proc */
                                     NULL) ) /* on_remove_callback_proc_user_arg */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to associate mesh instance with an ID");
        }
    } /* for (all mesh instances) */

    /* Serialize the scene graph */
    if (!scene_graph_save(serializer,
                          scene_ptr->graph,
                          camera_ptr_to_id_map,
                          light_ptr_to_id_map,
                          mesh_instance_ptr_to_id_map,
                          instance) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Scene graph serialization failed");

        result = false;
    }

    /* Release helper hash maps */
    if (camera_ptr_to_id_map != NULL)
    {
        system_hash64map_release(camera_ptr_to_id_map);

        camera_ptr_to_id_map = NULL;
    }

    if (gpu_mesh_to_id_map != NULL)
    {
        system_hash64map_release(gpu_mesh_to_id_map);

        gpu_mesh_to_id_map = NULL;
    }

    if (light_ptr_to_id_map != NULL)
    {
        system_hash64map_release(light_ptr_to_id_map);

        light_ptr_to_id_map = NULL;
    }

    if (mesh_instance_ptr_to_id_map != NULL)
    {
        system_hash64map_release(mesh_instance_ptr_to_id_map);

        mesh_instance_ptr_to_id_map = NULL;
    }

    if (mesh_material_ptr_to_id_map != NULL)
    {
        system_hash64map_release(mesh_material_ptr_to_id_map);

        mesh_material_ptr_to_id_map = NULL;
    }

    if (scene_material_ptr_to_id_map != NULL)
    {
        system_hash64map_release(scene_material_ptr_to_id_map);

        scene_material_ptr_to_id_map = NULL;
    }

    if (unique_meshes_to_id_map != NULL)
    {
        system_hash64map_release(unique_meshes_to_id_map);

        unique_meshes_to_id_map = NULL;
    }

    /* All done */
    return result;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_set_graph(scene       scene,
                                        scene_graph graph)
{
    _scene* scene_ptr = (_scene*) scene;

    if (scene_ptr->graph != NULL)
    {
        scene_graph_release(scene_ptr->graph);

        scene_ptr->graph = NULL;
    }

    scene_ptr->graph = graph;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_set_property(scene          scene,
                                           scene_property property,
                                           const void*    data)
{
    _scene* scene_ptr = (_scene*) scene;

    switch (property)
    {
        case SCENE_PROPERTY_FPS:
        {
            scene_ptr->fps = *(float*) data;

            break;
        }

        case SCENE_PROPERTY_MAX_ANIMATION_DURATION:
        {
            scene_ptr->max_animation_duration = *(float*) data;

            break;
        }

        case SCENE_PROPERTY_SHADOW_MAPPING_ENABLED:
        {
            scene_ptr->shadow_mapping_enabled = *(bool*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene property");
        }
    } /* switch (property) */
}
