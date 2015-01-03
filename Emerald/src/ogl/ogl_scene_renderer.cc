/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_bbox_preview.h"
#include "ogl/ogl_scene_renderer_normals_preview.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
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

/* Forward declarations */
PRIVATE void _ogl_scene_renderer_deinit_cached_ubers_map_contents         (__in __notnull system_hash64map           cached_materials_map);
PRIVATE void _ogl_scene_renderer_deinit_resizable_vector_for_resource_pool(               system_resource_pool_block);
PRIVATE void _ogl_scene_renderer_init_resizable_vector_for_resource_pool  (               system_resource_pool_block);

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
    ogl_scene_renderer_normals_preview normals_preview;

    ogl_context    context;
    ogl_materials  material_manager;
    scene          scene;
    system_variant temp_variant_float;

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
        bbox_preview             = NULL;
        context                  = NULL;
        current_model_matrix     = system_matrix4x4_create();
        mesh_id_data_pool        = system_resource_pool_create(sizeof(_ogl_scene_renderer_mesh_id_data),
                                                               4,     /* n_elements_to_preallocate */
                                                               NULL,  /* init_fn */
                                                               NULL); /* deinit_fn */
        mesh_id_map              = system_hash64map_create    (sizeof(_ogl_scene_renderer_mesh_id_data*) );
        normals_preview          = NULL;
        scene_renderer_mesh_pool = system_resource_pool_create(sizeof(_ogl_scene_renderer_mesh),
                                                               4,     /* n_elements_to_preallocate */
                                                               NULL,  /* init_fn */
                                                               NULL); /* deinit_fn */
        ubers_map                = system_hash64map_create    (sizeof(_ogl_scene_renderer_uber*) );
        vector_pool              = system_resource_pool_create(sizeof(system_resizable_vector),
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
 */
PRIVATE bool _ogl_scene_renderer_frustum_cull(__in __notnull ogl_scene_renderer renderer,
                                              __in __notnull mesh               mesh_gpu)
{
    bool result = true;

    /* Retrieve AABB for the mesh */
    const float*               aabb_max_ptr   = NULL;
    const float*               aabb_min_ptr   = NULL;
    const _ogl_scene_renderer* renderer_ptr   = (const _ogl_scene_renderer*) renderer;

    mesh_get_property      (mesh_gpu,
                            MESH_PROPERTY_AABB_MAX,
                           &aabb_max_ptr);
    mesh_get_property      (mesh_gpu,
                            MESH_PROPERTY_AABB_MIN,
                           &aabb_min_ptr);

    /* Transform the model-space AABB to world-space */
    float transformed_aabb_min[3];
    float transformed_aabb_max[3];

    for (uint32_t n_vertex = 0;
                  n_vertex < 8;
                ++n_vertex)
    {
        float transformed_vertex[4];
        float vertex[4] =
        {
            (n_vertex & 1) ? aabb_max_ptr[0] : aabb_min_ptr[0],
            (n_vertex & 2) ? aabb_max_ptr[1] : aabb_min_ptr[1],
            (n_vertex & 4) ? aabb_max_ptr[2] : aabb_min_ptr[2],
            1.0f
        };

        system_matrix4x4_multiply_by_vector4(renderer_ptr->current_model_matrix,
                                             vertex,
                                             transformed_vertex);

        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (n_vertex == 0 ||
                n_vertex != 0 && transformed_aabb_min[n] > transformed_vertex[n])
            {
                transformed_aabb_min[n] = transformed_vertex[n];
            }

            if (n_vertex == 0 ||
                n_vertex != 0 && transformed_aabb_max[n] < transformed_vertex[n])
            {
                transformed_aabb_max[n] = transformed_vertex[n];
            }
        }
    }

    /* Retrieve clipping planes from the MVP */
    float        clipping_plane_bottom[4];
    float        clipping_plane_front [4];
    float        clipping_plane_left  [4];
    float        clipping_plane_near  [4];
    float        clipping_plane_right [4];
    float        clipping_plane_top   [4];
    const float* clipping_planes      [] =
    {
        clipping_plane_bottom,
        clipping_plane_front,
        clipping_plane_left,
        clipping_plane_near,
        clipping_plane_right,
        clipping_plane_top
    };
    const uint32_t n_clipping_planes = sizeof(clipping_planes) / sizeof(clipping_planes[0]);

    system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                        SYSTEM_MATRIX4X4_CLIPPING_PLANE_BOTTOM,
                                        clipping_plane_bottom);
    system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                        SYSTEM_MATRIX4X4_CLIPPING_PLANE_FAR,
                                        clipping_plane_front);
    system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                        SYSTEM_MATRIX4X4_CLIPPING_PLANE_LEFT,
                                        clipping_plane_left);
    system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                        SYSTEM_MATRIX4X4_CLIPPING_PLANE_NEAR,
                                        clipping_plane_near);
    system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                        SYSTEM_MATRIX4X4_CLIPPING_PLANE_RIGHT,
                                        clipping_plane_right);
    system_matrix4x4_get_clipping_plane(renderer_ptr->current_vp,
                                        SYSTEM_MATRIX4X4_CLIPPING_PLANE_TOP,
                                        clipping_plane_top);

    /* Iterate over all clipping planes.. */
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
        float clipping_plane_normal_length = sqrt(clipping_plane_normal[0] * clipping_plane_normal[0] +
                                                  clipping_plane_normal[1] * clipping_plane_normal[1] +
                                                  clipping_plane_normal[2] * clipping_plane_normal[2]);

        clipping_plane_normal[0] /= clipping_plane_normal_length;
        clipping_plane_normal[1] /= clipping_plane_normal_length;
        clipping_plane_normal[2] /= clipping_plane_normal_length;

