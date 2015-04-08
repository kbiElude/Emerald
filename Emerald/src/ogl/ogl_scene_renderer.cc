/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_textures.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_bbox_preview.h"
#include "ogl/ogl_scene_renderer_frustum_preview.h"
#include "ogl/ogl_scene_renderer_lights_preview.h"
#include "ogl/ogl_scene_renderer_normals_preview.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_mesh.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_hash64map.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_variant.h"
#include <float.h>

/* Forward declarations */
PRIVATE void _ogl_scene_renderer_deinit_cached_ubers_map_contents         (__in            __notnull system_hash64map                cached_materials_map);
PRIVATE void _ogl_scene_renderer_deinit_resizable_vector_for_resource_pool(                          system_resource_pool_block);
PRIVATE void _ogl_scene_renderer_get_light_color                          (__in            __notnull scene_light                     light,
                                                                           __in                      system_timeline_time            time,
                                                                           __in            __notnull system_variant                  temp_float_variant,
                                                                           __out_ecount(3) __notnull float*                          out_color);
PRIVATE void _ogl_scene_renderer_get_ogl_uber_for_render_mode             (__in                      _ogl_scene_renderer_render_mode render_mode,
                                                                           __in            __notnull ogl_materials                   context_materials,
                                                                           __in            __notnull scene                           scene,
                                                                           __out           __notnull ogl_uber*                       result_uber_ptr);
PRIVATE void _ogl_scene_renderer_init_resizable_vector_for_resource_pool  (                          system_resource_pool_block);
PRIVATE void _ogl_scene_renderer_on_camera_show_frustum_setting_changed   (                          const void*                     unused,
                                                                                                           void*                     scene_renderer);
PRIVATE void _ogl_scene_renderer_on_ubers_map_invalidated                 (                          const void*                     unused,
                                                                                                           void*                     scene_renderer);
PRIVATE void _ogl_scene_renderer_process_mesh_for_forward_rendering       (                __notnull scene_mesh                      scene_mesh_instance,
                                                                           __in                      void*                           renderer);
PRIVATE void _ogl_scene_renderer_process_mesh_for_shadow_map_rendering    (                __notnull scene_mesh                      scene_mesh_instance,
                                                                           __in                      void*                           renderer);
PRIVATE void _ogl_scene_renderer_process_mesh_for_shadow_map_pre_pass     (                __notnull scene_mesh                      scene_mesh_instance,
                                                                           __in                      void*                           renderer);
PRIVATE void _ogl_scene_renderer_return_shadow_maps_to_pool               (__in            __notnull ogl_scene_renderer              renderer);
PRIVATE void _ogl_scene_renderer_update_frustum_preview_assigned_cameras  (__in            __notnull struct _ogl_scene_renderer*     renderer_ptr);
PRIVATE void _ogl_scene_renderer_update_ogl_uber_light_properties         (__in            __notnull ogl_uber                        material_uber,
                                                                           __in            __notnull scene                           scene,
                                                                           __in            __notnull system_matrix4x4                current_camera_view_matrix,
                                                                           __in                      system_timeline_time            frame_time,
                                                                           __in            __notnull system_variant                  temp_variant_float);


/* Private type definitions */
typedef struct _ogl_scene_renderer_mesh
{
    mesh_material    material;
    uint32_t         mesh_id;
    mesh             mesh_instance;
    system_matrix4x4 model_matrix;
    system_matrix4x4 normal_matrix;

    _ogl_scene_renderer_mesh()
    {
        material      = NULL;
        mesh_id       = -1;
        mesh_instance = NULL;
        model_matrix  = NULL;
        normal_matrix = NULL;
    }

    ~_ogl_scene_renderer_mesh()
    {
        if (model_matrix != NULL)
        {
            /* This is not expensive, since matrices are stored in a global
             * resource pool.
             */
            system_matrix4x4_release(model_matrix);

            model_matrix = NULL;
        }

        if (normal_matrix != NULL)
        {
            system_matrix4x4_release(normal_matrix);

            normal_matrix = NULL;
        }
    }

} _ogl_scene_renderer_mesh;

typedef struct _ogl_scene_renderer_mesh_id_data
{

    /** Model matrices should only be updated if necessary (eg. when the
     *  new model matrix is different at the beginning of ogl_scene_renderer_render_scene_graph().
     *
     *  In future, for GPU code-paths, this should be replaced with a persistent buffer storage.
     */
    mesh             mesh_instance;
    system_matrix4x4 model_matrix;
    system_matrix4x4 normal_matrix;

} _ogl_scene_renderer_mesh_id_data;

typedef struct _ogl_scene_renderer_uber
{
    system_resizable_vector meshes; /* holds _ogl_scene_renderer_mesh* items */

    _ogl_scene_renderer_uber()
    {
        meshes = NULL;
    }

    ~_ogl_scene_renderer_uber()
    {
        if (meshes != NULL)
        {
            system_resizable_vector_release(meshes);

            meshes = NULL;
        }
    }
} _ogl_scene_renderer_uber;

typedef struct _ogl_scene_renderer
{
    ogl_scene_renderer_bbox_preview    bbox_preview;
    ogl_scene_renderer_frustum_preview frustum_preview;
    ogl_scene_renderer_lights_preview  lights_preview;
    ogl_scene_renderer_normals_preview normals_preview;

    ogl_context    context;
    ogl_materials  material_manager;
    scene          scene;
    system_variant temp_variant_float;

    float                                    current_camera_visible_world_aabb_max[3];
    float                                    current_camera_visible_world_aabb_min[3];
    scene_camera                             current_camera;
    _ogl_scene_renderer_helper_visualization current_helper_visualization;
    system_matrix4x4                         current_model_matrix;
    system_matrix4x4                         current_projection;
    system_matrix4x4                         current_view;
    system_matrix4x4                         current_vp;

    /** Holds _ogl_scene_renderer_mesh_id_data entries */
    system_hash64map mesh_id_map;

    system_resource_pool    mesh_id_data_pool;
    system_resource_pool    scene_renderer_mesh_pool;
    system_resource_pool    vector_pool;

    system_hash64map        ubers_map;        /* key: ogl_uber; value: _ogl_scene_renderer_uber */

    _ogl_scene_renderer()
    {
        bbox_preview                    = NULL;
        context                         = NULL;
        current_camera                  = NULL;
        current_model_matrix            = system_matrix4x4_create();
        lights_preview                  = NULL;
        mesh_id_data_pool               = system_resource_pool_create(sizeof(_ogl_scene_renderer_mesh_id_data),
                                                                      4,     /* n_elements_to_preallocate */
                                                                      NULL,  /* init_fn */
                                                                      NULL); /* deinit_fn */
        mesh_id_map                     = system_hash64map_create    (sizeof(_ogl_scene_renderer_mesh_id_data*) );
        normals_preview                 = NULL;
        scene_renderer_mesh_pool        = system_resource_pool_create(sizeof(_ogl_scene_renderer_mesh),
                                                                      4,     /* n_elements_to_preallocate */
                                                                      NULL,  /* init_fn */
                                                                      NULL); /* deinit_fn */
        ubers_map                       = system_hash64map_create    (sizeof(_ogl_scene_renderer_uber*) );
        vector_pool                     = system_resource_pool_create(sizeof(system_resizable_vector),
                                                                      64, /* capacity */
                                                                      _ogl_scene_renderer_init_resizable_vector_for_resource_pool,
                                                                      _ogl_scene_renderer_deinit_resizable_vector_for_resource_pool);

        temp_variant_float = system_variant_create(SYSTEM_VARIANT_FLOAT);
    }

    ~_ogl_scene_renderer()
    {
        LOG_INFO("Scene renderer deallocating..");

        if (bbox_preview != NULL)
        {
            ogl_scene_renderer_bbox_preview_release(bbox_preview);

            bbox_preview = NULL;
        }

        if (current_model_matrix != NULL)
        {
            system_matrix4x4_release(current_model_matrix);

            current_model_matrix = NULL;
        }

        if (lights_preview != NULL)
        {
            ogl_scene_renderer_lights_preview_release(lights_preview);

            lights_preview = NULL;
        }

        if (mesh_id_data_pool != NULL)
        {
            system_resource_pool_release(mesh_id_data_pool);

            mesh_id_data_pool = NULL;
        }

        if (mesh_id_map != NULL)
        {
            system_hash64map_release(mesh_id_map);

            mesh_id_map = NULL;
        }

        if (normals_preview != NULL)
        {
            ogl_scene_renderer_normals_preview_release(normals_preview);

            normals_preview = NULL;
        }

        if (scene_renderer_mesh_pool != NULL)
        {
            system_resource_pool_release(scene_renderer_mesh_pool);

            scene_renderer_mesh_pool = NULL;
        }

        if (temp_variant_float != NULL)
        {
           system_variant_release(temp_variant_float);

           temp_variant_float = NULL;
        }

        if (ubers_map != NULL)
        {
            _ogl_scene_renderer_deinit_cached_ubers_map_contents(ubers_map);

            system_hash64map_release(ubers_map);
            ubers_map = NULL;
        }

        if (vector_pool != NULL)
        {
            system_resource_pool_release(vector_pool);

            vector_pool = NULL;
        }

        if (scene != NULL)
        {
            scene_release(scene);
        }
    }
} _ogl_scene_renderer;

