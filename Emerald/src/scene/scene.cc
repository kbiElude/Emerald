
/**
 *
 * Emerald (kbi/elude @2014)
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
#include "scene/scene_mesh.h"
#include "scene/scene_texture.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

#define BASE_OBJECT_STORAGE_CAPACITY (4)


/* Private declarations */
typedef struct
{
    ogl_context               context;

    system_resizable_vector   cameras;  /* TODO: make this a hasy64 map hashed by camera name */
    system_resizable_vector   curves;   /* TODO: make this a hash64 map hashed by curve_id */
    system_resizable_vector   lights;   /* TODO: make this a hash64 map hashed by light name */
    system_resizable_vector   mesh_instances;
    system_resizable_vector   textures; /* TODO: make this a hash64 map hashed by texture_id */

    scene_graph               graph;
    float                     max_animation_duration;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _scene;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene, scene, _scene);

/* Please see header for specification */
PRIVATE void _scene_release(__in __notnull __post_invalid void* arg)
{
    _scene* scene_ptr = (_scene*) arg;

    if (scene_ptr->cameras != NULL)
    {
        scene_camera camera = NULL;

        while (system_resizable_vector_pop(scene_ptr->cameras, &camera) )
        {
            scene_camera_release(camera);
        }
        system_resizable_vector_release(scene_ptr->cameras);

        scene_ptr->cameras = NULL;
    }

    if (scene_ptr->curves != NULL)
    {
        scene_curve curve_ptr = NULL;

        while (system_resizable_vector_pop(scene_ptr->curves, &curve_ptr) )
        {
            scene_curve_release(curve_ptr);
        }
        system_resizable_vector_release(scene_ptr->curves);

        scene_ptr->curves = NULL;
    }

    if (scene_ptr->graph != NULL)
    {
        scene_graph_release(scene_ptr->graph);

        scene_ptr->graph = NULL;
    }

    if (scene_ptr->lights != NULL)
    {
        scene_light light = NULL;

        while (system_resizable_vector_pop(scene_ptr->lights, &light) )
        {
            scene_light_release(light);

            light = NULL;
        }
        system_resizable_vector_release(scene_ptr->lights);

        scene_ptr->lights = NULL;
    }

    if (scene_ptr->mesh_instances != NULL)
    {
        scene_mesh mesh_instance_ptr = NULL;

        while (system_resizable_vector_pop(scene_ptr->mesh_instances, &mesh_instance_ptr) )
        {
            scene_mesh_release(mesh_instance_ptr);
        }
        system_resizable_vector_release(scene_ptr->mesh_instances);

        scene_ptr->mesh_instances = NULL;
    }

    if (scene_ptr->textures != NULL)
    {
        scene_texture texture_ptr = NULL;

        while (system_resizable_vector_pop(scene_ptr->textures, &texture_ptr) )
        {
            scene_texture_release(texture_ptr);
        }

        system_resizable_vector_release(scene_ptr->textures);
        scene_ptr->textures = NULL;
    }

    if (scene_ptr->context != NULL)
    {
        ogl_context_release(scene_ptr->context);
    }
}


/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_camera(__in __notnull scene        scene_instance,
                                         __in __notnull scene_camera new_camera)

