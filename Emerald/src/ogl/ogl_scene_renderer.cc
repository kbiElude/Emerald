/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_bbox_preview.h"
#include "ogl/ogl_scene_renderer_frustum_preview.h"
#include "ogl/ogl_scene_renderer_lights_preview.h"
#include "ogl/ogl_scene_renderer_normals_preview.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_textures.h"
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

#define DEFAULT_AABB_MAX_VALUE (FLT_MIN)
#define DEFAULT_AABB_MIN_VALUE (FLT_MAX)

/* Forward declarations */
PRIVATE void _ogl_scene_renderer_deinit_cached_ubers_map_contents         (__in  __notnull system_hash64map                               cached_materials_map);
PRIVATE void _ogl_scene_renderer_deinit_resizable_vector_for_resource_pool(                system_resource_pool_block);
PRIVATE void _ogl_scene_renderer_get_ogl_uber_for_render_mode             (__in            _ogl_scene_renderer_render_mode                render_mode,
                                                                           __in  __notnull ogl_materials                                  context_materials,
                                                                           __in  __notnull scene                                          scene,
                                                                           __out __notnull ogl_uber*                                      result_uber_ptr);
PRIVATE void _ogl_scene_renderer_init_resizable_vector_for_resource_pool  (                system_resource_pool_block);
PRIVATE void _ogl_scene_renderer_render_shadow_maps                       (__in __notnull  ogl_scene_renderer                             renderer,
                                                                           __in __notnull  scene_camera                                   target_camera,
                                                                           __in __notnull  const _ogl_scene_renderer_shadow_mapping_type& shadow_mapping_type,
                                                                           __in            system_timeline_time                           frame_time);
PRIVATE void _ogl_scene_renderer_update_frustum_preview_assigned_cameras  (__in __notnull  struct _ogl_scene_renderer*                    renderer_ptr);
PRIVATE void _ogl_scene_renderer_update_ogl_uber_light_properties         (__in __notnull  ogl_uber                                       material_uber,
                                                                           __in __notnull  scene                                          scene,
                                                                           __in            system_timeline_time                           frame_time,
                                                                           __in __notnull  system_variant                                 temp_variant_float);


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

    float                                    current_aabb_max[3];
    float                                    current_aabb_min[3];
    scene_camera                             current_camera;
    _ogl_scene_renderer_helper_visualization current_helper_visualization;
    system_matrix4x4                         current_model_matrix;
    system_resizable_vector                  current_no_rasterization_meshes; /* only used for the "no rasterization" mode. holds _ogl_scene_renderer_mesh* items */
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
        current_no_rasterization_meshes = system_resizable_vector_create(4, /* capacity */
                                                                         sizeof(_ogl_scene_renderer_mesh*) );
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

        if (current_no_rasterization_meshes != NULL)
        {
            system_resizable_vector_release(current_no_rasterization_meshes);

            current_no_rasterization_meshes = NULL;
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
        const uint32_t n_meshes = system_resizable_vector_get_amount_of_elements(uber_ptr->meshes);

        for (uint32_t n_mesh_item = 0;
                      n_mesh_item < n_meshes;
                    ++n_mesh_item)
        {
            _ogl_scene_renderer_mesh* mesh_ptr = NULL;

            system_resizable_vector_get_element_at(uber_ptr->meshes,
                                                   n_mesh_item,
                                                  &mesh_ptr);

            if (mesh_ptr != NULL)
            {
                system_matrix4x4_release(mesh_ptr->model_matrix);
                system_matrix4x4_release(mesh_ptr->normal_matrix);
            }
        }

        system_hash64map_remove(ubers_map,
                                ogl_uber_hash);
    }
}

/* TODO */
PRIVATE void _ogl_scene_renderer_deinit_resizable_vector_for_resource_pool(system_resource_pool_block block)
{
    system_resizable_vector* vector_ptr = (system_resizable_vector*) block;

    system_resizable_vector_release(*vector_ptr);
}

/** TODO.
 *
 *  Implementation based on http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes-ii/
 *  and http://gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf.
 *
 *  This function also adjusts ogl_scene_renderer's max & min AABB coordinates.
 */