#if USE_BRUTEFORCE_METHOD
        clipping_plane_normal[3] /= clipping_plane_normal_length; /* NOTE: this is needed for proper point-plane distance calculation */

        for (uint32_t n_vertex = 0;
                      n_vertex < 8 && (counter_inside == 0);
                    ++n_vertex)
        {
            float vertex[3] =
            {
                (n_vertex & 1) ? transformed_aabb_max[0] : transformed_aabb_min[0],
                (n_vertex & 2) ? transformed_aabb_max[1] : transformed_aabb_min[1],
                (n_vertex & 4) ? transformed_aabb_max[2] : transformed_aabb_min[2]
            };

            /* Is the vertex inside? */
            float distance = clipping_plane_normal[3] + ((vertex[0] * clipping_plane_normal[0]) +
                                                         (vertex[1] * clipping_plane_normal[1]) +
                                                         (vertex[2] * clipping_plane_normal[2]) );

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
        if (counter_outside)
        {
            /* Intersection, need to render */
            goto end;
        }
#else
        /* Get negative & positive vertex data */
        float negative_vertex[3] =
        {
            transformed_aabb_max[0],
            transformed_aabb_max[1],
            transformed_aabb_max[2]
        };
        float positive_vertex[3] =
        {
            transformed_aabb_min[0],
            transformed_aabb_min[1],
            transformed_aabb_min[2]
        };

        if (clipping_plane[0] >= 0.0f)
        {
            negative_vertex[0] = transformed_aabb_min[0];
            positive_vertex[0] = transformed_aabb_max[0];
        }
        if (clipping_plane[1] >= 0.0f)
        {
            negative_vertex[1] = transformed_aabb_min[1];
            positive_vertex[1] = transformed_aabb_max[1];
        }
        if (clipping_plane[2] >= 0.0f)
        {
            negative_vertex[2] = transformed_aabb_min[2];
            positive_vertex[2] = transformed_aabb_max[2];
        }

        /* Is the positive vertex outside? */
        float distance = clipping_plane[3] + ((positive_vertex[0] * clipping_plane[0]) +
                                              (positive_vertex[1] * clipping_plane[1]) +
                                              (positive_vertex[2] * clipping_plane[2]) );

        if (distance < 0)
        {
            result = false;

            goto end;
        }

        /* Is the negative vertex outside? */
        distance = clipping_plane[3] + ((negative_vertex[0] * clipping_plane[0]) +
                                        (negative_vertex[1] * clipping_plane[1]) +
                                        (negative_vertex[2] * clipping_plane[2]) );

        if (distance < 0)
        {
            goto end;
        }
#endif /* USE_BRUTEFORCE_METHOD */
    } /* for (all clipping planes) */

end:
    /* All done */
    return result;
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

    /* Reset all cached ubers */
    system_hash64map_clear(renderer_ptr->ubers_map);
}

