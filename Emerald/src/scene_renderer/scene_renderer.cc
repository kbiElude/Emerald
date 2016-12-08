/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "demo/demo_app.h"
#include "demo/demo_materials.h"
#include "mesh/mesh_material.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_bbox_preview.h"
#include "scene_renderer/scene_renderer_frustum_preview.h"
#include "scene_renderer/scene_renderer_lights_preview.h"
#include "scene_renderer/scene_renderer_normals_preview.h"
#include "scene_renderer/scene_renderer_sm.h"
#include "scene_renderer/scene_renderer_uber.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_hash64map.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_variant.h"
#include <float.h>


/* Private type definitions */

typedef struct _scene_renderer_mesh
{
    /** Model matrices should only be updated if necessary (eg. when the
     *  new model matrix is different at the beginning of scene_renderer_render_scene_graph().
     *
     *  In future, for GPU code-paths, this should be replaced with a persistent buffer storage.
     */
    uint32_t         mesh_id;
    mesh             mesh_instance;
    mesh_type        mesh_type;
    system_matrix4x4 model_matrix;
    system_matrix4x4 normal_matrix;

    /* Custom mesh fields only: */
    void*                              get_present_task_for_custom_mesh_user_arg;
    PFNGETPRESENTTASKFORCUSTOMMESHPROC pfn_get_present_task_for_custom_mesh_proc;

} _scene_renderer_mesh;

/** TODO: Do we really need the distinction between _scene_renderer_mesh and _scene_renderer_mesh_uber_item ???
 *        The pools would need to be separated, but it seems like we could re-use the same structure type.
 */
typedef struct _scene_renderer_mesh_uber_item
{
    mesh_material    material;
    uint32_t         mesh_id;
    mesh             mesh_instance;
    system_matrix4x4 model_matrix;
    system_matrix4x4 normal_matrix;
    mesh_type        type;

    _scene_renderer_mesh_uber_item()
    {
        material      = nullptr;
        mesh_id       = -1;
        mesh_instance = nullptr;
        model_matrix  = nullptr;
        normal_matrix = nullptr;
        type          = MESH_TYPE_UNKNOWN;
    }

    ~_scene_renderer_mesh_uber_item()
    {
        if (model_matrix != nullptr)
        {
            /* This is not expensive, since matrices are stored in a global
             * resource pool.
             */
            system_matrix4x4_release(model_matrix);

            model_matrix = nullptr;
        }

        if (normal_matrix != nullptr)
        {
            system_matrix4x4_release(normal_matrix);

            normal_matrix = nullptr;
        }
    }

} _scene_renderer_mesh_uber_item;

/* A single ogl_uber instance can be used to render multiple mesh instances. This structure holds all
 * information required to render these meshes.
 */
typedef struct _scene_renderer_uber
{
    /* holds _scene_renderer_mesh_uber_item* items.
     *
     * Note that the described meshes may be of either GPU stream or regular type.
     */
    system_resizable_vector regular_mesh_items;

    _scene_renderer_uber()
    {
        regular_mesh_items = nullptr;
    }

    ~_scene_renderer_uber()
    {
        if (regular_mesh_items != nullptr)
        {
            system_resizable_vector_release(regular_mesh_items);

            regular_mesh_items = nullptr;
        }
    }
} _scene_renderer_uber;


typedef struct _scene_renderer
{
    scene_renderer_bbox_preview    bbox_preview;
    scene_renderer_frustum_preview frustum_preview;
    scene_renderer_lights_preview  lights_preview;
    scene_renderer_normals_preview normals_preview;
    scene_renderer_sm              shadow_mapping;

    ral_context    context;
    demo_materials material_manager;
    scene          owned_scene;
    system_variant temp_variant_float;

    float                               current_camera_visible_world_aabb_max[3];
    float                               current_camera_visible_world_aabb_min[3];
    scene_camera                        current_camera;
    ral_texture_view                    current_color_rt;
    ral_texture_view                    current_depth_rt;
    scene_renderer_helper_visualization current_helper_visualization;
    bool                                current_is_shadow_mapping_enabled;
    system_matrix4x4                    current_model_matrix;
    system_matrix4x4                    current_projection;
    system_matrix4x4                    current_view;
    system_matrix4x4                    current_vp;

    system_resource_pool mesh_pool;           /* holds _scene_renderer_mesh instances */
    system_resource_pool mesh_uber_items_pool;
    system_resource_pool vector_pool;

    /** Maps mesh IDs to _scene_renderer_mesh instances.
     *
     *  This map provides quick access to useful properties of custom & regular meshes.
     *  The data is determined at pre-processing state for every render call made.
     *
     *  As of 06.08.2015, the hash-map is only used to feed the data to helper mesh visualizers.
     *  (eg. bounding box renderers).
     */
    system_hash64map current_mesh_id_to_mesh_map;

    /* This vector holds _scene_renderer_mesh instances for custom meshes which have passed
     * the frustum culling test and need to be rendered. The instances are taken from mesh_pool
     * so at no time should they be released manually.
     *
     * The vector is cleared at the rendering pre-processing stage. */
    system_resizable_vector current_custom_meshes_to_render;

    /* This hash-map is filled as more and more meshes in the scene get through the culling test
     * and ultimately get rendered. It maps ogl_uber instances to a structure, which describes
     * all mesh instances that can be drawn using the uber.
     *
     * The hash-map is cleared if scene configuration changes significantly. An example situation
     * is removal or addition of new light sources. The map must be purged at such time, since
     * the action is very likely to affect the shaders used by the uber instance.
     **/
    system_hash64map regular_mesh_ubers_map; /* key: ogl_uber; value: _scene_renderer_uber */

     _scene_renderer(ral_context in_context,
                     scene       in_scene);
    ~_scene_renderer();
} _scene_renderer;

/* Forward declarations */
PRIVATE void _scene_renderer_create_model_normal_matrices             (_scene_renderer*           renderer_ptr,
                                                                       system_matrix4x4*          out_model_matrix_ptr,
                                                                       system_matrix4x4*          out_normal_matrix_ptr);
PRIVATE void _scene_renderer_deinit_cached_ubers_map_contents         (system_hash64map           cached_materials_map);
PRIVATE void _scene_renderer_deinit_resizable_vector_for_resource_pool(system_resource_pool_block block);
PRIVATE void _scene_renderer_get_light_color                          (scene_light                light,
                                                                       system_time                time,
                                                                       system_variant             temp_float_variant,
                                                                       float*                     out_color);
PRIVATE void _scene_renderer_get_uber_for_render_mode                 (_scene_renderer*           renderer_ptr,
                                                                       scene_renderer_render_mode render_mode,
                                                                       demo_materials             context_materials,
                                                                       scene                      scene,
                                                                       scene_renderer_uber*       result_uber_ptr);
PRIVATE void _scene_renderer_init_resizable_vector_for_resource_pool  (system_resource_pool_block block);
PRIVATE void _scene_renderer_on_camera_show_frustum_setting_changed   (const void*                unused,
                                                                             void*                scene_renderer);
PRIVATE void _scene_renderer_on_ubers_map_invalidated                 (const void*                unused,
                                                                             void*                scene_renderer);
PRIVATE void _scene_renderer_process_mesh_for_forward_rendering       (scene_mesh                 scene_mesh_instance,
                                                                       void*                      renderer);
PRIVATE void _scene_renderer_release_mesh_matrices                    (void*                      mesh_entry);
PRIVATE void _scene_renderer_return_shadow_maps_to_pool               (scene_renderer             renderer);
PRIVATE void _scene_renderer_subscribe_for_general_notifications      (_scene_renderer*           scene_renderer_ptr,
                                                                       bool                       should_subscribe);
PRIVATE void _scene_renderer_subscribe_for_mesh_material_notifications(_scene_renderer*           scene_renderer_ptr,
                                                                       mesh_material              material,
                                                                       bool                       should_subscribe);
PRIVATE void _scene_renderer_update_frustum_preview_assigned_cameras  (_scene_renderer*           renderer_ptr);
PRIVATE void _scene_renderer_update_uber_light_properties             (scene_renderer_uber        material_uber,
                                                                       scene                      scene,
                                                                       system_matrix4x4           current_camera_view_matrix,
                                                                       system_time                frame_time,
                                                                       system_variant             temp_variant_float);


/** TODO */
_scene_renderer::_scene_renderer(ral_context in_context,
                                 scene       in_scene)
{
    bbox_preview                    = nullptr;
    context                         = in_context;
    current_camera                  = nullptr;
    current_color_rt                = nullptr;
    current_custom_meshes_to_render = system_resizable_vector_create(sizeof(_scene_renderer_mesh*) );
    current_depth_rt                = nullptr;
    current_mesh_id_to_mesh_map     = system_hash64map_create       (sizeof(_scene_renderer_mesh*) );
    current_model_matrix            = system_matrix4x4_create       ();
    frustum_preview                 = nullptr;    /* can be instantiated at draw time */
    lights_preview                  = nullptr;    /* can be instantiated at draw time */
    material_manager                = nullptr;
    mesh_pool                       = system_resource_pool_create(sizeof(_scene_renderer_mesh),
                                                                  4,     /* n_elements_to_preallocate */
                                                                  nullptr,  /* init_fn */
                                                                  nullptr); /* deinit_fn */
    mesh_uber_items_pool            = system_resource_pool_create(sizeof(_scene_renderer_mesh_uber_item),
                                                                  4,     /* n_elements_to_preallocate */
                                                                  nullptr,  /* init_fn */
                                                                  nullptr); /* deinit_fn */
    normals_preview                 = nullptr;
    owned_scene                     = in_scene;
    regular_mesh_ubers_map          = system_hash64map_create    (sizeof(_scene_renderer_uber*) );
    shadow_mapping                  = scene_renderer_sm_create   (in_context);
    temp_variant_float              = system_variant_create      (SYSTEM_VARIANT_FLOAT);
    vector_pool                     = system_resource_pool_create(sizeof(system_resizable_vector),
                                                                  64, /* capacity */
                                                                  _scene_renderer_init_resizable_vector_for_resource_pool,
                                                                  _scene_renderer_deinit_resizable_vector_for_resource_pool);

    scene_retain(owned_scene);
}