/* TODO */
PRIVATE void _ogl_scene_renderer_deinit_cached_ubers_map_contents(__in __notnull system_hash64map ubers_map)
{
    system_hash64             ogl_uber_hash = 0;
    _ogl_scene_renderer_uber* uber_ptr     = NULL;

    while (system_hash64map_get_element_at(ubers_map,
                                           0,
                                           &uber_ptr,
                                           &ogl_uber_hash) )
    {
        delete uber_ptr;
        uber_ptr = NULL;

        if (!system_hash64map_remove(ubers_map,
                                     ogl_uber_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "system_hash64map_remove() call failed.");
        }
    } 
}

/* TODO */
PRIVATE void _ogl_scene_renderer_deinit_resizable_vector_for_resource_pool(system_resource_pool_block block)
{
    system_resizable_vector* vector_ptr = (system_resizable_vector*) block;

    system_resizable_vector_release(*vector_ptr);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_get_light_color(__in            __notnull scene_light          light,
                                                 __in                      system_timeline_time time,
                                                 __in            __notnull system_variant       temp_float_variant,
                                                 __out_ecount(3) __notnull float*               out_color)
{
    curve_container  light_color_curves      [3];
    curve_container  light_color_intensity_curve;
    float            light_color_intensity = 0.0f;
    scene_light_type light_type            = SCENE_LIGHT_TYPE_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_COLOR,
                             light_color_curves);
    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_COLOR_INTENSITY,
                            &light_color_intensity_curve);

    curve_container_get_value(light_color_intensity_curve,
                              time,
                              false, /* should_force */
                              temp_float_variant);
    system_variant_get_float (temp_float_variant,
                             &light_color_intensity);

    for (unsigned int n_color_component = 0;
                      n_color_component < 3;
                    ++n_color_component)
    {
        curve_container_get_value(light_color_curves[n_color_component],
                                  time,
                                  false, /* should_force */
                                  temp_float_variant);
        system_variant_get_float (temp_float_variant,
                                  out_color + n_color_component);

        out_color[n_color_component] *= light_color_intensity;
    }
}

/** TODO */
PRIVATE void _ogl_scene_renderer_get_ogl_uber_for_render_mode(__in            _ogl_scene_renderer_render_mode render_mode,
                                                              __in  __notnull ogl_materials                   context_materials,
                                                              __in  __notnull scene                           scene,
                                                              __out __notnull ogl_uber*                       result_uber_ptr)
{
    switch (render_mode)
    {
        case RENDER_MODE_FORWARD:
        {
            /* Do not over-write the uber instance! */

            break;
        }

        case RENDER_MODE_NORMALS_ONLY:
        {
            *result_uber_ptr = mesh_material_get_ogl_uber(ogl_materials_get_special_material(context_materials,
                                                                                             SPECIAL_MATERIAL_NORMALS),
                                                          scene,
                                                          false); /* use_shadow_maps */

            break;
        }

        case RENDER_MODE_TEXCOORDS_ONLY:
        {
            *result_uber_ptr = mesh_material_get_ogl_uber(ogl_materials_get_special_material(context_materials,
                                                                                             SPECIAL_MATERIAL_TEXCOORD),
                                                          scene,
                                                          false); /* use_shadow_maps */

            break;
        }

        default:
        {
            /* This function should never be called for RENDER_MODE_SHADOW_MAP. SM-related tasks
             * should be handed over to ogl_shadow_mapping.
             */
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized render mode");
        }
    } /* switch (render_mode) */
}

/** TODO */
PRIVATE void _ogl_scene_renderer_init_resizable_vector_for_resource_pool(system_resource_pool_block block)
{
    system_resizable_vector* vector_ptr = (system_resizable_vector*) block;

    *vector_ptr = system_resizable_vector_create(64 /* capacity */,
                                                 sizeof(mesh_material) );
}

/** TODO */
PRIVATE void _ogl_scene_renderer_on_camera_show_frustum_setting_changed(const void* unused,
                                                                              void* scene_renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) scene_renderer;

    /* Use a shared handler to re-create the frustum assignments for the frustum preview renderer */
    _ogl_scene_renderer_update_frustum_preview_assigned_cameras(renderer_ptr);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_on_mesh_material_ogl_uber_invalidated(const void* callback_data,
                                                                             void* scene_renderer)
{
    mesh_material        source_material    = (mesh_material)        callback_data;
    _ogl_scene_renderer* scene_renderer_ptr = (_ogl_scene_renderer*) scene_renderer;

    _ogl_scene_renderer_deinit_cached_ubers_map_contents(scene_renderer_ptr->ubers_map);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_on_ubers_map_invalidated(const void* unused,
                                                                void* scene_renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) scene_renderer;

    /* TODO: Subscribe for "show frustum changed" setting! */
    ASSERT_DEBUG_SYNC(false,
                      "TODO: subscriptions!");

    /* Reset all cached ubers */
    system_hash64map_clear(renderer_ptr->ubers_map);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_process_mesh_for_forward_rendering(__notnull scene_mesh scene_mesh_instance,
                                                                    __in      void*      renderer)
{
    _ogl_scene_renderer*    renderer_ptr                  = (_ogl_scene_renderer*) renderer;
    mesh_material           material                      = NULL;
    mesh                    mesh_gpu                      = NULL;
    mesh                    mesh_instantiation_parent_gpu = NULL;
    uint32_t                mesh_id                       = -1;
    system_resizable_vector mesh_materials                = NULL;
    ogl_uber                mesh_uber                     = NULL;
    unsigned int            n_mesh_materials              = 0;

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_MESH,
                           &mesh_gpu);

    mesh_get_property(mesh_gpu,
                      MESH_PROPERTY_INSTANTIATION_PARENT,
                     &mesh_instantiation_parent_gpu);

    if (mesh_instantiation_parent_gpu == NULL)
    {
        mesh_instantiation_parent_gpu = mesh_gpu;
    }

    mesh_get_property(mesh_instantiation_parent_gpu,
                      MESH_PROPERTY_MATERIALS,
                     &mesh_materials);

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_ID,
                            &mesh_id);

    n_mesh_materials = system_resizable_vector_get_amount_of_elements(mesh_materials);

    /* Perform frustum culling to make sure it actually makes sense to render
     * this mesh.
     *
     * NOTE: We use the actual mesh instance for the culling process, NOT the
     *       parent which provides the mesh data. This is to ensure that the
     *       model matrix we use for the calculations refers to the actual object
     *       of our interest.
     */
    if (!ogl_scene_renderer_cull_against_frustum( (ogl_scene_renderer) renderer,
                                                  mesh_instantiation_parent_gpu,
                                                  OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES,
                                                  NULL) ) /* behavior_data */
    {
        goto end;
    }

    /* Is this mesh a shadow receiver? */
    bool is_shadow_receiver = false;

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER,
                           &is_shadow_receiver);

    /* Cache the mesh for rendering */
    for (unsigned int n_mesh_material = 0;
                      n_mesh_material < n_mesh_materials;
                    ++n_mesh_material)
    {
        _ogl_scene_renderer_uber* uber_ptr = NULL;

        if (!system_resizable_vector_get_element_at(mesh_materials,
                                                    n_mesh_material,
                                                   &material) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh material at index [%d]",
                              n_mesh_material);
        }

        /* Retrieve ogl_uber that can render the material for the currently processed
         * scene configuration.
         */
        mesh_uber = mesh_material_get_ogl_uber(material,
                                               renderer_ptr->scene,
                                               is_shadow_receiver);

        if (!system_hash64map_get(renderer_ptr->ubers_map,
                                  (system_hash64) mesh_uber,
                                 &uber_ptr) )
        {
            /* NOTE: This will only be called once, every time ogl_uber is used for the
             *       first time for current scene renderer instance. This should be a
             *       negligible cost, so let's leave this memory allocation here, in order
             *       to avoid making code more complex.
             *       We cannot resource pool this allocation, since the constructor instantiates
             *       a resizable vector.
             */
            uber_ptr = new (std::nothrow) _ogl_scene_renderer_uber;

            ASSERT_ALWAYS_SYNC(uber_ptr != NULL,
                               "Out of memory");

            uber_ptr->meshes = system_resizable_vector_create(64 /* capacity */,
                                                              sizeof(_ogl_scene_renderer_mesh*));

            system_hash64map_insert(renderer_ptr->ubers_map,
                                    (system_hash64) mesh_uber,
                                    uber_ptr,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_argument */
        }

        /* This is a new user of the material. Store it in the vector */
        _ogl_scene_renderer_mesh* new_mesh_item = (_ogl_scene_renderer_mesh*) system_resource_pool_get_from_pool(renderer_ptr->scene_renderer_mesh_pool);

        ASSERT_ALWAYS_SYNC(new_mesh_item != NULL,
                           "Out of memory");

        new_mesh_item->material      = material;
        new_mesh_item->mesh_id       = mesh_id;
        new_mesh_item->mesh_instance = mesh_gpu;
        new_mesh_item->model_matrix  = system_matrix4x4_create();
        new_mesh_item->normal_matrix = system_matrix4x4_create();

        system_matrix4x4_set_from_matrix4x4(new_mesh_item->model_matrix,
                                            renderer_ptr->current_model_matrix);

        /* Form normal matrix.
         *
         * TODO: This could be cached. Also, normal matrix is equal to model matrix,
         *       as long as no non-uniform scaling operator is applied. We could use
         *       this to avoid the calculations below.
         */
        system_matrix4x4_set_from_matrix4x4(new_mesh_item->normal_matrix,
                                            renderer_ptr->current_model_matrix);
        system_matrix4x4_invert            (new_mesh_item->normal_matrix);
        system_matrix4x4_transpose         (new_mesh_item->normal_matrix);

        /* Store the data */
        if (renderer_ptr->current_helper_visualization != 0)
        {
            _ogl_scene_renderer_mesh_id_data* new_entry = (_ogl_scene_renderer_mesh_id_data*) system_resource_pool_get_from_pool(renderer_ptr->mesh_id_data_pool);

            new_entry->mesh_instance = new_mesh_item->mesh_instance;
            new_entry->model_matrix  = new_mesh_item->model_matrix;
            new_entry->normal_matrix = new_mesh_item->normal_matrix;

            system_hash64map_insert(renderer_ptr->mesh_id_map,
                                    mesh_id,
                                    new_entry,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }

        system_resizable_vector_push(uber_ptr->meshes,
                                     new_mesh_item);
    } /* for (all mesh materials) */

end:
    ;
}