PRIVATE bool _ogl_scene_renderer_frustum_cull(__in __notnull ogl_scene_renderer renderer,
                                              __in __notnull mesh               mesh_gpu)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

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
    bool           result            = true;

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
            if (renderer_ptr->current_aabb_max[n_component] < world_aabb_max[n_component])
            {
                renderer_ptr->current_aabb_max[n_component] = world_aabb_max[n_component];
            }

            if (renderer_ptr->current_aabb_min[n_component] > world_aabb_min[n_component])
            {
                renderer_ptr->current_aabb_min[n_component] = world_aabb_min[n_component];
            }
        }
    }

    return result;
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

        case RENDER_MODE_NO_RASTERIZATION:
        {
            *result_uber_ptr = mesh_material_get_ogl_uber(ogl_materials_get_special_material(context_materials,
                                                                                             SPECIAL_MATERIAL_DUMMY),
                                                          scene);

            break;
        }

        case RENDER_MODE_NORMALS_ONLY:
        {
            *result_uber_ptr = mesh_material_get_ogl_uber(ogl_materials_get_special_material(context_materials,
                                                                                             SPECIAL_MATERIAL_NORMALS),
                                                          scene);

            break;
        }

        case RENDER_MODE_TEXCOORDS_ONLY:
        {
            *result_uber_ptr = mesh_material_get_ogl_uber(ogl_materials_get_special_material(context_materials,
                                                                                             SPECIAL_MATERIAL_TEXCOORD),
                                                          scene);

            break;
        }

        default:
        {
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
PRIVATE void _ogl_scene_renderer_on_light_added(const void* unused,
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
PRIVATE void _ogl_scene_renderer_on_camera_show_frustum_setting_changed(const void* unused,
                                                                              void* scene_renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) scene_renderer;

    /* Use a shared handler to re-create the frustum assignments for the frustum preview renderer */
    _ogl_scene_renderer_update_frustum_preview_assigned_cameras(renderer_ptr);
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
    bool                    mesh_needs_gl_update          = false;
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
    mesh_get_property(mesh_instantiation_parent_gpu,
                      MESH_PROPERTY_GL_THREAD_FILL_BUFFERS_CALL_NEEDED,
                     &mesh_needs_gl_update);

    if (mesh_needs_gl_update)
    {
        LOG_ERROR("Performance warning: executing deferred mesh_fill_gl_buffers() call.");

        mesh_fill_gl_buffers(mesh_instantiation_parent_gpu,
                             ogl_context_get_current_context() );
    }

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
    if (!_ogl_scene_renderer_frustum_cull( (ogl_scene_renderer) renderer,
                                          mesh_instantiation_parent_gpu) )
    {
        goto end;
    }

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
                                               renderer_ptr->scene);

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
            /* TODO: There is a known bug which causes multiple items to be stored
             *       in mesh_id_map if the mesh is described by multiple materials.
             *       FIX WHEN NEEDED.
             */
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
PRIVATE void _ogl_scene_renderer_process_mesh_for_no_rasterization_rendering(__notnull scene_mesh scene_mesh_instance,
                                                                             __in      void*      renderer)
{
    _ogl_scene_renderer*      renderer_ptr                  = (_ogl_scene_renderer*) renderer;
    mesh_material             material                      = ogl_materials_get_special_material(renderer_ptr->material_manager,
                                                                                                 SPECIAL_MATERIAL_DUMMY);
    mesh                      mesh_gpu                      = NULL;
    mesh                      mesh_instantiation_parent_gpu = NULL;
    uint32_t                  mesh_id                       = -1;
    bool                      mesh_needs_gl_update          = false;
    ogl_uber                  mesh_uber                     = NULL;
    _ogl_scene_renderer_uber* uber_ptr                      = NULL;

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

    /* Perform frustum culling to make sure it actually makes sense to render
     * this mesh.
     */
    if (!_ogl_scene_renderer_frustum_cull( (ogl_scene_renderer) renderer,
                                          mesh_instantiation_parent_gpu) )
    {
        goto end;
    }

    /* This is a visible mesh we should store. As opposed to the uber->mesh map we're
     * using for forward rendering, in "no rasterization" mode there is only one uber
     * that is used for the rendeirng process.
     * However, if we were to re-use the same map as in the other modes, we'd need
     * to clean it every ogl_scene_renderer_render_scene_graph() call which could
     * affect performance. It would also disrupt the shadow mapping baking.
     * Hence, for the "no rasterization mode", we use a renderer-level vector to store
     * visible meshes. This will work, as long the "no rasterization mode" does not
     * call graph rendeirng entry point from itself, which should never be the case.
     */
    _ogl_scene_renderer_mesh* new_mesh_item = (_ogl_scene_renderer_mesh*) system_resource_pool_get_from_pool(renderer_ptr->scene_renderer_mesh_pool);

    ASSERT_ALWAYS_SYNC(new_mesh_item != NULL,
                       "Out of memory");

    new_mesh_item->material      = material;
    new_mesh_item->mesh_id       = mesh_id;
    new_mesh_item->mesh_instance = mesh_gpu;
    new_mesh_item->model_matrix  = system_matrix4x4_create();
    new_mesh_item->normal_matrix = NULL;

    system_matrix4x4_set_from_matrix4x4(new_mesh_item->model_matrix,
                                        renderer_ptr->current_model_matrix);

    system_resizable_vector_push(renderer_ptr->current_no_rasterization_meshes,
                                 new_mesh_item);

end:
    ;
}

/** TODO */
PRIVATE void _ogl_scene_renderer_process_mesh_for_shadow_map_pre_pass(__notnull scene_mesh scene_mesh_instance,
                                                                      __in      void*      renderer)
{
    _ogl_scene_renderer* renderer_ptr                  = (_ogl_scene_renderer*) renderer;
    mesh                 mesh_gpu                      = NULL;
    mesh                 mesh_instantiation_parent_gpu = NULL;

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_MESH,
                           &mesh_gpu);
    mesh_get_property      (mesh_gpu,
                            MESH_PROPERTY_INSTANTIATION_PARENT,
                           &mesh_instantiation_parent_gpu);

    if (mesh_instantiation_parent_gpu != NULL)
    {
        mesh_gpu = mesh_instantiation_parent_gpu;
    }

    /* Perform frustum culling. This is where the AABBs are also updated. */
    _ogl_scene_renderer_frustum_cull( (ogl_scene_renderer) renderer,
                                     mesh_gpu);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_new_model_matrix(__in __notnull system_matrix4x4 transformation_matrix,
                                                  __in __notnull void*            renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    system_matrix4x4_set_from_matrix4x4(renderer_ptr->current_model_matrix,
                                        transformation_matrix);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_render_shadow_maps(__in __notnull ogl_scene_renderer                             renderer,
                                                    __in __notnull scene_camera                                   target_camera,
                                                    __in __notnull const _ogl_scene_renderer_shadow_mapping_type& shadow_mapping_type,
                                                    __in           system_timeline_time                           frame_time)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    scene_graph                                               graph           = NULL;
    uint32_t                                                  n_lights        = 0;
    _ogl_scene_renderer*                                      renderer_ptr    = (_ogl_scene_renderer*) renderer;
    ogl_shadow_mapping                                        shadow_mapping  = NULL;
    ogl_textures                                              texture_pool    = NULL;

    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_SHADOW_MAPPING,
                            &shadow_mapping);
    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &texture_pool);
    scene_get_property      (renderer_ptr->scene,
                             SCENE_PROPERTY_GRAPH,
                            &graph);
    scene_get_property      (renderer_ptr->scene,
                             SCENE_PROPERTY_N_LIGHTS,
                            &n_lights);

    /* Iterate over all lights defined for the scene. Focus only on those,
     * which act as shadow casters.. */
    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light      current_light                  = scene_get_light_by_index(renderer_ptr->scene,
                                                                                   n_light);
        bool             current_light_is_shadow_caster = false;
        scene_light_type current_light_type             = SCENE_LIGHT_TYPE_UNKNOWN;

        ASSERT_DEBUG_SYNC(current_light != NULL,
                          "Scene light is NULL");

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_TYPE,
                                &current_light_type);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                &current_light_is_shadow_caster);

        if (current_light_is_shadow_caster)
        {
            /* Grab a texture from the texture pool. It will serve as
             * storage for the shadow map.
             *
             * Note that it is an error for scene_light instance to hold
             * a shadow map texture at this point.
             */
            ogl_texture  current_light_shadow_map                = NULL;
            GLenum       current_light_shadow_map_internalformat = GL_NONE;
            unsigned int current_light_shadow_map_size[2]        = {0};

#if 0
            Taking the check out for UI preview of shadow maps.
            #ifdef _DEBUG
            {
                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                        &current_light_shadow_map);

                ASSERT_DEBUG_SYNC(current_light_shadow_map == NULL,
                                  "Shadow map texture is already assigned to a scene_light instance!");
            }
            #endif /* _DEBUG */