/** TODO */
_scene_renderer::~_scene_renderer()
{
    uint32_t n_scene_unique_meshes = 0;

    LOG_INFO("Scene renderer deallocating..");

    _scene_renderer_subscribe_for_general_notifications(this,
                                                        false /* should_subscribe */);

    scene_get_property(owned_scene,
                       SCENE_PROPERTY_N_UNIQUE_MESHES,
                      &n_scene_unique_meshes);

    for (uint32_t n_scene_unique_mesh = 0;
                  n_scene_unique_mesh < n_scene_unique_meshes;
                ++n_scene_unique_mesh)
    {
        mesh                    current_mesh             = scene_get_unique_mesh_by_index(owned_scene,
                                                                                          n_scene_unique_mesh);
        system_resizable_vector current_mesh_materials   = nullptr;
        uint32_t                n_current_mesh_materials = 0;

        ASSERT_DEBUG_SYNC(current_mesh != nullptr,
                          "Could not retrieve unique mesh at index [%d]",
                          n_scene_unique_mesh);

        mesh_get_property(current_mesh,
                          MESH_PROPERTY_MATERIALS,
                         &current_mesh_materials);

        ASSERT_DEBUG_SYNC(current_mesh_materials != nullptr,
                          "Could not retrieve unique mesh materials.");

        system_resizable_vector_get_property(current_mesh_materials,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_current_mesh_materials);

        for (uint32_t n_current_mesh_material = 0;
                      n_current_mesh_material < n_current_mesh_materials;
                    ++n_current_mesh_material)
        {
            mesh_material current_mesh_material = nullptr;

            system_resizable_vector_get_element_at(current_mesh_materials,
                                                   n_current_mesh_material,
                                                  &current_mesh_material);

            _scene_renderer_subscribe_for_mesh_material_notifications(this,
                                                                      current_mesh_material,
                                                                      false /* should_subscribe */);
        }
    }

    if (bbox_preview != nullptr)
    {
        scene_renderer_bbox_preview_release(bbox_preview);

        bbox_preview = nullptr;
    }

    if (current_model_matrix != nullptr)
    {
        system_matrix4x4_release(current_model_matrix);

        current_model_matrix = nullptr;
    }

    if (current_custom_meshes_to_render != nullptr)
    {
        system_resizable_vector_release(current_custom_meshes_to_render);

        current_custom_meshes_to_render = nullptr;
    }

    if (frustum_preview != nullptr)
    {
        scene_renderer_frustum_preview_release(frustum_preview);

        frustum_preview = nullptr;
    }

    if (lights_preview != nullptr)
    {
        scene_renderer_lights_preview_release(lights_preview);

        lights_preview = nullptr;
    }

    if (current_mesh_id_to_mesh_map != nullptr)
    {
        system_hash64map_release(current_mesh_id_to_mesh_map);

        current_mesh_id_to_mesh_map = nullptr;
    }

    if (mesh_pool != nullptr)
    {
        system_resource_pool_release(mesh_pool);

        mesh_pool = nullptr;
    }

    if (mesh_uber_items_pool != nullptr)
    {
        system_resource_pool_release(mesh_uber_items_pool);

        mesh_uber_items_pool = nullptr;
    }

    if (normals_preview != nullptr)
    {
        scene_renderer_normals_preview_release(normals_preview);

        normals_preview = nullptr;
    }

    if (regular_mesh_ubers_map != nullptr)
    {
        _scene_renderer_deinit_cached_ubers_map_contents(regular_mesh_ubers_map);

        system_hash64map_release(regular_mesh_ubers_map);
        regular_mesh_ubers_map = nullptr;
    }

    if (owned_scene != nullptr)
    {
        scene_release(owned_scene);

        owned_scene = nullptr;
    }

    if (shadow_mapping != nullptr)
    {
        scene_renderer_sm_release(shadow_mapping);

        shadow_mapping = nullptr;
    }

    if (temp_variant_float != nullptr)
    {
       system_variant_release(temp_variant_float);

       temp_variant_float = nullptr;
    }

    if (vector_pool != nullptr)
    {
        system_resource_pool_release(vector_pool);

        vector_pool = nullptr;
    }
}

/* TODO */
PRIVATE void _scene_renderer_create_model_normal_matrices(_scene_renderer*  renderer_ptr,
                                                          system_matrix4x4* out_model_matrix_ptr,
                                                          system_matrix4x4* out_normal_matrix_ptr)
{
    /* Form model & normal matrices. These will be cached for use later on, so do not
     * release them when leaving this function.
     *
     * TODO: These could be cached. Also, normal matrix is equal to model matrix,
     *       as long as no non-uniform scaling operator is applied. We could use
     *       this to avoid the calculations below.
     */
    system_matrix4x4 mesh_model_matrix  = system_matrix4x4_create();
    system_matrix4x4 mesh_normal_matrix = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4(mesh_model_matrix,
                                        renderer_ptr->current_model_matrix);
    system_matrix4x4_set_from_matrix4x4(mesh_normal_matrix,
                                        renderer_ptr->current_model_matrix);
    system_matrix4x4_invert            (mesh_normal_matrix);
    system_matrix4x4_transpose         (mesh_normal_matrix);

    *out_model_matrix_ptr  = mesh_model_matrix;
    *out_normal_matrix_ptr = mesh_normal_matrix;
}

/* TODO */
PRIVATE void _scene_renderer_deinit_cached_ubers_map_contents(system_hash64map ubers_map)
{
    system_hash64         uber_hash = 0;
    _scene_renderer_uber* uber_ptr  = nullptr;

    while (system_hash64map_get_element_at(ubers_map,
                                           0,
                                          &uber_ptr,
                                          &uber_hash) )
    {
        delete uber_ptr;
        uber_ptr = nullptr;

        if (!system_hash64map_remove(ubers_map,
                                     uber_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "system_hash64map_remove() call failed.");
        }
    } 
}

/* TODO */
PRIVATE void _scene_renderer_deinit_resizable_vector_for_resource_pool(system_resource_pool_block block)
{
    system_resizable_vector* vector_ptr = reinterpret_cast<system_resizable_vector*>(block);

    system_resizable_vector_release(*vector_ptr);
}

/** TODO */
PRIVATE void _scene_renderer_get_light_color(scene_light    light,
                                             system_time    time,
                                             system_variant temp_float_variant,
                                             float*         out_color)
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
PRIVATE void _scene_renderer_get_uber_for_render_mode(_scene_renderer*           renderer_ptr,
                                                      scene_renderer_render_mode render_mode,
                                                      demo_materials             context_materials,
                                                      scene                      scene,
                                                      scene_renderer_uber*       result_uber_ptr)
{
    switch (render_mode)
    {
        case RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS:
        case RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS:
        {
            /* Do not over-write the uber instance! */

            break;
        }

        case RENDER_MODE_NORMALS_ONLY:
        {
            *result_uber_ptr = mesh_material_get_uber(demo_materials_get_special_material(context_materials,
                                                                                          renderer_ptr->context,
                                                                                          SPECIAL_MATERIAL_NORMALS),
                                                      scene,
                                                      false); /* use_shadow_maps */

            break;
        }

        case RENDER_MODE_TEXCOORDS_ONLY:
        {
            *result_uber_ptr = mesh_material_get_uber(demo_materials_get_special_material(context_materials,
                                                                                          renderer_ptr->context,
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
    }
}

/** TODO */
PRIVATE void _scene_renderer_init_resizable_vector_for_resource_pool(system_resource_pool_block block)
{
    system_resizable_vector* vector_ptr = reinterpret_cast<system_resizable_vector*>(block);

    *vector_ptr = system_resizable_vector_create(64 /* capacity */);
}

/** TODO */
PRIVATE void _scene_renderer_on_camera_show_frustum_setting_changed(const void* unused,
                                                                          void* scene_renderer)
{
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(scene_renderer);

    /* Use a shared handler to re-create the frustum assignments for the frustum preview renderer */
    _scene_renderer_update_frustum_preview_assigned_cameras(renderer_ptr);
}

/** TODO */
PRIVATE void _scene_renderer_on_mesh_material_uber_invalidated(const void* callback_data,
                                                                     void* scene_renderer)
{
    const mesh_material source_material    = (mesh_material)                   (callback_data);
    _scene_renderer*    scene_renderer_ptr = reinterpret_cast<_scene_renderer*>(scene_renderer);

    _scene_renderer_deinit_cached_ubers_map_contents(scene_renderer_ptr->regular_mesh_ubers_map);
}

/** TODO */
PRIVATE void _scene_renderer_on_ubers_map_invalidated(const void* unused,
                                                            void* scene_renderer)
{
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(scene_renderer);

    /* TODO: Subscribe for "show frustum changed" setting! */
    ASSERT_DEBUG_SYNC(false,
                      "TODO: subscriptions!");

    /* Reset all cached ubers */
    system_hash64map_clear(renderer_ptr->regular_mesh_ubers_map);
}

/** TODO */
PRIVATE void _scene_renderer_process_mesh_for_forward_rendering(scene_mesh scene_mesh_instance,
                                                                void*      renderer)
{
    bool                    is_shadow_receiver            = false;
    mesh_material           material                      = nullptr;
    mesh                    mesh_gpu                      = nullptr;
    mesh_type               mesh_instance_type;
    mesh                    mesh_instantiation_parent_gpu = nullptr;
    uint32_t                mesh_id                       = -1;
    system_resizable_vector mesh_materials                = nullptr;
    scene_renderer_uber     mesh_uber                     = nullptr;
    unsigned int            n_mesh_materials              = 0;
    _scene_renderer*        renderer_ptr                  = reinterpret_cast<_scene_renderer*>(renderer);

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_MESH,
                           &mesh_gpu);

    mesh_get_property      (mesh_gpu,
                            MESH_PROPERTY_TYPE,
                           &mesh_instance_type);

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_ID,
                           &mesh_id);
    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER,
                           &is_shadow_receiver);

    if (mesh_instance_type == MESH_TYPE_REGULAR)
    {
        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_INSTANTIATION_PARENT,
                         &mesh_instantiation_parent_gpu);

        if (mesh_instantiation_parent_gpu == nullptr)
        {
            mesh_instantiation_parent_gpu = mesh_gpu;
        }
    }
    else
    {
        mesh_instantiation_parent_gpu = mesh_gpu;
    }

    if (mesh_instance_type == MESH_TYPE_GPU_STREAM ||
        mesh_instance_type == MESH_TYPE_REGULAR)
    {
        mesh_get_property(mesh_instantiation_parent_gpu,
                          MESH_PROPERTY_MATERIALS,
                         &mesh_materials);

        system_resizable_vector_get_property(mesh_materials,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_mesh_materials);
    }

    /* Perform frustum culling to make sure it actually makes sense to render
     * this mesh.
     *
     * NOTE: We use the actual mesh instance for the culling process, NOT the
     *       parent which provides the mesh data. This is to ensure that the
     *       model matrix we use for the calculations refers to the actual object
     *       of our interest.
     *
     */
    if (!scene_renderer_cull_against_frustum( (scene_renderer) renderer,
                                              mesh_instantiation_parent_gpu,
                                              SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES,
                                              nullptr) ) /* behavior_data */
    {
        goto end;
    }

    /* If any helper visualization is needed, we need to store a _mesh instance */
    if (renderer_ptr->current_helper_visualization != 0)
    {
        _scene_renderer_mesh* new_entry_ptr = reinterpret_cast<_scene_renderer_mesh*>(system_resource_pool_get_from_pool(renderer_ptr->mesh_pool));

        new_entry_ptr->mesh_id       = mesh_id;
        new_entry_ptr->mesh_instance = mesh_gpu;
        new_entry_ptr->mesh_type     = mesh_instance_type;

        _scene_renderer_create_model_normal_matrices(renderer_ptr,
                                                    &new_entry_ptr->model_matrix,
                                                    &new_entry_ptr->normal_matrix);

        system_hash64map_insert(renderer_ptr->current_mesh_id_to_mesh_map,
                                mesh_id,
                                new_entry_ptr,
                                _scene_renderer_release_mesh_matrices, /* on_remove_callback */
                                new_entry_ptr);                        /* on_remove_callback_user_arg */
    }

    /* Cache the mesh for rendering */
    if (mesh_instance_type == MESH_TYPE_GPU_STREAM ||
        mesh_instance_type == MESH_TYPE_REGULAR)
    {
        for (unsigned int n_mesh_material = 0;
                          n_mesh_material < n_mesh_materials;
                        ++n_mesh_material)
        {
            _scene_renderer_uber* uber_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(mesh_materials,
                                                        n_mesh_material,
                                                       &material) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh material at index [%d]",
                                  n_mesh_material);
            }

            /*
            /* Retrieve ogl_uber that can render the material for the currently processed
             * scene configuration.
             */
            mesh_uber = mesh_material_get_uber(material,
                                               renderer_ptr->owned_scene,
                                               is_shadow_receiver & renderer_ptr->current_is_shadow_mapping_enabled);

            if (!system_hash64map_get(renderer_ptr->regular_mesh_ubers_map,
                                      (system_hash64) mesh_uber,
                                     &uber_ptr) )
            {
                /* NOTE: This will only be called once, every time uber is used for the
                 *       first time for current scene renderer instance. This should be a
                 *       negligible cost, so let's leave this memory allocation here, in order
                 *       to avoid making code more complex.
                 *       We cannot resource pool this allocation, since the constructor instantiates
                 *       a resizable vector.
                 */
                uber_ptr = new (std::nothrow) _scene_renderer_uber;

                ASSERT_ALWAYS_SYNC(uber_ptr != nullptr,
                                   "Out of memory");

                uber_ptr->regular_mesh_items = system_resizable_vector_create(64 /* capacity */);

                system_hash64map_insert(renderer_ptr->regular_mesh_ubers_map,
                                        (system_hash64) mesh_uber,
                                        uber_ptr,
                                        nullptr,  /* on_remove_callback */
                                        nullptr); /* on_remove_callback_argument */
            }

            /* This is a new user of the material. Store it in the vector */
            _scene_renderer_mesh_uber_item* new_mesh_item_ptr = reinterpret_cast<_scene_renderer_mesh_uber_item*>(system_resource_pool_get_from_pool(renderer_ptr->mesh_uber_items_pool) );

            ASSERT_ALWAYS_SYNC(new_mesh_item_ptr != nullptr,
                               "Out of memory");

            new_mesh_item_ptr->material      = material;
            new_mesh_item_ptr->mesh_id       = mesh_id;
            new_mesh_item_ptr->mesh_instance = mesh_gpu;
            new_mesh_item_ptr->type          = mesh_instance_type;

            _scene_renderer_create_model_normal_matrices(renderer_ptr,
                                                        &new_mesh_item_ptr->model_matrix,
                                                        &new_mesh_item_ptr->normal_matrix);

            /* Store the data */
            system_resizable_vector_push(uber_ptr->regular_mesh_items,
                                         new_mesh_item_ptr);
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(mesh_instance_type == MESH_TYPE_CUSTOM,
                          "Unrecognized mesh type.");

        /* This mesh is rendered by the user. */
        void*                              custom_mesh_render_proc_user_arg = nullptr;
        _scene_renderer_mesh*              new_entry_ptr                    = reinterpret_cast<_scene_renderer_mesh*>(system_resource_pool_get_from_pool(renderer_ptr->mesh_pool) );
        PFNGETPRESENTTASKFORCUSTOMMESHPROC pfn_custom_mesh_render_proc      = nullptr;

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_GET_PRESENT_TASK_FOR_CUSTOM_MESH_FUNC_PTR,
                         &new_entry_ptr->pfn_get_present_task_for_custom_mesh_proc);
        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_GET_PRESENT_TASK_FOR_CUSTOM_MESH_FUNC_USER_ARG,
                         &new_entry_ptr->get_present_task_for_custom_mesh_user_arg);

        new_entry_ptr->mesh_id       = mesh_id;
        new_entry_ptr->mesh_instance = mesh_gpu;
        new_entry_ptr->mesh_type     = MESH_TYPE_CUSTOM;

        _scene_renderer_create_model_normal_matrices(renderer_ptr,
                                                    &new_entry_ptr->model_matrix,
                                                    &new_entry_ptr->normal_matrix);

        system_resizable_vector_push(renderer_ptr->current_custom_meshes_to_render,
                                     new_entry_ptr);
    }