/** TODO */
PRIVATE void _ogl_scene_renderer_return_shadow_maps_to_pool(__in __notnull ogl_scene_renderer renderer)
{
    uint32_t             n_lights       = 0;
    _ogl_scene_renderer* renderer_ptr   = (_ogl_scene_renderer*) renderer;
    ogl_shadow_mapping   shadow_mapping = NULL;
    ogl_context_textures texture_pool   = NULL;

    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_SHADOW_MAPPING,
                            &shadow_mapping);
    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &texture_pool);
    scene_get_property      (renderer_ptr->scene,
                             SCENE_PROPERTY_N_LIGHTS,
                            &n_lights);

    /* Iterate over all lights defined for the scene. */
    ogl_texture null_texture = NULL;

    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light current_light                  = scene_get_light_by_index(renderer_ptr->scene,
                                                                              n_light);
        ogl_texture current_light_sm_texture_color = NULL;
        ogl_texture current_light_sm_texture_depth = NULL;

        ASSERT_DEBUG_SYNC(current_light != NULL,
                          "Scene light is NULL");

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_COLOR,
                                &current_light_sm_texture_color);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_DEPTH,
                                &current_light_sm_texture_depth);

        if (current_light_sm_texture_color != NULL)
        {
            ogl_context_textures_return_reusable(renderer_ptr->context,
                                                 current_light_sm_texture_color);
        } /* if (current_light_sm_texture_color != NULL) */

        if (current_light_sm_texture_depth != NULL)
        {
            ogl_context_textures_return_reusable(renderer_ptr->context,
                                                 current_light_sm_texture_depth);
        } /* if (current_light_sm_texture_depth != NULL) */
    } /* for (all scene lights) */
}

