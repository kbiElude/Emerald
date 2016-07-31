/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "gfx/gfx_image.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_context.h"
#include "ral/ral_scheduler.h"
#include "ral/ral_texture.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_curve.h"
#include "scene/scene_light.h"
#include "scene/scene_graph.h"
#include "scene/scene_material.h"
#include "scene/scene_mesh.h"
#include "scene/scene_multiloader.h"
#include "scene/scene_texture.h"
#include "system/system_assertions.h"
#include "system/system_barrier.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_file_serializer.h"
#include "system/system_global.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_threads.h"
#include "system/system_thread_pool.h"


/* Private declarations */
typedef enum
{
    /* Multiloader created, loading not started */
    SCENE_MULTILOADER_STATE_CREATED,

    /* Scene loading in progress */
    SCENE_MULTILOADER_STATE_LOADING_IN_PROGRESS,

    /* All scenes have been loaded */
    SCENE_MULTILOADER_STATE_FINISHED

} _scene_multiloader_state;

typedef struct _scene_multiloader_deferred_gfx_image_load_op
{
    system_hashed_ansi_string  filename;
    struct _scene_multiloader* loader_ptr;
    system_hashed_ansi_string  name;

    explicit _scene_multiloader_deferred_gfx_image_load_op(system_hashed_ansi_string in_filename,
                                                           system_hashed_ansi_string in_name,
                                                           _scene_multiloader*       in_loader_ptr)
    {
        filename   = in_filename;
        name       = in_name;
        loader_ptr = in_loader_ptr;
    }

    ~_scene_multiloader_deferred_gfx_image_load_op()
    {
        /* Nothing to do here */
    }
} _scene_multiloader_deferred_gfx_image_load_op;

typedef struct _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op
{
    system_hashed_ansi_string filename;
    scene_texture             texture;
    system_hashed_ansi_string texture_name;
    bool                      uses_mipmaps;

    _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op()
    {
        filename     = nullptr;
        texture      = nullptr;
        texture_name = nullptr;
        uses_mipmaps = false;
    }
} _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op;

typedef struct _scene_multiloader_scene
{
    system_resizable_vector    enqueued_gfx_image_to_scene_texture_assignment_ops; /* _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op* */
    struct _scene_multiloader* loader_ptr;
    scene                      result_scene;
    system_file_serializer     serializer;

     _scene_multiloader_scene();
    ~_scene_multiloader_scene();

} _scene_multiloader_scene;

typedef struct _scene_multiloader
{
    system_barrier           barrier_all_scene_gfx_images_enqueued;
    system_barrier           barrier_all_scene_gfx_images_loaded;
    system_barrier           barrier_all_scenes_loaded;
    system_critical_section  cs;

    ral_context              context_ral;
    bool                     free_serializers_at_release_time;
    bool                     is_finished;
    system_resizable_vector  scenes;   /* _scene_multiloader_scene* */
    _scene_multiloader_state state;

    system_resizable_vector  enqueued_gfx_image_load_ops;      /* _scene_multiloader_deferred_gfx_image_load_op* */
    system_resizable_vector  enqueued_image_file_names_vector; /* system_hashed_ansi_string                      */
    system_hash64map         image_filename_to_gfx_image_map;

     explicit _scene_multiloader(ral_context  in_context_ral,
                                 bool         in_free_serializers_at_release_time,
                                 unsigned int in_n_scenes);
             ~_scene_multiloader();
} _scene_multiloader;


/** TODO */
_scene_multiloader_scene::_scene_multiloader_scene()
{
    enqueued_gfx_image_to_scene_texture_assignment_ops = system_resizable_vector_create(4,     /* capacity */
                                                                                        true); /* thread_safe */
    loader_ptr                                         = nullptr;
    result_scene                                       = nullptr;
    serializer                                         = nullptr;
}

/** TODO */
_scene_multiloader_scene::~_scene_multiloader_scene()
{
    if (enqueued_gfx_image_to_scene_texture_assignment_ops != nullptr)
    {
        _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op* setup_ptr = nullptr;

        while (system_resizable_vector_pop(enqueued_gfx_image_to_scene_texture_assignment_ops,
                                          &setup_ptr) )
        {
            delete setup_ptr;

            setup_ptr = nullptr;
        }
        system_resizable_vector_release(enqueued_gfx_image_to_scene_texture_assignment_ops);

        enqueued_gfx_image_to_scene_texture_assignment_ops = nullptr;
    }

    if (loader_ptr != nullptr)
    {
        if (loader_ptr->free_serializers_at_release_time &&
            serializer != nullptr)
        {
            system_file_serializer_release(serializer);

            serializer = nullptr;
        }
    }

    if (result_scene != nullptr)
    {
        scene_release(result_scene);

        result_scene = nullptr;
    }
}

/** TODO */
_scene_multiloader::_scene_multiloader(ral_context  in_context_ral,
                                       bool         in_free_serializers_at_release_time,
                                       unsigned int in_n_scenes)
{
    barrier_all_scene_gfx_images_enqueued = system_barrier_create(in_n_scenes);
    barrier_all_scene_gfx_images_loaded   = nullptr;
    barrier_all_scenes_loaded             = system_barrier_create(in_n_scenes);
    context_ral                           = in_context_ral;
    cs                                    = system_critical_section_create();
    enqueued_gfx_image_load_ops           = system_resizable_vector_create(4,     /* capacity */
                                                                           true);  /* should_be_thread_safe */
    enqueued_image_file_names_vector      = system_resizable_vector_create(4,      /* capacity */
                                                                           true); /* should_be_thread_safe */
    free_serializers_at_release_time      = in_free_serializers_at_release_time;
    image_filename_to_gfx_image_map       = system_hash64map_create       (4,     /* capacity */
                                                                           true); /* should_be_thread_safe */
    state                                 = SCENE_MULTILOADER_STATE_CREATED;
    scenes                                = system_resizable_vector_create(in_n_scenes);
}