{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (new_camera != NULL)
    {
        system_resizable_vector_push(scene_ptr->cameras,
                                     new_camera);

        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_curve(__in __notnull scene       scene_instance,
                                        __in __notnull scene_curve curve_instance)
{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (system_resizable_vector_find(scene_ptr->curves, curve_instance) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(scene_ptr->curves, curve_instance);

        scene_curve_retain(curve_instance);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_light(__in __notnull scene       scene_instance,
                                        __in __notnull scene_light light_instance)

{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (system_resizable_vector_find(scene_ptr->lights, light_instance) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(scene_ptr->lights, light_instance);

        scene_light_retain(light_instance);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_mesh_instance_defined(__in __notnull scene      scene,
                                                        __in __notnull scene_mesh mesh)
{
    bool    result    = true;
    _scene* scene_ptr = (_scene*) scene;

    system_resizable_vector_push(scene_ptr->mesh_instances,
                                 mesh);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_mesh_instance(__in __notnull scene                     scene,
                                                __in __notnull mesh                      mesh_data,
                                                __in __notnull system_hashed_ansi_string name)
{
    bool           result           = true;
    _scene*        scene_ptr        = (_scene*) scene;
    scene_mesh     new_instance     = scene_mesh_create(name, mesh_data);
    const uint32_t n_mesh_instances = system_resizable_vector_get_amount_of_elements(scene_ptr->mesh_instances);

    scene_mesh_set_property(new_instance,
                            SCENE_MESH_PROPERTY_ID,
                           &n_mesh_instances);

    system_resizable_vector_push(scene_ptr->mesh_instances, new_instance);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_add_texture(__in __notnull scene         scene_instance,
                                          __in __notnull scene_texture texture_instance)
{
    bool    result    = false;
    _scene* scene_ptr = (_scene*) scene_instance;

    if (system_resizable_vector_find(scene_ptr->textures, texture_instance) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(scene_ptr->textures, texture_instance);

        scene_texture_retain(texture_instance);
        result = true;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene scene_create(__in __notnull ogl_context               context,
                                      __in __notnull system_hashed_ansi_string name)
{
    _scene* new_scene = new (std::nothrow) _scene;

    ASSERT_DEBUG_SYNC(new_scene != NULL, "Out of memory");
    if (new_scene != NULL)
    {
        memset(new_scene, 0, sizeof(_scene) );

        ogl_context_retain(context);
        new_scene->context  = context;

        new_scene->cameras                = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY, sizeof(void*) );
        new_scene->curves                 = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY, sizeof(void*) );
        new_scene->lights                 = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY, sizeof(void*) );
        new_scene->mesh_instances         = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY, sizeof(void*) );
        new_scene->max_animation_duration = 0.0f;
        new_scene->name                   = name;
        new_scene->textures               = system_resizable_vector_create(BASE_OBJECT_STORAGE_CAPACITY, sizeof(void*) );

        if (new_scene->curves         == NULL || new_scene->lights   == NULL ||
            new_scene->mesh_instances == NULL || new_scene->textures == NULL)
        {
            ASSERT_ALWAYS_SYNC(false, "Out of memory");

            goto end_with_failure;
        }

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene,
                                                       _scene_release,
                                                       OBJECT_TYPE_SCENE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scenes\\", system_hashed_ansi_string_get_buffer(name)) );
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
PUBLIC EMERALD_API scene_camera scene_get_camera_by_index(__in __notnull scene        scene,
                                                          __in           unsigned int index)
{
    scene_camera result    = NULL;
    _scene*      scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->cameras, index, &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_name(__in __notnull scene                     scene,
                                                         __in __notnull system_hashed_ansi_string name)
{
    _scene*            scene_ptr = (_scene*) scene;
    const unsigned int n_cameras = system_resizable_vector_get_amount_of_elements(scene_ptr->cameras);
    scene_camera       result    = NULL;

    LOG_INFO("scene_get_camera_by_name(): slow code-path call");

    for (unsigned int n_camera = 0; n_camera < n_cameras; ++n_camera)
    {
        scene_camera              camera      = NULL;
        system_hashed_ansi_string camera_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->cameras, n_camera, &camera) )
        {
            scene_camera_get_property(camera,
                                      SCENE_CAMERA_PROPERTY_NAME,
                                     &camera_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(camera_name, name) )
            {
                result = camera;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve camera at index [%d]", n_camera);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_get_curve_by_id(__in __notnull scene          instance,
                                                     __in           scene_curve_id id)
{
    /* Todo: OPTIMISE THIS! Use hashed ids! */
    scene_curve result    = NULL;
    _scene*     scene_ptr = (_scene*) instance;
    size_t      n_curves  = (size_t)  system_resizable_vector_get_amount_of_elements(scene_ptr->curves);

    for (size_t n_curve = 0; n_curve < n_curves; ++n_curve)
    {
        scene_curve    curve    = NULL;
        scene_curve_id curve_id = -1;

        if (system_resizable_vector_get_element_at(scene_ptr->curves, n_curve, &curve) )
        {
            scene_curve_get(curve, SCENE_CURVE_PROPERTY_ID, &curve_id);
            ASSERT_DEBUG_SYNC(curve_id != -1, "Invalid curve id encountered");

            if (curve_id == id)
            {
                result = curve;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve %dth curve", n_curve);
        }
    }
    
    ASSERT_DEBUG_SYNC(result != NULL, "Could not retrieve curve with id [%d]", id);
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_get_light_by_index(__in __notnull scene        scene,
                                                        __in           unsigned int index)
{
    scene_light result    = NULL;
    _scene*     scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->lights, index, &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_index(__in __notnull scene        scene,
                                                               __in           unsigned int index)
{
    scene_mesh result    = NULL;
    _scene*    scene_ptr = (_scene*) scene;

    system_resizable_vector_get_element_at(scene_ptr->mesh_instances, index, &result);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_name(__in __notnull scene                     scene,
                                                              __in __notnull system_hashed_ansi_string name)
{
    _scene*            scene_ptr        = (_scene*) scene;
    const unsigned int n_mesh_instances = system_resizable_vector_get_amount_of_elements(scene_ptr->mesh_instances);
    scene_mesh         result           = NULL;

    LOG_INFO("scene_get_mesh_instance_by_name(): slow code-path call");

    for (unsigned int n_mesh_instance = 0; n_mesh_instance < n_mesh_instances; ++n_mesh_instance)
    {
        scene_mesh                mesh_instance      = NULL;
        system_hashed_ansi_string mesh_instance_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->mesh_instances, n_mesh_instance, &mesh_instance) )
        {
            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_NAME,
                                   &mesh_instance_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(mesh_instance_name, name) )
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
PUBLIC EMERALD_API void scene_get_property(__in  __notnull scene          scene,
                                           __in            scene_property property,
                                           __out __notnull void*          out_result)
{
    const _scene* scene_ptr = (_scene*) scene;

    switch (property)
    {
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
            *((uint32_t*) out_result) = system_resizable_vector_get_amount_of_elements(scene_ptr->cameras);

            break;
        }

        case SCENE_PROPERTY_N_LIGHTS:
        {
            *((uint32_t*) out_result) = system_resizable_vector_get_amount_of_elements(scene_ptr->lights);

            break;
        }

        case SCENE_PROPERTY_N_MESH_INSTANCES:
        {
            *((uint32_t*) out_result) = system_resizable_vector_get_amount_of_elements(scene_ptr->mesh_instances);

            break;
        }

        case SCENE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = scene_ptr->name;

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
PUBLIC EMERALD_API scene_texture scene_get_texture_by_name(__in __notnull scene                     instance,
                                                           __in __notnull system_hashed_ansi_string name)
{
    /* Todo: OPTIMISE THIS! Use hashed strings! */
    scene_texture result     = NULL;
    _scene*       scene_ptr  = (_scene*) instance;
    size_t        n_textures = (size_t)  system_resizable_vector_get_amount_of_elements(scene_ptr->textures);

    for (size_t n_texture = 0; n_texture < n_textures; ++n_texture)
    {
        scene_texture             texture      = NULL;
        system_hashed_ansi_string texture_name = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->textures, n_texture, &texture) )
        {
            scene_texture_get(texture, SCENE_TEXTURE_PROPERTY_NAME, &name);
            ASSERT_DEBUG_SYNC(texture_name                                       != NULL, "Invalid texture name encountered");
            ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(texture_name) != 0,    "Empty texture name encountered");

            if (system_hashed_ansi_string_is_equal_to_hash_string(name, texture_name) )
            {
                result = texture;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve %dth texture", n_texture);
        }
    }

    ASSERT_DEBUG_SYNC(result != NULL, "Could not retrieve texture with name [%s]", system_hashed_ansi_string_get_buffer(name) );
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene scene_load(__in __notnull ogl_context               context,
                                    __in __notnull system_hashed_ansi_string full_file_name_with_path)
{
    scene                  result     = NULL;
    system_file_serializer serializer = system_file_serializer_create_for_reading(full_file_name_with_path);

    if (serializer == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Could not load scene [%s]", system_hashed_ansi_string_get_buffer(full_file_name_with_path) );

        goto end;
    }

    result = scene_load_with_serializer(context, serializer);

    system_file_serializer_release(serializer);
end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene scene_load_with_serializer(__in __notnull ogl_context            context,
                                                    __in __notnull system_file_serializer serializer)
{
    system_hash64map        material_id_to_mesh_material_map = system_hash64map_create(sizeof(void*) );
    system_hash64map        mesh_id_to_mesh_map              = system_hash64map_create(sizeof(void*) );
    bool                    result                           = true;
    scene                   result_scene                     = NULL;
    system_resizable_vector serialized_scene_cameras         = system_resizable_vector_create(4 /* capacity */, sizeof(void*) );
    system_resizable_vector serialized_scene_lights          = system_resizable_vector_create(4 /* capacity */, sizeof(void*) );
    system_resizable_vector serialized_scene_mesh_instances  = system_resizable_vector_create(4 /* capacity */, sizeof(void*) );
    system_hash64map        texture_id_to_ogl_texture_map    = system_hash64map_create(sizeof(void*) );

    /* Read basic stuff */
    float                     scene_animation_duration = 0.0f;
    system_hashed_ansi_string scene_name               = NULL;
    uint32_t                  n_scene_cameras          = 0;
    uint32_t                  n_scene_curves           = 0;
    uint32_t                  n_scene_lights           = 0;
    uint32_t                  n_scene_materials        = 0;
    uint32_t                  n_scene_mesh_instances   = 0;
    uint32_t                  n_scene_textures         = 0;

    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &scene_name);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(scene_animation_duration),
                                                            &scene_animation_duration);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(n_scene_cameras),
                                                            &n_scene_cameras);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(n_scene_curves),
                                                            &n_scene_curves);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(n_scene_lights),
                                                            &n_scene_lights);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(n_scene_mesh_instances),
                                                            &n_scene_mesh_instances);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(n_scene_textures),
                                                            &n_scene_textures);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(n_scene_materials),
                                                            &n_scene_materials);
    if (!result)
    {
        goto end_error;
    }

    /* Spawn the scene */
    result_scene = scene_create(context, scene_name);

    if (result_scene == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not spawn result scene");

        result = false;

        goto end_error;
    }

    /* Load cameras */
    for (uint32_t n_scene_camera = 0;
                  n_scene_camera < n_scene_cameras;
                ++n_scene_camera)
    {
        scene_camera new_camera = scene_camera_load(serializer);

        ASSERT_DEBUG_SYNC(new_camera != NULL, "Cannot spawn new camera instance");
        if (new_camera == NULL)
        {
            result = false;

            goto end_error;
        }

        result &= scene_add_camera(result_scene, new_camera);

        if (result)
        {
            system_resizable_vector_push(serialized_scene_cameras,
                                         new_camera);
        }
    } /* for (all cameras defined for the scene) */

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Error adding cameras");

        goto end_error;
    }

    /* Load curves */
    for (uint32_t n_scene_curve = 0;
                  n_scene_curve < n_scene_curves;
                ++n_scene_curve)
    {
        scene_curve new_curve = scene_curve_load(serializer);

        ASSERT_DEBUG_SYNC(new_curve != NULL, "Could not load curve data");
        if (new_curve == NULL)
        {
            result = false;

            goto end_error;
        }

        result &= scene_add_curve(result_scene,
                                  new_curve);
    } /* for (all curves defined for the scene) */

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene curves");

        goto end_error;
    }

    /* Load scene lights */
    for (uint32_t n_scene_light = 0;
                  n_scene_light < n_scene_lights;
                ++n_scene_light)
    {
        scene_light new_light = scene_light_load(serializer);

        ASSERT_DEBUG_SYNC(new_light != NULL, "Could not load light data");
        if (new_light == NULL)
        {
            result = false;

            goto end_error;
        }

        result &= scene_add_light(result_scene, new_light);

        if (result)
        {
            system_resizable_vector_push(serialized_scene_lights,
                                         new_light);
        }
    } /* for (all lights defined for the scene) */

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene lights");

        goto end_error;
    }

    /* Load textures */
    for (unsigned int n_scene_texture = 0;
                      n_scene_texture < n_scene_textures;
                    ++n_scene_texture)
    {
        scene_texture new_texture = scene_texture_load_with_serializer(serializer, context);

        ASSERT_DEBUG_SYNC(new_texture != NULL,
                          "Could not load scene texture");
        if (new_texture == NULL)
        {
            result = false;

            goto end_error;
        }

        result &= scene_add_texture(result_scene,
                                    new_texture);

        /* Map the serialized texture ID to the ogl_texture instance, if necessary */
        unsigned int texture_gl_id    = 0;
        ogl_texture  texture_instance = NULL;

        scene_texture_get       (new_texture,
                                 SCENE_TEXTURE_PROPERTY_OGL_TEXTURE,
                                &texture_instance);
        ogl_texture_get_property(texture_instance,
                                 OGL_TEXTURE_PROPERTY_ID,
                                &texture_gl_id);

        if (!system_hash64map_contains(texture_id_to_ogl_texture_map,
                                       texture_gl_id) )
        {
            system_hash64map_insert(texture_id_to_ogl_texture_map,
                                    (system_hash64) texture_gl_id,
                                    texture_instance,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }
    } /* for (all textures defined for the scene) */

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene textures");

        goto end_error;
    }

    /* Load mesh materials */
    for (uint32_t n_mesh_material = 0;
                  n_mesh_material < n_scene_materials;
                ++n_mesh_material)
    {
        uint32_t      material_id  = 0;
        mesh_material new_material = NULL;

        result &= system_file_serializer_read(serializer,
                                              sizeof(material_id),
                                             &material_id);

        new_material = mesh_material_load(serializer,
                                          context,
                                          texture_id_to_ogl_texture_map);

        ASSERT_DEBUG_SYNC(new_material != NULL,
                          "Could not load scene material");
        if (new_material == NULL)
        {
            result = false;

            goto end_error;
        }
        else
        {
            system_hash64map_insert(material_id_to_mesh_material_map,
                                    (system_hash64) material_id,
                                    new_material,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }
    } /* for (all materials defined for the scene) */

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene materials");

        goto end_error;
    }

    /* Load meshes */
    uint32_t n_meshes = 0;

    result &= system_file_serializer_read(serializer,
                                          sizeof(n_meshes),
                                         &n_meshes);

    for (unsigned int n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
    {
        mesh         mesh_gpu    = NULL;
        unsigned int mesh_gpu_id = 0;

        result &= system_file_serializer_read(serializer,
                                              sizeof(mesh_gpu_id),
                                             &mesh_gpu_id);

        mesh_gpu = mesh_load_with_serializer(context,
                                             0, /* flags */
                                             serializer,
                                             material_id_to_mesh_material_map);

        ASSERT_DEBUG_SYNC(mesh_gpu != NULL,
                          "Could not load mesh data");
        if (mesh_gpu == NULL)
        {
            result = false;

            goto end_error;
        }

        /* Store the mesh in internal map that we'll need to use to initialize
         * mesh instances.
         */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(mesh_id_to_mesh_map,
                                                     (system_hash64) mesh_gpu_id),
                          "Unique mesh is not unique!");

        system_hash64map_insert(mesh_id_to_mesh_map,
                                (system_hash64) mesh_gpu_id,
                                mesh_gpu,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    } /* for (all unique meshes) */

    /* Load mesh instances */
    for (uint32_t n_mesh_instance = 0;
                  n_mesh_instance < n_scene_mesh_instances;
                ++n_mesh_instance)
    {
        scene_mesh new_mesh_instance = scene_mesh_load(serializer,
                                                       mesh_id_to_mesh_map);

        ASSERT_DEBUG_SYNC(new_mesh_instance != NULL,
                          "Could not load mesh instance");
        if (new_mesh_instance == NULL)
        {
            result = false;

            goto end_error;
        }

        result &= scene_add_mesh_instance_defined(result_scene,
                                                  new_mesh_instance);

        if (result)
        {
            uint32_t mesh_id = 0;

            scene_mesh_get_property(new_mesh_instance,
                                    SCENE_MESH_PROPERTY_ID,
                                   &mesh_id);

            system_resizable_vector_push(serialized_scene_mesh_instances,
                                         new_mesh_instance);
        }
    } /* for (all mesh instances) */

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene mesh instances");

        goto end_error;
    }

    /* Load the scene graph */
    scene_graph new_graph = scene_graph_load(serializer,
                                             serialized_scene_cameras,
                                             serialized_scene_lights,
                                             serialized_scene_mesh_instances);

    ASSERT_DEBUG_SYNC(new_graph != NULL,
                      "Could not load scene graph");
    if (new_graph == NULL)
    {
        result = false;

        goto end_error;
    }

    scene_set_graph(result_scene,
                    new_graph);

    /* Set other scene properties */
    scene_set_property(result_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &scene_animation_duration);

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false, "Could not load scene file.");

end:
    if (material_id_to_mesh_material_map != NULL)
    {
        system_hash64map_release(material_id_to_mesh_material_map);

        material_id_to_mesh_material_map = NULL;
    }

    if (mesh_id_to_mesh_map != NULL)
    {
        system_hash64map_release(mesh_id_to_mesh_map);

        mesh_id_to_mesh_map = NULL;
    }

    if (serialized_scene_cameras != NULL)
    {
        system_resizable_vector_release(serialized_scene_cameras);

        serialized_scene_cameras = NULL;
    }

    if (serialized_scene_lights != NULL)
    {
        system_resizable_vector_release(serialized_scene_lights);

        serialized_scene_lights = NULL;
    }

    if (serialized_scene_mesh_instances != NULL)
    {
        system_resizable_vector_release(serialized_scene_mesh_instances);

        serialized_scene_mesh_instances = NULL;
    }

    if (texture_id_to_ogl_texture_map != NULL)
    {
        system_hash64map_release(texture_id_to_ogl_texture_map);

        texture_id_to_ogl_texture_map = NULL;
    }

    return result_scene;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_save(__in __notnull scene                     instance,
                                   __in __notnull system_hashed_ansi_string full_file_name_with_path)
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

    result = scene_save_with_serializer(instance, serializer);
    ASSERT_DEBUG_SYNC(result,
                      "Could not save scene [%s] - serialization failed",
                      system_hashed_ansi_string_get_buffer(full_file_name_with_path) );

    system_file_serializer_release(serializer);
end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_save_with_serializer(__in __notnull scene                  instance,
                                                   __in __notnull system_file_serializer serializer)
{
    bool    result    = true;
    _scene* scene_ptr = (_scene*) instance;

    system_hash64map camera_ptr_to_id_map         = system_hash64map_create(sizeof(void*) );
    system_hash64map gpu_mesh_to_id_map           = system_hash64map_create(sizeof(void*) );
    system_hash64map light_ptr_to_id_map          = system_hash64map_create(sizeof(void*) );
    system_hash64map mesh_instance_ptr_to_id_map  = system_hash64map_create(sizeof(void*) );
    system_hash64map mesh_material_name_to_id_map = system_hash64map_create(sizeof(void*) );
    system_hash64map mesh_material_ptr_to_id_map  = system_hash64map_create(sizeof(void*) );
    system_hash64map unique_meshes_to_id_map      = system_hash64map_create(sizeof(void*) );

    /* Retrieve unique meshes that we will need to serialize */
    const uint32_t n_mesh_instances = system_resizable_vector_get_amount_of_elements(scene_ptr->mesh_instances);

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
                                        (void*) n_mesh_instance,
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
    uint32_t n_unique_meshes    = system_hash64map_get_amount_of_elements(unique_meshes_to_id_map);

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
        const unsigned int n_mesh_materials = system_resizable_vector_get_amount_of_elements(mesh_materials);

        for (uint32_t n_mesh_material = 0;
                      n_mesh_material < n_mesh_materials;
                    ++n_mesh_material)
        {
            mesh_material current_mesh_material = NULL;

            if (!system_resizable_vector_get_element_at(mesh_materials,
                                                        n_mesh_material,
                                                       &current_mesh_material) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh material at index [%d]",
                                  n_mesh_material);

                continue;
            }

            if (!system_hash64map_contains(mesh_material_ptr_to_id_map,
                                           (system_hash64) current_mesh_material) )
            {
                system_hash64map_insert(mesh_material_ptr_to_id_map,
                                        (system_hash64) current_mesh_material,
                                        (void*) n_unique_materials,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */

                /* Also map the name of the material to the instance */
                system_hashed_ansi_string material_name = mesh_material_get_name(current_mesh_material);

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(mesh_material_name_to_id_map,
                                                             (system_hash64) material_name),
                                  "Material name [%s] is already recognized",
                                  system_hashed_ansi_string_get_buffer(material_name) );

                system_hash64map_insert(mesh_material_name_to_id_map,
                                        (system_hash64) material_name,
                                        (void*) n_unique_materials,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */

                /* Increment the counter */
                n_unique_materials++;
            }
        } /* for (all mesh materials) */
    } /* for (all unique meshes) */

    /* Store basic stuff */
    system_file_serializer_write_hashed_ansi_string(serializer,
                                                    scene_ptr->name);
    system_file_serializer_write                   (serializer,
                                                    sizeof(scene_ptr->max_animation_duration),
                                                   &scene_ptr->max_animation_duration);

    /* Store the number of owned objects */
    const uint32_t n_cameras  = system_resizable_vector_get_amount_of_elements(scene_ptr->cameras);
    const uint32_t n_curves   = system_resizable_vector_get_amount_of_elements(scene_ptr->curves);
    const uint32_t n_lights   = system_resizable_vector_get_amount_of_elements(scene_ptr->lights);
    const uint32_t n_textures = system_resizable_vector_get_amount_of_elements(scene_ptr->textures);

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
                                 sizeof(n_mesh_instances),
                                &n_mesh_instances);
    system_file_serializer_write(serializer,
                                 sizeof(n_textures),
                                &n_textures);
    system_file_serializer_write(serializer,
                                 sizeof(n_unique_materials),
                                &n_unique_materials);

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
                                   current_camera) )
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
                                     (void*) n_camera,
                                     NULL,  /* on_remove_callback_proc */
                                     NULL) ) /* on_remove_callback_proc_user_arg */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to associate camera instance with an ID");
        }
    } /* for (all cameras) */

    /* Serialize curves */
    for (uint32_t n_curve = 0;
                  n_curve < n_curves;
                ++n_curve)
    {
        scene_curve current_curve = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->curves,
                                                   n_curve,
                                                  &current_curve) )
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

    /* Serialize lights */
    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light current_light = NULL;

        if (system_resizable_vector_get_element_at(scene_ptr->lights,
                                                   n_light,
                                                  &current_light) )
        {
            scene_light_save(serializer,
                             current_light);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve light at index [%d]",
                              n_light);

            result = false;
        }

        /* Store the ID */
        if (!system_hash64map_insert(light_ptr_to_id_map,
                                     (system_hash64) current_light,
                                     (void*) n_light,
                                     NULL,  /* on_remove_callback_proc */
                                     NULL) ) /* on_remove_callback_proc_user_arg */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to associate light instance with an ID");
        }
    } /* for (all lights) */

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

    /* Serialize mesh materials */
    for (unsigned int n_material = 0;
                      n_material < n_unique_materials;
                    ++n_material)
    {
        mesh_material material        = NULL;
        system_hash64 material_hash   = 0;
        uint32_t      material_id     = 0;
        void*         material_id_ptr = 0;

        if (!system_hash64map_get_element_at(mesh_material_ptr_to_id_map,
                                             (size_t) n_material,
                                            &material_id_ptr,
                                            &material_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve unique mesh material at index [%d]",
                              n_material);

            continue;
        }

        material_id = (uint32_t)      material_id_ptr;
        material    = (mesh_material) material_hash;

        /* Serialize */
        result &= system_file_serializer_write(serializer,
                                               sizeof(material_id),
                                              &material_id);

        result &= mesh_material_save(serializer, material);
    } /* for (unique materials) */

    /* Serialize meshes */
    const unsigned int n_meshes = system_hash64map_get_amount_of_elements(unique_meshes_to_id_map);

    system_file_serializer_write(serializer,
                                 sizeof(n_meshes),
                                &n_meshes);

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
            mesh         mesh_gpu    = (mesh) mesh_gpu_hash;
            unsigned int mesh_gpu_id = (unsigned int) mesh_gpu_id_ptr;

            system_file_serializer_write(serializer,
                                         sizeof(mesh_gpu_id),
                                        &mesh_gpu_id);

            result &= mesh_save_with_serializer(mesh_gpu,
                                                serializer,
                                                mesh_material_name_to_id_map);

            system_hash64map_insert(gpu_mesh_to_id_map,
                                    (system_hash64) mesh_gpu,
                                    (void*) mesh_gpu_id,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }
    } /* for (all meshes) */

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
                                     (void*) n_mesh_instance,
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
                          mesh_instance_ptr_to_id_map) )
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

    if (mesh_material_name_to_id_map != NULL)
    {
        system_hash64map_release(mesh_material_name_to_id_map);

        mesh_material_name_to_id_map = NULL;
    }

    if (mesh_material_ptr_to_id_map != NULL)
    {
        system_hash64map_release(mesh_material_ptr_to_id_map);

        mesh_material_ptr_to_id_map = NULL;
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
PUBLIC EMERALD_API void scene_set_graph(__in __notnull scene       scene,
                                        __in __notnull scene_graph graph)
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
PUBLIC EMERALD_API void scene_set_property(__in __notnull scene          scene,
                                           __in __notnull scene_property property,
                                           __in __notnull const void*    data)
{
    _scene* scene_ptr = (_scene*) scene;

    switch (property)
    {
        case SCENE_PROPERTY_MAX_ANIMATION_DURATION:
        {
            scene_ptr->max_animation_duration = *(float*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene property");
        }
    } /* switch (property) */
}