end:
    ;
}

/** TODO */
PRIVATE void _scene_renderer_release_mesh_matrices(void* mesh_entry)
{
    _scene_renderer_mesh* mesh_entry_ptr = reinterpret_cast<_scene_renderer_mesh*>(mesh_entry);

    if (mesh_entry_ptr->model_matrix != nullptr)
    {
        system_matrix4x4_release(mesh_entry_ptr->model_matrix);

        mesh_entry_ptr->model_matrix = nullptr;
    }

    if (mesh_entry_ptr->normal_matrix != nullptr)
    {
        system_matrix4x4_release(mesh_entry_ptr->normal_matrix);

        mesh_entry_ptr->normal_matrix = nullptr;
    }
}

/** TODO
 *
 *  @return Result present task. May be NULL.
 **/
PRIVATE ral_present_task _scene_renderer_render_helper_visualizations(_scene_renderer* renderer_ptr,
                                                                      system_time      frame_time)
{
    ral_present_task frustum_preview_render_present_task = nullptr;
    ral_present_task light_preview_render_present_task   = nullptr;
    ral_present_task result_present_task                 = nullptr;

    ASSERT_DEBUG_SYNC(renderer_ptr->current_color_rt != nullptr,
                      "No color rendertarget assigned.");

    if (renderer_ptr->current_helper_visualization & HELPER_VISUALIZATION_FRUSTUMS)
    {
        if (renderer_ptr->frustum_preview == nullptr)
        {
            /* TODO: This is wrong - we shouldn't need to pass viewport size at creation time. */
            uint32_t rt_size[2];

            ral_texture_view_get_mipmap_property(renderer_ptr->current_color_rt,
                                                 0, /* n_layer  */
                                                 0, /* n_mipmap */
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                                 rt_size + 0);
            ral_texture_view_get_mipmap_property(renderer_ptr->current_color_rt,
                                                 0, /* n_layer  */
                                                 0, /* n_mipmap */
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                                 rt_size + 1);

            renderer_ptr->frustum_preview = scene_renderer_frustum_preview_create(renderer_ptr->context,
                                                                                  renderer_ptr->owned_scene,
                                                                                  rt_size);

            /* Assign existing cameras to the call-back */
            _scene_renderer_update_frustum_preview_assigned_cameras(renderer_ptr);
        }

        frustum_preview_render_present_task = scene_renderer_frustum_preview_get_present_task(renderer_ptr->frustum_preview,
                                                                                              frame_time,
                                                                                              renderer_ptr->current_vp,
                                                                                              renderer_ptr->current_color_rt,
                                                                                              renderer_ptr->current_depth_rt);
    }

    if (renderer_ptr->current_helper_visualization & HELPER_VISUALIZATION_LIGHTS)
    {
        unsigned int n_scene_lights = 0;

        scene_get_property(renderer_ptr->owned_scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_scene_lights);

        if (renderer_ptr->lights_preview == nullptr)
        {
            renderer_ptr->lights_preview = scene_renderer_lights_preview_create(renderer_ptr->context,
                                                                                renderer_ptr->owned_scene);
        }

        scene_renderer_lights_preview_start(renderer_ptr->lights_preview,
                                            renderer_ptr->current_color_rt,
                                            renderer_ptr->current_depth_rt);
        {
            for (unsigned int n_light = 0;
                              n_light < n_scene_lights;
                            ++n_light)
            {
                scene_light      current_light      = scene_get_light_by_index(renderer_ptr->owned_scene,
                                                                              n_light);
                scene_light_type current_light_type = SCENE_LIGHT_TYPE_UNKNOWN;

                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_TYPE,
                                        &current_light_type);

                /* NOTE: We can only show point (& spot) lights.
                 *
                 * TODO: Include directional light support. These have no position
                 *       so we may need to come up with some kind of a *cough* design
                 *       for this.
                 */
                if (current_light_type == SCENE_LIGHT_TYPE_POINT ||
                    current_light_type == SCENE_LIGHT_TYPE_SPOT)
                {
                    float current_light_position[3];
                    float current_light_color   [4] =
                    {
                        0.0f,
                        0.0f,
                        0.0f,
                        1.0f
                    };

                    scene_light_get_property       (current_light,
                                                    SCENE_LIGHT_PROPERTY_POSITION,
                                                   &current_light_position);
                    _scene_renderer_get_light_color(current_light,
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
                    float current_light_position_mvp[4];

                    system_matrix4x4_multiply_by_vector4(renderer_ptr->current_vp,
                                                         current_light_position_m,
                                                         current_light_position_mvp);

                    scene_renderer_lights_preview_append_light(renderer_ptr->lights_preview,
                                                               current_light_position_mvp,
                                                               current_light_color,
                                                               nullptr); /* light_pos_plus_direction */
                }
            }
        }
        light_preview_render_present_task = scene_renderer_lights_preview_stop(renderer_ptr->lights_preview);
    }

    if (frustum_preview_render_present_task != nullptr ||
        light_preview_render_present_task   != nullptr)
    {
        uint32_t                            color_consumer_task_indices[2];
        uint32_t                            depth_consumer_task_indices[1];
        uint32_t                            n_color_consumer_task_indices = 0;
        uint32_t                            n_depth_consumer_task_indices = 0;
        uint32_t                            n_result_task_subtasks        = 0;
        uint32_t                            n_input_mappings              = 0;
        uint32_t                            n_output_mappings             = 0;
        uint32_t                            n_unique_ios                  = (renderer_ptr->current_depth_rt != nullptr) ? 2 : 1;
        ral_present_task                    result_task_subtasks[2];
        ral_present_task_group_create_info  result_task_create_info;
        ral_present_task_group_mapping      result_task_input_mappings[3];
        ral_present_task_group_mapping      result_task_output_mappings[3];

        /* Fill consumer index arrays */
        if (frustum_preview_render_present_task != nullptr)
        {
            color_consumer_task_indices[n_color_consumer_task_indices++] = n_result_task_subtasks;

            if (renderer_ptr->current_depth_rt != nullptr)
            {
                depth_consumer_task_indices[n_depth_consumer_task_indices++] = n_result_task_subtasks;
            }

            result_task_subtasks[n_result_task_subtasks++] = frustum_preview_render_present_task;
        }

        if (light_preview_render_present_task != nullptr)
        {
            color_consumer_task_indices[n_color_consumer_task_indices++] = n_result_task_subtasks;

            result_task_subtasks[n_result_task_subtasks++] = light_preview_render_present_task;
        }

        /* Configure IOs. Given rendering order is irrelevant, we don't need any ingroup connections */
        for (uint32_t n_color_consumer_task = 0;
                      n_color_consumer_task < n_color_consumer_task_indices;
                    ++n_color_consumer_task)
        {
            result_task_input_mappings[n_input_mappings].group_task_io_index   = 0;
            result_task_input_mappings[n_input_mappings].n_present_task        = color_consumer_task_indices[n_color_consumer_task];
            result_task_input_mappings[n_input_mappings].present_task_io_index = 0; /* color_rt */

            result_task_output_mappings[n_output_mappings] = result_task_input_mappings[n_input_mappings];

            ++n_input_mappings;
            ++n_output_mappings;
        }

        for (uint32_t n_depth_consumer_task = 0;
                      n_depth_consumer_task < n_depth_consumer_task_indices;
                    ++n_depth_consumer_task)
        {
            result_task_input_mappings[n_input_mappings].group_task_io_index   = 1;
            result_task_input_mappings[n_input_mappings].n_present_task        = depth_consumer_task_indices[n_depth_consumer_task];
            result_task_input_mappings[n_input_mappings].present_task_io_index = 1;

            result_task_output_mappings[n_output_mappings] = result_task_input_mappings[n_input_mappings];

            ++n_input_mappings;
            ++n_output_mappings;
        }

        result_task_create_info.ingroup_connections                      = nullptr;
        result_task_create_info.n_ingroup_connections                    = 0;
        result_task_create_info.n_present_tasks                          = n_result_task_subtasks;
        result_task_create_info.n_total_unique_inputs                    = n_input_mappings;
        result_task_create_info.n_total_unique_outputs                   = n_output_mappings;
        result_task_create_info.n_unique_input_to_ingroup_task_mappings  = n_input_mappings;
        result_task_create_info.n_unique_output_to_ingroup_task_mappings = n_output_mappings;
        result_task_create_info.present_tasks                            = result_task_subtasks;
        result_task_create_info.unique_input_to_ingroup_task_mapping     = result_task_input_mappings;
        result_task_create_info.unique_output_to_ingroup_task_mapping    = result_task_output_mappings;

        result_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("Helper visualization: rasterization"),
                                                            &result_task_create_info);
    }

    if (frustum_preview_render_present_task != nullptr)
    {
        ral_present_task_release(frustum_preview_render_present_task);
    }

    if (light_preview_render_present_task != nullptr)
    {
        ral_present_task_release(light_preview_render_present_task);
    }

    return result_present_task;
}

/** TODO.
 *
 *  @param renderer_ptr     TODO
 *  @param uber_details_ptr TODO. May be nullptr, in which case the helper visualizations will be rendered
 *                          for custom meshes.
 *
 *  @return TODO. May be NULL.
 **/