/** TODO */
PRIVATE void _ogl_scene_renderer_update_frustum_preview_assigned_cameras(__in __notnull _ogl_scene_renderer* renderer_ptr)
{
    /* This function may be called via a call-back. Make sure the frustum preview handler is instantiated
     * before carrying on.
     */
    if (renderer_ptr->frustum_preview == NULL)
    {
        renderer_ptr->frustum_preview = ogl_scene_renderer_frustum_preview_create(renderer_ptr->context,
                                                                                  renderer_ptr->scene);
    }

    /* Prepare a buffer that can hold up to the number of cameras added to the scene */
    scene_camera* assigned_cameras   = NULL;
    uint32_t      n_assigned_cameras = 0;
    uint32_t      n_scene_cameras    = 0;

    scene_get_property(renderer_ptr->scene,
                       SCENE_PROPERTY_N_CAMERAS,
                      &n_scene_cameras);

    if (n_scene_cameras != 0)
    {
        assigned_cameras = new (std::nothrow) scene_camera[n_scene_cameras];

        ASSERT_DEBUG_SYNC(assigned_cameras != NULL,
                          "Out of memory");

        for (uint32_t n_scene_camera = 0;
                      n_scene_camera < n_scene_cameras;
                    ++n_scene_camera)
        {
            bool         current_camera_show_frustum = false;
            scene_camera current_camera              = scene_get_camera_by_index(renderer_ptr->scene,
                                                                                 n_scene_camera);

            scene_camera_get_property(current_camera,
                                      SCENE_CAMERA_PROPERTY_SHOW_FRUSTUM,
                                      0, /* time is irrelevant */
                                     &current_camera_show_frustum);

            if (current_camera_show_frustum)
            {
                assigned_cameras[n_assigned_cameras] = current_camera;
                n_assigned_cameras++;
            }
        } /* for (all scene cameras) */
    } /* if (n_scene_cameras != 0) */

    /* Feed the data to the frustum preview renderer */
    ogl_scene_renderer_frustum_preview_assign_cameras(renderer_ptr->frustum_preview,
                                                      n_assigned_cameras,
                                                      assigned_cameras);

    /* Clean up */
    if (assigned_cameras != NULL)
    {
        delete [] assigned_cameras;

        assigned_cameras = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_scene_renderer_update_ogl_uber_light_properties(__in __notnull ogl_uber             material_uber,
                                                                  __in __notnull scene                scene,
                                                                  __in __notnull system_matrix4x4     current_camera_view_matrix,
                                                                  __in           system_timeline_time frame_time,
                                                                  __in __notnull system_variant       temp_variant_float)
{
    unsigned int n_scene_lights = 0;
    unsigned int n_uber_items   = 0;

    scene_get_property                  (scene,
                                         SCENE_PROPERTY_N_LIGHTS,
                                        &n_scene_lights);
    ogl_uber_get_shader_general_property(material_uber,
                                         OGL_UBER_GENERAL_PROPERTY_N_ITEMS,
                                        &n_uber_items);

    ASSERT_DEBUG_SYNC(n_uber_items == n_scene_lights,
                      "ogl_scene_renderer needs to be re-initialized, as the light dconfiguration has changed");

    for (unsigned int n_light = 0;
                      n_light < n_scene_lights;
                    ++n_light)
    {
        scene_light         current_light                       = scene_get_light_by_index(scene,
                                                                                           n_light);
        scene_light_type    current_light_type                  = SCENE_LIGHT_TYPE_UNKNOWN;
        curve_container     current_light_attenuation_curves[3];
        float               current_light_attenuation_floats[3];
        float               current_light_color_floats      [3];
        curve_container     current_light_cone_angle_half_curve;
        float               current_light_cone_angle_half_float;
        float               current_light_direction_floats  [3];
        curve_container     current_light_edge_angle_curve;
        float               current_light_edge_angle_float;
        float               current_light_position_floats   [3];
        curve_container     current_light_range_curve;
        float               current_light_range_float;
        scene_light_falloff current_light_falloff;
        bool                current_light_shadow_caster         = false;

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                &current_light_shadow_caster);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_TYPE,
                                &current_light_type);

        if (current_light_shadow_caster)
        {
            system_matrix4x4                 current_light_depth_view                  = NULL;
            const float*                     current_light_depth_view_row_major        = NULL;
            const float*                     current_light_depth_vp_row_major          = NULL;
            system_matrix4x4                 current_light_depth_vp                    = NULL;
            ogl_texture                      current_light_shadow_map_texture_color    = NULL;
            ogl_texture                      current_light_shadow_map_texture_depth    = NULL;
            float                            current_light_shadow_map_vsm_cutoff       = 0;
            float                            current_light_shadow_map_vsm_min_variance = 0;
            scene_light_shadow_map_algorithm current_light_sm_algo;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                                    &current_light_sm_algo);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_COLOR,
                                    &current_light_shadow_map_texture_color);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_DEPTH,
                                    &current_light_shadow_map_texture_depth);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VIEW,
                                    &current_light_depth_view);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP,
                                    &current_light_depth_vp);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_CUTOFF,
                                    &current_light_shadow_map_vsm_cutoff);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_MIN_VARIANCE,
                                    &current_light_shadow_map_vsm_min_variance);

            /* Depth view matrix */
            current_light_depth_view_row_major = system_matrix4x4_get_row_major_data(current_light_depth_view);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,
                                              current_light_depth_view);

            /* Depth VP matrix */
            current_light_depth_vp_row_major = system_matrix4x4_get_row_major_data(current_light_depth_vp);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP,
                                              current_light_depth_vp_row_major);

            /* Shadow map textures */
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_COLOR,
                                             &current_light_shadow_map_texture_color);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_DEPTH,
                                             &current_light_shadow_map_texture_depth);

            /* VSM cut-off (if VSM is enabled) */
            if (current_light_sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
            {
                ogl_uber_set_shader_item_property(material_uber,
                                                  n_light,
                                                  OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF,
                                                 &current_light_shadow_map_vsm_cutoff);
                ogl_uber_set_shader_item_property(material_uber,
                                                  n_light,
                                                  OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE,
                                                 &current_light_shadow_map_vsm_min_variance);
            } /* if (current_light_sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM) */
        }

        _ogl_scene_renderer_get_light_color(current_light,
                                            frame_time,
                                            temp_variant_float,
                                            current_light_color_floats);

        if (current_light_type == SCENE_LIGHT_TYPE_AMBIENT)
        {
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR,
                                              current_light_color_floats);
        }
        else
        {
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE,
                                              current_light_color_floats);
        }

        if (current_light_type == SCENE_LIGHT_TYPE_POINT ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            /* position curves */
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_POSITION,
                                     current_light_position_floats);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION,
                                              current_light_position_floats);

            /* Attenuation.
             *
             * The behavior depends on the fall-off type selected for the light. */
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_FALLOFF,
                                    &current_light_falloff);

            switch (current_light_falloff)
            {
                case SCENE_LIGHT_FALLOFF_CUSTOM:
                {
                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION,
                                             current_light_attenuation_curves + 0);
                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION,
                                             current_light_attenuation_curves + 1);
                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION,
                                             current_light_attenuation_curves + 2);

                    for (unsigned int n_curve = 0;
                                      n_curve < 3;
                                    ++n_curve)
                    {
                        curve_container src_curve     = current_light_attenuation_curves[n_curve];
                        float*          dst_float_ptr = current_light_attenuation_floats + n_curve;

                        curve_container_get_value(src_curve,
                                                  frame_time,
                                                  false, /* should_force */
                                                  temp_variant_float);
                        system_variant_get_float (temp_variant_float,
                                                  dst_float_ptr);
                    } /* for (all curves) */

                    ogl_uber_set_shader_item_property(material_uber,
                                                      n_light,
                                                      OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS,
                                                      current_light_attenuation_floats);

                    break;
                } /* case SCENE_LIGHT_FALLOFF_CUSTOM: */

                case SCENE_LIGHT_FALLOFF_LINEAR:
                case SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE:
                case SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE_SQUARE:
                {
                    scene_light_get_property (current_light,
                                              SCENE_LIGHT_PROPERTY_RANGE,
                                             &current_light_range_curve);
                    curve_container_get_value(current_light_range_curve,
                                              frame_time,
                                              false, /* should_force */
                                              temp_variant_float);
                    system_variant_get_float (temp_variant_float,
                                             &current_light_range_float);

                    ogl_uber_set_shader_item_property(material_uber,
                                                      n_light,
                                                      OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE,
                                                     &current_light_range_float);

                    break;
                } /* case SCENE_LIGHT_FALLOFF_LINEAR: */

                case SCENE_LIGHT_FALLOFF_OFF:
                {
                    /* Nothing to do here */
                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized light falloff type");
                }
            } /* switch (current_light_falloff) */
        }

        if (current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            scene_light_get_property         (current_light,
                                              SCENE_LIGHT_PROPERTY_DIRECTION,
                                              current_light_direction_floats);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION,
                                              current_light_direction_floats);
        }

        if (current_light_type == SCENE_LIGHT_TYPE_POINT)
        {
            float            current_light_far_plane;
            float            current_light_near_plane;
            float            current_light_plane_diff;
            const float*     current_light_projection_matrix_data = NULL;
            system_matrix4x4 current_light_projection_matrix      = NULL;
            const float*     current_light_view_matrix_data       = NULL;
            system_matrix4x4 current_light_view_matrix            = NULL;

            /* Update ogl_uber with point light-specific data */
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,
                                    &current_light_far_plane);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE,
                                    &current_light_near_plane);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_PROJECTION,
                                    &current_light_projection_matrix);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VIEW,
                                    &current_light_view_matrix);

            current_light_plane_diff = current_light_far_plane - current_light_near_plane;
            ASSERT_DEBUG_SYNC(current_light_plane_diff >= 0.0f,
                              "Something's wrong with far/near plane distance settings for one of the lights");

            current_light_projection_matrix_data = system_matrix4x4_get_row_major_data(current_light_projection_matrix);
            current_light_view_matrix_data       = system_matrix4x4_get_row_major_data(current_light_view_matrix);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF,
                                             &current_light_plane_diff);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE,
                                             &current_light_near_plane);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX,
                                              current_light_projection_matrix_data);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,
                                              current_light_view_matrix_data);
        }

        if (current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF,
                                    &current_light_cone_angle_half_curve);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_EDGE_ANGLE,
                                    &current_light_edge_angle_curve);

            curve_container_get_value(current_light_cone_angle_half_curve,
                                      frame_time,
                                      false, /* should_force */
                                      temp_variant_float);
            system_variant_get_float (temp_variant_float,
                                     &current_light_cone_angle_half_float);

            curve_container_get_value(current_light_edge_angle_curve,
                                      frame_time,
                                      false, /* should_force */
                                      temp_variant_float);
            system_variant_get_float (temp_variant_float,
                                     &current_light_edge_angle_float);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE,
                                             &current_light_cone_angle_half_float);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE,
                                             &current_light_edge_angle_float);
        }
    } /* for (all lights) */
}