/** TODO */
_scene_multiloader::~_scene_multiloader()
{
    ASSERT_DEBUG_SYNC(state != SCENE_MULTILOADER_STATE_LOADING_IN_PROGRESS,
                      "Scene loading process is in progress!");

    if (barrier_all_scene_gfx_images_enqueued != nullptr)
    {
        system_barrier_release(barrier_all_scene_gfx_images_enqueued);

        barrier_all_scene_gfx_images_enqueued = nullptr;
    }

    if (barrier_all_scene_gfx_images_loaded != nullptr)
    {
        system_barrier_release(barrier_all_scene_gfx_images_loaded);

        barrier_all_scene_gfx_images_loaded = nullptr;
    }

    if (barrier_all_scenes_loaded != nullptr)
    {
        system_barrier_release(barrier_all_scenes_loaded);

        barrier_all_scenes_loaded = nullptr;
    }

    if (context_ral != nullptr)
    {
        ral_context_release(context_ral);

        context_ral = nullptr;
    }

    if (cs != nullptr)
    {
        system_critical_section_release(cs);

        cs = nullptr;
    }

    if (enqueued_image_file_names_vector != nullptr)
    {
        system_resizable_vector_release(enqueued_image_file_names_vector);

        enqueued_image_file_names_vector = nullptr;
    }

    if (image_filename_to_gfx_image_map != nullptr)
    {
        uint32_t n_map_entries = 0;

        system_hash64map_get_property(image_filename_to_gfx_image_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_map_entries);

        for (uint32_t n_map_entry = 0;
                      n_map_entry < n_map_entries;
                    ++n_map_entry)
        {
            gfx_image     image               = nullptr;
            system_hash64 image_filename_hash = 0;

            if (!system_hash64map_get_element_at(image_filename_to_gfx_image_map,
                                                 n_map_entry,
                                                &image,
                                                &image_filename_hash) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve filename->gfx_image map entry at index [%d]",
                                  n_map_entry);

                continue;
            }

            gfx_image_release(image);
            image = nullptr;
        }

        system_hash64map_release(image_filename_to_gfx_image_map);
        image_filename_to_gfx_image_map = nullptr;
    }

    if (enqueued_gfx_image_load_ops != nullptr)
    {
        unsigned int n_ops = 0;

        system_resizable_vector_get_property(enqueued_gfx_image_load_ops,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_ops);

        ASSERT_DEBUG_SYNC(n_ops == 0,
                          "Multiloader being released while there are still gfx image load ops enqueued.");

        system_resizable_vector_release(enqueued_gfx_image_load_ops);
        enqueued_gfx_image_load_ops = nullptr;
    }

    if (scenes != nullptr)
    {
        struct _scene_multiloader_scene* scene_ptr = nullptr;

        while (system_resizable_vector_pop(scenes,
                                          &scene_ptr) )
        {
            delete scene_ptr;

            scene_ptr = nullptr;
        }
        system_resizable_vector_release(scenes);

        scenes = nullptr;
    }
}


/* Forward declarations */
PRIVATE  void _scene_multiloader_load_scene_internal_create_gfx_images_loaded_barrier(void*                                arg);
PRIVATE  void _scene_multiloader_load_scene_internal_enqueue_gfx_filenames           (scene_texture                        texture,
                                                                                      system_hashed_ansi_string            file_name,
                                                                                      system_hashed_ansi_string            texture_name,
                                                                                      bool                                 uses_mipmaps,
                                                                                      void*                                callback_user_data);
PRIVATE  bool _scene_multiloader_load_scene_internal_create_mesh_materials           (_scene_multiloader_scene*            scene_ptr,
                                                                                      unsigned int                         n_scene_materials,
                                                                                      system_hash64map                     scene_material_to_material_id_map,
                                                                                      system_hash64map                     material_id_to_mesh_material_map);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_basic_data                  (_scene_multiloader_scene*            scene_ptr,
                                                                                      system_hashed_ansi_string*           out_scene_name,
                                                                                      unsigned int*                        out_scene_fps,
                                                                                      float*                               out_scene_animation_duration,
                                                                                      uint32_t*                            out_n_scene_cameras,
                                                                                      uint32_t*                            out_n_scene_curves,
                                                                                      uint32_t*                            out_n_scene_lights,
                                                                                      uint32_t*                            out_n_scene_materials,
                                                                                      uint32_t*                            out_n_scene_mesh_instances,
                                                                                      uint32_t*                            out_n_scene_textures);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_camera_data                 (_scene_multiloader_scene*            scene_ptr,
                                                                                      uint32_t                             n_scene_cameras,
                                                                                      system_hashed_ansi_string            scene_file_name,
                                                                                      system_resizable_vector              serialized_scene_cameras);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_curve_data                  (_scene_multiloader_scene*            scene_ptr,
                                                                                      uint32_t                             n_scene_curves,
                                                                                      system_hashed_ansi_string            scene_file_name);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_light_data                  (_scene_multiloader_scene*            scene_ptr,
                                                                                      uint32_t                             n_scene_lights,
                                                                                      system_hashed_ansi_string            scene_file_name,
                                                                                      system_resizable_vector              serialized_scene_lights);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_material_data               (_scene_multiloader_scene*            scene_ptr,
                                                                                      uint32_t                             n_scene_materials,
                                                                                      system_hashed_ansi_string            scene_file_name,
                                                                                      system_hash64map                     material_id_to_scene_material_map,
                                                                                      system_hash64map                     scene_material_to_material_id_map);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_mesh_data                   (_scene_multiloader_scene*            scene_ptr,
                                                                                      system_hash64map                     material_id_to_mesh_material_map,
                                                                                      system_hash64map                     mesh_id_to_mesh_map,
                                                                                      system_hash64map                     mesh_name_to_mesh_map);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_mesh_instances_data         (_scene_multiloader_scene*            scene_ptr,
                                                                                      uint32_t                             n_scene_mesh_instances,
                                                                                      system_hashed_ansi_string            scene_name,
                                                                                      system_hash64map                     mesh_id_to_mesh_map,
                                                                                      system_resizable_vector              serialized_scene_mesh_instances);
PRIVATE  bool _scene_multiloader_load_scene_internal_get_texture_data                (_scene_multiloader_scene*            scene_ptr,
                                                                                      system_hashed_ansi_string            object_manager_path,
                                                                                      unsigned int                         n_scene_textures,
                                                                                      ral_context                          context,
                                                                                      system_hash64map                     texture_id_to_ogl_texture_map);
volatile void _scene_multiloader_load_scene_internal_load_gfx_image_entrypoint       (system_thread_pool_callback_argument op);
PRIVATE  void _scene_multiloader_load_scene_internal                                 (_scene_multiloader_scene*            scene_ptr);
PRIVATE  void _scene_multiloader_load_scene_thread_entrypoint                        (system_threads_entry_point_argument  arg);


/** TODO */
PRIVATE void _scene_multiloader_load_scene_internal_create_gfx_images_loaded_barrier(void* arg)
{
    _scene_multiloader* loader_ptr           = reinterpret_cast<_scene_multiloader*>(arg);
    unsigned int        n_enqueued_filenames = 0;

    ASSERT_DEBUG_SYNC(loader_ptr->barrier_all_scene_gfx_images_loaded == nullptr,
                      "'gfx images' barrier already created!");

    system_resizable_vector_get_property(loader_ptr->enqueued_image_file_names_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_enqueued_filenames);

    loader_ptr->barrier_all_scene_gfx_images_loaded = system_barrier_create(n_enqueued_filenames);
}