PRIVATE ral_present_task _scene_renderer_render_mesh_helper_visualizations(_scene_renderer*      renderer_ptr,
                                                                           _scene_renderer_uber* uber_details_ptr)
{
    ral_present_task bbox_preview_present_task    = nullptr;
    unsigned int     n_custom_meshes              = 0;
    unsigned int     n_uber_items                 = 0;
    ral_present_task normals_preview_present_task = nullptr;
    ral_present_task result_present_task          = nullptr;

    if (uber_details_ptr != nullptr)
    {
        system_resizable_vector_get_property(uber_details_ptr->regular_mesh_items,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_uber_items);
    }
    else
    {
        system_resizable_vector_get_property(renderer_ptr->current_custom_meshes_to_render,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_custom_meshes);
    }

    if (renderer_ptr->current_helper_visualization & HELPER_VISUALIZATION_BOUNDING_BOXES)
    {
        if (renderer_ptr->bbox_preview == nullptr)
        {
            renderer_ptr->bbox_preview = scene_renderer_bbox_preview_create(renderer_ptr->context,
                                                                            renderer_ptr->owned_scene,
                                                                            (scene_renderer) renderer_ptr);
        }

        scene_renderer_bbox_preview_start(renderer_ptr->bbox_preview,
                                          renderer_ptr->current_color_rt,
                                          renderer_ptr->current_depth_rt,
                                          renderer_ptr->current_vp);
        {
            if (uber_details_ptr != nullptr)
            {
                _scene_renderer_mesh_uber_item* mesh_uber_item_ptr = nullptr;

                for (uint32_t n_vector_item = 0;
                              n_vector_item < n_uber_items;
                            ++n_vector_item)
                {
                    system_resizable_vector_get_element_at(uber_details_ptr->regular_mesh_items,
                                                           n_vector_item,
                                                          &mesh_uber_item_ptr);

                    scene_renderer_bbox_preview_append_mesh(renderer_ptr->bbox_preview,
                                                            mesh_uber_item_ptr->mesh_id);
                }
            }
            else
            {
                _scene_renderer_mesh* mesh_ptr = nullptr;

                for (unsigned int n_custom_mesh = 0;
                                  n_custom_mesh < n_custom_meshes;
                                ++n_custom_mesh)
                {
                    system_resizable_vector_get_element_at(renderer_ptr->current_custom_meshes_to_render,
                                                           n_custom_mesh,
                                                          &mesh_ptr);

                    scene_renderer_bbox_preview_append_mesh(renderer_ptr->bbox_preview,
                                                            mesh_ptr->mesh_id);
                }
            }
        }
        bbox_preview_present_task = scene_renderer_bbox_preview_stop(renderer_ptr->bbox_preview);
    }

    /* Normals visualization is only supported for regular meshes */
    if (renderer_ptr->current_helper_visualization & HELPER_VISUALIZATION_NORMALS &&
        uber_details_ptr != nullptr)
    {
        if (renderer_ptr->normals_preview == nullptr)
        {
            renderer_ptr->normals_preview = scene_renderer_normals_preview_create(renderer_ptr->context,
                                                                                  renderer_ptr->owned_scene,
                                                                                  (scene_renderer) renderer_ptr);
        }

        scene_renderer_normals_preview_start(renderer_ptr->normals_preview,
                                             renderer_ptr->current_vp,
                                             renderer_ptr->current_color_rt,
                                             renderer_ptr->current_depth_rt);
        {
            _scene_renderer_mesh_uber_item* mesh_uber_item_ptr = nullptr;

            for (uint32_t n_vector_item = 0;
                          n_vector_item < n_uber_items;
                        ++n_vector_item)
            {
                system_resizable_vector_get_element_at(uber_details_ptr->regular_mesh_items,
                                                       n_vector_item,
                                                      &mesh_uber_item_ptr);

                scene_renderer_normals_preview_append_mesh(renderer_ptr->normals_preview,
                                                           mesh_uber_item_ptr->mesh_id);
            }
        }
        normals_preview_present_task = scene_renderer_normals_preview_stop(renderer_ptr->normals_preview);
    }

    if (bbox_preview_present_task    != nullptr ||
        normals_preview_present_task != nullptr)
    {
        /* TODO TODO TODO: This is a modified copy-paste version of the code doing a similar thing in
         *                 _scene_renderer_render_helper_visualizations. Consider implementing a generic
         *                 utility which parallelizes subtask execution for a predefined set of inputs.
         */
        uint32_t                            color_consumer_task_indices[2];
        uint32_t                            depth_consumer_task_indices[1];
        uint32_t                            n_color_consumer_task_indices = 0;
        uint32_t                            n_depth_consumer_task_indices = 0;
        uint32_t                            n_result_task_subtasks        = 0;
        uint32_t                            n_input_mappings              = 0;
        uint32_t                            n_output_mappings             = 0;
        ral_present_task                    result_task_subtasks[2];
        ral_present_task_group_create_info  result_task_create_info;
        ral_present_task_group_mapping      result_task_input_mappings[3];
        ral_present_task_group_mapping      result_task_output_mappings[3];

        /* Fill consumer index arrays */
        if (normals_preview_present_task != nullptr)
        {
            color_consumer_task_indices[n_color_consumer_task_indices++] = n_result_task_subtasks;

            if (renderer_ptr->current_depth_rt != nullptr)
            {
                depth_consumer_task_indices[n_depth_consumer_task_indices++] = n_result_task_subtasks;
            }

            result_task_subtasks[n_result_task_subtasks++] = normals_preview_present_task;
        }

        if (bbox_preview_present_task != nullptr)
        {
            color_consumer_task_indices[n_color_consumer_task_indices++] = n_result_task_subtasks;

            result_task_subtasks[n_result_task_subtasks++] = bbox_preview_present_task;
        }

        /* Configure IOs. Given rendering order is irrelevant, we don't need any ingroup connections */
        for (uint32_t n_color_consumer_task = 0;
                      n_color_consumer_task < n_color_consumer_task_indices;
                    ++n_color_consumer_task)
        {
            result_task_input_mappings[n_input_mappings].group_task_io_index   = 0;
            result_task_input_mappings[n_input_mappings].n_present_task        = color_consumer_task_indices[n_color_consumer_task];
            result_task_input_mappings[n_input_mappings].present_task_io_index = 0; /* color_rt */

            result_task_output_mappings[n_output_mappings] = result_task_input_mappings[n_input_mappings];

            ++n_input_mappings;
            ++n_output_mappings;
        }

        for (uint32_t n_depth_consumer_task = 0;
                      n_depth_consumer_task < n_depth_consumer_task_indices;
                    ++n_depth_consumer_task)
        {
            result_task_input_mappings[n_input_mappings].group_task_io_index   = 1;
            result_task_input_mappings[n_input_mappings].n_present_task        = depth_consumer_task_indices[n_depth_consumer_task];
            result_task_input_mappings[n_input_mappings].present_task_io_index = 1;

            result_task_output_mappings[n_output_mappings] = result_task_output_mappings[n_output_mappings];

            ++n_input_mappings;
            ++n_output_mappings;
        }

        result_task_create_info.ingroup_connections                      = nullptr;
        result_task_create_info.n_ingroup_connections                    = 0;
        result_task_create_info.n_present_tasks                          = n_result_task_subtasks;
        result_task_create_info.n_total_unique_inputs                    = n_input_mappings;
        result_task_create_info.n_total_unique_outputs                   = n_output_mappings;
        result_task_create_info.n_unique_input_to_ingroup_task_mappings  = n_input_mappings;
        result_task_create_info.n_unique_output_to_ingroup_task_mappings = n_output_mappings;
        result_task_create_info.present_tasks                            = result_task_subtasks;
        result_task_create_info.unique_input_to_ingroup_task_mapping     = result_task_input_mappings;
        result_task_create_info.unique_output_to_ingroup_task_mapping    = result_task_output_mappings;

        result_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("Mesh helper visualization: rasterization"),
                                                            &result_task_create_info);
    }

    if (bbox_preview_present_task != nullptr)
    {
        ral_present_task_release(bbox_preview_present_task);
    }

    if (normals_preview_present_task != nullptr)
    {
        ral_present_task_release(normals_preview_present_task);
    }

    return result_present_task;
}