/** Please see header for specification */
PUBLIC EMERALD_API void ogl_scene_renderer_bake_gpu_assets(__in __notnull ogl_scene_renderer renderer)
{
    ogl_materials        context_materials = NULL;
    _ogl_scene_renderer* renderer_ptr      = (_ogl_scene_renderer*) renderer;

    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_MATERIALS,
                            &context_materials);

    /* At the time of writing, the only thing we need to bake is ogl_uber instances, which are
     * specific to materials used by meshes. */
    unsigned int n_meshes = 0;

    scene_get_property(renderer_ptr->scene,
                       SCENE_PROPERTY_N_UNIQUE_MESHES,
                      &n_meshes);

    ASSERT_DEBUG_SYNC(n_meshes > 0,
                      "No unique meshes found for ogl_scene_renderer's scene.");

    for (unsigned int n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
    {
        mesh                    current_mesh           = scene_get_unique_mesh_by_index(renderer_ptr->scene,
                                                                                        n_mesh);
        system_resizable_vector current_mesh_materials = NULL;

        ASSERT_DEBUG_SYNC(current_mesh != NULL,
                          "Could not retrieve mesh instance");

        mesh_get_property(current_mesh,
                          MESH_PROPERTY_MATERIALS,
                         &current_mesh_materials);

        ASSERT_DEBUG_SYNC(current_mesh_materials != NULL,
                          "Could not retrieve material vector for current mesh instance");

        /* Iterate over all materials defined for the current mesh */
        unsigned int n_mesh_materials = system_resizable_vector_get_amount_of_elements(current_mesh_materials);

        ASSERT_DEBUG_SYNC(n_mesh_materials > 0,
                          "No mesh_material instances assigned ot a mesh!");

        for (unsigned int n_mesh_material = 0;
                          n_mesh_material < n_mesh_materials;
                        ++n_mesh_material)
        {
            mesh_material current_mesh_material = NULL;

            system_resizable_vector_get_element_at(current_mesh_materials,
                                                   n_mesh_material,
                                                  &current_mesh_material);

            ASSERT_DEBUG_SYNC(current_mesh_material != NULL,
                              "mesh_material instance is NULL");

            /* Ensure there's an ogl_uber instance prepared for both SM and non-SM cases */
            ogl_uber uber_w_sm  = mesh_material_get_ogl_uber(current_mesh_material,
                                                             renderer_ptr->scene,
                                                             false); /* use_shadow_maps */
            ogl_uber uber_wo_sm = mesh_material_get_ogl_uber(current_mesh_material,
                                                             renderer_ptr->scene,
                                                             true); /* use_shadow_maps */

            ogl_uber_link(uber_w_sm);
            ogl_uber_link(uber_wo_sm);
        } /* for (all mesh materials) */
    } /* for (all GPU meshes) */
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_scene_renderer ogl_scene_renderer_create(__in __notnull ogl_context context,
                                                                __in __notnull scene       scene)
{
    _ogl_scene_renderer* scene_renderer_ptr = new (std::nothrow) _ogl_scene_renderer;

    ASSERT_ALWAYS_SYNC(scene_renderer_ptr != NULL,
                       "Out of memory");

    if (scene_renderer_ptr != NULL)
    {
        scene_renderer_ptr->bbox_preview     = NULL;    /* can be instantiated at draw time */
        scene_renderer_ptr->context          = context;
        scene_renderer_ptr->frustum_preview  = NULL;    /* can be instantiated at draw time */
        scene_renderer_ptr->lights_preview   = NULL;    /* can be instantiated at draw time */
        scene_renderer_ptr->material_manager = NULL;
        scene_renderer_ptr->normals_preview  = NULL;    /* can be instantiated at draw time */
        scene_renderer_ptr->scene            = scene;

        scene_retain(scene);

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_MATERIALS,
                                &scene_renderer_ptr->material_manager);

        /* Sign up for "show frustum" state changes for cameras & lights. This is needed to properly
         * re-route the events to the scene renderer */
        uint32_t                n_scene_cameras        = 0;
        system_callback_manager scene_callback_manager = NULL;

        scene_get_property(scene,
                           SCENE_PROPERTY_CALLBACK_MANAGER,
                          &scene_callback_manager);
        scene_get_property(scene,
                           SCENE_PROPERTY_N_CAMERAS,
                          &n_scene_cameras);

        for (uint32_t n_scene_camera = 0;
                      n_scene_camera < n_scene_cameras;
                    ++n_scene_camera)
        {
            system_callback_manager current_camera_callback_manager = NULL;
            scene_camera            current_camera                  = scene_get_camera_by_index(scene,
                                                                                                n_scene_camera);

            scene_camera_get_property(current_camera,
                                      SCENE_CAMERA_PROPERTY_CALLBACK_MANAGER,
                                      0, /* time - irrelevant */
                                     &current_camera_callback_manager);

            system_callback_manager_subscribe_for_callbacks(current_camera_callback_manager,
                                                            SCENE_CAMERA_CALLBACK_ID_SHOW_FRUSTUM_CHANGED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _ogl_scene_renderer_on_camera_show_frustum_setting_changed,
                                                            scene_renderer_ptr);
        } /* for (all scene cameras) */

        /* Since ogl_scene_renderer caches ogl_uber instances, given current scene configuration,
         * we need to register for various scene call-backs in order to ensure these instances
         * are reset, if the scene configuration ever changes.
         */
        system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                        SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _ogl_scene_renderer_on_ubers_map_invalidated,
                                                        scene_renderer_ptr);                /* callback_proc_user_arg */

        /* Sign up for mesh_material callbacks. In case the material becomes outdated, we need
         * to re-create the entry for mesh_uber */
        uint32_t n_scene_unique_meshes = 0;

        scene_get_property(scene,
                           SCENE_PROPERTY_N_UNIQUE_MESHES,
                          &n_scene_unique_meshes);

        for (uint32_t n_scene_unique_mesh = 0;
                      n_scene_unique_mesh < n_scene_unique_meshes;
                    ++n_scene_unique_mesh)
        {
            mesh                    current_mesh             = scene_get_unique_mesh_by_index(scene,
                                                                                              n_scene_unique_mesh);
            system_resizable_vector current_mesh_materials   = NULL;
            uint32_t                n_current_mesh_materials = 0;

            ASSERT_DEBUG_SYNC(current_mesh != NULL,
                              "Could not retrieve unique mesh at index [%d]",
                              n_scene_unique_mesh);

            mesh_get_property(current_mesh,
                              MESH_PROPERTY_MATERIALS,
                             &current_mesh_materials);

            ASSERT_DEBUG_SYNC(current_mesh_materials != NULL,
                              "Could not retrieve unique mesh materials.");

            n_current_mesh_materials = system_resizable_vector_get_amount_of_elements(current_mesh_materials);

            for (uint32_t n_current_mesh_material = 0;
                          n_current_mesh_material < n_current_mesh_materials;
                        ++n_current_mesh_material)
            {
                mesh_material           current_mesh_material                  = NULL;
                system_callback_manager current_mesh_material_callback_manager = NULL;

                system_resizable_vector_get_element_at(current_mesh_materials,
                                                       n_current_mesh_material,
                                                      &current_mesh_material);

                ASSERT_DEBUG_SYNC(current_mesh_material != NULL,
                                  "Could not retrieve mesh_material instance.");

                /* Finally, time to sign up! */
                mesh_material_get_property(current_mesh_material,
                                           MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,
                                          &current_mesh_material_callback_manager);

                ASSERT_DEBUG_SYNC(current_mesh_material_callback_manager != NULL,
                                  "Could not retrieve callback manager from a mesh_material instance.");

                system_callback_manager_subscribe_for_callbacks(current_mesh_material_callback_manager,
                                                                MESH_MATERIAL_CALLBACK_ID_OGL_UBER_UPDATED,
                                                                CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                                _ogl_scene_renderer_on_mesh_material_ogl_uber_invalidated,
                                                                scene_renderer_ptr);
            } /* for (all current mesh materials) */
        } /* for (all unique meshes) */
    } /* if (scene_renderer_ptr != NULL) */

    return (ogl_scene_renderer) scene_renderer_ptr;
}