#endif

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT,
                                    &current_light_shadow_map_internalformat);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,
                                     current_light_shadow_map_size);

            ASSERT_DEBUG_SYNC(current_light_shadow_map_size[0] > 0 &&
                              current_light_shadow_map_size[1] > 0,
                              "Invalid shadow map size requested for a scene_light instance.");

            current_light_shadow_map = ogl_textures_get_texture_from_pool(renderer_ptr->context,
                                                                          OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D,
                                                                          1, /* n_mipmaps */
                                                                          current_light_shadow_map_internalformat,
                                                                          current_light_shadow_map_size[0],
                                                                          current_light_shadow_map_size[1],
                                                                          1,      /* base_mipmap_depth */
                                                                          0,      /* n_samples */
                                                                          false); /* fixed_samples_location */

            ASSERT_DEBUG_SYNC(current_light_shadow_map != NULL,
                             "Could not retrieve a shadow map texture from the texture pool.");

            dsa_entrypoints->pGLTextureParameteriEXT(current_light_shadow_map,
                                                     GL_TEXTURE_2D,
                                                     GL_TEXTURE_MAG_FILTER,
                                                     GL_NEAREST);
            dsa_entrypoints->pGLTextureParameteriEXT(current_light_shadow_map,
                                                     GL_TEXTURE_2D,
                                                     GL_TEXTURE_MIN_FILTER,
                                                     GL_NEAREST);
            dsa_entrypoints->pGLTextureParameteriEXT(current_light_shadow_map,
                                                     GL_TEXTURE_2D,
                                                     GL_TEXTURE_COMPARE_FUNC,
                                                     GL_LESS);
            dsa_entrypoints->pGLTextureParameteriEXT(current_light_shadow_map,
                                                     GL_TEXTURE_2D,
                                                     GL_TEXTURE_COMPARE_MODE,
                                                     GL_COMPARE_REF_TO_TEXTURE);

            /* Assign the shadow map texture to the light */
            scene_light_set_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                    &current_light_shadow_map);

            /* Configure the GL for depth map rendering */
            system_matrix4x4 sm_projection_matrix    = NULL;
            float            sm_camera_location[3];
            system_matrix4x4 sm_view_matrix          = NULL;
            system_matrix4x4 sm_vp_matrix            = NULL;

            ogl_shadow_mapping_toggle(shadow_mapping,
                                      current_light,
                                      true); /* should_enable */

            if (renderer_ptr->current_aabb_max[0] != renderer_ptr->current_aabb_min[0] &&
                renderer_ptr->current_aabb_max[1] != renderer_ptr->current_aabb_min[1] &&
                renderer_ptr->current_aabb_max[2] != renderer_ptr->current_aabb_min[2])
            {
                switch (current_light_type)
                {
                    case SCENE_LIGHT_TYPE_DIRECTIONAL:
                    {
                        ogl_shadow_mapping_get_matrices_for_directional_light(current_light,
                                                                              renderer_ptr->current_aabb_min,
                                                                              renderer_ptr->current_aabb_max,
                                                                             &sm_view_matrix,
                                                                             &sm_projection_matrix,
                                                                              sm_camera_location);

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unsupported light encountered for enabled plain shadow mapping.");
                    }
                } /* switch (current_light_type) */

                /* Update light's shadow VP matrix */
                ASSERT_DEBUG_SYNC(sm_view_matrix       != NULL &&
                                  sm_projection_matrix != NULL,
                                  "Projection/view matrix for shadow map rendering is NULL");

                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP,
                                        &sm_vp_matrix);

                system_matrix4x4_set_from_matrix4x4   (sm_vp_matrix,
                                                       sm_projection_matrix);
                system_matrix4x4_multiply_by_matrix4x4(sm_vp_matrix,
                                                       sm_view_matrix);

                /* NOTE: This call is recursive (this function was called by exactly the same function,
                 *       but we're requesting no shadow maps this time AND the scene graph has already
                 *       been traversed, so it should be fairly inexpensive and focus solely on drawing
                 *       geometry.
                 */
                ogl_scene_renderer_render_scene_graph( (ogl_scene_renderer) renderer_ptr,
                                                       sm_view_matrix,
                                                       sm_projection_matrix,
                                                       NULL, /* camera */
                                                       NULL, /* camera_location */
                                                       RENDER_MODE_NO_RASTERIZATION,
                                                       SHADOW_MAPPING_TYPE_DISABLED,
                                                       HELPER_VISUALIZATION_NONE,
                                                       frame_time);
           } /* if (it makes sense to render SM) */

           ogl_shadow_mapping_toggle(shadow_mapping,
                                     current_light,
                                     false); /* should_enable */

           /* Clean up */
           if (sm_projection_matrix != NULL)
           {
                system_matrix4x4_release(sm_projection_matrix);
                sm_projection_matrix = NULL;
           }

           if (sm_view_matrix != NULL)
           {
                system_matrix4x4_release(sm_view_matrix);
                sm_view_matrix = NULL;
           }
        } /* if (current_light_is_shadow_caster) */
    } /* for (all scene lights) */
}