/** TODO */
PRIVATE ral_present_task _scene_renderer_render_traversed_scene_graph(_scene_renderer*                  renderer_ptr,
                                                                      const scene_renderer_render_mode& render_mode,
                                                                      system_time                       frame_time,
                                                                      ral_gfx_state_create_info         ref_gfx_state_create_info)
{
    float                   camera_location[4];
    system_hash64           material_hash              = 0;
    scene_renderer_uber     material_uber              = nullptr;
    demo_materials          materials                  = nullptr;
    uint32_t                n_custom_meshes_to_render  = 0;
    system_resizable_vector present_subtasks           = system_resizable_vector_create(128); /* TODO: We should avoid vector creation at frame render time */

    demo_app_get_property(DEMO_APP_PROPERTY_MATERIALS,
                         &materials);

    /* Calculate camera location */
    {
        /* Matrix inverse = transposal for view matrices (which never use scaling).
         *
         * However, it seems like flyby matrix does incorporate some scaling. For simplicity,
         * let's just go with the inversed matrix for now. (TODO?)
         */
        system_matrix4x4 view_inverted      = system_matrix4x4_create();
        const float*     view_inverted_data = nullptr;

        system_matrix4x4_set_from_matrix4x4(view_inverted,
                                            renderer_ptr->current_view);
        system_matrix4x4_invert            (view_inverted);

        view_inverted_data = system_matrix4x4_get_row_major_data(view_inverted);
        camera_location[0] = view_inverted_data[3];
        camera_location[1] = view_inverted_data[7];
        camera_location[2] = view_inverted_data[11];
        camera_location[3] = 1.0f;

        system_matrix4x4_release(view_inverted);
    }

    /* Forward renderer can work in two modes:
     *
     * 1) With a depth pre-pass. In this case, we need to render all the meshes twice.
     *    For the first pass, we only render the depth information. For the other one,
     *    we re-use that depth information to perform actual shading.
     * 2) Without a depth pre-pass. In this case, the meshes need to be rendered once.
     *
     * Rasterization order within a single pass is irrelevant. THIS IMPLIES LACK OF
     * SUPPORT FOR RENDERING OF SEMI-TRANSPARENT MESHES. Passes should be executed by the GPU
     * one after another, never in parallel.
     */
    const uint32_t   n_passes                        = (render_mode == RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS) ? 2 : 1;
    ral_present_task result_present_task_subtasks[2] = {nullptr, nullptr};

    for (uint32_t n_pass = 0;
                  n_pass < n_passes;
                ++n_pass)
    {
        bool       are_color_writes_enabled = true;
        const bool is_depth_prepass         = (render_mode == RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS    && n_pass == 0);
        uint32_t   n_iterations             = 0;
        const bool use_material_uber        = (render_mode == RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS                ||
                                               render_mode == RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS    && n_pass == 1);

        if (is_depth_prepass)
        {
            n_iterations = 1;
        }
        else
        {
            system_hash64map_get_property(renderer_ptr->regular_mesh_ubers_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_iterations);
        }

        /* Set up the "depth pre-pass" rendering state */
        if (render_mode == RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS)
        {
            if (n_pass == 0)
            {
                are_color_writes_enabled                        = false;
                ref_gfx_state_create_info.depth_test_compare_op = RAL_COMPARE_OP_LESS;
                ref_gfx_state_create_info.depth_writes          = true;
            }
            else
            {
                ASSERT_DEBUG_SYNC(n_pass == 1,
                                  "Sanity check failed");

                are_color_writes_enabled                        = true;
                ref_gfx_state_create_info.depth_writes          = false;
                ref_gfx_state_create_info.depth_test_compare_op = RAL_COMPARE_OP_EQUAL;
            }
        }
        else
        {
            ref_gfx_state_create_info.depth_writes = true;
        }

        for (uint32_t n_iteration = 0;
                      n_iteration < n_iterations; /* n_iterations = no of separate scene_renderer_ubers needed to render the scene */
                    ++n_iteration)
        {
            uint32_t              n_iteration_items = 0;
            _scene_renderer_uber* uber_details_ptr  = nullptr;

            /* Depending on the pass, we may either need to use render mode-specific scene_renderer_uber instance,
             * or one that corresponds to the current material. */
            system_hash64map_get_element_at(renderer_ptr->regular_mesh_ubers_map,
                                            n_iteration,
                                           &uber_details_ptr,
                                           &material_hash);

            ASSERT_DEBUG_SYNC(material_hash != 0,
                              "No scene_renderer_uber instance available!");

            if (use_material_uber)
            {
                /* Retrieve uber instance */
                system_hash64map_get_element_at(renderer_ptr->regular_mesh_ubers_map,
                                                n_iteration,
                                               &uber_details_ptr,
                                               &material_hash);

                ASSERT_DEBUG_SYNC(material_hash != 0,
                                  "No scene_renderer_uber instance available!");

                material_uber = reinterpret_cast<scene_renderer_uber>(material_hash);

                /* Make sure its configuration takes the frame-specific light configuration into account. */
                _scene_renderer_update_uber_light_properties(material_uber,
                                                             renderer_ptr->owned_scene,
                                                             renderer_ptr->current_view,
                                                             frame_time,
                                                             renderer_ptr->temp_variant_float);
            }
            else
            {
                /* If this is not a "depth pre-pass" pass .. */
                if (!is_depth_prepass)
                {
                    _scene_renderer_get_uber_for_render_mode(renderer_ptr,
                                                             render_mode,
                                                             materials,
                                                             renderer_ptr->owned_scene,
                                                            &material_uber);
                }
                else
                {
                    /* Use the 'clip depth' material to output the clip-space depth data */
                    material_uber = mesh_material_get_uber(demo_materials_get_special_material(materials,
                                                                                               renderer_ptr->context,
                                                                                               SPECIAL_MATERIAL_DEPTH_CLIP),
                                                           renderer_ptr->owned_scene,
                                                           false); /* use_shadow_maps */
                }

                ASSERT_DEBUG_SYNC(material_uber != nullptr,
                                  "No ogl_uber instance available!");
            }

            n_iteration_items = 0;

            system_resizable_vector_get_property(uber_details_ptr->regular_mesh_items,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_iteration_items);

            /* Update global properties of uber's vertex shader  */
            scene_renderer_uber_set_shader_general_property(material_uber,
                                                            SCENE_RENDERER_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,
                                                            camera_location);
            scene_renderer_uber_set_shader_general_property(material_uber,
                                                            SCENE_RENDERER_UBER_GENERAL_PROPERTY_VP,
                                                            renderer_ptr->current_vp);

            /* Okay. Go on with the rendering. Start from the regular meshes. These are stored in helper maps */
            scene_renderer_uber_start_info uber_start_info;

            ASSERT_DEBUG_SYNC(renderer_ptr->current_color_rt != nullptr,
                              "No color render-target specified.");

            uber_start_info.color_rt = (are_color_writes_enabled)               ? renderer_ptr->current_color_rt
                                                                                : nullptr;
            uber_start_info.depth_rt = (ref_gfx_state_create_info.depth_writes) ? renderer_ptr->current_depth_rt
                                                                                : nullptr;

            scene_renderer_uber_rendering_start(material_uber,
                                               &uber_start_info);
            {
                if (is_depth_prepass)
                {
                    uint32_t n_uber_map_items = 0;

                    system_hash64map_get_property(renderer_ptr->regular_mesh_ubers_map,
                                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                 &n_uber_map_items);

                    for (uint32_t n_uber_map_item = 0;
                                  n_uber_map_item < n_uber_map_items;
                                ++n_uber_map_item)
                    {
                        uint32_t              n_meshes      = 0;
                        _scene_renderer_uber* uber_item_ptr = nullptr;

                        if (!system_hash64map_get_element_at(renderer_ptr->regular_mesh_ubers_map,
                                                             n_uber_map_item,
                                                            &uber_item_ptr,
                                                             nullptr) ) /* result_hash */
                        {
                            continue;
                        }

                        system_resizable_vector_get_property(uber_item_ptr->regular_mesh_items,
                                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                            &n_meshes);

                        for (uint32_t n_mesh = 0;
                                      n_mesh < n_meshes;
                                    ++n_mesh)
                        {
                            _scene_renderer_mesh_uber_item* mesh_ptr = nullptr;

                            if (!system_resizable_vector_get_element_at(uber_item_ptr->regular_mesh_items,
                                                                        n_mesh,
                                                                       &mesh_ptr) )
                            {
                                continue;
                            }

                            scene_renderer_uber_render_mesh(mesh_ptr->mesh_instance,
                                                            mesh_ptr->model_matrix,
                                                            mesh_ptr->normal_matrix,
                                                            material_uber,
                                                            mesh_ptr->material,
                                                            frame_time,
                                                           &ref_gfx_state_create_info);
                        }
                    }
                }
                else
                {
                    _scene_renderer_mesh_uber_item* item_ptr = nullptr;

                    for (uint32_t n_iteration_item = 0;
                                  n_iteration_item < n_iteration_items;
                                ++n_iteration_item)
                    {
                        system_resizable_vector_get_element_at(uber_details_ptr->regular_mesh_items,
                                                               n_iteration_item,
                                                              &item_ptr);

                        scene_renderer_uber_render_mesh(item_ptr->mesh_instance,
                                                        item_ptr->model_matrix,
                                                        item_ptr->normal_matrix,
                                                        material_uber,
                                                        item_ptr->material,
                                                        frame_time,
                                                       &ref_gfx_state_create_info);
                    }
                }

                /* Any mesh helper visualization needed? */
                if (n_iteration_items > 0 &&
                    uber_details_ptr  != nullptr)
                {
                    ral_present_task helper_vis_task = _scene_renderer_render_mesh_helper_visualizations(renderer_ptr,
                                                                                                         uber_details_ptr);

                    if (helper_vis_task != nullptr)
                    {
                        system_resizable_vector_push(present_subtasks,
                                                     helper_vis_task);
                    }
                }
            }
            system_resizable_vector_push(present_subtasks,
                                         scene_renderer_uber_rendering_stop(material_uber) );

            /* Clean up */
            _scene_renderer_mesh_uber_item* mesh_ptr = nullptr;

            while (system_resizable_vector_pop(uber_details_ptr->regular_mesh_items,
                                              &mesh_ptr) )
            {
                if (mesh_ptr->model_matrix != nullptr)
                {
                    system_matrix4x4_release(mesh_ptr->model_matrix);

                    mesh_ptr->model_matrix = nullptr;
                }

                if (mesh_ptr->normal_matrix != nullptr)
                {
                    system_matrix4x4_release(mesh_ptr->normal_matrix);

                    mesh_ptr->normal_matrix = nullptr;
                }

                system_resource_pool_return_to_pool(renderer_ptr->mesh_uber_items_pool,
                                                    (system_resource_pool_block) mesh_ptr);
            }
        }

        /* Continue with custom meshes. */
        system_resizable_vector_get_property(renderer_ptr->current_custom_meshes_to_render,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_custom_meshes_to_render);

        if (n_custom_meshes_to_render > 0)
        {
            ral_present_task helper_vis_task = _scene_renderer_render_mesh_helper_visualizations(renderer_ptr,
                                                                                                 nullptr); /* uber_details_ptr */

            if (helper_vis_task != nullptr)
            {
                system_resizable_vector_push(present_subtasks,
                                             helper_vis_task); 
            }
        }

        for (uint32_t n_custom_mesh = 0;
                      n_custom_mesh < n_custom_meshes_to_render;
                    ++n_custom_mesh)
        {
            _scene_renderer_mesh* custom_mesh_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(renderer_ptr->current_custom_meshes_to_render,
                                                        n_custom_mesh,
                                                       &custom_mesh_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve descriptor for the custom mesh at index [%d]",
                                  n_custom_mesh);

                continue;
            }

            system_resizable_vector_push(present_subtasks,
                                         custom_mesh_ptr->pfn_get_present_task_for_custom_mesh_proc(renderer_ptr->context,
                                                                                                    custom_mesh_ptr->get_present_task_for_custom_mesh_user_arg,
                                                                                                    custom_mesh_ptr->model_matrix,
                                                                                                    renderer_ptr->current_vp,
                                                                                                    custom_mesh_ptr->normal_matrix,
                                                                                                    is_depth_prepass,
                                                                                                    renderer_ptr->current_color_rt,
                                                                                                    renderer_ptr->current_depth_rt) );

            if (n_pass == (n_passes - 1) )
            {
                _scene_renderer_release_mesh_matrices(custom_mesh_ptr);

                system_resource_pool_return_to_pool(renderer_ptr->mesh_pool,
                                                    (system_resource_pool_block) custom_mesh_ptr);
            }
        }

        /* Any helper visualization? */
        ral_present_task helper_vis_task = nullptr;

        helper_vis_task = _scene_renderer_render_helper_visualizations(renderer_ptr,
                                                                       frame_time);

        if (helper_vis_task != nullptr)
        {
            system_resizable_vector_push(present_subtasks,
                                         helper_vis_task);
        }

        /* Form a group present task which is going to run all cached present sub-tasks in parallel. */
        ral_present_task                   current_subtask               = nullptr;
        uint32_t                           n_max_input_mappings_needed   = 0;
        uint32_t                           n_max_output_mappings_needed  = 0;
        uint32_t                           n_pass_input_mappings_filled  = 0;
        uint32_t                           n_pass_input_objects_exposed  = 0;
        uint32_t                           n_pass_output_mappings_filled = 0;
        uint32_t                           n_pass_output_objects_exposed = 0;
        uint32_t                           n_present_subtasks            = 0;
        ral_present_task_group_mapping*    pass_input_mappings           = nullptr;
        void**                             pass_input_objects            = nullptr;
        ral_present_task_group_mapping*    pass_output_mappings          = nullptr;
        void**                             pass_output_objects           = nullptr;
        ral_present_task*                  present_subtasks_raw          = nullptr;
        ral_present_task_group_create_info subtask_create_info;

        system_resizable_vector_get_property(present_subtasks,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                            &present_subtasks_raw);
        system_resizable_vector_get_property(present_subtasks,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_present_subtasks);

        for (uint32_t n_present_subtask = 0;
                      n_present_subtask < n_present_subtasks;
                    ++n_present_subtask)
        {
            uint32_t              n_subtask_inputs  = 0;
            uint32_t              n_subtask_outputs = 0;
            ral_present_task_type subtask_type;

            ral_present_task_get_property(present_subtasks_raw[n_present_subtask],
                                          RAL_PRESENT_TASK_PROPERTY_TYPE,
                                         &subtask_type);

            ral_present_task_get_property(present_subtasks_raw[n_present_subtask],
                                          (subtask_type == RAL_PRESENT_TASK_TYPE_GROUP) ? RAL_PRESENT_TASK_PROPERTY_N_UNIQUE_INPUT_MAPPINGS
                                                                                        : RAL_PRESENT_TASK_PROPERTY_N_INPUTS,
                                         &n_subtask_inputs);
            ral_present_task_get_property(present_subtasks_raw[n_present_subtask],
                                          (subtask_type == RAL_PRESENT_TASK_TYPE_GROUP) ? RAL_PRESENT_TASK_PROPERTY_N_UNIQUE_OUTPUT_MAPPINGS
                                                                                        : RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS,
                                         &n_subtask_outputs);

            n_max_input_mappings_needed  += n_subtask_inputs;
            n_max_output_mappings_needed += n_subtask_outputs;
        }

        pass_input_mappings  = reinterpret_cast<ral_present_task_group_mapping*>(_malloca(sizeof(ral_present_task_group_mapping) * n_max_input_mappings_needed) );
        pass_input_objects   = reinterpret_cast<void**>                         (_malloca(sizeof(void*)                          * n_max_input_mappings_needed) );
        pass_output_mappings = reinterpret_cast<ral_present_task_group_mapping*>(_malloca(sizeof(ral_present_task_group_mapping) * n_max_output_mappings_needed) );
        pass_output_objects  = reinterpret_cast<void**>                         (_malloca(sizeof(void*)                          * n_max_output_mappings_needed) );

        for (uint32_t n_present_subtask = 0;
                      n_present_subtask < n_present_subtasks;
                    ++n_present_subtask)
        {
            uint32_t              n_task_inputs    = 0;
            uint32_t              n_task_outputs   = 0;
            ral_present_task_type subtask_type;

            ral_present_task_get_property(present_subtasks_raw[n_present_subtask],
                                          RAL_PRESENT_TASK_PROPERTY_TYPE,
                                         &subtask_type);

            ral_present_task_get_property(present_subtasks_raw[n_present_subtask],
                                          (subtask_type == RAL_PRESENT_TASK_TYPE_GROUP) ? RAL_PRESENT_TASK_PROPERTY_N_UNIQUE_INPUT_MAPPINGS
                                                                                        : RAL_PRESENT_TASK_PROPERTY_N_INPUTS,
                                         &n_task_inputs);
            ral_present_task_get_property(present_subtasks_raw[n_present_subtask],
                                          (subtask_type == RAL_PRESENT_TASK_TYPE_GROUP) ? RAL_PRESENT_TASK_PROPERTY_N_UNIQUE_OUTPUT_MAPPINGS
                                                                                        : RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS,
                                         &n_task_outputs);

            for (uint32_t n_io_type = 0;
                          n_io_type < 2;
                        ++n_io_type)
            {
                const ral_present_task_io_type io_type           = (n_io_type == 0) ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                                    : RAL_PRESENT_TASK_IO_TYPE_OUTPUT;
                const uint32_t                 n_ios             = (n_io_type == 0) ? n_task_inputs
                                                                                    : n_task_outputs;
                uint32_t&                      n_mappings_filled = (n_io_type == 0) ? n_pass_input_mappings_filled
                                                                                    : n_pass_output_mappings_filled;
                uint32_t&                      n_objects_exposed = (n_io_type == 0) ? n_pass_input_objects_exposed
                                                                                    : n_pass_output_objects_exposed;

                for (uint32_t n_io = 0;
                              n_io < n_ios;
                            ++n_io)
                {
                    ral_present_task_group_mapping& current_pass_io_mapping = (n_io_type == 0) ? pass_input_mappings [n_mappings_filled++]
                                                                                               : pass_output_mappings[n_mappings_filled++];
                    void**                          exposed_objects         = (n_io_type == 0) ? pass_input_objects
                                                                                               : pass_output_objects;
                    void*                           io_object               = nullptr;

                    ral_present_task_get_io_property(present_subtasks_raw[n_present_subtask],
                                                     io_type,
                                                     n_io,
                                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT,
                                                     (void**) &io_object);

                    #ifdef _DEBUG
                   { 
                        ral_context_object_type io_object_type;

                        ral_present_task_get_io_property(present_subtasks_raw[n_present_subtask],
                                                         io_type,
                                                         n_io,
                                                         RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                                         (void**) &io_object_type);

                        ASSERT_DEBUG_SYNC(io_object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW ||
                                          io_object_type == RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                          "Invalid object attached to the IO");
                    }
                    #endif

                    ASSERT_DEBUG_SYNC(io_object != nullptr,
                                      "Null task IO was specified.");

                    /* If the object is already exposed, map to the existing IO. Otherwise,
                     * we first need to create a new IO.
                     *
                     * TODO: We really should be using a hashmap here.
                     */
                    uint32_t n_exposed_object;

                    for (n_exposed_object = 0;
                         n_exposed_object < n_objects_exposed;
                       ++n_exposed_object)
                    {
                        if (exposed_objects[n_exposed_object] == io_object)
                        {
                            current_pass_io_mapping.group_task_io_index   = n_exposed_object;
                            current_pass_io_mapping.n_present_task        = n_present_subtask;
                            current_pass_io_mapping.present_task_io_index = n_io;

                            break;
                        }
                    }

                    if (n_exposed_object == n_objects_exposed)
                    {
                        exposed_objects[n_exposed_object] = io_object;

                        current_pass_io_mapping.group_task_io_index   = n_exposed_object;
                        current_pass_io_mapping.n_present_task        = n_present_subtask;
                        current_pass_io_mapping.present_task_io_index = n_io;

                        ++n_objects_exposed;
                    }
                }
            }
        }

        subtask_create_info.ingroup_connections                      = nullptr;
        subtask_create_info.n_ingroup_connections                    = 0;
        subtask_create_info.n_present_tasks                          = n_present_subtasks;
        subtask_create_info.n_total_unique_inputs                    = n_pass_input_objects_exposed;
        subtask_create_info.n_total_unique_outputs                   = n_pass_output_objects_exposed;
        subtask_create_info.n_unique_input_to_ingroup_task_mappings  = n_pass_input_mappings_filled;
        subtask_create_info.n_unique_output_to_ingroup_task_mappings = n_pass_output_mappings_filled;
        subtask_create_info.present_tasks                            = present_subtasks_raw;
        subtask_create_info.unique_input_to_ingroup_task_mapping     = pass_input_mappings;
        subtask_create_info.unique_output_to_ingroup_task_mapping    = pass_output_mappings;

        current_subtask = ral_present_task_create_group(system_hashed_ansi_string_create("Forward scene renderer: rasterization"),
                                                        &subtask_create_info);

        ASSERT_DEBUG_SYNC(result_present_task_subtasks[n_pass] == nullptr,
                          "Subtask already defined for pass [%d]",
                          n_pass);

        result_present_task_subtasks[n_pass] = current_subtask;

        /* Clean up */
        ral_present_task temp;

        while (system_resizable_vector_pop(present_subtasks,
                                          &temp) )
        {
            ral_present_task_release(temp);
        }

        system_resizable_vector_release(present_subtasks);

        _freea(pass_input_mappings);
        _freea(pass_input_objects);
        _freea(pass_output_mappings);
        _freea(pass_output_objects);
    }

    system_resizable_vector_clear(renderer_ptr->current_custom_meshes_to_render);

    /* Form the final present task */
    ASSERT_DEBUG_SYNC(n_passes == 1,
                      "Two-pass scenario is TODO");

    return result_present_task_subtasks[0];
}