/** Please see header for specification */
PUBLIC bool ogl_scene_renderer_cull_against_frustum(__in __notnull ogl_scene_renderer                           renderer,
                                                    __in __notnull mesh                                         mesh_gpu,
                                                    __in           _ogl_scene_renderer_frustum_culling_behavior behavior,
                                                    __in __notnull const void*                                  behavior_data)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;
    bool                 result       = true;

    /* Retrieve AABB for the mesh */
    const float* aabb_max_ptr = NULL;
    const float* aabb_min_ptr = NULL;

    mesh_get_property      (mesh_gpu,
                            MESH_PROPERTY_MODEL_AABB_MAX,
                           &aabb_max_ptr);
    mesh_get_property      (mesh_gpu,
                            MESH_PROPERTY_MODEL_AABB_MIN,
                           &aabb_min_ptr);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(aabb_max_ptr[0] != aabb_min_ptr[0] &&
                      aabb_max_ptr[1] != aabb_min_ptr[1] &&
                      aabb_max_ptr[2] != aabb_min_ptr[2],
                      "Sanity checks failed");

    /* Prepare an array holding OBB of the model's AABB transformed into clip space. */
    /* Transform the model-space AABB to world-space */
    float world_aabb_max     [3];
    float world_aabb_min     [3];
    float world_aabb_vertices[8][4];

    for (uint32_t n_vertex = 0;
                  n_vertex < 8;
                ++n_vertex)
    {
        float vertex[4] =
        {
            (n_vertex & 1) ? aabb_max_ptr[0] : aabb_min_ptr[0],
            (n_vertex & 2) ? aabb_max_ptr[1] : aabb_min_ptr[1],
            (n_vertex & 4) ? aabb_max_ptr[2] : aabb_min_ptr[2],
            1.0f
        };

        system_matrix4x4_multiply_by_vector4(renderer_ptr->current_model_matrix,
                                             vertex,
                                             world_aabb_vertices[n_vertex]);

        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (n_vertex == 0 ||
                n_vertex != 0 && world_aabb_min[n] > world_aabb_vertices[n_vertex][n])
            {
                world_aabb_min[n] = world_aabb_vertices[n_vertex][n];
            }

            if (n_vertex == 0 ||
                n_vertex != 0 && world_aabb_max[n] < world_aabb_vertices[n_vertex][n])
            {
                world_aabb_max[n] = world_aabb_vertices[n_vertex][n];
            }
        }
    }

    /* Execute requested culling behavior */
    switch (behavior)
    {
        case OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_PASS_OBJECTS_IN_FRONT_OF_CAMERA:
        {
            const float* camera_location_world_vec3 = ((const float*) behavior_data);
            const float* camera_view_direction_vec3 = ((const float*) behavior_data) + 3;

            /* Is any of the mesh's AABB vertices in front of the camera? */
            result = false;

            for (uint32_t n_aabb_vertex = 0;
                          n_aabb_vertex < 8 && !result;
                        ++n_aabb_vertex)
            {
                      float camera_to_vertex     [3];
                      float camera_to_vertex_norm[3];
                      float dot_product;
                const float world_vertex[3] =
                {
                    (n_aabb_vertex & 1) ? world_aabb_max[0] : world_aabb_min[0],
                    (n_aabb_vertex & 2) ? world_aabb_max[1] : world_aabb_min[1],
                    (n_aabb_vertex & 4) ? world_aabb_max[2] : world_aabb_min[2],
                };

                system_math_vector_minus3    (world_vertex,
                                              camera_location_world_vec3,
                                              camera_to_vertex);
                system_math_vector_normalize3(camera_to_vertex,
                                              camera_to_vertex_norm);

                /*            V
                 *            ^
                 *            |------
                 *            | angle\
                 *270' --------------------- 90'
                 *             \
                 *              \
                 *               v V'
                 *
                 * Basic linear algebra fact:            A.B = |A| |B| cos(angle).
                 * Since |A| = 1 and |B|, this gives us: A.B = cos(angle)
                 *
                 * To determine if V' is pointing in the same direction as V,
                 * we need to check if angle is between <0', 90'> or <270', 360'>.
                 * This requirement is met if cos(angle) >= 0.
                 */
                dot_product = system_math_vector_dot3(camera_view_direction_vec3,
                                                      camera_to_vertex_norm);

                if (dot_product >= 0.0f)
                {
                    result = true;
                }
            } /* for (all AABB vertices) */

            if (result)
            {
                /* TODO: Only take the mesh into account if it is within point light range */
            }

            break;
        } /* case OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_PASS_OBJECTS_IN_FRONT_OF_CAMERA: */

        case OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES:
        {
            /* Retrieve clipping planes from the VP. This gives us world clipping planes in world space. */
            float world_clipping_plane_bottom[4];
            float world_clipping_plane_front [4];
            float world_clipping_plane_left  [4];
            float world_clipping_plane_near  [4];
            float world_clipping_plane_right [4];
            float world_clipping_plane_top   [4];

            system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                                SYSTEM_MATRIX4X4_CLIPPING_PLANE_BOTTOM,
                                                world_clipping_plane_bottom);
            system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                                SYSTEM_MATRIX4X4_CLIPPING_PLANE_FAR,
                                                world_clipping_plane_front);
            system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                                SYSTEM_MATRIX4X4_CLIPPING_PLANE_LEFT,
                                                world_clipping_plane_left);
            system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                                SYSTEM_MATRIX4X4_CLIPPING_PLANE_NEAR,
                                                world_clipping_plane_near);
            system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                                SYSTEM_MATRIX4X4_CLIPPING_PLANE_RIGHT,
                                                world_clipping_plane_right);
            system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                                SYSTEM_MATRIX4X4_CLIPPING_PLANE_TOP,
                                                world_clipping_plane_top);

            system_math_vector_normalize4_use_vec3_length(world_clipping_plane_bottom,
                                                          world_clipping_plane_bottom);
            system_math_vector_normalize4_use_vec3_length(world_clipping_plane_front,
                                                          world_clipping_plane_front);
            system_math_vector_normalize4_use_vec3_length(world_clipping_plane_left,
                                                          world_clipping_plane_left);
            system_math_vector_normalize4_use_vec3_length(world_clipping_plane_near,
                                                          world_clipping_plane_near);
            system_math_vector_normalize4_use_vec3_length(world_clipping_plane_right,
                                                          world_clipping_plane_right);
            system_math_vector_normalize4_use_vec3_length(world_clipping_plane_top,
                                                          world_clipping_plane_top);

            /* Iterate over all clipping planes.. */
            const float* clipping_planes[] =
            {
                world_clipping_plane_bottom,
                world_clipping_plane_front,
                world_clipping_plane_left,
                world_clipping_plane_near,
                world_clipping_plane_right,
                world_clipping_plane_top
            };
            const uint32_t n_clipping_planes = sizeof(clipping_planes) / sizeof(clipping_planes[0]);

            for (uint32_t n_clipping_plane = 0;
                          n_clipping_plane < n_clipping_planes;
                        ++n_clipping_plane)
            {
                const float* clipping_plane  = clipping_planes[n_clipping_plane];
                unsigned int counter_inside  = 0;
                unsigned int counter_outside = 0;

                /* Compute plane normal */
                float clipping_plane_normal[4] =
                {
                    clipping_plane[0],
                    clipping_plane[1],
                    clipping_plane[2],
                    clipping_plane[3]
                };

                for (uint32_t n_vertex = 0;
                              n_vertex < 8 && (counter_inside == 0 || counter_outside == 0);
                            ++n_vertex)
                {
                    /* Is the vertex inside? */
                    float distance = clipping_plane_normal[3] + ((world_aabb_vertices[n_vertex][0] * clipping_plane_normal[0]) +
                                                                 (world_aabb_vertices[n_vertex][1] * clipping_plane_normal[1]) +
                                                                 (world_aabb_vertices[n_vertex][2] * clipping_plane_normal[2]) );

                    if (distance < 0)
                    {
                        counter_outside++;
                    }
                    else
                    {
                        counter_inside++;
                    }
                } /* for (all bbox vertices) */

                if (counter_inside == 0)
                {
                    /* BBox is outside, no need to render */
                    result = false;

                    goto end;
                }
                else
                if (counter_outside > 0)
                {
                    /* Intersection, need to keep iterating */
                }
            } /* for (all clipping planes) */

            break;
        } /* case OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES: */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized frustum culling behavior requested");
        }
    } /* switch (behavior) */