/** TODO */
PRIVATE void _ogl_scene_renderer_process_mesh(__notnull scene_mesh scene_mesh_instance,
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

    system_hashed_ansi_string test = NULL;
    scene_mesh_get_property(scene_mesh_instance, SCENE_MESH_PROPERTY_NAME, &test);

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
             *       This can be easily caught when running Emerald in debug build
             *       (an assertion failure will occur). FIX WHEN NEEDED.
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
PRIVATE void _ogl_scene_renderer_new_model_matrix(__in __notnull system_matrix4x4 transformation_matrix,
                                                  __in __notnull void*            renderer)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;

    system_matrix4x4_set_from_matrix4x4(renderer_ptr->current_model_matrix,
                                        transformation_matrix);
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

        final_light_position[2] *= -1;

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
        scene_renderer_ptr->material_manager = NULL;
        scene_renderer_ptr->normals_preview  = ogl_scene_renderer_normals_preview_create(context,
                                                                                         scene,
                                                                                         (ogl_scene_renderer) scene_renderer_ptr);
        scene_renderer_ptr->scene            = scene;

        scene_retain(scene);

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_MATERIALS,
                                &scene_renderer_ptr->material_manager);

        /* Since ogl_scene_renderer caches ogl_uber instances, given current scene configuration,
         * we need to register for various scene call-backs in order to ensure these instances
         * are reset, if the scene configuration ever changes.
         */
        system_callback_manager scene_callback_manager = NULL;

        scene_get_property(scene,
                           SCENE_PROPERTY_CALLBACK_MANAGER,
                          &scene_callback_manager);

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
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_render_scene_graph(__in           __notnull ogl_scene_renderer                       renderer,
                                                                         __in           __notnull system_matrix4x4                         view,
                                                                         __in           __notnull system_matrix4x4                         projection,
                                                                         __in_ecount(3) __notnull const float*                             camera_location,
                                                                         __in                     const _ogl_scene_renderer_render_mode&   render_mode,
                                                                         __in           __notnull _ogl_scene_renderer_helper_visualization helper_visualization,
                                                                         __in                     system_timeline_time                     frame_time)
{
    _ogl_scene_renderer* renderer_ptr = (_ogl_scene_renderer*) renderer;
    scene_graph          graph        = NULL;
    ogl_materials        materials    = NULL;

    scene_get_property      (renderer_ptr->scene,
                             SCENE_PROPERTY_GRAPH,
                            &graph);
    ogl_context_get_property(renderer_ptr->context,
                             OGL_CONTEXT_PROPERTY_MATERIALS,
                            &materials);

    /* 1. Traverse the scene graph and:
     *
     *    - update light direction vectors.
     *    - determine mesh visibility via frustum culling.
     *
     *  NOTE: We cache 'helper visualization' setting, as the helper rendering requires
     *        additional information that needs not be stored otherwise.
     */
    system_matrix4x4 vp = system_matrix4x4_create_by_mul(projection, view);

    renderer_ptr->current_projection           = projection;
    renderer_ptr->current_view                 = view;
    renderer_ptr->current_vp                   = vp;
    renderer_ptr->current_helper_visualization = helper_visualization;

    scene_graph_traverse(graph,
                         _ogl_scene_renderer_new_model_matrix,
                         NULL, /* insert_camera_proc */
                         _ogl_scene_renderer_update_light_properties,
                         _ogl_scene_renderer_process_mesh,
                         renderer,
                         frame_time);

    /* 2. Start uber rendering. Issue as many render requests as there are materials.
     *
     *    TODO: We could obviously expand this by performing more complicated sorting
     *          but this will do for now.
     */
    system_hash64             material_hash    = 0;
    ogl_uber                  material_uber    = NULL;
    const uint32_t            n_cached_ubers   = system_hash64map_get_amount_of_elements(renderer_ptr->ubers_map);
    _ogl_scene_renderer_uber* uber_details_ptr = NULL;

    for (uint32_t n_uber = 0;
                  n_uber < n_cached_ubers;
                ++n_uber)
    {
        system_hash64map_get_element_at(renderer_ptr->ubers_map,
                                        n_uber,
                                        &uber_details_ptr,
                                        &material_hash);

        material_uber = (ogl_uber) material_hash;

        switch (render_mode)
        {
            case RENDER_MODE_REGULAR:
            {
                /* Do not over-write the uber instance! */

                break;
            }

            case RENDER_MODE_NORMALS_ONLY:
            {
                material_uber = mesh_material_get_ogl_uber(ogl_materials_get_special_material(materials,
                                                                                              SPECIAL_MATERIAL_NORMALS),
                                                           renderer_ptr->scene);

                break;
            }

            case RENDER_MODE_TEXCOORDS_ONLY:
            {
                material_uber = mesh_material_get_ogl_uber(ogl_materials_get_special_material(materials,
                                                                                              SPECIAL_MATERIAL_TEXCOORD),
                                                           renderer_ptr->scene);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized render mode");
            }
        } /* switch (render_mode) */

        ASSERT_DEBUG_SYNC(material_uber != NULL,
                          "No ogl_uber instance available for mesh material!");

        ogl_uber_set_shader_general_property(material_uber,
                                             OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,
                                             camera_location);
        ogl_uber_set_shader_general_property(material_uber,
                                             OGL_UBER_GENERAL_PROPERTY_VP,
                                             vp);

        /* Don't forget to update light properties */
        unsigned int n_uber_items   = 0;
        unsigned int n_scene_lights = 0;

        scene_get_property(renderer_ptr->scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_scene_lights);

        ogl_uber_get_shader_general_property(material_uber,
                                             OGL_UBER_GENERAL_PROPERTY_N_ITEMS,
                                            &n_uber_items);

        if (render_mode == RENDER_MODE_REGULAR)
        {
            ASSERT_DEBUG_SYNC(n_uber_items == n_scene_lights,
                              "ogl_scene_renderer needs to be re-initialized, as the light dconfiguration has changed");

            for (unsigned int n_light = 0;
                              n_light < n_scene_lights;
                            ++n_light)
            {
                scene_light      current_light                       = scene_get_light_by_index(renderer_ptr->scene,
                                                                                               n_light);
                scene_light_type current_light_type                  = SCENE_LIGHT_TYPE_UNKNOWN;
                curve_container  current_light_attenuation_curves[3];
                float            current_light_attenuation_floats[3];
                curve_container  current_light_color_curves      [3];
                curve_container  current_light_color_intensity_curve;
                float            current_light_color_floats      [3];
                float            current_light_color_intensity = 0.0f;
                float            current_light_direction_floats  [3];
                float            current_light_position_floats   [3];

                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_TYPE,
                                        &current_light_type);

                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_COLOR,
                                         current_light_color_curves);
                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_COLOR_INTENSITY,
                                        &current_light_color_intensity_curve);

                curve_container_get_value(current_light_color_intensity_curve,
                                          frame_time,
                                          false, /* should_force */
                                          renderer_ptr->temp_variant_float);
                system_variant_get_float (renderer_ptr->temp_variant_float,
                                         &current_light_color_intensity);

                for (unsigned int n_color_component = 0;
                                  n_color_component < 3;
                                ++n_color_component)
                {
                    curve_container_get_value(current_light_color_curves[n_color_component],
                                              frame_time,
                                              false, /* should_force */
                                              renderer_ptr->temp_variant_float);
                    system_variant_get_float (renderer_ptr->temp_variant_float,
                                              current_light_color_floats + n_color_component);

                    current_light_color_floats[n_color_component] *= current_light_color_intensity;
                }

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
                                                  renderer_ptr->temp_variant_float);
                        system_variant_get_float (renderer_ptr->temp_variant_float,
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
        } /* if (render_mode == RENDER_MODE_REGULAR) */

        /* Okay. Go on with the rendering */
        const uint32_t n_vector_items = system_resizable_vector_get_amount_of_elements(uber_details_ptr->meshes);

        ogl_uber_rendering_start(material_uber);
        {
            _ogl_scene_renderer_mesh* item_ptr = NULL;

            for (uint32_t n_vector_item = 0;
                          n_vector_item < n_vector_items;
                        ++n_vector_item)
            {
                system_resizable_vector_get_element_at(uber_details_ptr->meshes,
                                                       n_vector_item,
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
        if (helper_visualization & HELPER_VISUALIZATION_BOUNDING_BOXES)
        {
            if (n_vector_items > 0)
            {
                ogl_scene_renderer_bbox_preview_start(renderer_ptr->bbox_preview,
                                                      vp);
                {
                    _ogl_scene_renderer_mesh* item_ptr = NULL;

                    for (uint32_t n_vector_item = 0;
                                  n_vector_item < n_vector_items;
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
            }
        }

        if (helper_visualization & HELPER_VISUALIZATION_NORMALS)
        {
            if (n_vector_items > 0)
            {
                ogl_scene_renderer_normals_preview_start(renderer_ptr->normals_preview,
                                                         vp);
                {
                    _ogl_scene_renderer_mesh* item_ptr = NULL;

                    for (uint32_t n_vector_item = 0;
                                  n_vector_item < n_vector_items;
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
            }
        }

        /* Clean up */
        _ogl_scene_renderer_mesh* mesh_ptr = NULL;

        while (system_resizable_vector_pop(uber_details_ptr->meshes,
                                          &mesh_ptr) )
        {
            system_matrix4x4_release(mesh_ptr->model_matrix);
            system_matrix4x4_release(mesh_ptr->normal_matrix);

            system_resource_pool_return_to_pool(renderer_ptr->scene_renderer_mesh_pool,
                                                (system_resource_pool_block) mesh_ptr);
        }
    } /* for (all ubers) */

    /* 3. Clean up in anticipation for the next call. We specifically do not cache any of the
     *    data because of the frustum culling which needs to be performed, every time model matrix
     *    or VP changes.
     **/
    if (helper_visualization != 0)
    {
        system_resource_pool_return_all_allocations(renderer_ptr->mesh_id_data_pool);
        system_hash64map_clear                     (renderer_ptr->mesh_id_map);
    }

    /* All done! Good to shut down the show */
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