/** TODO */
PRIVATE void _scene_renderer_return_shadow_maps_to_pool(scene_renderer renderer)
{
    uint32_t         n_lights     = 0;
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(renderer);

    scene_get_property(renderer_ptr->owned_scene,
                       SCENE_PROPERTY_N_LIGHTS,
                      &n_lights);

    /* Iterate over all lights defined for the scene. */
    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light      current_light                     = scene_get_light_by_index(renderer_ptr->owned_scene,
                                                                                      n_light);
        ral_texture_view current_light_sm_texture_views[2] = {nullptr, nullptr};

        ASSERT_DEBUG_SYNC(current_light != nullptr,
                          "Scene light is nullptr");

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_COLOR_RAL,
                                &current_light_sm_texture_views + 0);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_DEPTH_RAL,
                                &current_light_sm_texture_views + 1);

        ral_context_delete_objects(renderer_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   sizeof(current_light_sm_texture_views) / sizeof(current_light_sm_texture_views[0]),
                                   reinterpret_cast<void* const*>(current_light_sm_texture_views) );
    }
}

/** TODO */
PRIVATE void _scene_renderer_subscribe_for_general_notifications(_scene_renderer* scene_renderer_ptr,
                                                                 bool             should_subscribe)
{
    uint32_t                n_scene_cameras        = 0;
    system_callback_manager scene_callback_manager = nullptr;

    scene_get_property(scene_renderer_ptr->owned_scene,
                       SCENE_PROPERTY_CALLBACK_MANAGER,
                      &scene_callback_manager);
    scene_get_property(scene_renderer_ptr->owned_scene,
                       SCENE_PROPERTY_N_CAMERAS,
                      &n_scene_cameras);

    if (should_subscribe)
    {
        /* Sign up for "show frustum" state changes for cameras & lights. This is needed to properly
         * re-route the events to the scene renderer */
        for (uint32_t n_scene_camera = 0;
                      n_scene_camera < n_scene_cameras;
                    ++n_scene_camera)
        {
            system_callback_manager current_camera_callback_manager = nullptr;
            scene_camera            current_camera                  = scene_get_camera_by_index(scene_renderer_ptr->owned_scene,
                                                                                                n_scene_camera);

            scene_camera_get_property(current_camera,
                                      SCENE_CAMERA_PROPERTY_CALLBACK_MANAGER,
                                      0, /* time - irrelevant */
                                     &current_camera_callback_manager);

            system_callback_manager_subscribe_for_callbacks(current_camera_callback_manager,
                                                            SCENE_CAMERA_CALLBACK_ID_SHOW_FRUSTUM_CHANGED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _scene_renderer_on_camera_show_frustum_setting_changed,
                                                            scene_renderer_ptr);
        }

        /* Since scene_renderer caches ogl_uber instances, given current scene configuration,
         * we need to register for various scene call-backs in order to ensure these instances
         * are reset, if the scene configuration ever changes.
         */
        system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                        SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _scene_renderer_on_ubers_map_invalidated,
                                                        scene_renderer_ptr);  /* callback_proc_user_arg */
    }
    else
    {
        for (uint32_t n_scene_camera = 0;
                      n_scene_camera < n_scene_cameras;
                    ++n_scene_camera)
        {
            system_callback_manager current_camera_callback_manager = nullptr;
            scene_camera            current_camera                  = scene_get_camera_by_index(scene_renderer_ptr->owned_scene,
                                                                                                n_scene_camera);

            scene_camera_get_property(current_camera,
                                      SCENE_CAMERA_PROPERTY_CALLBACK_MANAGER,
                                      0, /* time - irrelevant */
                                     &current_camera_callback_manager);

            system_callback_manager_unsubscribe_from_callbacks(current_camera_callback_manager,
                                                               SCENE_CAMERA_CALLBACK_ID_SHOW_FRUSTUM_CHANGED,
                                                               _scene_renderer_on_camera_show_frustum_setting_changed,
                                                               scene_renderer_ptr);
        }

        system_callback_manager_unsubscribe_from_callbacks(scene_callback_manager,
                                                           SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                           _scene_renderer_on_ubers_map_invalidated,
                                                           scene_renderer_ptr);
    }
}