/** TODO */
PRIVATE void _ogl_scene_renderer_return_shadow_maps_to_pool(__in __notnull ogl_scene_renderer renderer)
{
    uint32_t             n_lights       = 0;
    _ogl_scene_renderer* renderer_ptr   = (_ogl_scene_renderer*) renderer;
    ogl_shadow_mapping   shadow_mapping = NULL;
    ogl_textures         texture_pool   = NULL;

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
        scene_light current_light            = scene_get_light_by_index(renderer_ptr->scene,
                                                                        n_light);
        ogl_texture current_light_sm_texture = NULL;

        ASSERT_DEBUG_SYNC(current_light != NULL,
                          "Scene light is NULL");

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                &current_light_sm_texture);

        if (current_light_sm_texture != NULL)
        {
            ogl_textures_return_reusable(renderer_ptr->context,
                                         current_light_sm_texture);

#if 0
            Let's leave the assignment for UI preview

            scene_light_set_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                     &null_texture);
#endif
        } /* if (current_light_is_shadow_caster) */
    } /* for (all scene lights) */
}

/** TODO */
PRIVATE void _ogl_scene_renderer_update_frustum_preview_assigned_cameras(__in __notnull _ogl_scene_renderer* renderer_ptr)
{
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
PRIVATE void _ogl_scene_renderer_update_light_properties(__in __notnull scene_light light,
                                                         __in           void*       renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    /* Directional vector needs to be only updated for directional lights */
    scene_light_type light_type = SCENE_LIGHT_TYPE_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    /* Update light position for point light */
    if (light_type == SCENE_LIGHT_TYPE_POINT)
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

    /* Update directional vector for directional lights */
    if (light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
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
    else
    {
        ASSERT_DEBUG_SYNC(light_type == SCENE_LIGHT_TYPE_AMBIENT ||
                          light_type == SCENE_LIGHT_TYPE_POINT,
                          "Unrecognized light type, expand.");
    }
}

PRIVATE void _ogl_scene_renderer_update_ogl_uber_light_properties(__in __notnull ogl_uber             material_uber,
                                                                  __in __notnull scene                scene,
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
        scene_light      current_light                       = scene_get_light_by_index(scene,
                                                                                        n_light);
        scene_light_type current_light_type                  = SCENE_LIGHT_TYPE_UNKNOWN;
        curve_container  current_light_attenuation_curves[3];
        float            current_light_attenuation_floats[3];
        float            current_light_color_floats      [3];
        bool             current_light_shadow_caster         = false;
        float            current_light_direction_floats  [3];
        float            current_light_position_floats   [3];

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                &current_light_shadow_caster);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_TYPE,
                                &current_light_type);

        if (current_light_shadow_caster)
        {
            const float*     current_light_depth_vp_row_major = NULL;
            system_matrix4x4 current_light_depth_vp           = NULL;
            ogl_texture      current_light_shadow_map_texture = NULL;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                    &current_light_shadow_map_texture);
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP,
                                    &current_light_depth_vp);

            /* Depth VP matrix */
            current_light_depth_vp_row_major = system_matrix4x4_get_row_major_data(current_light_depth_vp);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_VERTEX_DEPTH_VP,
                                              current_light_depth_vp_row_major);

            /* Shadow map */
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP,
                                             &current_light_shadow_map_texture);
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

        if (current_light_type == SCENE_LIGHT_TYPE_POINT)
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
                curve_container src_curve     = NULL;
                float*          dst_float_ptr = NULL;

                if (n_curve < 3)
                {
                    /* attenuation curves */
                    src_curve     = current_light_attenuation_curves[n_curve];
                    dst_float_ptr = current_light_attenuation_floats + n_curve;
                }

                curve_container_get_value(src_curve,
                                          frame_time,
                                          false, /* should_force */
                                          temp_variant_float);
                system_variant_get_float (temp_variant_float,
                                          dst_float_ptr);
            }

            /* position curves */
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_POSITION,
                                     current_light_position_floats);

            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS,
                                              current_light_attenuation_floats);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION,
                                              current_light_position_floats);
        }
        else
        if (current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
        {
            scene_light_get_property         (current_light,
                                              SCENE_LIGHT_PROPERTY_DIRECTION,
                                              current_light_direction_floats);
            ogl_uber_set_shader_item_property(material_uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION,
                                              current_light_direction_floats);
        }
    } /* for (all lights) */
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
        scene_renderer_ptr->bbox_preview     = ogl_scene_renderer_bbox_preview_create(context,
                                                                                      scene,
                                                                                      (ogl_scene_renderer) scene_renderer_ptr);
        scene_renderer_ptr->context          = context;
        scene_renderer_ptr->frustum_preview  = ogl_scene_renderer_frustum_preview_create(context);
        scene_renderer_ptr->lights_preview   = ogl_scene_renderer_lights_preview_create (context,
                                                                                         scene);
        scene_renderer_ptr->material_manager = NULL;
        scene_renderer_ptr->normals_preview  = ogl_scene_renderer_normals_preview_create(context,
                                                                                         scene,
                                                                                         (ogl_scene_renderer) scene_renderer_ptr);
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

        /* Assign existing cameras to the call-back */
        _ogl_scene_renderer_update_frustum_preview_assigned_cameras(scene_renderer_ptr);

        /* Since ogl_scene_renderer caches ogl_uber instances, given current scene configuration,
         * we need to register for various scene call-backs in order to ensure these instances
         * are reset, if the scene configuration ever changes.
         */
        system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                        SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _ogl_scene_renderer_on_light_added,
                                                        scene_renderer_ptr);                /* callback_proc_user_arg */
    }

    return (ogl_scene_renderer) scene_renderer_ptr;
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
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve model matrix for mesh id [%d]",
                                  index);
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
        case OGL_SCENE_RENDERER_PROPERTY_GRAPH:
        {
            scene_get_property(renderer_ptr->scene,
                               SCENE_PROPERTY_GRAPH,
                               out_result);

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
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_render_scene_graph(__in           __notnull ogl_scene_renderer                             renderer,
                                                                         __in           __notnull system_matrix4x4                               view,
                                                                         __in           __notnull system_matrix4x4                               projection,
                                                                         __in           __notnull scene_camera                                   camera,
                                                                         __in_ecount(3) __notnull const float*                                   camera_location,   /* TODO: MOVE OUT */
                                                                         __in                     const _ogl_scene_renderer_render_mode&         render_mode,
                                                                         __in                     const _ogl_scene_renderer_shadow_mapping_type& shadow_mapping,
                                                                         __in           __notnull _ogl_scene_renderer_helper_visualization       helper_visualization,
                                                                         __in                     system_timeline_time                           frame_time)
{
    _ogl_scene_renderer*              renderer_ptr   = (_ogl_scene_renderer*) renderer;
    const ogl_context_gl_entrypoints* entry_points   = NULL;
    scene_graph                       graph          = NULL;
    ogl_materials                     materials      = NULL;
    unsigned int                      n_scene_lights = 0;

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

    /* Prepare graph traversal parameters */
    system_matrix4x4 vp = system_matrix4x4_create_by_mul(projection,
                                                         view);

    renderer_ptr->current_aabb_max[0] = DEFAULT_AABB_MAX_VALUE;
    renderer_ptr->current_aabb_max[1] = DEFAULT_AABB_MAX_VALUE;
    renderer_ptr->current_aabb_max[2] = DEFAULT_AABB_MAX_VALUE;
    renderer_ptr->current_aabb_min[0] = DEFAULT_AABB_MIN_VALUE;
    renderer_ptr->current_aabb_min[1] = DEFAULT_AABB_MIN_VALUE;
    renderer_ptr->current_aabb_min[2] = DEFAULT_AABB_MIN_VALUE;

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

    if (shadow_mapping != SHADOW_MAPPING_TYPE_DISABLED)
    {
        scene_set_property(renderer_ptr->scene,
                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                          &shadow_mapping_disabled);

        /* For the call below, in order to support distant lights,
         * we need to know what the AABB (expressed in the world space)
         * for the scene, as seen from the camera viewpoint, is. */
        scene_graph_traverse(graph,
                             _ogl_scene_renderer_new_model_matrix,
                             NULL, /* insert_camera_proc */
                             NULL, /* insert_light_proc  */
                             _ogl_scene_renderer_process_mesh_for_shadow_map_pre_pass,
                             renderer,
                             frame_time);

        /* Proceed with shadow map generation even if no meshes are in the range.
         * The SM still needs to be cleared! */
        if (renderer_ptr->current_aabb_max[0] == DEFAULT_AABB_MAX_VALUE ||
            renderer_ptr->current_aabb_min[0] == DEFAULT_AABB_MIN_VALUE)
        {
            memset(renderer_ptr->current_aabb_max,
                   0,
                   sizeof(renderer_ptr->current_aabb_max) );

            memset(renderer_ptr->current_aabb_min,
                   0,
                   sizeof(renderer_ptr->current_aabb_min) );
        }

        /* Carry on with the shadow map preparation */
        _ogl_scene_renderer_render_shadow_maps(renderer,
                                               camera,
                                               shadow_mapping,
                                               frame_time);

        scene_set_property(renderer_ptr->scene,
                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                          &shadow_mapping_enabled);

        /* The _ogl_scene_renderer_render_shadow_maps() call might have messed up
         * projection, view & vp matrices we set up.
         *
         * Revert the original settings */
        renderer_ptr->current_projection = projection;
        renderer_ptr->current_view       = view;
        renderer_ptr->current_vp         = vp;
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
                         _ogl_scene_renderer_new_model_matrix,
                         NULL, /* insert_camera_proc */
                         _ogl_scene_renderer_update_light_properties,
                         (render_mode == RENDER_MODE_NO_RASTERIZATION) ? _ogl_scene_renderer_process_mesh_for_no_rasterization_rendering
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
        entry_points->pGLEnable(GL_FRAMEBUFFER_SRGB);
    }

    /* 2. Start uber rendering. Issue as many render requests as there are materials.
     *
     *    TODO: We could obviously expand this by performing more complicated sorting
     *          but this will do for now.
     */
    system_hash64             material_hash     = 0;
    ogl_uber                  material_uber     = NULL;
    uint32_t                  n_iteration_items = 0;
    uint32_t                  n_iterations      = 0;
    _ogl_scene_renderer_uber* uber_details_ptr  = NULL;

    if (render_mode == RENDER_MODE_NO_RASTERIZATION)
    {
        mesh_material dummy_material = ogl_materials_get_special_material(materials,
                                                                          SPECIAL_MATERIAL_DUMMY);

        material_uber     = mesh_material_get_ogl_uber                    (dummy_material,
                                                                           renderer_ptr->scene);
        n_iterations      = 1;
        n_iteration_items = system_resizable_vector_get_amount_of_elements(renderer_ptr->current_no_rasterization_meshes);
    }
    else
    {
        n_iterations = system_hash64map_get_amount_of_elements(renderer_ptr->ubers_map);
    }

    for (uint32_t n_iteration = 0;
                  n_iteration < n_iterations;
                ++n_iteration)
    {
        /* Retrieve ogl_uber instance */
        if (render_mode == RENDER_MODE_NO_RASTERIZATION)
        {
            /* Nothing to do - n_iteration_items is already set */
        }
        else
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

        /* Update ogl_uber's camera location and vp properties */
        if (camera_location != NULL)
        {
            ogl_uber_set_shader_general_property(material_uber,
                                                 OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,
                                                 camera_location);
        }

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
                if (render_mode == RENDER_MODE_NO_RASTERIZATION)
                {
                    system_resizable_vector_get_element_at(renderer_ptr->current_no_rasterization_meshes,
                                                           n_iteration_item,
                                                          &item_ptr);
                }
                else
                {
                    system_resizable_vector_get_element_at(uber_details_ptr->meshes,
                                                           n_iteration_item,
                                                          &item_ptr);
                }

                ogl_uber_rendering_render_mesh(item_ptr->mesh_instance,
                                               item_ptr->model_matrix,
                                               item_ptr->normal_matrix,
                                               material_uber,
                                               (render_mode != RENDER_MODE_NO_RASTERIZATION) ? item_ptr->material : NULL,
                                               frame_time);
            }
        }
        ogl_uber_rendering_stop(material_uber);

        /* Any helper visualization, handle it at this point */
        if (render_mode != RENDER_MODE_NO_RASTERIZATION)
        {
            if (helper_visualization & HELPER_VISUALIZATION_BOUNDING_BOXES && n_iteration_items > 0)
            {
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
        } /* if (render_mode != RENDER_MODE_NO_RASTERIZATION) */

        /* Clean up */
        system_resizable_vector   mesh_item_vector = (render_mode == RENDER_MODE_NO_RASTERIZATION) ? renderer_ptr->current_no_rasterization_meshes
                                                                                                   : uber_details_ptr->meshes;
        _ogl_scene_renderer_mesh* mesh_ptr         = NULL;

        while (system_resizable_vector_pop(mesh_item_vector,
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
        ogl_scene_renderer_frustum_preview_render(renderer_ptr->frustum_preview,
                                                  frame_time,
                                                  vp);
    }

    if (helper_visualization & HELPER_VISUALIZATION_LIGHTS)
    {
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

    /* 3. Clean up in anticipation for the next call. We specifically do not cache any of the
     *    data because of the frustum culling which needs to be performed, every time model matrix
     *    or VP changes.
     **/
    if (helper_visualization != 0)
    {
        system_resource_pool_return_all_allocations(renderer_ptr->mesh_id_data_pool);
        system_hash64map_clear                     (renderer_ptr->mesh_id_map);
    }

    if (shadow_mapping != SHADOW_MAPPING_TYPE_DISABLED)
    {
        /* Release any shadow maps we might've allocated back to the texture pool */
        _ogl_scene_renderer_return_shadow_maps_to_pool(renderer);
    }

    /* All done! Good to shut down the show */
    if (render_mode == RENDER_MODE_FORWARD)
    {
        entry_points->pGLDisable(GL_FRAMEBUFFER_SRGB);
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