end:

    /* All done */
    if (result)
    {
        /* Update max & min AABB coordinates stored in ogl_scene_renderer,
         * as the tested mesh is a part of the frustum */
        for (unsigned int n_component = 0;
                          n_component < 3;
                        ++n_component)
        {
            if (renderer_ptr->current_camera_visible_world_aabb_max[n_component] < world_aabb_max[n_component])
            {
                renderer_ptr->current_camera_visible_world_aabb_max[n_component] = world_aabb_max[n_component];
            }

            if (renderer_ptr->current_camera_visible_world_aabb_min[n_component] > world_aabb_min[n_component])
            {
                renderer_ptr->current_camera_visible_world_aabb_min[n_component] = world_aabb_min[n_component];
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_scene_renderer_get_indexed_property(__in  __notnull const ogl_scene_renderer    renderer,
                                                    __in            ogl_scene_renderer_property property,
                                                    __in            uint32_t                    index,
                                                    __out __notnull void*                       out_result)
{
    const _ogl_scene_renderer* renderer_ptr = (const _ogl_scene_renderer*) renderer;

    switch (property)
    {
        case OGL_SCENE_RENDERER_PROPERTY_MESH_INSTANCE:
        {
            const _ogl_scene_renderer_mesh_id_data* entry_data_ptr = NULL;

            if (system_hash64map_get(renderer_ptr->mesh_id_map,
                                     index,
                                    &entry_data_ptr) )
            {
                *((mesh*) out_result) = entry_data_ptr->mesh_instance;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh instance for mesh id [%d]",
                                  index);
            }

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX:
        {
            const _ogl_scene_renderer_mesh_id_data* entry_data_ptr = NULL;

            if (system_hash64map_get(renderer_ptr->mesh_id_map,
                                     index,
                                     &entry_data_ptr) )
            {
                *((system_matrix4x4*) out_result) = entry_data_ptr->model_matrix;
            }

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX:
        {
            const _ogl_scene_renderer_mesh_id_data* entry_data_ptr = NULL;

            if (system_hash64map_get(renderer_ptr->mesh_id_map,
                                     index,
                                     &entry_data_ptr) )
            {
                *((system_matrix4x4*) out_result) = entry_data_ptr->normal_matrix;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve normal matrix for mesh id [%d]",
                                  index);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized indexed ogl_scene_renderer_property");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_scene_renderer_get_property(__in  __notnull ogl_scene_renderer          renderer,
                                                        __in            ogl_scene_renderer_property property,
                                                        __out __notnull void*                       out_result)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    switch (property)
    {
        case OGL_SCENE_RENDERER_PROPERTY_CONTEXT:
        {
            *(ogl_context *) out_result = renderer_ptr->context;

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_GRAPH:
        {
            scene_get_property(renderer_ptr->scene,
                               SCENE_PROPERTY_GRAPH,
                               out_result);

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX:
        {
            *(system_matrix4x4*) out_result = renderer_ptr->current_model_matrix;

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX:
        {
            memcpy(out_result,
                   renderer_ptr->current_camera_visible_world_aabb_max,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_max) );

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN:
        {
            memcpy(out_result,
                   renderer_ptr->current_camera_visible_world_aabb_min,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_min) );

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_VP:
        {
            *(system_matrix4x4*) out_result = renderer_ptr->current_vp;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_graph property");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_render_scene_graph(__in           __notnull ogl_scene_renderer                       renderer,
                                                                         __in           __notnull system_matrix4x4                         view,
                                                                         __in           __notnull system_matrix4x4                         projection,
                                                                         __in           __notnull scene_camera                             camera,
                                                                         __in                     const _ogl_scene_renderer_render_mode&   render_mode,
                                                                         __in                     bool                                     apply_shadow_mapping,
                                                                         __in           __notnull _ogl_scene_renderer_helper_visualization helper_visualization,
                                                                         __in                     system_timeline_time                     frame_time)
{
    _ogl_scene_renderer*              renderer_ptr       = (_ogl_scene_renderer*) renderer;
    float                             camera_location[4];
    const ogl_context_gl_entrypoints* entry_points       = NULL;
    scene_graph                       graph              = NULL;
    ogl_materials                     materials          = NULL;
    unsigned int                      n_scene_lights     = 0;
    ogl_shadow_mapping                shadow_mapping     = NULL;

    scene_get_property      (renderer_ptr->scene,
                             SCENE_PROPERTY_GRAPH,
                            &graph);
    scene_get_property      (renderer_ptr->scene,
                             SCENE_PROPERTY_N_LIGHTS,
                            &n_scene_lights);

    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_MATERIALS,
                            &materials);
    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_SHADOW_MAPPING,
                            &shadow_mapping);

    /* Calculate camera location */
    {
        /* Matrix inverse = transposal for view matrices (which never use scaling) */
        system_matrix4x4 view_transposed      = system_matrix4x4_create();
        const float*     view_transposed_data = NULL;

        system_matrix4x4_set_from_matrix4x4(view_transposed,
                                            view);
        system_matrix4x4_transpose         (view_transposed);

        view_transposed_data = system_matrix4x4_get_row_major_data(view_transposed);
        camera_location[0]   = view_transposed_data[3];
        camera_location[1]   = view_transposed_data[7];
        camera_location[2]   = view_transposed_data[11];
        camera_location[3]   = 1.0f;

        system_matrix4x4_release(view_transposed);
    }

    /* Prepare graph traversal parameters */
    system_matrix4x4 vp = system_matrix4x4_create_by_mul(projection,
                                                         view);

    renderer_ptr->current_camera               = camera;
    renderer_ptr->current_projection           = projection;
    renderer_ptr->current_view                 = view;
    renderer_ptr->current_vp                   = vp;
    renderer_ptr->current_helper_visualization = helper_visualization;

    /* If the caller has requested shadow mapping support, we need to render shadow maps before actual
     * rendering proceeds.
     */
    static const bool shadow_mapping_disabled = false;
    static const bool shadow_mapping_enabled  = true;

    if (apply_shadow_mapping)
    {
        scene_set_property(renderer_ptr->scene,
                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                          &shadow_mapping_disabled);

        /* Prepare the shadow maps */
        ogl_shadow_mapping_render_shadow_maps(shadow_mapping,
                                              renderer,
                                              renderer_ptr->scene,
                                              camera,
                                              frame_time);

        scene_set_property(renderer_ptr->scene,
                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                          &shadow_mapping_enabled);

        /* The _ogl_scene_renderer_render_shadow_maps() call might have messed up
         * projection, view & vp matrices we set up.
         *
         * Revert the original settings */
        renderer_ptr->current_helper_visualization = helper_visualization;
        renderer_ptr->current_projection           = projection;
        renderer_ptr->current_view                 = view;
        renderer_ptr->current_vp                   = vp;
    } /* if (shadow_mapping != SHADOW_MAPPING_DISABLED) */

    /* 1. Traverse the scene graph and:
     *
     *    - update light direction vectors.
     *    - determine mesh visibility via frustum culling.
     *
     *  NOTE: We cache 'helper visualization' setting, as the helper rendering requires
     *        additional information that needs not be stored otherwise.
     */
    scene_graph_traverse(graph,
                         ogl_scene_renderer_update_current_model_matrix,
                         NULL, /* insert_camera_proc */
                         ogl_scene_renderer_update_light_properties,
                         (render_mode == RENDER_MODE_SHADOW_MAP) ? ogl_shadow_mapping_process_mesh_for_shadow_map_rendering
                                                                 : _ogl_scene_renderer_process_mesh_for_forward_rendering,
                         renderer,
                         frame_time);

    /* Set up GL before we continue */
    entry_points->pGLFrontFace(GL_CCW);
    entry_points->pGLDepthFunc(GL_LESS);
    entry_points->pGLEnable   (GL_CULL_FACE);
    entry_points->pGLEnable   (GL_DEPTH_TEST);

    if (render_mode == RENDER_MODE_FORWARD)
    {
        entry_points->pGLEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        entry_points->pGLEnable(GL_FRAMEBUFFER_SRGB);
    }

    /* 2. Start uber rendering. Issue as many render requests as there are materials.
     *
     *    TODO: We could obviously expand this by performing more complicated sorting
     *          but this will do for now.
     */
    if (render_mode == RENDER_MODE_SHADOW_MAP)
    {
        ogl_shadow_mapping_render_shadow_map_meshes(shadow_mapping,
                                                    renderer,
                                                    renderer_ptr->scene,
                                                    frame_time);
    } /* if (render_mode == RENDER_MODE_SHADOW_MAP) */
    else
    {
        system_hash64             material_hash     = 0;
        ogl_uber                  material_uber     = NULL;
        uint32_t                  n_iteration_items = 0;
        uint32_t                  n_iterations      = system_hash64map_get_amount_of_elements(renderer_ptr->ubers_map);
        _ogl_scene_renderer_uber* uber_details_ptr  = NULL;

        for (uint32_t n_iteration = 0;
                      n_iteration < n_iterations;
                    ++n_iteration)
        {
            /* Retrieve ogl_uber instance */
            if (render_mode == RENDER_MODE_FORWARD)
            {
                system_hash64map_get_element_at(renderer_ptr->ubers_map,
                                                n_iteration,
                                                &uber_details_ptr,
                                                &material_hash);

                ASSERT_DEBUG_SYNC(material_hash != 0,
                                  "No ogl_uber instance available!");

                material_uber     = (ogl_uber) material_hash;
                n_iteration_items = system_resizable_vector_get_amount_of_elements(uber_details_ptr->meshes);

                _ogl_scene_renderer_update_ogl_uber_light_properties(material_uber,
                                                                     renderer_ptr->scene,
                                                                     renderer_ptr->current_view,
                                                                     frame_time,
                                                                     renderer_ptr->temp_variant_float);
            }
            else
            {
                _ogl_scene_renderer_get_ogl_uber_for_render_mode(render_mode,
                                                                 materials,
                                                                 renderer_ptr->scene,
                                                                &material_uber);

                ASSERT_DEBUG_SYNC(material_uber != NULL,
                                  "No ogl_uber instance available!");

                system_hash64map_get(renderer_ptr->ubers_map,
                                     (system_hash64) material_uber,
                                     &uber_details_ptr);

                n_iteration_items = system_resizable_vector_get_amount_of_elements(uber_details_ptr->meshes);
            }

            /* Update global properties of ogl_uber's vertex shader  */
            ogl_uber_set_shader_general_property(material_uber,
                                                 OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,
                                                 camera_location);
            ogl_uber_set_shader_general_property(material_uber,
                                                 OGL_UBER_GENERAL_PROPERTY_VP,
                                                 vp);

            /* Okay. Go on with the rendering */
            ogl_uber_rendering_start(material_uber);
            {
                _ogl_scene_renderer_mesh* item_ptr = NULL;

                for (uint32_t n_iteration_item = 0;
                              n_iteration_item < n_iteration_items;
                            ++n_iteration_item)
                {
                    system_resizable_vector_get_element_at(uber_details_ptr->meshes,
                                                           n_iteration_item,
                                                          &item_ptr);

                    ogl_uber_rendering_render_mesh(item_ptr->mesh_instance,
                                                   item_ptr->model_matrix,
                                                   item_ptr->normal_matrix,
                                                   material_uber,
                                                   item_ptr->material,
                                                   frame_time);
                }
            }
            ogl_uber_rendering_stop(material_uber);

            /* Any helper visualization, handle it at this point */
            if (helper_visualization & HELPER_VISUALIZATION_BOUNDING_BOXES &&
                n_iteration_items > 0)
            {
                if (renderer_ptr->bbox_preview == NULL)
                {
                    renderer_ptr->bbox_preview = ogl_scene_renderer_bbox_preview_create(renderer_ptr->context,
                                                                                        renderer_ptr->scene,
                                                                                        (ogl_scene_renderer) renderer_ptr);
                } /* if (renderer_ptr->bbox_preview == NULL) */

                ogl_scene_renderer_bbox_preview_start(renderer_ptr->bbox_preview,
                                                      vp);
                {
                    _ogl_scene_renderer_mesh* item_ptr = NULL;

                    for (uint32_t n_vector_item = 0;
                                  n_vector_item < n_iteration_items;
                                ++n_vector_item)
                    {
                        system_resizable_vector_get_element_at(uber_details_ptr->meshes,
                                                               n_vector_item,
                                                              &item_ptr);

                        ogl_scene_renderer_bbox_preview_render(renderer_ptr->bbox_preview,
                                                               item_ptr->mesh_id);
                    }
                }
                ogl_scene_renderer_bbox_preview_stop(renderer_ptr->bbox_preview);
            } /* if (helper_visualization & HELPER_VISUALIZATION_BOUNDING_BOXES && n_iteration_items > 0) */

            if (helper_visualization & HELPER_VISUALIZATION_NORMALS && n_iteration_items > 0)
            {
                if (renderer_ptr->normals_preview == NULL)
                {
                    renderer_ptr->normals_preview = ogl_scene_renderer_normals_preview_create(renderer_ptr->context,
                                                                                              renderer_ptr->scene,
                                                                                              (ogl_scene_renderer) renderer_ptr);
                } /* if (renderer_ptr->normals_preview == NULL) */

                ogl_scene_renderer_normals_preview_start(renderer_ptr->normals_preview,
                                                         vp);
                {
                    _ogl_scene_renderer_mesh* item_ptr = NULL;

                    for (uint32_t n_vector_item = 0;
                                  n_vector_item < n_iteration_items;
                                ++n_vector_item)
                    {
                        system_resizable_vector_get_element_at(uber_details_ptr->meshes,
                                                               n_vector_item,
                                                              &item_ptr);

                        ogl_scene_renderer_normals_preview_render(renderer_ptr->normals_preview,
                                                                  item_ptr->mesh_id);
                    }
                }
                ogl_scene_renderer_normals_preview_stop(renderer_ptr->normals_preview);
            } /* if (helper_visualization & HELPER_VISUALIZATION_NORMALS && n_iteration_items > 0) */

            /* Clean up */
            _ogl_scene_renderer_mesh* mesh_ptr = NULL;

            while (system_resizable_vector_pop(uber_details_ptr->meshes,
                                              &mesh_ptr) )
            {
                if (mesh_ptr->model_matrix != NULL)
                {
                    system_matrix4x4_release(mesh_ptr->model_matrix);
                }

                if (mesh_ptr->normal_matrix != NULL)
                {
                    system_matrix4x4_release(mesh_ptr->normal_matrix);
                }

                system_resource_pool_return_to_pool(renderer_ptr->scene_renderer_mesh_pool,
                                                    (system_resource_pool_block) mesh_ptr);
            }
        } /* for (all ubers) */

        if (helper_visualization & HELPER_VISUALIZATION_FRUSTUMS)
        {
            if (renderer_ptr->frustum_preview == NULL)
            {
                renderer_ptr->frustum_preview = ogl_scene_renderer_frustum_preview_create(renderer_ptr->context,
                                                                                          renderer_ptr->scene);

                /* Assign existing cameras to the call-back */
                _ogl_scene_renderer_update_frustum_preview_assigned_cameras(renderer_ptr);
            } /* if (renderer_ptr->frustum_preview == NULL) */

            ogl_scene_renderer_frustum_preview_render(renderer_ptr->frustum_preview,
                                                      frame_time,
                                                      vp);
        }

        if (helper_visualization & HELPER_VISUALIZATION_LIGHTS)
        {
            if (renderer_ptr->lights_preview == NULL)
            {
                renderer_ptr->lights_preview = ogl_scene_renderer_lights_preview_create(renderer_ptr->context,
                                                                                        renderer_ptr->scene);
            } /* if (renderer_ptr->lights_preview == NULL) */

            ogl_scene_renderer_lights_preview_start(renderer_ptr->lights_preview);
            {
                for (unsigned int n_light = 0;
                                  n_light < n_scene_lights;
                                ++n_light)
                {
                    scene_light      current_light              = scene_get_light_by_index(renderer_ptr->scene,
                                                                                          n_light);
                    scene_light_type current_light_type         = SCENE_LIGHT_TYPE_UNKNOWN;

                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_TYPE,
                                            &current_light_type);

                    /* NOTE: We can only show point (& spot) lights.
                     *
                     * TODO: Include directional light support. These have no position
                     *       so we may need to come up with some kind of a *cough* design
                     *       for this.
                     */
                    if (current_light_type == SCENE_LIGHT_TYPE_POINT)
                    {
#if 0
                        float current_light_direction[3] = {NULL};

                        scene_light_get_property(current_light,
                                                 SCENE_LIGHT_PROPERTY_DIRECTION,
                                                 current_light_direction);

#endif
                        float current_light_position[3] = {NULL};
                        float current_light_color   [4] =
                        {
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f
                        };

                        scene_light_get_property           (current_light,
                                                            SCENE_LIGHT_PROPERTY_POSITION,
                                                           &current_light_position);
                        _ogl_scene_renderer_get_light_color(current_light,
                                                            frame_time,
                                                            renderer_ptr->temp_variant_float,
                                                            current_light_color);

                        /* Dispatch the rendering request */
                        float current_light_position_m[4] =
                        {
                            current_light_position[0],
                            current_light_position[1],
                            current_light_position[2],
                            1.0f
                        };
                        float current_light_position_mvp      [4];
#if 0
                        float current_light_position_w_dir[4] =
                        {
                            current_light_position[0] + current_light_direction[0],
                            current_light_position[1] + current_light_direction[1],
                            current_light_position[2] + current_light_direction[2],
                            1.0f
                        };
                        float current_light_position_w_dir_mvp[4];
#endif

                        system_matrix4x4_multiply_by_vector4(vp,
                                                             current_light_position_m,
                                                             current_light_position_mvp);
#if 0
                        system_matrix4x4_multiply_by_vector4(vp,
                                                             current_light_position_w_dir,
                                                             current_light_position_w_dir_mvp);
#endif

                        ogl_scene_renderer_lights_preview_render(renderer_ptr->lights_preview,
                                                                 current_light_position_mvp,
                                                                 current_light_color,
#if 0
                                                                 current_light_position_w_dir_mvp);
#else
                                                                 NULL);
#endif
                    }
                }
            }
            ogl_scene_renderer_lights_preview_stop(renderer_ptr->lights_preview);
        }
    } /* if (render_mode != RENDER_MODE_SHADOW_MAP) */

    /* 3. Clean up in anticipation for the next call. We specifically do not cache any of the
     *    data because of the frustum culling which needs to be performed, every time model matrix
     *    or VP changes.
     **/
    if (helper_visualization != 0)
    {
        system_resource_pool_return_all_allocations(renderer_ptr->mesh_id_data_pool);
        system_hash64map_clear                     (renderer_ptr->mesh_id_map);
    }

    if (apply_shadow_mapping)
    {
        /* Release any shadow maps we might've allocated back to the texture pool */
        _ogl_scene_renderer_return_shadow_maps_to_pool(renderer);
    }

    /* All done! Good to shut down the show */
    if (render_mode == RENDER_MODE_FORWARD)
    {
        entry_points->pGLDisable(GL_FRAMEBUFFER_SRGB);
        entry_points->pGLDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    if (vp != NULL)
    {
        system_matrix4x4_release(vp);

        vp = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_scene_renderer_release(__in __notnull ogl_scene_renderer scene_renderer)
{
    if (scene_renderer != NULL)
    {
        delete (_ogl_scene_renderer*) scene_renderer;

        scene_renderer = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_scene_renderer_set_property(__in __notnull ogl_scene_renderer          renderer,
                                                        __in           ogl_scene_renderer_property property,
                                                        __in __notnull const void*                 data)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    switch (property)
    {
        case OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX:
        {
            memcpy(renderer_ptr->current_camera_visible_world_aabb_max,
                   data,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_max) );

            break;
        }

        case OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN:
        {
            memcpy(renderer_ptr->current_camera_visible_world_aabb_min,
                   data,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_min) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_scene_renderer_property value");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC void ogl_scene_renderer_update_current_model_matrix(__in __notnull system_matrix4x4 transformation_matrix,
                                                           __in __notnull void*            renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    system_matrix4x4_set_from_matrix4x4(renderer_ptr->current_model_matrix,
                                        transformation_matrix);
}

/** Please see header for specification */
PUBLIC  void ogl_scene_renderer_update_light_properties(__in __notnull scene_light light,
                                                        __in           void*       renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    /* Directional vector needs to be only updated for directional lights */
    scene_light_type light_type = SCENE_LIGHT_TYPE_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    /* Update light position for point & spot light */
    if (light_type == SCENE_LIGHT_TYPE_POINT    ||
        light_type == SCENE_LIGHT_TYPE_SPOT)
    {
        const float default_light_position[4] = {0, 0, 0, 1};
              float final_light_position  [4];

        system_matrix4x4_multiply_by_vector4(renderer_ptr->current_model_matrix,
                                             default_light_position,
                                             final_light_position);

        scene_light_set_property(light,
                                 SCENE_LIGHT_PROPERTY_POSITION,
                                 final_light_position);
    }

    /* Update directional vector for directional & spot lights */
    if (light_type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
        light_type == SCENE_LIGHT_TYPE_SPOT)
    {
        /* The default light direction vector is <0, 0, -1>. Transform it against the current
         * model matrix to obtain final direction vector for the light
         */
        const float default_direction_vector[3] = {0, 0, -1};
              float final_direction_vector  [3];

        system_matrix4x4_multiply_by_vector3(renderer_ptr->current_model_matrix,
                                             default_direction_vector,
                                             final_direction_vector);

        system_math_vector_normalize3(final_direction_vector,
                                      final_direction_vector);

        /* Update the light */
        scene_light_set_property(light,
                                 SCENE_LIGHT_PROPERTY_DIRECTION,
                                 final_direction_vector);
    }
}