/** TODO */
PRIVATE void _scene_renderer_subscribe_for_mesh_material_notifications(_scene_renderer* scene_renderer_ptr,
                                                                       mesh_material    material,
                                                                       bool             should_subscribe)
{
    system_callback_manager material_callback_manager = nullptr;

    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,
                              &material_callback_manager);

    ASSERT_DEBUG_SYNC(material_callback_manager != nullptr,
                      "Could not retrieve callback manager from a mesh_material instance.");

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(material_callback_manager,
                                                        MESH_MATERIAL_CALLBACK_ID_UBER_UPDATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _scene_renderer_on_mesh_material_uber_invalidated,
                                                        scene_renderer_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(material_callback_manager,
                                                           MESH_MATERIAL_CALLBACK_ID_UBER_UPDATED,
                                                           _scene_renderer_on_mesh_material_uber_invalidated,
                                                           scene_renderer_ptr);
    }
}
/** TODO */
PRIVATE void _scene_renderer_update_frustum_preview_assigned_cameras(_scene_renderer* renderer_ptr)
{
    /* This function may be called via a call-back. Make sure the frustum preview handler is instantiated
     * before carrying on.
     */
    if (renderer_ptr->frustum_preview == nullptr)
    {
        /* TODO: This is wrong. We shouldn't need to be passing viewport size at creation time */
        uint32_t viewport_size[2];

        ASSERT_DEBUG_SYNC(renderer_ptr->current_color_rt != nullptr,
                          "No color RT assigned at the time of the call");

        ral_texture_view_get_mipmap_property(renderer_ptr->current_color_rt,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                             viewport_size + 0);
        ral_texture_view_get_mipmap_property(renderer_ptr->current_color_rt,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                             viewport_size + 1);

        renderer_ptr->frustum_preview = scene_renderer_frustum_preview_create(renderer_ptr->context,
                                                                              renderer_ptr->owned_scene,
                                                                              viewport_size);
    }

    /* Prepare a buffer that can hold up to the number of cameras added to the scene */
    scene_camera* assigned_cameras   = nullptr;
    uint32_t      n_assigned_cameras = 0;
    uint32_t      n_scene_cameras    = 0;

    scene_get_property(renderer_ptr->owned_scene,
                       SCENE_PROPERTY_N_CAMERAS,
                      &n_scene_cameras);

    if (n_scene_cameras != 0)
    {
        assigned_cameras = new (std::nothrow) scene_camera[n_scene_cameras];

        ASSERT_DEBUG_SYNC(assigned_cameras != nullptr,
                          "Out of memory");

        for (uint32_t n_scene_camera = 0;
                      n_scene_camera < n_scene_cameras;
                    ++n_scene_camera)
        {
            bool         current_camera_show_frustum = false;
            scene_camera current_camera              = scene_get_camera_by_index(renderer_ptr->owned_scene,
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
        }
    }

    /* Feed the data to the frustum preview renderer */
    scene_renderer_frustum_preview_assign_cameras(renderer_ptr->frustum_preview,
                                                  n_assigned_cameras,
                                                  assigned_cameras);

    /* Clean up */
    if (assigned_cameras != nullptr)
    {
        delete [] assigned_cameras;

        assigned_cameras = nullptr;
    }
}

/** TODO */
PRIVATE void _scene_renderer_update_uber_light_properties(scene_renderer_uber material_uber,
                                                          scene               scene,
                                                          system_matrix4x4    current_camera_view_matrix,
                                                          system_time         frame_time,
                                                          system_variant      temp_variant_float)
{
    unsigned int n_scene_lights = 0;
    unsigned int n_uber_items   = 0;

    scene_get_property                             (scene,
                                                    SCENE_PROPERTY_N_LIGHTS,
                                                   &n_scene_lights);
    scene_renderer_uber_get_shader_general_property(material_uber,
                                                    SCENE_RENDERER_UBER_GENERAL_PROPERTY_N_ITEMS,
                                                   &n_uber_items);

    ASSERT_DEBUG_SYNC(n_uber_items == n_scene_lights,
                      "scene_renderer needs to be re-initialized, as the light dconfiguration has changed");

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
            system_matrix4x4                 current_light_depth_view                    = nullptr;
            const float*                     current_light_depth_view_row_major          = nullptr;
            const float*                     current_light_depth_vp_row_major            = nullptr;
            system_matrix4x4                 current_light_depth_vp                      = nullptr;
            ral_texture_view                 current_light_shadow_map_texture_view_color = nullptr;
            ral_texture_view                 current_light_shadow_map_texture_view_depth = nullptr;
            float                            current_light_shadow_map_vsm_cutoff         = 0;
            float                            current_light_shadow_map_vsm_min_variance   = 0;
            scene_light_shadow_map_algorithm current_light_sm_algo;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                                    &current_light_sm_algo);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_COLOR_RAL,
                                    &current_light_shadow_map_texture_view_color);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_DEPTH_RAL,
                                    &current_light_shadow_map_texture_view_depth);
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

            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,
                                                         current_light_depth_view);

            /* Depth VP matrix */
            current_light_depth_vp_row_major = system_matrix4x4_get_row_major_data(current_light_depth_vp);

            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP,
                                                         current_light_depth_vp_row_major);

            /* Shadow map textures */
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_VIEW_COLOR,
                                                        &current_light_shadow_map_texture_view_color);
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_VIEW_DEPTH,
                                                        &current_light_shadow_map_texture_view_depth);

            /* VSM cut-off (if VSM is enabled) */
            if (current_light_sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
            {
                scene_renderer_uber_set_shader_item_property(material_uber,
                                                             n_light,
                                                             SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF,
                                                            &current_light_shadow_map_vsm_cutoff);
                scene_renderer_uber_set_shader_item_property(material_uber,
                                                             n_light,
                                                             SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE,
                                                            &current_light_shadow_map_vsm_min_variance);
            }
        }

        _scene_renderer_get_light_color(current_light,
                                        frame_time,
                                        temp_variant_float,
                                        current_light_color_floats);

        if (current_light_type == SCENE_LIGHT_TYPE_AMBIENT)
        {
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR,
                                                         current_light_color_floats);
        }
        else
        {
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE,
                                                         current_light_color_floats);
        }

        if (current_light_type == SCENE_LIGHT_TYPE_POINT ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            /* position curves */
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_POSITION,
                                     current_light_position_floats);

            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION,
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
                    }

                    scene_renderer_uber_set_shader_item_property(material_uber,
                                                                 n_light,
                                                                 SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS,
                                                                 current_light_attenuation_floats);

                    break;
                }

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

                    scene_renderer_uber_set_shader_item_property(material_uber,
                                                                 n_light,
                                                                 SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE,
                                                                &current_light_range_float);

                    break;
                }

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
            }
        }

        if (current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            scene_light_get_property                    (current_light,
                                                         SCENE_LIGHT_PROPERTY_DIRECTION,
                                                         current_light_direction_floats);
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION,
                                                         current_light_direction_floats);
        }

        if (current_light_type == SCENE_LIGHT_TYPE_POINT)
        {
            float            current_light_far_plane;
            float            current_light_near_plane;
            float            current_light_plane_diff;
            const float*     current_light_projection_matrix_data = nullptr;
            system_matrix4x4 current_light_projection_matrix      = nullptr;
            const float*     current_light_view_matrix_data       = nullptr;
            system_matrix4x4 current_light_view_matrix            = nullptr;

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

            if (current_light_far_plane < current_light_near_plane)
            {
                float temp = current_light_near_plane;

                current_light_near_plane = current_light_far_plane;
                current_light_far_plane  = temp;
            }

            current_light_plane_diff = current_light_far_plane - current_light_near_plane;

            current_light_projection_matrix_data = system_matrix4x4_get_row_major_data(current_light_projection_matrix);
            current_light_view_matrix_data       = system_matrix4x4_get_row_major_data(current_light_view_matrix);

            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF,
                                                        &current_light_plane_diff);
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE,
                                                        &current_light_near_plane);
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX,
                                                         current_light_projection_matrix_data);
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,
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

            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE,
                                                        &current_light_cone_angle_half_float);
            scene_renderer_uber_set_shader_item_property(material_uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE,
                                                        &current_light_edge_angle_float);
        }
    }
}