/** TODO */
PRIVATE void _scene_multiloader_load_scene_internal_enqueue_gfx_filenames(scene_texture             texture,
                                                                          system_hashed_ansi_string file_name,
                                                                          system_hashed_ansi_string texture_name,
                                                                          bool                      uses_mipmaps,
                                                                          void*                     callback_user_data)
{
    _scene_multiloader_scene* scene_ptr = reinterpret_cast<_scene_multiloader_scene*>(callback_user_data);

    /* Enqueue the image load op only if the filename has not already been pushed */
    system_resizable_vector_lock(scene_ptr->loader_ptr->enqueued_image_file_names_vector,
                                 ACCESS_WRITE);
    {
        if (system_resizable_vector_find(scene_ptr->loader_ptr->enqueued_image_file_names_vector,
                                          file_name) == ITEM_NOT_FOUND)
        {
            /* Enqueue the filename. This will be used later on to fill the image_filename_to_gfx_image_map vector. */
            system_resizable_vector_push(scene_ptr->loader_ptr->enqueued_image_file_names_vector,
                                         file_name);

            /* Spawn a load op for the file.
             *
             * Release is performed right after consumption.
             */
            _scene_multiloader_deferred_gfx_image_load_op* op_ptr = new (std::nothrow) _scene_multiloader_deferred_gfx_image_load_op(file_name,
                                                                                                                                     texture_name,
                                                                                                                                     scene_ptr->loader_ptr);

            system_resizable_vector_push(scene_ptr->loader_ptr->enqueued_gfx_image_load_ops,
                                         op_ptr);
        }
    }
    system_resizable_vector_unlock(scene_ptr->loader_ptr->enqueued_image_file_names_vector,
                                   ACCESS_WRITE);

    /* Set up a descriptor which will later tell which ogl_texture backing needs to be
     * assigned to which scene_texture instance.
     *
     * Release is performed right after consumption.
     */
    _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op* setup_ptr = new (std::nothrow) _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op;

    ASSERT_DEBUG_SYNC(setup_ptr != nullptr,
                      "Out of memory");

    setup_ptr->filename     = file_name;
    setup_ptr->texture      = texture;
    setup_ptr->texture_name = texture_name;
    setup_ptr->uses_mipmaps = uses_mipmaps;

    system_resizable_vector_push(scene_ptr->enqueued_gfx_image_to_scene_texture_assignment_ops,
                                 setup_ptr);
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_create_mesh_materials(_scene_multiloader_scene* scene_ptr,
                                                                          unsigned int              n_scene_materials,
                                                                          system_hash64map          scene_material_to_material_id_map,
                                                                          system_hash64map          material_id_to_mesh_material_map)
{
    bool result = true;

    for (uint32_t n_scene_material = 0;
                  n_scene_material < n_scene_materials;
                ++n_scene_material)
    {
        scene_material current_scene_material = nullptr;
        mesh_material  new_mesh_material      = nullptr;

        current_scene_material = scene_get_material_by_index             (scene_ptr->result_scene,
                                                                          n_scene_material);
        new_mesh_material      = mesh_material_create_from_scene_material(current_scene_material,
                                                                          scene_ptr->loader_ptr->context_ral);

        ASSERT_DEBUG_SYNC(new_mesh_material != nullptr,
                          "Could not create a mesh_material out of a scene_material");

        if (new_mesh_material == nullptr)
        {
            result = false;

            goto end;
        }
        else
        {
            unsigned int new_mesh_material_id = 0;

            if (!system_hash64map_get(scene_material_to_material_id_map,
                                      (system_hash64) current_scene_material,
                                     &new_mesh_material_id))
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not map a scene_material instance onto a ID");
            }

            system_hash64map_insert(material_id_to_mesh_material_map,
                                    (system_hash64) new_mesh_material_id,
                                    new_mesh_material,
                                    nullptr,  /* on_remove_callback */
                                    nullptr); /* on_remove_callback_user_arg */
        }
    }

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create mesh materials");
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_basic_data(_scene_multiloader_scene*  scene_ptr,
                                                                   system_hashed_ansi_string* out_scene_name,
                                                                   unsigned int*              out_scene_fps,
                                                                   float*                     out_scene_animation_duration,
                                                                   uint32_t*                  out_n_scene_cameras,
                                                                   uint32_t*                  out_n_scene_curves,
                                                                   uint32_t*                  out_n_scene_lights,
                                                                   uint32_t*                  out_n_scene_materials,
                                                                   uint32_t*                  out_n_scene_mesh_instances,
                                                                   uint32_t*                  out_n_scene_textures)
{
    bool  result = true;
    float temp_fps;

    result &= system_file_serializer_read_hashed_ansi_string(scene_ptr->serializer,
                                                             out_scene_name);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(temp_fps),
                                                            &temp_fps);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_scene_animation_duration),
                                                             out_scene_animation_duration);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_n_scene_cameras),
                                                             out_n_scene_cameras);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_n_scene_curves),
                                                             out_n_scene_curves);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_n_scene_lights),
                                                             out_n_scene_lights);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_n_scene_materials),
                                                             out_n_scene_materials);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_n_scene_mesh_instances),
                                                             out_n_scene_mesh_instances);
    result &= system_file_serializer_read                   (scene_ptr->serializer,
                                                             sizeof(*out_n_scene_textures),
                                                             out_n_scene_textures);

    *out_scene_fps = (unsigned int) temp_fps;

    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_camera_data(_scene_multiloader_scene* scene_ptr,
                                                                    uint32_t                  n_scene_cameras,
                                                                    system_hashed_ansi_string scene_file_name,
                                                                    system_resizable_vector   serialized_scene_cameras)
{
    bool result = true;

    for (uint32_t n_scene_camera = 0;
                  n_scene_camera < n_scene_cameras;
                ++n_scene_camera)
    {
        scene_camera new_camera = scene_camera_load(scene_ptr->serializer,
                                                    scene_ptr->result_scene,
                                                    scene_file_name);

        ASSERT_DEBUG_SYNC(new_camera != nullptr,
                          "Cannot spawn new camera instance");

        if (new_camera == nullptr)
        {
            result = false;

            goto end;
        }

        result &= scene_add_camera(scene_ptr->result_scene,
                                   new_camera);

        if (result)
        {
            system_resizable_vector_push(serialized_scene_cameras,
                                         new_camera);

            /* Camera is now owned by the scene */
            scene_camera_release(new_camera);
        }
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_curve_data(_scene_multiloader_scene* scene_ptr,
                                                                   uint32_t                  n_scene_curves,
                                                                   system_hashed_ansi_string scene_file_name)
{
    bool result = true;

    for (uint32_t n_scene_curve = 0;
                  n_scene_curve < n_scene_curves;
                ++n_scene_curve)
    {
        scene_curve new_curve = scene_curve_load(scene_ptr->serializer,
                                                 scene_file_name);

        ASSERT_DEBUG_SYNC(new_curve != nullptr,
                          "Could not load curve data");

        if (new_curve == nullptr)
        {
            result = false;

            goto end;
        }

        result &= scene_add_curve(scene_ptr->result_scene,
                                  new_curve);

        scene_curve_release(new_curve);
        new_curve = nullptr;
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_light_data(_scene_multiloader_scene* scene_ptr,
                                                                   uint32_t                  n_scene_lights,
                                                                   system_hashed_ansi_string scene_file_name,
                                                                   system_resizable_vector   serialized_scene_lights)
{
    bool result = true;

    for (uint32_t n_scene_light = 0;
                  n_scene_light < n_scene_lights;
                ++n_scene_light)
    {
        scene_light new_light = scene_light_load(scene_ptr->serializer,
                                                 scene_ptr->result_scene,
                                                 scene_file_name);

        ASSERT_DEBUG_SYNC(new_light != nullptr,
                          "Could not load light data");

        if (new_light == nullptr)
        {
            result = false;

            goto end;
        }

        result &= scene_add_light(scene_ptr->result_scene,
                                  new_light);

        if (result)
        {
            system_resizable_vector_push(serialized_scene_lights,
                                         new_light);

            /* Light is now owned by the scene */
            scene_light_release(new_light);
        }
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_material_data(_scene_multiloader_scene* scene_ptr,
                                                                      uint32_t                  n_scene_materials,
                                                                      system_hashed_ansi_string scene_file_name,
                                                                      system_hash64map          material_id_to_scene_material_map,
                                                                      system_hash64map          scene_material_to_material_id_map)
{
    bool result = true;

    for (uint32_t n_scene_material = 0;
                  n_scene_material < n_scene_materials;
                ++n_scene_material)
    {
        scene_material new_material    = scene_material_load(scene_ptr->serializer,
                                                             scene_ptr->result_scene,
                                                             scene_file_name);
        unsigned int   new_material_id = -1;

        ASSERT_DEBUG_SYNC(new_material != nullptr,
                          "Could not load material data");

        if (new_material == nullptr)
        {
            result = false;

            goto end;
        }

        /* Load the serialization ID for the material */
        system_file_serializer_read(scene_ptr->serializer,
                                    sizeof(new_material_id),
                                   &new_material_id);

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(material_id_to_scene_material_map,
                                                     (system_hash64) new_material_id),
                          "Material ID is already recognized");

        system_hash64map_insert(material_id_to_scene_material_map,
                                (system_hash64) new_material_id,
                                new_material,
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */
        system_hash64map_insert(scene_material_to_material_id_map,
                                reinterpret_cast<system_hash64>(new_material),
                                reinterpret_cast<void*>        (static_cast<intptr_t>(new_material_id) ),
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */

        /* Attach the material to the scene */
        result &= scene_add_material(scene_ptr->result_scene,
                                     new_material);

        if (result)
        {
            /* scene_add_material() retained the material - release it now */
            scene_material_release(new_material);
        }
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_mesh_data(_scene_multiloader_scene* scene_ptr,
                                                                  system_hash64map          material_id_to_mesh_material_map,
                                                                  system_hash64map          mesh_id_to_mesh_map,
                                                                  system_hash64map          mesh_name_to_mesh_map)
{
    uint32_t n_meshes = 0;
    bool     result   = true;

    result &= system_file_serializer_read(scene_ptr->serializer,
                                          sizeof(n_meshes),
                                         &n_meshes);

    for (unsigned int n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
    {
        mesh                      mesh_gpu     = nullptr;
        unsigned int              mesh_gpu_id  = 0;
        system_hashed_ansi_string mesh_gpu_name = nullptr;

        result &= system_file_serializer_read(scene_ptr->serializer,
                                              sizeof(mesh_gpu_id),
                                             &mesh_gpu_id);

        mesh_gpu = mesh_load_with_serializer(scene_ptr->loader_ptr->context_ral,
                                             MESH_CREATION_FLAGS_LOAD_ASYNC,
                                             scene_ptr->serializer,
                                             material_id_to_mesh_material_map,
                                             mesh_name_to_mesh_map);

        ASSERT_DEBUG_SYNC(mesh_gpu != nullptr,
                          "Could not load mesh data");

        if (mesh_gpu == nullptr)
        {
            result = false;

            goto end;
        }

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_NAME,
                         &mesh_gpu_name);

        /* Store the mesh in internal map that we'll need to use to initialize
         * mesh instances.
         */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(mesh_id_to_mesh_map,
                                                     (system_hash64) mesh_gpu_id),
                          "Unique mesh is not unique!");

        system_hash64map_insert(mesh_id_to_mesh_map,
                                (system_hash64) mesh_gpu_id,
                                mesh_gpu,
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */

        system_hash64map_insert(mesh_name_to_mesh_map,
                                system_hashed_ansi_string_get_hash(mesh_gpu_name),
                                mesh_gpu,
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_mesh_instances_data(_scene_multiloader_scene* scene_ptr,
                                                                            uint32_t                  n_scene_mesh_instances,
                                                                            system_hashed_ansi_string scene_name,
                                                                            system_hash64map          mesh_id_to_mesh_map,
                                                                            system_resizable_vector   serialized_scene_mesh_instances)
{
    bool result = true;

    for (uint32_t n_mesh_instance = 0;
                  n_mesh_instance < n_scene_mesh_instances;
                ++n_mesh_instance)
    {
        scene_mesh new_mesh_instance = scene_mesh_load(scene_ptr->serializer,
                                                       scene_name,
                                                       mesh_id_to_mesh_map);

        ASSERT_DEBUG_SYNC(new_mesh_instance != nullptr,
                          "Could not load mesh instance");

        if (new_mesh_instance == nullptr)
        {
            result = false;

            goto end;
        }

        result &= scene_add_mesh_instance_defined(scene_ptr->result_scene,
                                                  new_mesh_instance);

        if (result)
        {
            uint32_t mesh_id = 0;

            scene_mesh_get_property(new_mesh_instance,
                                    SCENE_MESH_PROPERTY_ID,
                                   &mesh_id);

            system_resizable_vector_push(serialized_scene_mesh_instances,
                                         new_mesh_instance);

            /* Mesh instance is now owned by the scene */
            scene_mesh_release(new_mesh_instance);
        }
    }

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene mesh instances");
    }

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_multiloader_load_scene_internal_get_texture_data(_scene_multiloader_scene* scene_ptr,
                                                                     system_hashed_ansi_string object_manager_path,
                                                                     unsigned int              n_scene_textures,
                                                                     ral_context               context
#if 0
                                                                    ,system_hash64map          texture_id_to_ral_texture_map)
#else
                                                                                                       )
#endif
{
    /*
     * We break the usual scene_texture_load_with_serializer() routine by first
     * caching names of all gfx_images, that there are to load (ensuring only one
     * copy if loaded for each image), and then create a single ogl_texture instance
     * for each gfx_image instance.
     *
     * Since we have multiple scene loading threads in flight, and these scenes
     * may actually use the same texture data, we wait for all threads to create
     * a list of gfx_images which need to be loaded, then load these images using the
     * system thread pool, and then carry on.
     */
    bool result = true;

    for (unsigned int n_scene_texture = 0;
                      n_scene_texture < n_scene_textures;
                    ++n_scene_texture)
    {
        scene_texture new_texture = scene_texture_load_with_serializer(scene_ptr->serializer,
                                                                       object_manager_path,
                                                                       scene_ptr->loader_ptr->context_ral,
                                                                       _scene_multiloader_load_scene_internal_enqueue_gfx_filenames,
                                                                       scene_ptr);

        ASSERT_DEBUG_SYNC(new_texture != nullptr,
                          "Could not load scene texture");

        result &= scene_add_texture(scene_ptr->result_scene,
                                    new_texture);

        scene_texture_release(new_texture); /* texture now owned by the scene */
    }

    /* Wait for all threads to reach this point.
     *
     * NOTE: We want the thread that reached this point as last to set up a new barrier which will
     *       be used to wait until all gfx_image objects are loaded. */
    system_barrier_signal(scene_ptr->loader_ptr->barrier_all_scene_gfx_images_enqueued,
                          true, /* wait_until_signalled */
                          _scene_multiloader_load_scene_internal_create_gfx_images_loaded_barrier,
                          scene_ptr->loader_ptr);

    /* Instantiate gfx_image objects. Of course, the loading process will be
     * carried out by thread pool threads, not in the loader thread.
     *
     * Since this location will be executed in parallel by many threads, we make
     * sure only one thread sets up a barrier that will be waited on by all threads,
     * and released only after all gfx_images are loaded.
     **/
    _scene_multiloader_deferred_gfx_image_load_op* load_op_ptr = nullptr;

    while (system_resizable_vector_pop(scene_ptr->loader_ptr->enqueued_gfx_image_load_ops,
                                      &load_op_ptr) )
    {
        ASSERT_DEBUG_SYNC(load_op_ptr != nullptr,
                          "Load op descriptor is nullptr");

        system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                   _scene_multiloader_load_scene_internal_load_gfx_image_entrypoint,
                                                                                   load_op_ptr);

        system_thread_pool_submit_single_task(task);
    }

    system_barrier_wait_until_signalled(scene_ptr->loader_ptr->barrier_all_scene_gfx_images_loaded);

    /* Create ral_texture for each loaded gfx_image.
     *
     * TODO: This is currently handled by a single rendering thread, but helper contexts will
     *       be introduced shortly to distribute this load off to four helper threads.
     */
    system_critical_section_enter(scene_ptr->loader_ptr->cs);
    {
        /* Note: a single thread handles all loaded gfx_images at the moment. */
        uint32_t n_entries = 0;

        system_hash64map_get_property(scene_ptr->loader_ptr->image_filename_to_gfx_image_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_entries);

        for (uint32_t n_entry = 0;
                      n_entry < n_entries;
                    ++n_entry)
        {
            gfx_image                 gfx_image_instance      = nullptr;
            system_hashed_ansi_string gfx_image_filename      = nullptr;
            system_hash64             gfx_image_filename_hash = 0;
            ral_texture               gfx_image_texture       = nullptr;

            system_hash64map_get_element_at(scene_ptr->loader_ptr->image_filename_to_gfx_image_map,
                                            n_entry,
                                           &gfx_image_instance,
                                           &gfx_image_filename_hash);

            ASSERT_DEBUG_SYNC(gfx_image_instance != nullptr,
                              "gfx_image_instance instance is nullptr");

            gfx_image_filename = (system_hashed_ansi_string) gfx_image_filename_hash;

            ral_context_create_textures_from_gfx_images(scene_ptr->loader_ptr->context_ral,
                                                        1, /* n_images */
                                                       &gfx_image_instance,
                                                       &gfx_image_texture);

            ASSERT_DEBUG_SYNC(gfx_image_texture != nullptr,
                              "ogl_texture_create_from_gfx_image() call failed.");

            gfx_image_release(gfx_image_instance);
        }

        /* All entries processed! */
        system_hash64map_clear(scene_ptr->loader_ptr->image_filename_to_gfx_image_map);
    }
    system_critical_section_leave(scene_ptr->loader_ptr->cs);

    /* Assign ral_texture instances to scene_texture instances */
    system_critical_section_enter(scene_ptr->loader_ptr->cs);
    {
        _scene_multiloader_deferred_gfx_image_to_scene_texture_assignment_op* op_ptr = nullptr;

        while (system_resizable_vector_pop(scene_ptr->enqueued_gfx_image_to_scene_texture_assignment_ops,
                                          &op_ptr) )
        {
            ral_texture texture = ral_context_get_texture_by_file_name(context,
                                                                       op_ptr->filename);

            ASSERT_DEBUG_SYNC(texture != nullptr,
                              "Could not retrieve an ogl_texture instance for filename [%s]",
                              system_hashed_ansi_string_get_buffer(op_ptr->filename) );

            if (op_ptr->uses_mipmaps)
            {
                bool     are_texture_mips_initialized = true;
                uint32_t n_texture_mips               = 0;

                ral_texture_get_property(texture,
                                         RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                        &n_texture_mips);

                for (uint32_t n_texture_mip = 0;
                              n_texture_mip < n_texture_mips;
                            ++n_texture_mip)
                {
                    bool is_current_mip_initialized = false;

                    ral_texture_get_mipmap_property(texture,
                                                    0, /* n_layer */
                                                    n_texture_mip,
                                                    RAL_TEXTURE_MIPMAP_PROPERTY_CONTENTS_SET,
                                                   &is_current_mip_initialized);

                    if (!is_current_mip_initialized)
                    {
                        are_texture_mips_initialized = false;

                        break;
                    }
                }

                if (!are_texture_mips_initialized)
                {
                    ral_texture_generate_mipmaps(texture,
                                                 true /* async */);
                }
            }

            scene_texture_set(op_ptr->texture,
                              SCENE_TEXTURE_PROPERTY_TEXTURE_RAL,
                             &texture);

            /* Release the op descriptor */
            delete op_ptr;

            op_ptr = nullptr;
        }
    }
    system_critical_section_leave(scene_ptr->loader_ptr->cs);

    #if 0

    TODO: This seems to be a functional NOP ?

    /* Carry on with scene_texture initialization */
    for (unsigned int n_scene_texture = 0;
                      n_scene_texture < n_scene_textures;
                    ++n_scene_texture)
    {
        scene_texture current_texture = scene_get_texture_by_index(scene_ptr->result_scene,
                                                                   n_scene_texture);

        /* Map the serialized texture ID to the ogl_texture instance, if necessary */
        ral_texture texture_instance = nullptr;

        scene_texture_get(current_texture,
                          SCENE_TEXTURE_PROPERTY_TEXTURE_RAL,
                         &texture_instance);

        {
            texture_raGL = ral_context_get_texture_gl(scene_ptr->loader_ptr->context_ral,
                                                      texture_instance);

            raGL_texture_get_property(texture_raGL,
                                      RAGL_TEXTURE_PROPERTY_ID,
                                     &texture_gl_id);
        }

        if (!system_hash64map_contains(texture_id_to_ral_texture_map,
                                       texture_instance) )
        {
            system_hash64map_insert(texture_id_to_ral_texture_map,
                                    (system_hash64) texture_gl_id,
                                    texture_instance,
                                    nullptr,  /* on_remove_callback */
                                    nullptr); /* on_remove_callback_user_arg */
        }
    }
    #endif

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene textures");

        result = false;
    }

    return result;
}

/** TODO */
volatile void _scene_multiloader_load_scene_internal_load_gfx_image_entrypoint(system_thread_pool_callback_argument op)
{
    /* BEWARE: This function is executed in parallel by many threads */
    _scene_multiloader_deferred_gfx_image_load_op* op_ptr       = reinterpret_cast<_scene_multiloader_deferred_gfx_image_load_op*>(op);
    gfx_image                                      result_image = nullptr;

    LOG_INFO("Creating a gfx_image instance for file [%s]",
             system_hashed_ansi_string_get_buffer(op_ptr->filename) );

    result_image = gfx_image_create_from_file(op_ptr->name,
                                              op_ptr->filename,
                                              true); /* use_alternative_filename_getter */

    if (result_image == nullptr)
    {
        LOG_FATAL("Could not load texture data from file [%s]",
                  system_hashed_ansi_string_get_buffer(op_ptr->filename) );
    }
    else
    {
        /* Store the result image.
         *
         * The hash-map is in MT-safe mode, so no worries about multiple threads accessing
         * the container at the same time.
         */
        system_hash64map_insert(op_ptr->loader_ptr->image_filename_to_gfx_image_map,
                                (system_hash64) op_ptr->filename,
                                result_image,
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */
    }

    /* Signal the gfx_image barrier to indicate this specific image has been loaded. */
    system_barrier_signal(op_ptr->loader_ptr->barrier_all_scene_gfx_images_loaded,
                          false, /* wait_until_signalled */
                          nullptr,  /* about_to_signal_callback_proc */
                          nullptr); /* about_to_signal_callback_proc_user_arg */

    /* Release the op descriptor */
    delete op_ptr;
    op_ptr = nullptr;
}

/** TODO */
PRIVATE void _scene_multiloader_load_scene_internal(_scene_multiloader_scene* scene_ptr)
{
    system_hash64map          material_id_to_scene_material_map = system_hash64map_create(sizeof(scene_material));
    system_hash64map          material_id_to_mesh_material_map  = system_hash64map_create(sizeof(void*)         );
    system_hash64map          mesh_id_to_mesh_map               = system_hash64map_create(sizeof(void*)         );
    system_hash64map          mesh_name_to_mesh_map             = system_hash64map_create(sizeof(mesh)          );
    scene_graph               new_graph                         = nullptr;
    bool                      result                            = true;
    system_hashed_ansi_string scene_file_name                   = nullptr;
    const char*               scene_file_name_raw               = nullptr;
    const char*               scene_file_name_last_slash_ptr    = nullptr;
    system_resizable_vector   serialized_scene_cameras          = system_resizable_vector_create(4 /* capacity */);
    system_resizable_vector   serialized_scene_lights           = system_resizable_vector_create(4 /* capacity */);
    system_resizable_vector   serialized_scene_mesh_instances   = system_resizable_vector_create(4 /* capacity */);
    system_hash64map          scene_material_to_material_id_map = system_hash64map_create       (sizeof(unsigned int) );

    #if 0
        system_hash64map          texture_id_to_ral_texture_map     = system_hash64map_create       (sizeof(void*) );
    #endif

    /* Read basic stuff */
    float                     scene_animation_duration = 0.0f;
    unsigned int              scene_fps                = 0;
    system_hashed_ansi_string scene_name               = nullptr;
    uint32_t                  n_scene_cameras          = 0;
    uint32_t                  n_scene_curves           = 0;
    uint32_t                  n_scene_lights           = 0;
    uint32_t                  n_scene_materials        = 0;
    uint32_t                  n_scene_mesh_instances   = 0;
    uint32_t                  n_scene_textures         = 0;

    result &= _scene_multiloader_load_scene_internal_get_basic_data(scene_ptr,
                                                                   &scene_name,
                                                                   &scene_fps,
                                                                   &scene_animation_duration,
                                                                   &n_scene_cameras,
                                                                   &n_scene_curves,
                                                                   &n_scene_lights,
                                                                   &n_scene_materials,
                                                                   &n_scene_mesh_instances,
                                                                   &n_scene_textures);

    if (!result)
    {
        goto end_error;
    }

    /* Spawn the scene.
     *
     * NOTE: Scene name is in majority of the cases useless, so switch to
     *       the file name.
     */
    system_file_serializer_get_property(scene_ptr->serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,
                                       &scene_file_name);

    scene_ptr->result_scene = scene_create(scene_ptr->loader_ptr->context_ral,
                                           scene_file_name);

    if (scene_ptr->result_scene == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not spawn result scene");

        result = false;

        goto end_error;
    }

    /* Extract the path to the scene file and add it to the global asset path storage.
     * This is used later on by gfx_image loader to locate the assets, if they are not
     * available under the scene-specified locations (which is usually the case when
     * loading blobs on a different computer, than the one that was used to export the
     * scene).
     */
    scene_file_name_raw            = system_hashed_ansi_string_get_buffer(scene_file_name);
    scene_file_name_last_slash_ptr = strrchr(scene_file_name_raw, '/');

    if (scene_file_name_last_slash_ptr != nullptr)
    {
        system_hashed_ansi_string scene_file_path;

        scene_file_path = system_hashed_ansi_string_create_substring(scene_file_name_raw,
                                                                     0,                                                     /* start_offset */
                                                                     scene_file_name_last_slash_ptr - scene_file_name_raw); /* length */

        system_global_add_asset_path(scene_file_path);
    }

    /* Load curves.
     *
     * This task is pretty light-weight, so no need to carry it out via thread pool tasks.
     */
    result &= _scene_multiloader_load_scene_internal_get_curve_data(scene_ptr,
                                                                    n_scene_curves,
                                                                    scene_file_name);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene curves");

        goto end_error;
    }

    /* Load cameras.
     *
     * Light-weight as well.
     */
    result &= _scene_multiloader_load_scene_internal_get_camera_data(scene_ptr,
                                                                     n_scene_cameras,
                                                                     scene_file_name,
                                                                     serialized_scene_cameras);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene cameras");

        goto end_error;
    }

    /* Load scene lights.
     *
     * You guessed it - this is cheap.
     */
    result &= _scene_multiloader_load_scene_internal_get_light_data(scene_ptr,
                                                                    n_scene_lights,
                                                                    scene_file_name,
                                                                    serialized_scene_lights);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene lights");

        goto end_error;
    }

    /* Load scene materials.
     *
     * We're not loading any textures or mesh data here yet, so we're safe to load these
     * in this thread.
     * */
    result &= _scene_multiloader_load_scene_internal_get_material_data(scene_ptr,
                                                                       n_scene_materials,
                                                                       scene_file_name,
                                                                       material_id_to_scene_material_map,
                                                                       scene_material_to_material_id_map);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load scene materials");

        goto end_error;
    }

    /* Load textures. This is where things get funny.
     *
     * Please check func internals for more details */
    result &= _scene_multiloader_load_scene_internal_get_texture_data(scene_ptr,
                                                                      scene_name,
                                                                      n_scene_textures,
                                                                      scene_ptr->loader_ptr->context_ral
#if 0
                                                                     ,texture_id_to_ral_texture_map);
#else
        );
#endif

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load texture data");

        goto end_error;
    }

    /* Create mesh materials out of the scene materials we've loaded. */
    result &= _scene_multiloader_load_scene_internal_create_mesh_materials(scene_ptr,
                                                                           n_scene_materials,
                                                                           scene_material_to_material_id_map,
                                                                           material_id_to_mesh_material_map);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create mesh_material instances");

        goto end_error;
    }

    /* Load meshes */
    result &= _scene_multiloader_load_scene_internal_get_mesh_data(scene_ptr,
                                                                   material_id_to_mesh_material_map,
                                                                   mesh_id_to_mesh_map,
                                                                   mesh_name_to_mesh_map);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load unique meshes");

        goto end_error;
    }

    /* Load mesh instances */
    result &= _scene_multiloader_load_scene_internal_get_mesh_instances_data(scene_ptr,
                                                                             n_scene_mesh_instances,
                                                                             scene_name,
                                                                             mesh_id_to_mesh_map,
                                                                             serialized_scene_mesh_instances);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load mesh instances");

        goto end_error;
    }

    /* Load the scene graph */
    new_graph = scene_graph_load(scene_ptr->result_scene,
                                 scene_ptr->serializer,
                                 serialized_scene_cameras,
                                 serialized_scene_lights,
                                 serialized_scene_mesh_instances,
                                 scene_file_name);

    ASSERT_DEBUG_SYNC(new_graph != nullptr,
                      "Could not load scene graph");

    if (new_graph == nullptr)
    {
        result = false;

        goto end_error;
    }

    /* Set other scene properties */
    scene_set_property(scene_ptr->result_scene,
                       SCENE_PROPERTY_FPS,
                      &scene_fps);
    scene_set_property(scene_ptr->result_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &scene_animation_duration);

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false,
                      "Could not load scene file [%s]",
                      system_hashed_ansi_string_get_buffer(scene_name) );

end:
    if (material_id_to_mesh_material_map != nullptr)
    {
        uint32_t n_mesh_materials = 0;

        system_hash64map_get_property(material_id_to_mesh_material_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_mesh_materials);

        for (uint32_t n_mesh_material = 0;
                      n_mesh_material < n_mesh_materials;
                    ++n_mesh_material)
        {
            mesh_material material    = nullptr;
            system_hash64 material_id = 0;

            system_hash64map_get_element_at(material_id_to_mesh_material_map,
                                            n_mesh_material,
                                           &material,
                                           &material_id);

            mesh_material_release(material);
            material = nullptr;
        }

        system_hash64map_release(material_id_to_mesh_material_map);

        material_id_to_mesh_material_map = nullptr;
    }

    if (material_id_to_scene_material_map != nullptr)
    {
        system_hash64map_release(material_id_to_scene_material_map);

        material_id_to_scene_material_map = nullptr;
    }

    if (mesh_id_to_mesh_map != nullptr)
    {
        /* All mesh instances can be released, since they should've been
         * retained by scene_mesh_load().
         */
        uint32_t n_meshes = 0;

        system_hash64map_get_property(mesh_id_to_mesh_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_meshes);

        for (uint32_t n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
        {
            mesh current_mesh = nullptr;

            if (system_hash64map_get_element_at(mesh_id_to_mesh_map,
                                                n_mesh,
                                               &current_mesh,
                                                nullptr) ) /* outHash */
            {
                mesh_release(current_mesh);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh instance at index [%d]",
                                  n_mesh);
            }
        }

        system_hash64map_release(mesh_id_to_mesh_map);

        mesh_id_to_mesh_map = nullptr;
    }

    if (mesh_name_to_mesh_map != nullptr)
    {
        system_hash64map_release(mesh_name_to_mesh_map);

        mesh_name_to_mesh_map = nullptr;
    }

    if (scene_material_to_material_id_map != nullptr)
    {
        system_hash64map_release(scene_material_to_material_id_map);

        scene_material_to_material_id_map = nullptr;
    }

    if (serialized_scene_cameras != nullptr)
    {
        /* All camera instances have already been released by this point, so
         * do not release them again.*/
        system_resizable_vector_release(serialized_scene_cameras);

        serialized_scene_cameras = nullptr;
    }

    if (serialized_scene_lights != nullptr)
    {
        /* All light instances have already been released by this point, so
         * do not release them again. */
        system_resizable_vector_release(serialized_scene_lights);

        serialized_scene_lights = nullptr;
    }

    if (serialized_scene_mesh_instances != nullptr)
    {
        /* All mesh instances have already been released by this point, so
         * do not release them again. */
        system_resizable_vector_release(serialized_scene_mesh_instances);

        serialized_scene_mesh_instances = nullptr;
    }

    #if 0
    {
        if (texture_id_to_ral_texture_map != nullptr)
        {
            system_hash64map_release(texture_id_to_ral_texture_map);

            texture_id_to_ral_texture_map = nullptr;
        }
    }
    #endif
}