/** Please see header for specification */
PUBLIC void scene_renderer_bake_gpu_assets(scene_renderer renderer)
{
    demo_materials   context_materials = nullptr;
    _scene_renderer* renderer_ptr      = reinterpret_cast<_scene_renderer*>(renderer);

    demo_app_get_property(DEMO_APP_PROPERTY_MATERIALS,
                         &context_materials);

    /* At the time of writing, the only thing we need to bake is ogl_uber instances, which are
     * specific to materials used by meshes. */
    unsigned int n_meshes = 0;

    scene_get_property(renderer_ptr->owned_scene,
                       SCENE_PROPERTY_N_UNIQUE_MESHES,
                      &n_meshes);

    ASSERT_DEBUG_SYNC(n_meshes > 0,
                      "No unique meshes found for scene_renderer's scene.");

    for (unsigned int n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
    {
        mesh                    current_mesh           = scene_get_unique_mesh_by_index(renderer_ptr->owned_scene,
                                                                                        n_mesh);
        system_resizable_vector current_mesh_materials = nullptr;

        ASSERT_DEBUG_SYNC(current_mesh != nullptr,
                          "Could not retrieve mesh instance");

        mesh_get_property(current_mesh,
                          MESH_PROPERTY_MATERIALS,
                         &current_mesh_materials);

        ASSERT_DEBUG_SYNC(current_mesh_materials != nullptr,
                          "Could not retrieve material vector for current mesh instance");

        /* Iterate over all materials defined for the current mesh */
        unsigned int n_mesh_materials = 0;

        system_resizable_vector_get_property(current_mesh_materials,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_mesh_materials);

        ASSERT_DEBUG_SYNC(n_mesh_materials > 0,
                          "No mesh_material instances assigned ot a mesh!");

        for (unsigned int n_mesh_material = 0;
                          n_mesh_material < n_mesh_materials;
                        ++n_mesh_material)
        {
            mesh_material current_mesh_material = nullptr;

            system_resizable_vector_get_element_at(current_mesh_materials,
                                                   n_mesh_material,
                                                  &current_mesh_material);

            ASSERT_DEBUG_SYNC(current_mesh_material != nullptr,
                              "mesh_material instance is nullptr");

            /* Ensure there's an ogl_uber instance prepared for both SM and non-SM cases */
            scene_renderer_uber uber_w_sm  = mesh_material_get_uber(current_mesh_material,
                                                                    renderer_ptr->owned_scene,
                                                                    false); /* use_shadow_maps */
            scene_renderer_uber uber_wo_sm = mesh_material_get_uber(current_mesh_material,
                                                                    renderer_ptr->owned_scene,
                                                                    true); /* use_shadow_maps */

            scene_renderer_uber_link(uber_w_sm);
            scene_renderer_uber_link(uber_wo_sm);
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_renderer scene_renderer_create(ral_context context,
                                                        scene       scene)
{
    _scene_renderer* scene_renderer_ptr = new (std::nothrow) _scene_renderer(context,
                                                                             scene);

    ASSERT_ALWAYS_SYNC(scene_renderer_ptr != nullptr,
                       "Out of memory");

    if (scene_renderer_ptr != nullptr)
    {
        demo_app_get_property(DEMO_APP_PROPERTY_MATERIALS,
                             &scene_renderer_ptr->material_manager);

        /* Subscribe for scene notifications */
        _scene_renderer_subscribe_for_general_notifications(scene_renderer_ptr,
                                                            true /* should_subscribe */);

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
            system_resizable_vector current_mesh_materials   = nullptr;
            uint32_t                n_current_mesh_materials = 0;

            ASSERT_DEBUG_SYNC(current_mesh != nullptr,
                              "Could not retrieve unique mesh at index [%d]",
                              n_scene_unique_mesh);

            mesh_get_property(current_mesh,
                              MESH_PROPERTY_MATERIALS,
                             &current_mesh_materials);

            ASSERT_DEBUG_SYNC(current_mesh_materials != nullptr,
                              "Could not retrieve unique mesh materials.");

            system_resizable_vector_get_property(current_mesh_materials,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_current_mesh_materials);

            for (uint32_t n_current_mesh_material = 0;
                          n_current_mesh_material < n_current_mesh_materials;
                        ++n_current_mesh_material)
            {
                mesh_material current_mesh_material = nullptr;

                system_resizable_vector_get_element_at(current_mesh_materials,
                                                       n_current_mesh_material,
                                                      &current_mesh_material);

                ASSERT_DEBUG_SYNC(current_mesh_material != nullptr,
                                  "Could not retrieve mesh_material instance.");

                /* Finally, time to sign up! */
                _scene_renderer_subscribe_for_mesh_material_notifications(scene_renderer_ptr,
                                                                          current_mesh_material,
                                                                          true /* should_subscribe */);
            }
        }
    }

    return (scene_renderer) scene_renderer_ptr;
}

/** Please see header for specification */
PUBLIC bool scene_renderer_cull_against_frustum(scene_renderer                          renderer,
                                                mesh                                    mesh_gpu,
                                                scene_renderer_frustum_culling_behavior behavior,
                                                const void*                             behavior_data)
{
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(renderer);
    bool             result       = true;

    /* Retrieve AABB for the mesh */
    const float* aabb_max_ptr = nullptr;
    const float* aabb_min_ptr = nullptr;

    mesh_get_property(mesh_gpu,
                      MESH_PROPERTY_MODEL_AABB_MAX,
                     &aabb_max_ptr);
    mesh_get_property(mesh_gpu,
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
        case SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_PASS_OBJECTS_IN_FRONT_OF_CAMERA:
        {
            const float* camera_location_world_vec3 = (reinterpret_cast<const float*>(behavior_data));
            const float* camera_view_direction_vec3 = (reinterpret_cast<const float*>(behavior_data)) + 3;

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
            }

            if (result)
            {
                /* TODO: Only take the mesh into account if it is within point light range */
            }

            break;
        }

        case SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES:
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
                }

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
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized frustum culling behavior requested");
        }
    }

end:

    /* NOTE: For normals preview to work correctly, frustum culling MUST always succeed */
    if (renderer_ptr->current_helper_visualization == HELPER_VISUALIZATION_NORMALS)
    {
        result = true;
    }

    /* All done */
    if (result)
    {
        /* Update max & min AABB coordinates stored in scene_renderer,
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
PUBLIC void scene_renderer_get_indexed_property(const scene_renderer    renderer,
                                                scene_renderer_property property,
                                                uint32_t                index,
                                                void*                   out_result_ptr)
{
    const _scene_renderer* renderer_ptr = reinterpret_cast<const _scene_renderer*>(renderer);

    switch (property)
    {
        case SCENE_RENDERER_PROPERTY_MESH_INSTANCE:
        {
            const _scene_renderer_mesh* mesh_ptr = nullptr;

            if (system_hash64map_get(renderer_ptr->current_mesh_id_to_mesh_map,
                                     index,
                                    &mesh_ptr) )
            {
                *(reinterpret_cast<mesh*>(out_result_ptr)) = mesh_ptr->mesh_instance;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh instance for mesh id [%d]",
                                  index);
            }

            break;
        }

        case SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX:
        {
            const _scene_renderer_mesh* mesh_ptr = nullptr;

            if (system_hash64map_get(renderer_ptr->current_mesh_id_to_mesh_map,
                                     index,
                                    &mesh_ptr) )
            {
                *(reinterpret_cast<system_matrix4x4*>(out_result_ptr)) = mesh_ptr->model_matrix;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh instance for mesh id [%d]",
                                  index);
            }

            break;
        }

        case SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX:
        {
            const _scene_renderer_mesh* mesh_ptr = nullptr;

            if (system_hash64map_get(renderer_ptr->current_mesh_id_to_mesh_map,
                                     index,
                                    &mesh_ptr) )
            {
                *(reinterpret_cast<system_matrix4x4*>(out_result_ptr) ) = mesh_ptr->normal_matrix;
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
                              "Unrecognized indexed scene_renderer_property");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task scene_renderer_get_present_task_for_scene_graph(scene_renderer                      renderer,
                                                                                    system_matrix4x4                    view,
                                                                                    system_matrix4x4                    projection,
                                                                                    scene_camera                        camera,
                                                                                    const scene_renderer_render_mode&   render_mode,
                                                                                    bool                                apply_shadow_mapping,
                                                                                    scene_renderer_helper_visualization helper_visualization,
                                                                                    system_time                         frame_time,
                                                                                    ral_texture_view                    color_rt,
                                                                                    ral_texture_view                    depth_rt)
{
    scene_graph      graph               = nullptr;
    _scene_renderer* renderer_ptr        = reinterpret_cast<_scene_renderer*>(renderer);
    ral_present_task result_present_task = nullptr;

    scene_get_property(renderer_ptr->owned_scene,
                       SCENE_PROPERTY_GRAPH,
                      &graph);

    /* Prepare graph traversal parameters */
    system_matrix4x4 vp = system_matrix4x4_create_by_mul(projection,
                                                         view);

    renderer_ptr->current_color_rt                  = color_rt;
    renderer_ptr->current_depth_rt                  = depth_rt;
    renderer_ptr->current_camera                    = camera;
    renderer_ptr->current_helper_visualization      = helper_visualization;
    renderer_ptr->current_is_shadow_mapping_enabled = apply_shadow_mapping;
    renderer_ptr->current_projection                = projection;
    renderer_ptr->current_view                      = view;
    renderer_ptr->current_vp                        = vp;

    /* If the caller has requested shadow mapping support, we need to render shadow maps before actual
     * rendering proceeds.
     */
    static const bool shadow_mapping_disabled = false;
    static const bool shadow_mapping_enabled  = true;

    if (apply_shadow_mapping)
    {
        scene_set_property(renderer_ptr->owned_scene,
                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                          &shadow_mapping_disabled);

        /* Prepare the shadow maps */
        scene_renderer_sm_render_shadow_maps(renderer_ptr->shadow_mapping,
                                             renderer,
                                             renderer_ptr->owned_scene,
                                             camera,
                                             frame_time);

        scene_set_property(renderer_ptr->owned_scene,
                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                          &shadow_mapping_enabled);

        /* The _scene_renderer_render_shadow_maps() call might have messed up
         * projection, view & vp matrices we set up.
         *
         * Revert the original settings */
        renderer_ptr->current_color_rt                  = color_rt;
        renderer_ptr->current_depth_rt                  = depth_rt;
        renderer_ptr->current_helper_visualization      = helper_visualization;
        renderer_ptr->current_is_shadow_mapping_enabled = apply_shadow_mapping;
        renderer_ptr->current_projection                = projection;
        renderer_ptr->current_view                      = view;
        renderer_ptr->current_vp                        = vp;
    }
    else
    {
        uint32_t viewport_size[2];

        ral_texture_view_get_mipmap_property(color_rt,
                                             0, /* n_layer */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                             viewport_size + 0);
        ral_texture_view_get_mipmap_property(color_rt,
                                             0, /* n_layer */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                             viewport_size + 1);

        scene_camera_set_property(renderer_ptr->current_camera,
                                  SCENE_CAMERA_PROPERTY_VIEWPORT,
                                  viewport_size);

    }
    /* 1. Traverse the scene graph and:
     *
     *    - update light direction vectors.
     *    - determine mesh visibility via frustum culling.
     *
     *  NOTE: We cache 'helper visualization' setting, as the helper rendering requires
     *        additional information that needs not be stored otherwise.
     */
    scene_graph_traverse(graph,
                         scene_renderer_update_current_model_matrix,
                         nullptr, /* insert_camera_proc */
                         scene_renderer_update_light_properties,
                         (render_mode == RENDER_MODE_SHADOW_MAP) ? scene_renderer_sm_process_mesh_for_shadow_map_rendering
                                                                 : _scene_renderer_process_mesh_for_forward_rendering,
                         renderer,
                         frame_time);

    /* Prepare a reference gfx state create info configuration */
    ral_gfx_state_create_info ref_gfx_state_create_info;

    ref_gfx_state_create_info.culling               = true;
    ref_gfx_state_create_info.depth_test            = true;
    ref_gfx_state_create_info.depth_test_compare_op = RAL_COMPARE_OP_LESS;
    ref_gfx_state_create_info.depth_writes          = (depth_rt != nullptr);

    /* 2. Start uber rendering. Issue as many render requests as there are materials. */
    if (render_mode == RENDER_MODE_SHADOW_MAP)
    {
        result_present_task = scene_renderer_sm_render_shadow_map_meshes(renderer_ptr->shadow_mapping,
                                                                         renderer,
                                                                         renderer_ptr->owned_scene,
                                                                         frame_time,
                                                                        &ref_gfx_state_create_info);
    }
    else
    {
        result_present_task = _scene_renderer_render_traversed_scene_graph(renderer_ptr,
                                                                           render_mode,
                                                                           frame_time,
                                                                           ref_gfx_state_create_info);
    }

    /* 3. Clean up in anticipation for the next call. We specifically do not cache any of the
     *    data because of the frustum culling which needs to be performed, every time model matrix
     *    or VP changes.
     **/
    if (helper_visualization != 0)
    {
        system_resource_pool_return_all_allocations(renderer_ptr->mesh_pool);

        system_resizable_vector_clear(renderer_ptr->current_custom_meshes_to_render);
        system_hash64map_clear       (renderer_ptr->current_mesh_id_to_mesh_map);
    }

    if (apply_shadow_mapping)
    {
        /* Release any shadow maps we might've allocated back to the texture pool */
        _scene_renderer_return_shadow_maps_to_pool(renderer);
    }

    /* Good to shut down the show */
    if (vp != nullptr)
    {
        system_matrix4x4_release(vp);

        vp = nullptr;
    }

    return result_present_task;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_renderer_get_property(const scene_renderer    renderer,
                                                    scene_renderer_property property,
                                                    void*                   out_result_ptr)
{
    const _scene_renderer* renderer_ptr = reinterpret_cast<const _scene_renderer*>(renderer);

    switch (property)
    {
        case SCENE_RENDERER_PROPERTY_CONTEXT_RAL:
        {
            *reinterpret_cast<ral_context *>(out_result_ptr) = renderer_ptr->context;

            break;
        }

        case SCENE_RENDERER_PROPERTY_GRAPH:
        {
            scene_get_property(renderer_ptr->owned_scene,
                               SCENE_PROPERTY_GRAPH,
                               out_result_ptr);

            break;
        }

        case SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX:
        {
            *reinterpret_cast<system_matrix4x4*>(out_result_ptr) = renderer_ptr->current_model_matrix;

            break;
        }

        case SCENE_RENDERER_PROPERTY_SHADOW_MAPPING_MANAGER:
        {
            *reinterpret_cast<scene_renderer_sm*>(out_result_ptr) = renderer_ptr->shadow_mapping;

            break;
        }

        case SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX:
        {
            memcpy(out_result_ptr,
                   renderer_ptr->current_camera_visible_world_aabb_max,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_max) );

            break;
        }

        case SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN:
        {
            memcpy(out_result_ptr,
                   renderer_ptr->current_camera_visible_world_aabb_min,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_min) );

            break;
        }

        case SCENE_RENDERER_PROPERTY_VP:
        {
            *reinterpret_cast<system_matrix4x4*>(out_result_ptr) = renderer_ptr->current_vp;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_graph property");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_renderer_release(scene_renderer scene_renderer_instance)
{
    if (scene_renderer_instance != nullptr)
    {
        delete reinterpret_cast<_scene_renderer*>(scene_renderer_instance);

        scene_renderer_instance = nullptr;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_renderer_set_property(scene_renderer          renderer,
                                                    scene_renderer_property property,
                                                    const void*             data)
{
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(renderer);

    switch (property)
    {
        case SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX:
        {
            memcpy(renderer_ptr->current_camera_visible_world_aabb_max,
                   data,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_max) );

            break;
        }

        case SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN:
        {
            memcpy(renderer_ptr->current_camera_visible_world_aabb_min,
                   data,
                   sizeof(renderer_ptr->current_camera_visible_world_aabb_min) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_renderer_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC void scene_renderer_update_current_model_matrix(system_matrix4x4 transformation_matrix,
                                                       void*            renderer)
{
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(renderer);

    system_matrix4x4_set_from_matrix4x4(renderer_ptr->current_model_matrix,
                                        transformation_matrix);
}

/** Please see header for specification */
PUBLIC void scene_renderer_update_light_properties(scene_light light,
                                                   void*       renderer)
{
    _scene_renderer* renderer_ptr = reinterpret_cast<_scene_renderer*>(renderer);

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