/** TODO */
PRIVATE void _scene_multiloader_load_scene_thread_entrypoint(system_threads_entry_point_argument arg)
{
    /* NOTE: This function is a new thread's entry-point.
     *
     *       The thread is brand & new and does not come from the thread pool, as if there
     *       would be no free threads to dispatch tasks to, if more scene files were to be
     *       loaded at once, than there were threads hosted by the thread pool.
     */
    _scene_multiloader_scene* scene_ptr = reinterpret_cast<_scene_multiloader_scene*>(arg);
    unsigned int              n_scenes  = 0;

    system_resizable_vector_get_property(scene_ptr->loader_ptr->scenes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_scenes);

    /* Load the scene */
    scene_ptr->loader_ptr->state = SCENE_MULTILOADER_STATE_LOADING_IN_PROGRESS;

    _scene_multiloader_load_scene_internal(scene_ptr);

    /* Signal the thread as finished. */
    scene_ptr->loader_ptr->state = SCENE_MULTILOADER_STATE_FINISHED;

    system_barrier_signal(scene_ptr->loader_ptr->barrier_all_scenes_loaded,
                         false); /* wait_until_signalled */
}


/** Please see header for specification */
PUBLIC EMERALD_API scene_multiloader scene_multiloader_create_from_filenames(ral_context                      context,
                                                                             unsigned int                     n_scenes,
                                                                             const system_hashed_ansi_string* scene_filenames)
{
    ASSERT_DEBUG_SYNC(n_scenes > 0,
                      "n_scenes is 0");

    /* Prepare an array of serializer instances */
    system_file_serializer* serializers = new (std::nothrow) system_file_serializer[n_scenes];

    ASSERT_DEBUG_SYNC(serializers != nullptr,
                      "Out of memory");

    for (unsigned int n_scene = 0;
                      n_scene < n_scenes;
                    ++n_scene)
    {
        serializers[n_scene] = system_file_serializer_create_for_reading(scene_filenames[n_scene]);

        ASSERT_DEBUG_SYNC(serializers[n_scene] != nullptr,
                          "Could not spawn a serializer for filename [%s]",
                          system_hashed_ansi_string_get_buffer(scene_filenames[n_scene]) );
    }

    /* Spawn a multiloader */
    scene_multiloader result = scene_multiloader_create_from_system_file_serializers(context,
                                                                                     n_scenes,
                                                                                     serializers,
                                                                                     true); /* free_serializers_at_release_time */

    /* Good to release the array at this point. The actual serializers will be released
     * by the multiloader, as that's the behavior we have requested.
     */
    delete [] serializers;

    serializers = nullptr;

    /* All done */
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_multiloader scene_multiloader_create_from_system_file_serializers(ral_context                   context,
                                                                                           unsigned int                  n_scenes,
                                                                                           const system_file_serializer* scene_file_serializers,
                                                                                           bool                          free_serializers_at_release_time)
{
    _scene_multiloader* multiloader_ptr = new (std::nothrow) _scene_multiloader(context,
                                                                                free_serializers_at_release_time,
                                                                                n_scenes);

    ASSERT_DEBUG_SYNC(multiloader_ptr != nullptr,
                      "Out of memory");

    if (multiloader_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(multiloader_ptr->scenes != nullptr,
                          "Could not spawn scenes vector");

        /* Create a descriptor for each enqueued scene */
        for (unsigned int n_scene = 0;
                          n_scene < n_scenes;
                        ++n_scene)
        {
            _scene_multiloader_scene* scene_ptr = new (std::nothrow) _scene_multiloader_scene;

            ASSERT_DEBUG_SYNC(scene_ptr != nullptr,
                              "Out of memory");

            scene_ptr->loader_ptr = multiloader_ptr;
            scene_ptr->serializer = scene_file_serializers[n_scene];

            system_resizable_vector_push(multiloader_ptr->scenes,
                                         scene_ptr);
        }

        /* Retain the context - we'll need it for the loading process. */
        ral_context_retain(context);
    }

    return (scene_multiloader) multiloader_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_get_loaded_scene(scene_multiloader loader,
                                                           unsigned int      n_scene,
                                                           scene*            out_result_scene)
{
    _scene_multiloader* loader_ptr = reinterpret_cast<_scene_multiloader*>(loader);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(loader_ptr != nullptr,
                      "scene_multiloader instance is nullptr");
    ASSERT_DEBUG_SYNC(loader_ptr->state == SCENE_MULTILOADER_STATE_FINISHED,
                      "Scene loading process is not finished yet!");
    ASSERT_DEBUG_SYNC(out_result_scene != nullptr,
                      "Out argument is nullptr");

    /* Retrieve the requested scene */
    unsigned int n_scenes = 0;

    system_resizable_vector_get_property(loader_ptr->scenes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_scenes);

    ASSERT_DEBUG_SYNC(n_scene < n_scenes,
                      "Invalid scene index requested");

    if (n_scenes > n_scene)
    {
        _scene_multiloader_scene* scene_ptr = nullptr;

        system_resizable_vector_get_element_at(loader_ptr->scenes,
                                               n_scene,
                                              &scene_ptr);

        *out_result_scene = scene_ptr->result_scene;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_load_async(scene_multiloader loader)
{
    _scene_multiloader* instance_ptr = reinterpret_cast<_scene_multiloader*>(loader);
    unsigned int        n_scenes     = 0;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(instance_ptr != nullptr,
                      "scene_multiloader instance is nullptr.");

    if (instance_ptr->state != SCENE_MULTILOADER_STATE_CREATED)
    {
        ASSERT_DEBUG_SYNC(false,
                          "scene_multiloader_load_async() failed: scene_multiloader() has already been used to load scenes.");

        goto end;
    }

    /* Use system thread pool to load the scene.
     *
     * NOTE: This will break in non-deterministic way if any of the scene assets are shared between scenes!
     *       The implementation will shortly be changed to make the multiloader work for such cases as well.
     */
    system_resizable_vector_get_property(instance_ptr->scenes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_scenes);

    for (unsigned int n_scene = 0;
                      n_scene < n_scenes;
                    ++n_scene)
    {
        _scene_multiloader_scene* scene_ptr = nullptr;

        system_resizable_vector_get_element_at(instance_ptr->scenes,
                                               n_scene,
                                              &scene_ptr);

        ASSERT_DEBUG_SYNC(scene_ptr != nullptr,
                          "Could not retrieve scene descriptor at index [%d]",
                          n_scene);

        system_threads_spawn(_scene_multiloader_load_scene_thread_entrypoint,
                             scene_ptr,
                             nullptr, /* thread_wait_event */
                             system_hashed_ansi_string_create("Scene multi-loader thread") );
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_release(scene_multiloader instance)
{
    _scene_multiloader* instance_ptr = reinterpret_cast<_scene_multiloader*>(instance);

    if (instance_ptr->context_ral != nullptr)
    {
        ral_context_release(instance_ptr->context_ral);

        instance_ptr->context_ral = nullptr;
    }

    delete instance_ptr;
    instance_ptr = nullptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_wait_until_finished(scene_multiloader loader)
{
    ral_backend_type    backend_type;
    _scene_multiloader* loader_ptr = reinterpret_cast<_scene_multiloader*>(loader);
    ral_scheduler       scheduler  = nullptr;

    demo_app_get_property   (DEMO_APP_PROPERTY_GPU_SCHEDULER,
                            &scheduler);
    ral_context_get_property(loader_ptr->context_ral,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    system_barrier_wait_until_signalled(loader_ptr->barrier_all_scenes_loaded);
    ral_scheduler_finish               (scheduler,
                                        backend_type);
}