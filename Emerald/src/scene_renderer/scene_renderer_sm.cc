/**
 *
 * Emerald (kbi/elude @2015-2016)
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "demo/demo_app.h"
#include "glsl/glsl_shader_constructor.h"
#include "mesh/mesh.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_types.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_materials.h"
#include "scene_renderer/scene_renderer_sm.h"
#include "scene_renderer/scene_renderer_uber.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_variant.h"
#include "system/system_window.h"
#include <string>
#include <sstream>

#define N_MAX_BLUR_TAPS (16)
#define N_MIN_BLUR_TAPS (2)


/** TODO */
typedef struct
{

    mesh             mesh_instance;
    system_matrix4x4 model_matrix;

} _scene_renderer_sm_mesh_item;

/** TODO */
typedef struct _scene_renderer_sm
{
    /* Blur handler */
    postprocessing_blur_gaussian blur_handler;

    /* DO NOT retain/release. */
    ral_context context;

    /* Set by scene_renderer_sm_render_shadow_maps(). Stores scene_camera
     * instance, for which the SMs are to be rendered. */
    scene_camera current_camera;

    /** TODO */
    ral_command_buffer current_command_buffer;

    /** TODO */
    ral_gfx_state_create_info current_gfx_state_create_info;

    /** TODO */
    ral_command_buffer_set_viewport_command_info current_viewport;

    /* Set by scene_renderer_sm_render_shadow_maps(). Stores scene_light
     * instance, for which the SM is to be rendered. */
    scene_light current_light;

    /* Set by scene_renderer_sm_render_shadow_maps(). Stores info about the
     * target face the meshes are rendered to. This is used by subsequent
     * call to scene_renderer_sm_render_shadow_map_meshes() */
    scene_renderer_sm_target_face current_target_face;

    /* Used during pre-processing phase, called for each scene_renderer_render_scene_graph()
     * call with SM enabled.
     *
     * Holds meshes that should be rendered for the SM pass that follows.
     */
    system_resizable_vector meshes_to_render;

    /* A resource pool which holds _scene_renderer_sm_mesh_item items.
     *
     * Used for storing meshes which are visible from the camera PoV -
     * this is determined during pre-processing phase for each
     * scene_renderer_render_scene_graph() call with SM enabled.
     */
    system_resource_pool mesh_item_pool;

    /* A variant of FP type */
    system_variant temp_variant_float;

    /* Tells which texture view is currently being used as a zeroth color attachment for SM rendering.
     *
     * These only make sense while is_enabled is true.
     */
    ral_texture_view current_sm_color0_l0_texture_view;
    ral_texture_view current_sm_color0_l1_texture_view;
    ral_texture_view current_sm_color0_texture_view;

    /* Tells what ogl_texture is currently being used as a depth attachment for SM rendering.
     *
     * These only make sense while is_enabled is true.
     */
    ral_texture_view current_sm_depth_l0_texture_view;
    ral_texture_view current_sm_depth_l1_texture_view;
    ral_texture_view current_sm_depth_texture_view;

    /* Tells whether scene_renderer_sm has been toggled on.
     *
     * This is useful for point lights, for which we need to re-configure
     * the render-target at least twice to render a single shadow map.
     * When is_enabled is true and scene_renderer_sm_toggle() is called,
     * the SM texture set-up code is skipped.
     */
    bool is_enabled;

    ral_sampler pcf_color_sampler;
    ral_sampler pcf_depth_sampler;
    ral_sampler plain_color_sampler;
    ral_sampler plain_depth_sampler;

    _scene_renderer_sm()
    {
        blur_handler                      = nullptr;
        context                           = nullptr;
        current_camera                    = nullptr;
        current_command_buffer            = nullptr;
        current_light                     = nullptr;
        current_sm_color0_texture_view    = nullptr;
        current_sm_color0_l0_texture_view = nullptr;
        current_sm_color0_l1_texture_view = nullptr;
        current_sm_depth_texture_view     = nullptr;
        current_sm_depth_l0_texture_view  = nullptr;
        current_sm_depth_l1_texture_view  = nullptr;
        current_target_face               = SCENE_RENDERER_SM_TARGET_FACE_UNKNOWN;
        is_enabled                        = false;
        meshes_to_render                  = system_resizable_vector_create(16,     /* capacity              */
                                                                           false); /* should_be_thread_safe */
        mesh_item_pool                    = system_resource_pool_create   (sizeof(_scene_renderer_sm_mesh_item),
                                                                           64 ,      /* n_elements_to_preallocate */
                                                                           nullptr,  /* init_fn                   */
                                                                           nullptr); /* deinit_fn                 */
        pcf_color_sampler                 = nullptr;
        pcf_depth_sampler                 = nullptr;
        plain_color_sampler               = nullptr;
        plain_depth_sampler               = nullptr;
        temp_variant_float                = system_variant_create(SYSTEM_VARIANT_FLOAT);
    }

    ~_scene_renderer_sm()
    {
        ASSERT_DEBUG_SYNC(current_command_buffer == nullptr,
                          "An active command buffer was found at _scene_renderer_sm destruction time.");

        if (blur_handler != nullptr)
        {
            postprocessing_blur_gaussian_release(blur_handler);

            blur_handler = nullptr;
        }

        if (meshes_to_render != nullptr)
        {
            system_resizable_vector_release(meshes_to_render);

            meshes_to_render = nullptr;
        }

        if (mesh_item_pool != nullptr)
        {
            system_resource_pool_release(mesh_item_pool);

            mesh_item_pool = nullptr;
        }

        if (temp_variant_float != nullptr)
        {
            system_variant_release(temp_variant_float);

            temp_variant_float = nullptr;
        }
    }
} _scene_renderer_sm;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _scene_renderer_sm_verify_context_type(ogl_context);
#else
    #define _scene_renderer_sm_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _scene_renderer_sm_add_bias_variable_to_fragment_uber(std::stringstream&               code_snippet_sstream,
                                                                   const uint32_t                   n_light,
                                                                   scene_light_shadow_map_bias      light_sm_bias,
                                                                   scene_light_shadow_map_algorithm sm_algo,
                                                                   system_hashed_ansi_string*       out_light_bias_var_name_has)
{
    /* Create the new code snippet */
    std::stringstream light_bias_var_name_sstream;

    light_bias_var_name_sstream << "light_"
                                << n_light
                                << "_bias";

    *out_light_bias_var_name_has = system_hashed_ansi_string_create(light_bias_var_name_sstream.str().c_str() );

    code_snippet_sstream << "float "
                         << light_bias_var_name_sstream.str()
                         << " = ";

    switch (light_sm_bias)
    {
        case SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE:
        {
            code_snippet_sstream << "clamp(0.001 * tan(acos(light"
                                 << n_light
                                 << "_LdotN_clamped)), 0.0, 1.0);\n";

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE_FAST:
        {
            code_snippet_sstream << "0.001 * acos(clamp(light"
                                 << n_light
                                 << "_LdotN_clamped, 0.0, 1.0));\n";

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_CONSTANT:
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO");

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_NONE:
        {
            code_snippet_sstream << "0.0;\n";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized shadow map bias requested");
        }
    }
}

/** TODO */
PRIVATE void _scene_renderer_sm_add_constructor_variable(glsl_shader_constructor                  constructor,
                                                         uint32_t                                 n_light,
                                                         system_hashed_ansi_string                var_suffix,
                                                         glsl_shader_constructor_variable_type    var_type,
                                                         glsl_shader_constructor_layout_qualifier layout_qualifier,
                                                         ral_program_variable_type                shader_var_type,
                                                         glsl_shader_constructor_uniform_block_id ub_id,
                                                         system_hashed_ansi_string*               out_var_name_ptr)
{
    bool              is_var_added = false;
    std::stringstream var_name_sstream;

    var_name_sstream << "light"
                     << n_light
                     << "_"
                     << system_hashed_ansi_string_get_buffer(var_suffix);

    *out_var_name_ptr = system_hashed_ansi_string_create                 (var_name_sstream.str().c_str() );
    is_var_added      = glsl_shader_constructor_is_general_variable_in_ub(constructor,
                                                                          0, /* uniform_block */
                                                                          *out_var_name_ptr);

    ASSERT_DEBUG_SYNC(!is_var_added,
                      "Variable already added!");

    glsl_shader_constructor_add_general_variable_to_ub(constructor,
                                                       var_type,
                                                       layout_qualifier,
                                                       shader_var_type,
                                                       0,     /* array_size */
                                                       ub_id,
                                                      *out_var_name_ptr,
                                                       nullptr); /* out_variable_id */
}

/** TODO */
PRIVATE void _scene_renderer_sm_add_uniforms_to_fragment_uber_for_non_point_light(glsl_shader_constructor                  constructor,
                                                                                  glsl_shader_constructor_uniform_block_id ub_fs,
                                                                                  scene_light                              light,
                                                                                  uint32_t                                 n_light,
                                                                                  system_hashed_ansi_string*               out_light_shadow_coord_var_name_has_ptr,
                                                                                  system_hashed_ansi_string*               out_shadow_map_color_sampler_var_name_has_ptr,
                                                                                  system_hashed_ansi_string*               out_shadow_map_depth_sampler_var_name_has_ptr,
                                                                                  system_hashed_ansi_string*               out_vsm_cutoff_var_name_has_ptr,
                                                                                  system_hashed_ansi_string*               out_vsm_min_variance_var_name_has_ptr)
{
    /* Add the light-specific shadow coordinate input variable.
     *
     * shadow_coord is only used for non-point lights. see _adjust_vertex_uber_code() for details.
     */
    _scene_renderer_sm_add_constructor_variable(constructor,
                                                n_light,
                                                system_hashed_ansi_string_create("shadow_coord"),
                                                VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                LAYOUT_QUALIFIER_NONE,
                                                RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                0, /* ub_id */
                                                out_light_shadow_coord_var_name_has_ptr);

    /* Add the shadow map texture sampler.
     *
     * For plain shadow mapping, we go with a shadow texture.
     * For VSM, which uses a 2-channel color texture to hold the depth data, we use a regular 2D sampler.
     */
    scene_light_type                 light_type   = SCENE_LIGHT_TYPE_UNKNOWN;
    scene_light_shadow_map_algorithm sm_algorithm = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);
    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &sm_algorithm);

    switch (sm_algorithm)
    {
        case SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN:
        {
            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("shadow_map_depth"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_NONE,
                                                        RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_SHADOW,
                                                        0, /* ub_id */
                                                        out_shadow_map_depth_sampler_var_name_has_ptr);

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM:
        {
            ASSERT_DEBUG_SYNC(light_type != SCENE_LIGHT_TYPE_POINT,
                              "Point lights not allowed.");

            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("shadow_map_color"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_NONE,
                                                        RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D,
                                                        0, /* ub_id */
                                                        out_shadow_map_color_sampler_var_name_has_ptr);

            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("shadow_map_vsm_cutoff"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_NONE,
                                                        RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                        ub_fs,
                                                        out_vsm_cutoff_var_name_has_ptr);

            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("shadow_map_vsm_min_variance"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_NONE,
                                                        RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                        ub_fs,
                                                        out_vsm_min_variance_var_name_has_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light_shadow_map_algorithm value.");
        }
    }
}

/** TODO */
PRIVATE void _scene_renderer_sm_add_uniforms_to_fragment_uber_for_point_light(glsl_shader_constructor                  constructor,
                                                                              glsl_shader_constructor_uniform_block_id ub_fs,
                                                                              scene_light                              light,
                                                                              uint32_t                                 n_light,
                                                                              system_hashed_ansi_string*               out_light_far_near_diff_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_light_near_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_light_projection_matrix_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_light_view_matrix_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_shadow_map_color_sampler_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_shadow_map_depth_sampler_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_vsm_cutoff_var_name_has_ptr,
                                                                              system_hashed_ansi_string*               out_vsm_min_variance_var_name_has_ptr)
{
    scene_light_shadow_map_algorithm            sm_algorithm;
    scene_light_shadow_map_pointlight_algorithm sm_pl_algorithm;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &sm_algorithm);
    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                            &sm_pl_algorithm);

    /* VSM-specific uniforms */
    if (sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
    {
        _scene_renderer_sm_add_constructor_variable(constructor,
                                                    n_light,
                                                    system_hashed_ansi_string_create("shadow_map_vsm_cutoff"),
                                                    VARIABLE_TYPE_UNIFORM,
                                                    LAYOUT_QUALIFIER_NONE,
                                                    RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                    ub_fs,
                                                    out_vsm_cutoff_var_name_has_ptr);

        _scene_renderer_sm_add_constructor_variable(constructor,
                                                    n_light,
                                                    system_hashed_ansi_string_create("shadow_map_vsm_min_variance"),
                                                    VARIABLE_TYPE_UNIFORM,
                                                    LAYOUT_QUALIFIER_NONE,
                                                    RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                    ub_fs,
                                                    out_vsm_min_variance_var_name_has_ptr);
    }

    /* Uniforms we need are directly correlated to the light's point light SM algorithm */
    switch (sm_pl_algorithm)
    {
        case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL:
        {
            bool              is_light_projection_matrix_var_added = false;
            std::stringstream light_projection_matrix_var_name_sstream;

            /* Add light_projection matrix uniform */
            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("projection"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_ROW_MAJOR,
                                                        RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                        ub_fs, /* uniform_block */
                                                        out_light_projection_matrix_var_name_has_ptr);

            /* Add the shadow map texture sampler */
            if (sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
            {
                _scene_renderer_sm_add_constructor_variable(constructor,
                                                            n_light,
                                                            system_hashed_ansi_string_create("shadow_map_color"),
                                                            VARIABLE_TYPE_UNIFORM,
                                                            LAYOUT_QUALIFIER_NONE,
                                                            RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE,
                                                            0, /* ub_fs */
                                                            out_shadow_map_color_sampler_var_name_has_ptr);
            }
            else
            {
                _scene_renderer_sm_add_constructor_variable(constructor,
                                                            n_light,
                                                            system_hashed_ansi_string_create("shadow_map_depth"),
                                                            VARIABLE_TYPE_UNIFORM,
                                                            LAYOUT_QUALIFIER_NONE,
                                                            (sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN) ? RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_SHADOW
                                                                                                                     : RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE,
                                                            0, /* ub_fs */
                                                            out_shadow_map_depth_sampler_var_name_has_ptr);
            }

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID:
        {
            /* Add light_far_near_diff uniform */
            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("far_near_diff"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_NONE,
                                                        RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                        ub_fs, /* uniform_block */
                                                        out_light_far_near_diff_var_name_has_ptr);

            /* Add light_near uniform */
            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("near"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_NONE,
                                                        RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                        ub_fs, /* uniform_block */
                                                        out_light_near_var_name_has_ptr);

            /* Add light_view matrix uniform */
            _scene_renderer_sm_add_constructor_variable(constructor,
                                                        n_light,
                                                        system_hashed_ansi_string_create("view"),
                                                        VARIABLE_TYPE_UNIFORM,
                                                        LAYOUT_QUALIFIER_ROW_MAJOR,
                                                        RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                        ub_fs, /* uniform_block */
                                                        out_light_view_matrix_var_name_has_ptr);

            /* Add the shadow map texture sampler */
            if (sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
            {
                _scene_renderer_sm_add_constructor_variable(constructor,
                                                            n_light,
                                                            system_hashed_ansi_string_create("shadow_map_color"),
                                                            VARIABLE_TYPE_UNIFORM,
                                                            LAYOUT_QUALIFIER_NONE,
                                                            RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY,
                                                            0, /* ub_fs */
                                                            out_shadow_map_color_sampler_var_name_has_ptr);
            }
            else
            {
                _scene_renderer_sm_add_constructor_variable(constructor,
                                                            n_light,
                                                            system_hashed_ansi_string_create("shadow_map_depth"),
                                                            VARIABLE_TYPE_UNIFORM,
                                                            LAYOUT_QUALIFIER_NONE,
                                                            RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY_SHADOW,
                                                            0, /* uniform_block */
                                                            out_shadow_map_depth_sampler_var_name_has_ptr);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized point light SM algorithm");
        }
    }
}


/** TODO */
 PRIVATE void _scene_renderer_sm_get_aabb_for_camera_frustum_and_scene_aabb(const scene_camera current_camera,
                                                                            system_time        time,
                                                                            const float*       aabb_min_world,
                                                                            const float*       aabb_max_world,
                                                                            system_matrix4x4   view_matrix,
                                                                            float*             result_min_ptr,
                                                                            float*             result_max_ptr)
{
    /* Retrieve camera frustum data */
    float camera_frustum_vec4_fbl_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_fbr_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_ftl_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_ftr_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_nbl_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_nbr_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_ntl_world[4] = {0, 0, 0, 1.0f};
    float camera_frustum_vec4_ntr_world[4] = {0, 0, 0, 1.0f};

    {
        float            camera_frustum_vec4_fbl_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_fbr_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_ftl_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_ftr_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_nbl_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_nbr_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_ntl_model[4] = {0, 0, 0, 1.0f};
        float            camera_frustum_vec4_ntr_model[4] = {0, 0, 0, 1.0f};
        system_matrix4x4 current_camera_model_matrix      = nullptr;
        scene_graph_node current_camera_node              = nullptr;

        scene_camera_get_property    (current_camera,
                                      SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                      time,
                                     &current_camera_node);
        scene_graph_node_get_property(current_camera_node,
                                      SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                     &current_camera_model_matrix);

        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT,
                                  time,
                                  camera_frustum_vec4_fbl_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT,
                                  time,
                                  camera_frustum_vec4_fbr_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT,
                                  time,
                                  camera_frustum_vec4_ftl_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT,
                                  time,
                                  camera_frustum_vec4_ftr_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT,
                                  time,
                                  camera_frustum_vec4_nbl_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT,
                                  time,
                                  camera_frustum_vec4_nbr_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT,
                                  time,
                                  camera_frustum_vec4_ntl_model);
        scene_camera_get_property(current_camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT,
                                  time,
                                  camera_frustum_vec4_ntr_model);

        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_fbl_model,
                                             camera_frustum_vec4_fbl_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_fbr_model,
                                             camera_frustum_vec4_fbr_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_ftl_model,
                                             camera_frustum_vec4_ftl_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_ftr_model,
                                             camera_frustum_vec4_ftr_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_nbl_model,
                                             camera_frustum_vec4_nbl_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_nbr_model,
                                             camera_frustum_vec4_nbr_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_ntl_model,
                                             camera_frustum_vec4_ntl_world);
        system_matrix4x4_multiply_by_vector4(current_camera_model_matrix,
                                             camera_frustum_vec4_ntr_model,
                                             camera_frustum_vec4_ntr_world);
    }

    /* Transfer both camera frustum AABB *AND* visible scene AABB to the light's eye space */
    float obb_world_vec4_fbl_light[4];
    float obb_world_vec4_fbr_light[4];
    float obb_world_vec4_ftl_light[4];
    float obb_world_vec4_ftr_light[4];
    float obb_world_vec4_nbl_light[4];
    float obb_world_vec4_nbr_light[4];
    float obb_world_vec4_ntl_light[4];
    float obb_world_vec4_ntr_light[4];
    float obb_camera_frustum_vec4_fbl_light[4];
    float obb_camera_frustum_vec4_fbr_light[4];
    float obb_camera_frustum_vec4_ftl_light[4];
    float obb_camera_frustum_vec4_ftr_light[4];
    float obb_camera_frustum_vec4_nbl_light[4];
    float obb_camera_frustum_vec4_nbr_light[4];
    float obb_camera_frustum_vec4_ntl_light[4];
    float obb_camera_frustum_vec4_ntr_light[4];

    {
        const float world_vec4_fbl_world[4] = {aabb_min_world[0], aabb_min_world[1], aabb_max_world[2], 1.0f};
        const float world_vec4_fbr_world[4] = {aabb_max_world[0], aabb_min_world[1], aabb_max_world[2], 1.0f};
        const float world_vec4_ftl_world[4] = {aabb_min_world[0], aabb_max_world[1], aabb_max_world[2], 1.0f};
        const float world_vec4_ftr_world[4] = {aabb_max_world[0], aabb_max_world[1], aabb_max_world[2], 1.0f};
        const float world_vec4_nbl_world[4] = {aabb_min_world[0], aabb_min_world[1], aabb_min_world[2], 1.0f};
        const float world_vec4_nbr_world[4] = {aabb_max_world[0], aabb_min_world[1], aabb_min_world[2], 1.0f};
        const float world_vec4_ntl_world[4] = {aabb_min_world[0], aabb_max_world[1], aabb_min_world[2], 1.0f};
        const float world_vec4_ntr_world[4] = {aabb_max_world[0], aabb_max_world[1], aabb_min_world[2], 1.0f};

        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_fbl_world,
                                             obb_world_vec4_fbl_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_fbr_world,
                                             obb_world_vec4_fbr_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_ftl_world,
                                             obb_world_vec4_ftl_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_ftr_world,
                                             obb_world_vec4_ftr_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_nbl_world,
                                             obb_world_vec4_nbl_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_nbr_world,
                                             obb_world_vec4_nbr_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_ntl_world,
                                             obb_world_vec4_ntl_light);
        system_matrix4x4_multiply_by_vector4(view_matrix,
                                             world_vec4_ntr_world,
                                             obb_world_vec4_ntr_light);
    }

    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_fbl_world,
                                         obb_camera_frustum_vec4_fbl_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_fbr_world,
                                         obb_camera_frustum_vec4_fbr_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_ftl_world,
                                         obb_camera_frustum_vec4_ftl_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_ftr_world,
                                         obb_camera_frustum_vec4_ftr_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_nbl_world,
                                         obb_camera_frustum_vec4_nbl_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_nbr_world,
                                         obb_camera_frustum_vec4_nbr_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_ntl_world,
                                         obb_camera_frustum_vec4_ntl_light);
    system_matrix4x4_multiply_by_vector4(view_matrix,
                                         camera_frustum_vec4_ntr_world,
                                         obb_camera_frustum_vec4_ntr_light);

    /* This gives us coordinates of an OBB for both AABB that encompasses the visible
     * part of the scene *and* the camera frustum's AABB.
     *
     * Compute an AABB for the latter, first. */
    const float* obb_camera_frustum_vertices[]  =
    {
        obb_camera_frustum_vec4_fbl_light,
        obb_camera_frustum_vec4_fbr_light,
        obb_camera_frustum_vec4_ftl_light,
        obb_camera_frustum_vec4_ftr_light,
        obb_camera_frustum_vec4_nbl_light,
        obb_camera_frustum_vec4_nbr_light,
        obb_camera_frustum_vec4_ntl_light,
        obb_camera_frustum_vec4_ntr_light,
    };
    const uint32_t n_obb_camera_frustum_vertices = sizeof(obb_camera_frustum_vertices) / sizeof(obb_camera_frustum_vertices[0]);

    result_max_ptr[0] = obb_camera_frustum_vertices[0][0];
    result_max_ptr[1] = obb_camera_frustum_vertices[0][1];
    result_max_ptr[2] = obb_camera_frustum_vertices[0][2];
    result_min_ptr[0] = obb_camera_frustum_vertices[0][0];
    result_min_ptr[1] = obb_camera_frustum_vertices[0][1];
    result_min_ptr[2] = obb_camera_frustum_vertices[0][2];

    for (uint32_t n_obb_camera_frustum_vertex = 1; /* skip the first entry */
                  n_obb_camera_frustum_vertex < n_obb_camera_frustum_vertices;
                ++n_obb_camera_frustum_vertex)
    {
        if (result_max_ptr[0] < obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0])
        {
            result_max_ptr[0] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0];
        }
        if (result_min_ptr[0] > obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0])
        {
            result_min_ptr[0] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0];
        }

        if (result_max_ptr[1] < obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1])
        {
            result_max_ptr[1] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1];
        }
        if (result_min_ptr[1] > obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1])
        {
            result_min_ptr[1] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1];
        }

        if (result_max_ptr[2] < obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2])
        {
            result_max_ptr[2] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2];
        }
        if (result_min_ptr[2] > obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2])
        {
            result_min_ptr[2] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2];
        }
    }

    /* Intersect the AABB we came up with with the scene's span.
     *
     * This effectively truncates the min/max 3D coordinates we have to a BB
     * that really matters.
     */
    const float* obb_world_vertices[]  =
    {
        obb_world_vec4_fbl_light,
        obb_world_vec4_fbr_light,
        obb_world_vec4_ftl_light,
        obb_world_vec4_ftr_light,
        obb_world_vec4_nbl_light,
        obb_world_vec4_nbr_light,
        obb_world_vec4_ntl_light,
        obb_world_vec4_ntr_light,
    };
    const uint32_t n_obb_world_vertices = sizeof(obb_world_vertices) / sizeof(obb_world_vertices[0]);

    for (uint32_t n_obb_world_vertex = 0;
                  n_obb_world_vertex < n_obb_world_vertices;
                ++n_obb_world_vertex)
    {
        if (result_max_ptr[0] > obb_world_vertices[n_obb_world_vertex][0])
        {
            result_max_ptr[0] = obb_world_vertices[n_obb_world_vertex][0];
        }
        if (result_min_ptr[0] < obb_world_vertices[n_obb_world_vertex][0])
        {
            result_min_ptr[0] = obb_world_vertices[n_obb_world_vertex][0];
        }

        if (result_max_ptr[1] > obb_world_vertices[n_obb_world_vertex][1])
        {
            result_max_ptr[1] = obb_world_vertices[n_obb_world_vertex][1];
        }
        if (result_min_ptr[1] < obb_world_vertices[n_obb_world_vertex][1])
        {
            result_min_ptr[1] = obb_world_vertices[n_obb_world_vertex][1];
        }

        if (result_max_ptr[2] > obb_world_vertices[n_obb_world_vertex][2])
        {
            result_max_ptr[2] = obb_world_vertices[n_obb_world_vertex][2];
        }
        if (result_min_ptr[2] < obb_world_vertices[n_obb_world_vertex][2])
        {
            result_min_ptr[2] = obb_world_vertices[n_obb_world_vertex][2];
        }
    }

    /* Min/max values may have been shuffled. Make sure the order is in place. */
    if (result_min_ptr[0] > result_max_ptr[0])
    {
        float temp = result_max_ptr[0];

        result_max_ptr[0] = result_min_ptr[0];
        result_min_ptr[0] = temp;
    }
    if (result_min_ptr[1] > result_max_ptr[1])
    {
        float temp = result_max_ptr[1];

        result_max_ptr[1] = result_min_ptr[1];
        result_min_ptr[1] = temp;
    }
    if (result_min_ptr[2] > result_max_ptr[2])
    {
        float temp = result_max_ptr[2];

        result_max_ptr[2] = result_min_ptr[2];
        result_min_ptr[2] = temp;
    }

    ASSERT_DEBUG_SYNC(result_min_ptr[0] <= result_max_ptr[0] &&
                      result_min_ptr[1] <= result_max_ptr[1] &&
                      result_min_ptr[2] <= result_max_ptr[2],
                      "Something's nuts with the OBB min/max calcs");
}

/** TODO */
uint32_t _scene_renderer_sm_get_number_of_sm_passes(scene_light      light,
                                                    scene_light_type light_type)
{
    uint32_t result = 0;

    if (light_type == SCENE_LIGHT_TYPE_POINT)
    {
        scene_light_shadow_map_pointlight_algorithm light_point_sm_algorithm;

        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                &light_point_sm_algorithm);

        switch (light_point_sm_algorithm)
        {
            case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL:
            {
                result = 6;

                break;
            }

            case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID:
            {
                result = 2;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized point light SM algorithm");
            }
        }
    }
    else
    {
        result = 1;
    }

    return result;
}

/** TODO */
PRIVATE scene_renderer_sm_target_face _scene_renderer_sm_get_target_face_for_point_light(scene_light_shadow_map_pointlight_algorithm algorithm,
                                                                                         uint32_t                                    n_sm_pass)
{
    scene_renderer_sm_target_face result = SCENE_RENDERER_SM_TARGET_FACE_UNKNOWN;

    switch (n_sm_pass)
    {
        case 0:
        {
            if (algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL)
            {
                result = SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_X;
            }
            else
            {
                ASSERT_DEBUG_SYNC(algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID,
                                  "Unsupported point light SM algorithm");

                result = SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT;
            }

            break;
        }

        case 1:
        {
            if (algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL)
            {
                result = (scene_renderer_sm_target_face) (SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_X + 1);
            }
            else
            {
                ASSERT_DEBUG_SYNC(algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID,
                                  "Unsupported point light SM algorithm");

                result = SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR;
            }

            break;
        }

        case 2:
        case 3:
        case 4:
        case 5:
        {
            result = (scene_renderer_sm_target_face) (SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_X + n_sm_pass);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid SM pass index");
        }
    }

    return result;
}

/** TODO */
PRIVATE scene_renderer_sm_target_face _scene_renderer_sm_get_target_face_for_nonpoint_light(uint32_t n_sm_pass)
{
    scene_renderer_sm_target_face result = SCENE_RENDERER_SM_TARGET_FACE_UNKNOWN;

    switch (n_sm_pass)
    {
        case 0:
        {
            result = SCENE_RENDERER_SM_TARGET_FACE_2D;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid SM pass index");
        }
    }

    return result;
}

/** TODO */
PRIVATE void _scene_renderer_sm_get_texture_targets_from_target_face(scene_renderer_sm_target_face target_face,
                                                                     ral_texture_type*             out_texture_type_ptr,
                                                                     uint32_t*                     out_n_layer_ptr)
{
    switch (target_face)
    {
        case SCENE_RENDERER_SM_TARGET_FACE_2D:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_2D;
            *out_n_layer_ptr      = 0;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT:
        case SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_2D_ARRAY;
            *out_n_layer_ptr      = 0;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_X:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_CUBE_MAP;
            *out_n_layer_ptr      = 1;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_Y:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_CUBE_MAP;
            *out_n_layer_ptr      = 3;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_Z:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_CUBE_MAP;
            *out_n_layer_ptr      = 5;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_X:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_CUBE_MAP;
            *out_n_layer_ptr      = 0;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_Y:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_CUBE_MAP;
            *out_n_layer_ptr      = 2;

            break;
        }

        case SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_Z:
        {
            *out_texture_type_ptr = RAL_TEXTURE_TYPE_CUBE_MAP;
            *out_n_layer_ptr      = 4;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_renderer_sm_target_face value");
        }
    }
}

/** TODO */
PRIVATE void _scene_renderer_sm_deinit_samplers(_scene_renderer_sm* sm_ptr)
{
    const ral_sampler samplers[] =
    {
        sm_ptr->pcf_color_sampler,
        sm_ptr->pcf_depth_sampler,
        sm_ptr->plain_color_sampler,
        sm_ptr->plain_depth_sampler
    };
    const uint32_t n_samplers = sizeof(samplers) / sizeof(samplers[0]);

    ral_context_delete_objects(sm_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                               n_samplers,
                               (const void**) samplers);

    sm_ptr->pcf_color_sampler  = nullptr;
    sm_ptr->pcf_depth_sampler  = nullptr;
    sm_ptr->plain_color_sampler = nullptr;
    sm_ptr->plain_depth_sampler = nullptr;
}
/** TODO */
PRIVATE void _scene_renderer_sm_init_samplers(_scene_renderer_sm* sm_ptr)
{
    ral_color               border_color;
    ral_sampler_create_info pcf_color_sampler_create_info;
    ral_sampler_create_info pcf_depth_sampler_create_info;
    ral_sampler_create_info plain_color_sampler_create_info;
    ral_sampler_create_info plain_depth_sampler_create_info;

    /* TODO: This border color will not work with Vulkan back-end */
    border_color.data_type = RAL_COLOR_DATA_TYPE_FLOAT;
    border_color.f32[0]    = 1.0f;
    border_color.f32[1]    = 0.0f;
    border_color.f32[2]    = 0.0f;
    border_color.f32[3]    = 0.0f;

    pcf_color_sampler_create_info.border_color         = border_color;
    pcf_color_sampler_create_info.compare_mode_enabled = false;
    pcf_color_sampler_create_info.compare_op           = RAL_COMPARE_OP_ALWAYS;
    pcf_color_sampler_create_info.mag_filter           = RAL_TEXTURE_FILTER_LINEAR;
    pcf_color_sampler_create_info.min_filter           = RAL_TEXTURE_FILTER_LINEAR;
    pcf_color_sampler_create_info.mipmap_mode          = RAL_TEXTURE_MIPMAP_MODE_LINEAR;
    pcf_color_sampler_create_info.wrap_r               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    pcf_color_sampler_create_info.wrap_s               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    pcf_color_sampler_create_info.wrap_t               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;

    pcf_depth_sampler_create_info.border_color         = border_color;
    pcf_depth_sampler_create_info.compare_mode_enabled = true;
    pcf_depth_sampler_create_info.compare_op           = RAL_COMPARE_OP_LESS;
    pcf_depth_sampler_create_info.mag_filter           = RAL_TEXTURE_FILTER_LINEAR;
    pcf_depth_sampler_create_info.min_filter           = RAL_TEXTURE_FILTER_LINEAR;
    pcf_depth_sampler_create_info.mipmap_mode          = RAL_TEXTURE_MIPMAP_MODE_NEAREST;
    pcf_depth_sampler_create_info.wrap_r               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    pcf_depth_sampler_create_info.wrap_s               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    pcf_depth_sampler_create_info.wrap_t               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;

    plain_color_sampler_create_info.border_color         = border_color;
    plain_color_sampler_create_info.compare_mode_enabled = false;
    plain_color_sampler_create_info.compare_op           = RAL_COMPARE_OP_ALWAYS;
    plain_color_sampler_create_info.mag_filter           = RAL_TEXTURE_FILTER_NEAREST;
    plain_color_sampler_create_info.min_filter           = RAL_TEXTURE_FILTER_LINEAR;
    plain_color_sampler_create_info.mipmap_mode          = RAL_TEXTURE_MIPMAP_MODE_LINEAR;
    plain_color_sampler_create_info.wrap_r               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    plain_color_sampler_create_info.wrap_s               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    plain_color_sampler_create_info.wrap_t               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;

    plain_depth_sampler_create_info.border_color         = border_color;
    plain_depth_sampler_create_info.compare_mode_enabled = true;
    plain_depth_sampler_create_info.compare_op           = RAL_COMPARE_OP_LESS;
    plain_depth_sampler_create_info.mag_filter           = RAL_TEXTURE_FILTER_NEAREST;
    plain_depth_sampler_create_info.min_filter           = RAL_TEXTURE_FILTER_LINEAR;
    plain_depth_sampler_create_info.mipmap_mode          = RAL_TEXTURE_MIPMAP_MODE_NEAREST;
    plain_depth_sampler_create_info.wrap_r               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    plain_depth_sampler_create_info.wrap_s               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;
    plain_depth_sampler_create_info.wrap_t               = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER;

    ral_sampler_create_info create_info_items[] =
    {
        pcf_color_sampler_create_info,
        pcf_depth_sampler_create_info,
        plain_color_sampler_create_info,
        plain_depth_sampler_create_info
    };
    const uint32_t n_create_info_items = sizeof(create_info_items) / sizeof(create_info_items[0]);
    ral_sampler    result_samplers[n_create_info_items];

    ral_context_create_samplers(sm_ptr->context,
                                n_create_info_items,
                                create_info_items,
                                result_samplers);

    sm_ptr->pcf_color_sampler   = result_samplers[0];
    sm_ptr->pcf_depth_sampler   = result_samplers[1];
    sm_ptr->plain_color_sampler = result_samplers[2];
    sm_ptr->plain_depth_sampler = result_samplers[3];
}


/** This method is called to calculate AABB of the visible part of the scene, as perceived by
 *  active camera.
 *
 *  @param scene_mesh_instance TODO
 *  @param renderer
 *
 **/
PRIVATE void _scene_renderer_sm_process_mesh_for_shadow_map_pre_pass(scene_mesh scene_mesh_instance,
                                                                     void*      renderer)
{
    mesh mesh_gpu                      = nullptr;
    mesh mesh_instantiation_parent_gpu = nullptr;
    bool mesh_is_shadow_caster         = false;

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_IS_SHADOW_CASTER,
                           &mesh_is_shadow_caster);

    /* Only update visibility AABB for meshes that are shadow casters. */
    if (mesh_is_shadow_caster)
    {
        scene_mesh_get_property(scene_mesh_instance,
                                SCENE_MESH_PROPERTY_MESH,
                               &mesh_gpu);

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_INSTANTIATION_PARENT,
                         &mesh_instantiation_parent_gpu);

        if (mesh_instantiation_parent_gpu != nullptr)
        {
            mesh_gpu = mesh_instantiation_parent_gpu;
        }

        /* Perform frustum culling. This is where the AABBs are also updated. */
        scene_renderer_cull_against_frustum( (scene_renderer) renderer,
                                             mesh_gpu,
                                             SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES,
                                             nullptr);
    }
}

/** Please see header for spec */
PRIVATE ral_command_buffer _scene_renderer_sm_start(_scene_renderer_sm*           handler_ptr,
                                                    scene_light                   light,
                                                    scene_renderer_sm_target_face target_face = SCENE_RENDERER_SM_TARGET_FACE_2D)
{
    /* If scene_renderer_sm is not already enabled, grab a texture from the texture pool.
     * It will serve as storage for the shadow map. After the SM is rendered, the ownership
     * is assumed to be passed to the caller.
     */
    system_hashed_ansi_string light_name                   = nullptr;
    uint32_t                  light_shadow_map_n_layer;
    uint32_t                  light_shadow_map_size[3]     = {0, 0, 1};
    ral_texture_type          light_shadow_map_texture_type;
    scene_light_type          light_type                   = SCENE_LIGHT_TYPE_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_NAME,
                            &light_name);
    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,
                             light_shadow_map_size);
    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    _scene_renderer_sm_get_texture_targets_from_target_face(target_face,
                                                           &light_shadow_map_texture_type,
                                                           &light_shadow_map_n_layer);

    if (!handler_ptr->is_enabled)
    {
        ral_command_buffer_create_info   command_buffer_create_info;
        scene_light_shadow_map_algorithm light_shadow_map_algorithm        = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
        bool                             light_shadow_map_cull_front_faces = false;
        scene_light_shadow_map_filtering light_shadow_map_filtering        = SCENE_LIGHT_SHADOW_MAP_FILTERING_UNKNOWN;
        ral_format                       light_shadow_map_format_color     = RAL_FORMAT_UNKNOWN;
        ral_format                       light_shadow_map_format_depth     = RAL_FORMAT_UNKNOWN;
        ral_texture_type                 light_shadow_map_type             = RAL_TEXTURE_TYPE_UNKNOWN;
        ral_texture                      sm_color_texture                  = nullptr;
        ral_texture_create_info          sm_color_texture_create_info;
        ral_texture_view_create_info     sm_color_texture_view_create_info;
        ral_texture                      sm_depth_texture                  = nullptr;
        ral_texture_create_info          sm_depth_texture_create_info;
        ral_texture_view_create_info     sm_depth_texture_view_create_info;

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        command_buffer_create_info.is_executable                           = true;
        command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        command_buffer_create_info.is_resettable                           = false;
        command_buffer_create_info.is_transient                            = true;

        ASSERT_DEBUG_SYNC(handler_ptr->current_command_buffer == nullptr,
                          "A command buffer is already being recorded");

        handler_ptr->current_command_buffer = ral_command_buffer_create(handler_ptr->context,
                                                                       &command_buffer_create_info);

        ral_command_buffer_start_recording(handler_ptr->current_command_buffer);

        /* Determine shadow map texture type */
        switch (light_type)
        {
            case SCENE_LIGHT_TYPE_POINT:
            {
                if (target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT ||
                    target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR)
                {
                    light_shadow_map_type    = RAL_TEXTURE_TYPE_2D_ARRAY;
                    light_shadow_map_size[2] = 2; /* front + rear */
                }
                else
                {
                    light_shadow_map_type = RAL_TEXTURE_TYPE_CUBE_MAP;
                }

                break;
            }

            case SCENE_LIGHT_TYPE_DIRECTIONAL:
            case SCENE_LIGHT_TYPE_SPOT:
            {
                light_shadow_map_type = RAL_TEXTURE_TYPE_2D;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized light type");
            }
        }

        /* Set up shadow map texture(s).
         *
         * For plain shadow mapping, we only use a single depth texture attachment. The texture
         * must only define a single mip-map, as the depth data is not linear.
         *
         * For VSM, we use a two-component color attachment & a depth texture attachment. Since we're
         * working with moments, the data fetches can be linearized. We therefore want the whole
         * mip-map chain to be present for the color texture.
         */
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                                &light_shadow_map_algorithm);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_CULL_FRONT_FACES,
                                &light_shadow_map_cull_front_faces);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING,
                                &light_shadow_map_filtering);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_FORMAT_COLOR,
                                &light_shadow_map_format_color);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_FORMAT_DEPTH,
                                &light_shadow_map_format_depth);

        ASSERT_DEBUG_SYNC(light_shadow_map_size[0] > 0 &&
                          light_shadow_map_size[1] > 0,
                          "Invalid shadow map size requested for a scene_light instance.");
        ASSERT_DEBUG_SYNC(handler_ptr->current_sm_color0_texture_view == nullptr &&
                          handler_ptr->current_sm_depth_texture_view  == nullptr,
                          "SM texture views are already defined");

        sm_depth_texture_create_info.base_mipmap_depth      = 1;
        sm_depth_texture_create_info.base_mipmap_height     = light_shadow_map_size[1];
        sm_depth_texture_create_info.base_mipmap_width      = light_shadow_map_size[0];
        sm_depth_texture_create_info.fixed_sample_locations = false;
        sm_depth_texture_create_info.format                 = light_shadow_map_format_depth;
        sm_depth_texture_create_info.name                   = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_name),
                                                                                                                      " [shadow map depth texture]");
        sm_depth_texture_create_info.n_layers               = light_shadow_map_size[2];
        sm_depth_texture_create_info.n_samples              = 1;
        sm_depth_texture_create_info.type                   = light_shadow_map_type;
        sm_depth_texture_create_info.usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                              RAL_TEXTURE_USAGE_SAMPLED_BIT;
        sm_depth_texture_create_info.use_full_mipmap_chain  = false;

        ral_context_create_textures(handler_ptr->context,
                                    1, /* n_textures */
                                   &sm_depth_texture_create_info,
                                   &sm_depth_texture);

        sm_depth_texture_view_create_info.aspect             = RAL_TEXTURE_ASPECT_DEPTH_BIT;
        sm_depth_texture_view_create_info.component_order[0] = RAL_TEXTURE_COMPONENT_IDENTITY;
        sm_depth_texture_view_create_info.component_order[1] = RAL_TEXTURE_COMPONENT_IDENTITY;
        sm_depth_texture_view_create_info.component_order[2] = RAL_TEXTURE_COMPONENT_IDENTITY;
        sm_depth_texture_view_create_info.component_order[3] = RAL_TEXTURE_COMPONENT_IDENTITY;
        sm_depth_texture_view_create_info.format             = sm_depth_texture_create_info.format;
        sm_depth_texture_view_create_info.n_base_layer       = 0;
        sm_depth_texture_view_create_info.n_base_mip         = 0;
        sm_depth_texture_view_create_info.n_layers           = sm_depth_texture_create_info.n_layers;
        sm_depth_texture_view_create_info.n_mips             = 1;
        sm_depth_texture_view_create_info.texture            = sm_depth_texture;
        sm_depth_texture_view_create_info.type               = sm_depth_texture_create_info.type;

        ral_context_create_texture_views(handler_ptr->context,
                                         1, /* n_texture_views */
                                        &sm_depth_texture_view_create_info,
                                        &handler_ptr->current_sm_depth_texture_view);

        if (sm_depth_texture_create_info.n_layers != 1)
        {
            ASSERT_DEBUG_SYNC(sm_depth_texture_create_info.n_layers == 2,
                              "TODO");

            sm_depth_texture_view_create_info.n_base_layer = 0;
            sm_depth_texture_view_create_info.n_layers     = 1;

            ral_context_create_texture_views(handler_ptr->context,
                                             1, /* n_texture_views */
                                            &sm_depth_texture_view_create_info,
                                            &handler_ptr->current_sm_depth_l0_texture_view);

            sm_depth_texture_view_create_info.n_base_layer = 1;

            ral_context_create_texture_views(handler_ptr->context,
                                             1, /* n_texture_views */
                                            &sm_depth_texture_view_create_info,
                                            &handler_ptr->current_sm_depth_l1_texture_view);
        }
        else
        {
            handler_ptr->current_sm_depth_l0_texture_view = handler_ptr->current_sm_depth_texture_view;
            handler_ptr->current_sm_depth_l1_texture_view = nullptr;

            ral_context_retain_object(handler_ptr->context,
                                      RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                      handler_ptr->current_sm_depth_texture_view);
        }

        ral_context_delete_objects(handler_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_objects */
                                   (const void**) &sm_depth_texture);

        if (light_shadow_map_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
        {
            ASSERT_DEBUG_SYNC(light_shadow_map_size[0] == light_shadow_map_size[1],
                              "For VSM, shadow map textures must be square.");
            ASSERT_DEBUG_SYNC(light_shadow_map_type == RAL_TEXTURE_TYPE_2D       && light_shadow_map_size[2] == 1 ||
                              light_shadow_map_type == RAL_TEXTURE_TYPE_2D_ARRAY && light_shadow_map_size[2] >  1 ||
                              light_shadow_map_type == RAL_TEXTURE_TYPE_CUBE_MAP && light_shadow_map_size[2] == 1,
                              "Sanity check failed");

            sm_color_texture_create_info.base_mipmap_depth      = 1;
            sm_color_texture_create_info.base_mipmap_height     = light_shadow_map_size[1];
            sm_color_texture_create_info.base_mipmap_width      = light_shadow_map_size[0];
            sm_color_texture_create_info.fixed_sample_locations = false;
            sm_color_texture_create_info.format                 = light_shadow_map_format_color;
            sm_color_texture_create_info.name                   = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_name),
                                                                                                                          " [shadow map color texture]");
            sm_color_texture_create_info.n_layers               = light_shadow_map_size[2];
            sm_color_texture_create_info.n_samples              = 1;
            sm_color_texture_create_info.type                   = light_shadow_map_type;
            sm_color_texture_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                  RAL_TEXTURE_USAGE_SAMPLED_BIT;
            sm_color_texture_create_info.use_full_mipmap_chain  = true;

            ral_context_create_textures(handler_ptr->context,
                                        1, /* n_textures */
                                       &sm_color_texture_create_info,
                                       &sm_color_texture);

            ral_texture_get_property(sm_color_texture,
                                     RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                     &sm_color_texture_view_create_info.n_mips);

            sm_color_texture_view_create_info.aspect             = RAL_TEXTURE_ASPECT_COLOR_BIT;
            sm_color_texture_view_create_info.component_order[0] = RAL_TEXTURE_COMPONENT_IDENTITY;
            sm_color_texture_view_create_info.component_order[1] = RAL_TEXTURE_COMPONENT_IDENTITY;
            sm_color_texture_view_create_info.component_order[2] = RAL_TEXTURE_COMPONENT_IDENTITY;
            sm_color_texture_view_create_info.component_order[3] = RAL_TEXTURE_COMPONENT_IDENTITY;
            sm_color_texture_view_create_info.format             = sm_color_texture_create_info.format;
            sm_color_texture_view_create_info.n_base_layer       = 0;
            sm_color_texture_view_create_info.n_base_mip         = 0;
            sm_color_texture_view_create_info.n_layers           = sm_color_texture_create_info.n_layers;
            sm_color_texture_view_create_info.texture            = sm_color_texture;
            sm_color_texture_view_create_info.type               = sm_color_texture_create_info.type;

            ral_context_create_texture_views(handler_ptr->context,
                                             1, /* n_texture_views */
                                            &sm_color_texture_view_create_info,
                                            &handler_ptr->current_sm_color0_texture_view);

            if (sm_color_texture_create_info.n_layers != 1)
            {
                ASSERT_DEBUG_SYNC(sm_color_texture_create_info.n_layers == 2,
                                  "TODO");

                sm_color_texture_view_create_info.n_base_layer = 0;
                sm_color_texture_view_create_info.n_layers     = 1;

                ral_context_create_texture_views(handler_ptr->context,
                                                 1, /* n_texture_views */
                                                &sm_color_texture_view_create_info,
                                                &handler_ptr->current_sm_color0_l0_texture_view);

                sm_color_texture_view_create_info.n_base_layer = 1;

                ral_context_create_texture_views(handler_ptr->context,
                                                 1, /* n_texture_views */
                                                &sm_color_texture_view_create_info,
                                                &handler_ptr->current_sm_color0_l1_texture_view);
            }
            else
            {
                handler_ptr->current_sm_color0_l0_texture_view = handler_ptr->current_sm_color0_texture_view;
                handler_ptr->current_sm_color0_l1_texture_view = nullptr;

                ral_context_retain_object(handler_ptr->context,
                                          RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                          handler_ptr->current_sm_color0_texture_view);
            }

            ral_context_delete_objects(handler_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                       1, /* n_objects */
                                       (const void**) &sm_color_texture);
        }

        scene_light_set_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_COLOR_RAL,
                                &handler_ptr->current_sm_color0_texture_view);
        scene_light_set_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_DEPTH_RAL,
                                &handler_ptr->current_sm_depth_texture_view);

        /* Set up color & depth masks */
        handler_ptr->current_gfx_state_create_info.depth_writes       = true;
        handler_ptr->current_gfx_state_create_info.rasterizer_discard = (handler_ptr->current_sm_color0_texture_view == nullptr);

        /* render back-facing faces only: THIS WON'T WORK FOR NON-CONVEX GEOMETRY */
        if (light_shadow_map_cull_front_faces)
        {
            handler_ptr->current_gfx_state_create_info.culling   = true;
            handler_ptr->current_gfx_state_create_info.cull_mode = RAL_CULL_MODE_FRONT;
        }
        else
        {
            /* :C */
            handler_ptr->current_gfx_state_create_info.culling = false;
        }

        /* Set up depth function */
        handler_ptr->current_gfx_state_create_info.depth_test            = true;
        handler_ptr->current_gfx_state_create_info.depth_test_compare_op = RAL_COMPARE_OP_LESS;

        /* Adjust viewport to match shadow map size */
        handler_ptr->current_viewport.depth_range[0] = 0.0f;
        handler_ptr->current_viewport.depth_range[1] = 1.0f;
        handler_ptr->current_viewport.index          = 0;
        handler_ptr->current_viewport.size[0]        = static_cast<float>(light_shadow_map_size[0]);
        handler_ptr->current_viewport.size[1]        = static_cast<float>(light_shadow_map_size[1]);
        handler_ptr->current_viewport.xy[0]          = 0;
        handler_ptr->current_viewport.xy[1]          = 0;

        handler_ptr->current_gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        handler_ptr->current_gfx_state_create_info.static_viewports_enabled             = true;
        handler_ptr->current_gfx_state_create_info.static_viewports                     = &handler_ptr->current_viewport;

    }
    else
    {
        ASSERT_DEBUG_SYNC(handler_ptr->current_sm_depth_texture_view != nullptr,
                          "scene_renderer_sm is enabled but no depth SM texture view is associated with the instance");
    }

    /* Set up render-targets */
    ral_command_buffer_set_binding_command_info color_rt_binding_info;

    color_rt_binding_info.binding_type                  = RAL_BINDING_TYPE_RENDERTARGET;
    color_rt_binding_info.name                          = system_hashed_ansi_string_create("result_fragment");
    color_rt_binding_info.rendertarget_binding.rt_index = (handler_ptr->current_sm_color0_texture_view != nullptr)  ? 0 : -1;

    ral_command_buffer_record_set_bindings(handler_ptr->current_command_buffer,
                                           1, /* n_bindings */
                                          &color_rt_binding_info);

    if (light_shadow_map_texture_type != RAL_TEXTURE_TYPE_2D_ARRAY)
    {
        ral_command_buffer_set_color_rendertarget_command_info color_rt_info = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        color_rt_info.rendertarget_index = 0;

        color_rt_info.texture_view       = handler_ptr->current_sm_color0_texture_view;

        ral_command_buffer_record_set_color_rendertargets(handler_ptr->current_command_buffer,
                                                          1, /* n_rendertargets */
                                                         &color_rt_info);
        ral_command_buffer_record_set_depth_rendertarget (handler_ptr->current_command_buffer,
                                                          handler_ptr->current_sm_depth_texture_view);
    }
    else
    {
        uint32_t slice_index = -1;

        switch (target_face)
        {
            case SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT:
            {
                slice_index = 0;

                break;
            }

            case SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR:
            {
                slice_index = 1;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized target face value");
            }
        }

        if (handler_ptr->current_sm_color0_texture_view != nullptr)
        {
            ral_command_buffer_set_color_rendertarget_command_info color_rt_info = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();

            ASSERT_DEBUG_SYNC(slice_index == 0 || slice_index == 1,
                              "Invalid slice index");

            color_rt_info.rendertarget_index = 0;
            color_rt_info.texture_view       = (slice_index == 0) ? handler_ptr->current_sm_color0_l0_texture_view
                                                                  : handler_ptr->current_sm_color0_l1_texture_view;

            ral_command_buffer_record_set_color_rendertargets(handler_ptr->current_command_buffer,
                                                              1, /* n_rendertargets */
                                                             &color_rt_info);
        }

        if (handler_ptr->current_sm_depth_texture_view != nullptr)
        {
            ASSERT_DEBUG_SYNC(slice_index == 0 || slice_index == 1,
                              "Invalid slice index");

            ral_command_buffer_record_set_depth_rendertarget(handler_ptr->current_command_buffer,
                                                             (slice_index == 0) ? handler_ptr->current_sm_depth_l0_texture_view
                                                                                : handler_ptr->current_sm_depth_l1_texture_view);
        }
    }

    /* Clear the color & depth buffers */
    ral_command_buffer_clear_rt_binding_command_info clear_command_info;

    clear_command_info.clear_regions[0].n_base_layer      = 0;
    clear_command_info.clear_regions[0].n_layers          = 1;
    clear_command_info.clear_regions[0].size[0]           = static_cast<uint32_t>(light_shadow_map_size[0]);
    clear_command_info.clear_regions[0].size[1]           = static_cast<uint32_t>(light_shadow_map_size[1]);
    clear_command_info.n_clear_regions                    = 1;
    clear_command_info.n_rendertargets                    = 1 + ((handler_ptr->current_sm_color0_texture_view != nullptr) ? 1 : 0);
    clear_command_info.rendertargets[0].aspect            = RAL_TEXTURE_ASPECT_DEPTH_BIT;
    clear_command_info.rendertargets[0].clear_value.depth = 1.0f;
    clear_command_info.rendertargets[0].rt_index          = -1; /* irrelevant */

    if (handler_ptr->current_sm_color0_texture_view != nullptr)
    {
        clear_command_info.rendertargets[1].aspect                   = RAL_TEXTURE_ASPECT_COLOR_BIT;
        clear_command_info.rendertargets[1].clear_value.color.f32[0] = 1.0f;
        clear_command_info.rendertargets[1].clear_value.color.f32[1] = 1.0f;
        clear_command_info.rendertargets[1].clear_value.color.f32[2] = 1.0f;
        clear_command_info.rendertargets[1].clear_value.color.f32[3] = 1.0f;
        clear_command_info.rendertargets[1].rt_index                 = 0;
    }

    ral_command_buffer_record_clear_rendertarget_binding(handler_ptr->current_command_buffer,
                                                         1, /* n_clear_ops */
                                                        &clear_command_info);

    handler_ptr->is_enabled = true;

    return handler_ptr->current_command_buffer;
}

/** TODO
 *
 *  NOTE: Exposes either 1 or 2 unique texture view outputs, depending on SM algorithm type:
 *
 *        - VSM outputs a color & a depth texture view (in the described order)
 *        - All other SM algorithms expose 1 color texture view.
 **/
PRIVATE ral_present_task _scene_renderer_sm_stop(_scene_renderer_sm*           handler_ptr,
                                                 scene_light                   light,
                                                 scene_renderer_sm_target_face target_face = SCENE_RENDERER_SM_TARGET_FACE_2D)
{
    ral_present_task result_task          = nullptr;
    ral_present_task sm_blur_present_task = nullptr;

    /* Blur the SM if it makes sense */
    scene_light_shadow_map_algorithm light_shadow_map_algorithm = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &light_shadow_map_algorithm);

    if (light_shadow_map_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
    {
        float                                   sm_blur_n_iterations;
        unsigned int                            sm_blur_n_taps;
        postprocessing_blur_gaussian_resolution sm_blur_resolution;
        ral_texture_view                        sm_color_texture_view = nullptr;

        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_RESOLUTION,
                                &sm_blur_resolution);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_N_PASSES,
                                &sm_blur_n_iterations);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_N_TAPS,
                                &sm_blur_n_taps);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_COLOR_RAL,
                                &sm_color_texture_view);

        if (sm_blur_n_taps < N_MIN_BLUR_TAPS)
        {
            LOG_ERROR("SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_N_TAPS clamped to lower boundary!");

            sm_blur_n_taps = N_MIN_BLUR_TAPS;
        }
        else
        if (sm_blur_n_taps > N_MAX_BLUR_TAPS)
        {
            LOG_ERROR("SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_N_TAPS clamped to upper boundary!");

            sm_blur_n_taps = N_MAX_BLUR_TAPS;
        }

        sm_blur_present_task = postprocessing_blur_gaussian_create_present_task(handler_ptr->blur_handler,
                                                                                sm_blur_n_taps,
                                                                                sm_blur_n_iterations,
                                                                                sm_color_texture_view,
                                                                                sm_blur_resolution);
    }

    /* If necessary, also generate mipmaps for the color texture */
    if (handler_ptr->current_sm_color0_texture_view != nullptr)
    {
        uint32_t n_mips = 0;

        ral_texture_view_get_property(handler_ptr->current_sm_color0_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                     &n_mips);

        if (n_mips > 1)
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO");

            #if 0
            {
                /* TODO: Must be implemented as a GPU task */
                ral_texture_generate_mipmaps(handler_ptr->current_sm_color0_texture,
                                             false /* async */);
            }
            #endif
        }
    }

    /* Form the result present task */
    if (sm_blur_present_task == nullptr)
    {
        /* This is the easy case, where we only need to execute the command buffer and expose
         * the result texture view(s) as present task's output(s). */
        ral_present_task_gpu_create_info result_task_create_info;
        ral_present_task_io              result_task_unique_outputs[2];

        result_task_unique_outputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        result_task_unique_outputs[0].texture_view = handler_ptr->current_sm_color0_texture_view;

        result_task_unique_outputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        result_task_unique_outputs[1].texture_view = handler_ptr->current_sm_depth_texture_view;

        result_task_create_info.command_buffer   = handler_ptr->current_command_buffer;
        result_task_create_info.n_unique_inputs  = 0;
        result_task_create_info.n_unique_outputs = (handler_ptr->current_sm_color0_texture_view != nullptr) ? 2 : 1;
        result_task_create_info.unique_inputs    = nullptr;
        result_task_create_info.unique_outputs   = result_task_unique_outputs;

        result_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Shadow map renderer: rasterization"),
                                                 &result_task_create_info);
    }
    else
    {
        /* This one is a bit more complex. Need a group present task here */
        ASSERT_DEBUG_SYNC(false,
                          "TODO: Implement");
    }

    /* "Unbind" the SM texture from the scene_renderer_sm instance */
    ASSERT_DEBUG_SYNC(handler_ptr->current_sm_depth_texture_view != nullptr,
                      "scene_renderer_sm_toggle() called to disable SM rendering without an active SM");

    handler_ptr->current_sm_color0_texture_view = nullptr;
    handler_ptr->current_sm_depth_texture_view  = nullptr;

    /* "Specialized" views are not shared with external objects, and therefore need to be
     * released explicitly */
    ral_texture_view tvs_to_release[] =
    {
        handler_ptr->current_sm_color0_l0_texture_view,
        handler_ptr->current_sm_color0_l1_texture_view,
        handler_ptr->current_sm_depth_l0_texture_view,
        handler_ptr->current_sm_depth_l1_texture_view,
    };
    const uint32_t n_tvs_to_release = sizeof(tvs_to_release) / sizeof(tvs_to_release[0]);

    ral_context_delete_objects(handler_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               n_tvs_to_release,
                               (const void**) tvs_to_release);

    handler_ptr->current_sm_color0_l0_texture_view = nullptr;
    handler_ptr->current_sm_color0_l1_texture_view = nullptr;
    handler_ptr->current_sm_depth_l0_texture_view  = nullptr;
    handler_ptr->current_sm_depth_l1_texture_view  = nullptr;

    /* Release the command buffer as well */
    ral_context_delete_objects(handler_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               (const void**) &handler_ptr->current_command_buffer);

    handler_ptr->current_command_buffer = nullptr;
    handler_ptr->is_enabled             = false;

    return result_task;
}

/** Please see header for spec */
PUBLIC void scene_renderer_sm_adjust_fragment_uber_code(glsl_shader_constructor                  shader_constructor_fs,
                                                        uint32_t                                 n_light,
                                                        scene_light                              light_instance,
                                                        glsl_shader_constructor_uniform_block_id ub_fs,
                                                        system_hashed_ansi_string                light_world_pos_var_name,
                                                        system_hashed_ansi_string                light_vector_norm_var_name,
                                                        system_hashed_ansi_string                light_vector_non_norm_var_name,
                                                        system_hashed_ansi_string*               out_visibility_var_name_ptr)
{
    scene_light_shadow_map_algorithm            light_sm_algorithm;
    scene_light_shadow_map_pointlight_algorithm light_sm_pointlight_algorithm;
    scene_light_shadow_map_bias                 light_sm_bias;
    scene_light_type                            light_type;

    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &light_sm_algorithm);
    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);
    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                            &light_sm_pointlight_algorithm);
    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS,
                            &light_sm_bias);


    const bool                is_directional_light                        = (light_type == SCENE_LIGHT_TYPE_DIRECTIONAL);
    const bool                is_point_light                              = (light_type == SCENE_LIGHT_TYPE_POINT);
    const bool                is_spot_light                               = (light_type == SCENE_LIGHT_TYPE_SPOT);
    system_hashed_ansi_string light_far_near_diff_var_name_has            = nullptr;
    system_hashed_ansi_string light_near_var_name_has                     = nullptr;
    system_hashed_ansi_string light_projection_matrix_var_name_has        = nullptr;
    system_hashed_ansi_string light_shadow_coord_var_name_has             = nullptr;
    system_hashed_ansi_string light_shadow_map_color_sampler_var_name_has = nullptr;
    system_hashed_ansi_string light_shadow_map_depth_sampler_var_name_has = nullptr;
    system_hashed_ansi_string light_view_matrix_var_name_has              = nullptr;
    system_hashed_ansi_string light_vsm_cutoff_var_name_has               = nullptr;
    system_hashed_ansi_string light_vsm_min_variance_var_name_has         = nullptr;

    if (!is_point_light)
    {
        _scene_renderer_sm_add_uniforms_to_fragment_uber_for_non_point_light(shader_constructor_fs,
                                                                             ub_fs,
                                                                             light_instance,
                                                                             n_light,
                                                                            &light_shadow_coord_var_name_has,
                                                                            &light_shadow_map_color_sampler_var_name_has,
                                                                            &light_shadow_map_depth_sampler_var_name_has,
                                                                            &light_vsm_cutoff_var_name_has,
                                                                            &light_vsm_min_variance_var_name_has);
    }
    else
    {
        _scene_renderer_sm_add_uniforms_to_fragment_uber_for_point_light(shader_constructor_fs,
                                                                         ub_fs,
                                                                         light_instance,
                                                                         n_light,
                                                                        &light_far_near_diff_var_name_has,
                                                                        &light_near_var_name_has,
                                                                        &light_projection_matrix_var_name_has,
                                                                        &light_view_matrix_var_name_has,
                                                                        &light_shadow_map_color_sampler_var_name_has,
                                                                        &light_shadow_map_depth_sampler_var_name_has,
                                                                        &light_vsm_cutoff_var_name_has,
                                                                        &light_vsm_min_variance_var_name_has);
    }

    /* Add helper calculations for point lights */
    system_hashed_ansi_string code_snippet_has     = nullptr;
    std::stringstream         code_snippet_sstream;
    std::stringstream         light_uv_var_name_sstream;
    std::stringstream         light_vertex_depth_var_name_sstream;
    std::stringstream         light_vertex_depth_window_var_name_sstream;
    std::stringstream         light_vertex_var_name_sstream;

    if (is_point_light)
    {
        switch (light_sm_pointlight_algorithm)
        {
            case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL:
            {
                std::stringstream light_vertex_abs_var_name_sstream;
                std::stringstream light_vertex_abs_max_var_name_sstream;
                std::stringstream light_vertex_clip_var_name_sstream;

                light_vertex_abs_var_name_sstream          << "light_"
                                                           << n_light
                                                           << "_vertex_abs";
                light_vertex_abs_max_var_name_sstream      << "light_"
                                                           << n_light
                                                           << "_vertex_abs_max";
                light_vertex_clip_var_name_sstream         << "light_"
                                                           << n_light
                                                           << "_vertex_clip";
                light_vertex_depth_window_var_name_sstream << "light_"
                                                           << n_light
                                                           << "_vertex_depth_window";
                light_vertex_var_name_sstream              << "light_"
                                                           << n_light
                                                           << "_vertex";

                /* 1. Move current fragment's world position relative to the light position.
                 *    The fragment is now in light space.
                 * 2. Figure out which cube map we should sample from.
                 * 3. Compute the depth of the fragment in light space.
                 */
                code_snippet_sstream << "vec4 "
                                     << light_vertex_var_name_sstream.str()
                                     << " = vec4(world_vertex - "
                                     << system_hashed_ansi_string_get_buffer(light_world_pos_var_name)
                                     << ".xyz, 1.0);\n"
                                        "vec4 "
                                     << light_vertex_abs_var_name_sstream.str()
                                     << " = abs("
                                     << light_vertex_var_name_sstream.str()
                                     << ");\n"
                                     << "float "
                                     << light_vertex_abs_max_var_name_sstream.str()
                                     << " = -max(" << light_vertex_abs_var_name_sstream.str() << ".x, "
                                            "max(" << light_vertex_abs_var_name_sstream.str() << ".y, "
                                                   << light_vertex_abs_var_name_sstream.str() << ".z) );\n"
                                     << "vec4 "
                                     << light_vertex_clip_var_name_sstream.str()
                                     << " = "
                                     << system_hashed_ansi_string_get_buffer(light_projection_matrix_var_name_has)
                                     << " * vec4(0.0, 0.0, " << light_vertex_abs_max_var_name_sstream.str() << ", 1.0);\n"
                                     << "float "
                                     << light_vertex_depth_window_var_name_sstream.str()
                                     << " = "
                                     << light_vertex_clip_var_name_sstream.str()
                                     << ".z / "
                                     << light_vertex_clip_var_name_sstream.str()
                                     << ".w * 0.5 + 0.5;\n";

                break;
            }

            case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID:
            {
                std::stringstream light_vertex_length_var_name_sstream;
                std::stringstream light_vertex_z_abs_var_name_sstream;

                light_uv_var_name_sstream            << "light_"
                                                     << n_light
                                                     << "_uv";
                light_vertex_depth_var_name_sstream  << "light_"
                                                     << n_light
                                                     << "vertex_depth";
                light_vertex_length_var_name_sstream << "light_"
                                                     << n_light
                                                     << "_vertex_length";
                light_vertex_var_name_sstream        << "light_"
                                                     << n_light
                                                     << "_vertex";
                light_vertex_z_abs_var_name_sstream  << "light_"
                                                     << n_light
                                                     << "_vertex_z_abs";

                code_snippet_sstream 
                                     /* Transform world space vertex into light space */
                                     << "vec3 "
                                     << light_vertex_var_name_sstream.str()
                                     << " = ("
                                     << system_hashed_ansi_string_get_buffer(light_view_matrix_var_name_has)
                                     << " * vec4(world_vertex, 1.0)).xyz;\n"
                                     /* Calculate the distance between light and the vertex */
                                     << "float "
                                     << light_vertex_length_var_name_sstream.str()
                                     << " = length("
                                     << light_vertex_var_name_sstream.str()
                                     << ");\n"
                                     /* Normalize the vector */
                                     << light_vertex_var_name_sstream.str()
                                     << " /= vec3("
                                     << light_vertex_length_var_name_sstream.str()
                                     << ");\n"
                                     /* Compute the depth value for the current fragment. */
                                     << "float "
                                     << light_vertex_z_abs_var_name_sstream.str()
                                     << " = abs("
                                     << light_vertex_var_name_sstream.str() << ".z);\n"
                                     << "vec2 "
                                     << light_uv_var_name_sstream.str()
                                     << " = vec2("  << light_vertex_var_name_sstream.str() << ".x / (1.0 + " << light_vertex_z_abs_var_name_sstream.str() << ") * 0.5 + 0.5, "
                                                    << light_vertex_var_name_sstream.str() << ".y / (1.0 + " << light_vertex_z_abs_var_name_sstream.str() << ") * 0.5 + 0.5);\n"
                                     << "float "
                                     << light_vertex_depth_var_name_sstream.str()
                                     << " =  (("
                                     << light_vertex_length_var_name_sstream.str()
                                     << " - "
                                     << system_hashed_ansi_string_get_buffer(light_near_var_name_has)
                                     << ") / "
                                     << system_hashed_ansi_string_get_buffer(light_far_near_diff_var_name_has)
                                     << ");\n";

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized shadow map point light algorithm");
            }
        }
    }

    /* Add bias calculation */
    system_hashed_ansi_string light_bias_var_name_has = nullptr;

    _scene_renderer_sm_add_bias_variable_to_fragment_uber(code_snippet_sstream,
                                                          n_light,
                                                          light_sm_bias,
                                                          light_sm_algorithm,
                                                         &light_bias_var_name_has);

    /* Add the light-specific visibility calculations */
    std::stringstream light_ref_var_name_sstream;
    std::stringstream light_visibility_helper_var_name_sstream;
    std::stringstream light_visibility_var_name_sstream;

    light_ref_var_name_sstream               << "light_"
                                             << n_light
                                             << "_ref";
    light_visibility_helper_var_name_sstream << "light_"
                                             << n_light
                                             << "_visibility_helper";
    light_visibility_var_name_sstream        << "light_"
                                             << n_light
                                             << "_visibility";

    /* Reference comparison value */
    code_snippet_sstream << "float "
                         << light_ref_var_name_sstream.str()
                         << " = ";

    if (is_directional_light)
    {
        code_snippet_sstream << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                             << ".z - "
                             << system_hashed_ansi_string_get_buffer(light_bias_var_name_has)
                             << ";\n";
    }
    else
    if (is_point_light)
    {
        scene_light_shadow_map_pointlight_algorithm sm_algorithm;

        scene_light_get_property(light_instance,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                &sm_algorithm);

        switch (sm_algorithm)
        {
            case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL:
            {
                code_snippet_sstream << light_vertex_depth_window_var_name_sstream.str()
                                     << " - "
                                     << system_hashed_ansi_string_get_buffer(light_bias_var_name_has)
                                     << ";\n";

                break;
            }

            case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID:
            {
                /* Fragment depth, as seen in light space */
                code_snippet_sstream << light_vertex_depth_var_name_sstream.str()
                                     << " - "
                                     /* Bias */
                                     << system_hashed_ansi_string_get_buffer(light_bias_var_name_has)
                                     << ";\n";

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized SM point light algorithm");
            }
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(is_spot_light,
                          "This should never happen");

        code_snippet_sstream << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                             << ".z ";

        if (light_sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
        {
            code_snippet_sstream << " / "
                                 << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                                 << ".w";
        }

        code_snippet_sstream << " - "
                             << system_hashed_ansi_string_get_buffer(light_bias_var_name_has)
                             << ";\n";
    }

    /* Helpers for light visibility calculations */
    if (light_sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
    {
        /* We're using a 2D texture sampler in this case, so need to do the comparison "by hand" */
        std::stringstream cutoff_var_name_sstream;
        std::stringstream mean_var_name_sstream;
        std::stringstream variance_var_name_sstream;

        mean_var_name_sstream     << "light"
                                  << n_light
                                  << "_mean";
        variance_var_name_sstream << "light"
                                  << n_light
                                  << "_variance";

        code_snippet_sstream << "vec2 "
                             << light_visibility_helper_var_name_sstream.str()
                             << " = ";

        if (is_directional_light)
        {
            /* first & second moment */
            code_snippet_sstream << "texture("
                                 << system_hashed_ansi_string_get_buffer(light_shadow_map_color_sampler_var_name_has)
                                 << ", "
                                 << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                                 << ".xy).xy;\n";
        }
        else
        if (is_point_light)
        {
            if (light_sm_pointlight_algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL)
            {
                /* first & second moment */
                code_snippet_sstream << "texture("
                                     << system_hashed_ansi_string_get_buffer(light_shadow_map_color_sampler_var_name_has)
                                     << ", "
                                        "normalize(" << light_vertex_var_name_sstream.str() << ".xyz)"
                                     << ").xy;\n";
            }
            else
            {
                ASSERT_DEBUG_SYNC(light_sm_pointlight_algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID,
                                  "Sanity check failed");

                code_snippet_sstream << "texture("
                                     << system_hashed_ansi_string_get_buffer(light_shadow_map_color_sampler_var_name_has)
                                     << ", "
                                     << "vec3("
                                     /* UV */
                                     << light_uv_var_name_sstream.str()
                                     << ", "
                                     /* Which face should we sample from? */
                                     << "("
                                     << light_vertex_var_name_sstream.str() << ".z >= 0.0) ? 0.0 : 1.0) ).xy;\n";
            }
        }
        else
        if (is_spot_light)
        {
            /* first & second moment */
            code_snippet_sstream << "textureProj("
                                 << system_hashed_ansi_string_get_buffer(light_shadow_map_color_sampler_var_name_has)
                                 << ", "
                                 << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                                 << ".xyw).xy;\n";
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized light type");
        }

        code_snippet_sstream << "\n"
                                "if ("
                             << light_visibility_helper_var_name_sstream.str()
                             << ".x > "
                             << light_ref_var_name_sstream.str()
                             << ") "
                             << light_visibility_helper_var_name_sstream.str()
                             << ".x = 1.0;else\n"
                                "{\n"
                             /* Calculate variance */
                             << "float "
                             << mean_var_name_sstream.str()
                             << " = "
                             << light_visibility_helper_var_name_sstream.str()
                             << ".x;\n"
                             << "float "
                             << variance_var_name_sstream.str()
                             << " = max("
                             << light_visibility_helper_var_name_sstream.str()
                             << ".y - "
                             << mean_var_name_sstream.str()
                             << " * "
                             << mean_var_name_sstream.str()
                             << ", "
                             << system_hashed_ansi_string_get_buffer(light_vsm_min_variance_var_name_has)
                             << ");\n"
                             /* Calculate result light attenuation */
                             << light_visibility_helper_var_name_sstream.str() /* result */
                             << ".x = "
                             << variance_var_name_sstream.str() /* variance squared */
                             << " / ("
                             << variance_var_name_sstream.str() /* variance squared */
                             << " + ("
                             << light_ref_var_name_sstream.str() /* depth */
                             << " - "
                             << mean_var_name_sstream.str() /* mean */
                             << ") * ("
                             << light_ref_var_name_sstream.str() /* depth */
                             << " - "
                             << mean_var_name_sstream.str() /* mean */
                             << ") );\n"
                             /* cut off the <0, start_cutoff> region and rescale to full <0, 1> to fight the light bleeding effect */
                             << light_visibility_helper_var_name_sstream.str()
                             << ".x = smoothstep("
                             << system_hashed_ansi_string_get_buffer(light_vsm_cutoff_var_name_has)
                             << ", 1.0, "
                             << light_visibility_helper_var_name_sstream.str()
                             << ".x);\n"
                                "}\n";
    }

    /* Light visibility value */
    code_snippet_sstream << "float "
                         << light_visibility_var_name_sstream.str()
                         << " = 0.1 + 0.9 * ";

    if (is_directional_light)
    {
        switch (light_sm_algorithm)
        {
            case SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN:
            {
                /* We're using a shadow sampler in this case */
                code_snippet_sstream << "textureLod("
                                     << system_hashed_ansi_string_get_buffer(light_shadow_map_depth_sampler_var_name_has)
                                     << ", vec3("
                                     << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                                     << ".xy, "
                                     << light_ref_var_name_sstream.str()
                                     << "), 0.0);\n";

                break;
            }

            case SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM:
            {
                code_snippet_sstream << light_visibility_helper_var_name_sstream.str() << ".x;\n";

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized scene_light_shadow_map_algorithm value.");
            }
        }
    }
    else
    {
        /* All other light use shadow maps constructed with perspective projection matrices */
        if (is_point_light)
        {
            scene_light_shadow_map_pointlight_algorithm sm_algorithm;

            scene_light_get_property(light_instance,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                    &sm_algorithm);

            if (light_sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
            {
                code_snippet_sstream << light_visibility_helper_var_name_sstream.str() << ".x;\n";
            }
            else
            {
                switch (sm_algorithm)
                {
                    case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL:
                    {
                        code_snippet_sstream << "texture("
                                             << system_hashed_ansi_string_get_buffer(light_shadow_map_depth_sampler_var_name_has)
                                             << ", vec4("
                                             << "normalize(" << light_vertex_var_name_sstream.str() << ".xyz)"
                                             << ", "
                                             << light_ref_var_name_sstream.str()
                                             << ") );\n";

                        break;
                    }

                    case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID:
                    {
                        code_snippet_sstream << "texture("
                                             << system_hashed_ansi_string_get_buffer(light_shadow_map_depth_sampler_var_name_has)
                                             << ", "
                                             << "vec4("
                                             /* UV */
                                             << light_uv_var_name_sstream.str()
                                             << ", "
                                             /* Which face should we sample from? */
                                             << "("
                                             << light_vertex_var_name_sstream.str() << ".z >= 0.0) ? 0.0 : 1.0, "
                                             /* Reference value */
                                             << light_ref_var_name_sstream.str()
                                             << ") );\n";

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized point light SM algorithm");
                    }
                }
            }
        }
        else
        {
            switch (light_sm_algorithm)
            {
                case SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN:
                {
                    code_snippet_sstream << "textureProjLod("
                                         << system_hashed_ansi_string_get_buffer(light_shadow_map_depth_sampler_var_name_has)
                                         << ", vec4("
                                         << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                                         << ".xy, "
                                         << light_ref_var_name_sstream.str()
                                         << ", "
                                         << system_hashed_ansi_string_get_buffer(light_shadow_coord_var_name_has)
                                         << ".w"
                                         << "), 0.0);\n";

                    break;
                }

                case SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM:
                {
                    code_snippet_sstream << light_visibility_helper_var_name_sstream.str() << ".x;\n";

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized scene_light_shadow_map_algorithm value.");
                }
            }
        }
    }

    code_snippet_has = system_hashed_ansi_string_create(code_snippet_sstream.str().c_str());

    glsl_shader_constructor_append_to_function_body(shader_constructor_fs,
                                                    0, /* function_id */
                                                    code_snippet_has);

    /* Tell the caller what the name of the visibility variable is. */
    *out_visibility_var_name_ptr = system_hashed_ansi_string_create(light_visibility_var_name_sstream.str().c_str() );
}

/** Please see header for spec */
PUBLIC void scene_renderer_sm_adjust_vertex_uber_code(glsl_shader_constructor                  shader_constructor_vs,
                                                      uint32_t                                 n_light,
                                                      shaders_fragment_uber_light_type         light_type,
                                                      glsl_shader_constructor_uniform_block_id ub_vs,
                                                      system_hashed_ansi_string                world_vertex_vec4_variable_name)
{
    /* NOTE: The following code is only used for the directional and spot lights. We're using a
     *       "light_view * camera_view_inversed" matrix directly in FS for point lights, so we
     *       don't really need to pass a depth_vp or shadow_coord in their case.
     */
    const bool is_point_light = (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
                                 light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT);

    if (!is_point_light)
    {
        static const float depth_bias_matrix_data[] = /* column_major */
        {
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f
        };
        static uint32_t           n_depth_bias_matrix_data_entries = sizeof(depth_bias_matrix_data) / sizeof(depth_bias_matrix_data[0]);
        system_hashed_ansi_string depth_bias_matrix_var_name       = system_hashed_ansi_string_create("depth_bias");

        /* Add a global depth bias matrix which normalizes coordinates at range <-1, 1> to <0, 1> */
        bool is_depth_bias_matrix_added = glsl_shader_constructor_is_general_variable_in_ub(shader_constructor_vs,
                                                                                            0, /* uniform_block */
                                                                                            depth_bias_matrix_var_name);

        if (!is_depth_bias_matrix_added)
        {
            uint32_t                            matrix_variable_data_size = 0;
            glsl_shader_constructor_variable_id matrix_variable_id        = -1;

            glsl_shader_constructor_add_general_variable_to_ub(shader_constructor_vs,
                                                              VARIABLE_TYPE_CONST,
                                                              LAYOUT_QUALIFIER_NONE,
                                                              RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                              0, /* array_size */
                                                              0, /* uniform_block */
                                                              depth_bias_matrix_var_name,
                                                             &matrix_variable_id);

            glsl_shader_constructor_set_general_variable_default_value(shader_constructor_vs,
                                                                       0,                         /* uniform_block */
                                                                       matrix_variable_id,
                                                                       nullptr,                      /* data */
                                                                      &matrix_variable_data_size);

            ASSERT_DEBUG_SYNC(matrix_variable_data_size == sizeof(depth_bias_matrix_data),
                              "glsl_shader_constructor glitch detected");

            glsl_shader_constructor_set_general_variable_default_value(shader_constructor_vs ,
                                                                       0,                      /* uniform_block */
                                                                       matrix_variable_id,
                                                                       depth_bias_matrix_data,
                                                                       nullptr);                  /* n_bytes_to_read */
        }
        /* Add the light-specific depth VP matrix. */
        system_hashed_ansi_string depth_vp_matrix_name_has     = nullptr;
        std::stringstream         depth_vp_matrix_name_sstream;
        bool                      is_depth_vp_matrix_added     = false;

        depth_vp_matrix_name_sstream << "light"
                                     << n_light
                                     << "_depth_vp";

        depth_vp_matrix_name_has = system_hashed_ansi_string_create                 (depth_vp_matrix_name_sstream.str().c_str() );
        is_depth_vp_matrix_added = glsl_shader_constructor_is_general_variable_in_ub(shader_constructor_vs,
                                                                                     ub_vs,
                                                                                     depth_vp_matrix_name_has);

        ASSERT_DEBUG_SYNC(!is_depth_vp_matrix_added,
                          "Depth VP already added to VS UB for light [%d]",
                          n_light);

        glsl_shader_constructor_add_general_variable_to_ub(shader_constructor_vs,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_ROW_MAJOR,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                           0, /* array_size */
                                                           ub_vs,
                                                           depth_vp_matrix_name_has,
                                                           nullptr); /* out_variable_id */

        /* Add an output variable that calculates the vertex position in light-view space */
        bool                      is_light_shadow_coord_added = false;
        system_hashed_ansi_string light_shadow_coord_name_has = nullptr;
        std::stringstream         light_shadow_coord_name_sstream;

        light_shadow_coord_name_sstream << "light"
                                        << n_light
                                        << "_shadow_coord";

        light_shadow_coord_name_has = system_hashed_ansi_string_create                 (light_shadow_coord_name_sstream.str().c_str() );
        is_light_shadow_coord_added = glsl_shader_constructor_is_general_variable_in_ub(shader_constructor_vs,
                                                                                        ub_vs,
                                                                                        light_shadow_coord_name_has);

        ASSERT_DEBUG_SYNC(!is_light_shadow_coord_added,
                          "Light shadow coord for light [%d] already added",
                          n_light);

        glsl_shader_constructor_add_general_variable_to_ub(shader_constructor_vs,
                                                           VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                           0, /* array_size */
                                                           0, /* ub_id */
                                                           light_shadow_coord_name_has,
                                                           nullptr); /* out_variable_id */

        /* Add new code snippet to main() */
        const char*               world_vertex_vec4_name_raw = system_hashed_ansi_string_get_buffer(world_vertex_vec4_variable_name);
        system_hashed_ansi_string vs_code_has                = nullptr;
        std::stringstream         vs_code_sstream;

        vs_code_sstream << light_shadow_coord_name_sstream.str()
                        << " = "
                        << system_hashed_ansi_string_get_buffer(depth_bias_matrix_var_name)
                        << " * "
                        << depth_vp_matrix_name_sstream.str()
                        << " * "
                        << world_vertex_vec4_name_raw
                        << ";\n";

        vs_code_has = system_hashed_ansi_string_create(vs_code_sstream.str().c_str() );

        glsl_shader_constructor_append_to_function_body(shader_constructor_vs,
                                                        0, /* function_id */
                                                        vs_code_has);
    }
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL scene_renderer_sm scene_renderer_sm_create(ral_context context)
{
    _scene_renderer_sm* new_instance_ptr = new (std::nothrow) _scene_renderer_sm;

    ASSERT_ALWAYS_SYNC(new_instance_ptr != nullptr,
                       "Out of memory");

    if (new_instance_ptr != nullptr)
    {
        new_instance_ptr->context    = context;
        new_instance_ptr->is_enabled = false;

        _scene_renderer_sm_init_samplers(new_instance_ptr);

        /** TODO: Cache & re-use for the same context! */
        new_instance_ptr->blur_handler = postprocessing_blur_gaussian_create(context,
                                                                             system_hashed_ansi_string_create("Gaussian blur handler"),
                                                                             N_MIN_BLUR_TAPS,
                                                                             N_MAX_BLUR_TAPS);
    }

    return reinterpret_cast<scene_renderer_sm>(new_instance_ptr);
}

/** Please see header for spec */
PUBLIC void scene_renderer_sm_get_matrices_for_light(scene_renderer_sm             shadow_mapping,
                                                     scene_light                   light,
                                                     scene_renderer_sm_target_face light_target_face,
                                                     scene_camera                  current_camera,
                                                     system_time                   time,
                                                     const float*                  aabb_min_world_ptr,
                                                     const float*                  aabb_max_world_ptr,
                                                     system_matrix4x4*             out_view_matrix_ptr,
                                                     system_matrix4x4*             out_projection_matrix_ptr)
{
    _scene_renderer_sm* shadow_mapping_ptr = reinterpret_cast<_scene_renderer_sm*>(shadow_mapping);

    /* First, we spawn a view matrix.
     *
     * For directional light, the view matrix presents stuff from a fake position of the light.
     * The actual position doesn't really matter, since the real nitty gritty happens when projection
     * matrix is formed.
     *
     * For point lights, we support two different approaches:
     *
     * 1) Cubical rendering:            we use a look-at matrix, where the eye position is set to the light's position,
     *                                  and the look-at point is set according to @param light_target_face.
     * 2) Dual-Paraboloid SM rendering: we use a look-at matrix, where the eye position is set to the light's position,
     *                                  and the look-at point is set according to which side of the paraboloid we are
     *                                  rendering to.
     *
     * For spot light, we use an inverse of the light's transformation matrix for the view matrix.
     * We then proceed with forming the perspective projection matrix, using the cone angle information we have.
     *
     *
     * For projection matrices, there are two cases:
     *
     * For directional / point (cubical case only!) / spot lights: we transfer both frustum AABBs AND the AABB
     *      we're provided to the light's eye space (which usually does funky stuff with the AABB coordinates,
     *      making it an OBB!). We then calculate an AABB by:
     *
     *          1. Computing a max & min AABB for the camera frustum.
     *          2. Intersecting the AABB from 1) with the AABB of the visible part of the scene,
     *             as seen from the camera.
     *
     *      We use the result to compute a projection matrix.
     *
     * For point light DPSM approach, we don't calculate any projection matrix at all. Projection is handled
     * entirely in the vertex shader in this case.
     *
     *
     * Once we have the data, we output both the view and projection matrices, so they can be used to render the
     * shadow maps for the light.
     */
    uint32_t              light_sm_size[2];
    scene_light_type      light_type      = SCENE_LIGHT_TYPE_UNKNOWN;
    static const uint32_t zero_sm_size[2] = {0, 0};

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,
                             light_sm_size);
    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    /* sanity checks */
    ASSERT_DEBUG_SYNC(aabb_min_world_ptr[0] <= aabb_max_world_ptr[0] &&
                      aabb_min_world_ptr[1] <= aabb_max_world_ptr[1] &&
                      aabb_min_world_ptr[2] <= aabb_max_world_ptr[2],
                      "AABB corruption");

    /* Set up camera & lookat positions */
    switch (light_type)
    {
        case SCENE_LIGHT_TYPE_DIRECTIONAL:
        {
            ASSERT_DEBUG_SYNC(light_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D,
                              "Invalid target face requested for directional light SM");

            /* Retrieve the light direction vector */
            float light_direction_vector[3] = {0};

            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_DIRECTION,
                                     light_direction_vector);

            ASSERT_DEBUG_SYNC(fabs(system_math_vector_length3(light_direction_vector) - 1.0f) < 1e-5f,
                              "Light direction vector is not normalized");

            /* Move away from the frustum centroid in the light direction. */
            const float sm_eye_world[3] =
            {
                aabb_min_world_ptr[0] + (aabb_max_world_ptr[0] - aabb_min_world_ptr[0]) * 0.5f,
                aabb_min_world_ptr[1] + (aabb_max_world_ptr[1] - aabb_min_world_ptr[1]) * 0.5f,
                aabb_min_world_ptr[2] + (aabb_max_world_ptr[2] - aabb_min_world_ptr[2]) * 0.5f
            };
            const float sm_lookat_world[3] =
            {
                sm_eye_world[0] - light_direction_vector[0],
                sm_eye_world[1] - light_direction_vector[1],
                sm_eye_world[2] - light_direction_vector[2],
            };

            /* Set up the light's view matrix */
            static const float up_vector[3] = {
                0.0f,
                1.0f,
                0.0f
            };

            *out_view_matrix_ptr = system_matrix4x4_create_lookat_matrix(sm_eye_world,    /* eye_position */
                                                                         sm_lookat_world, /* lookat_point */
                                                                         up_vector);      /* up_vector    */
            break;
        }

        case SCENE_LIGHT_TYPE_POINT:
        {
            /* Identify view direction vector */
            float light_position[3];
            float up_direction  [3] = {0.0f, 0.0f, 0.0f};
            float view_direction[3] = {0.0f, 0.0f, 0.0f};

            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_POSITION,
                                     light_position);


            light_position[0] *= -1.0f;
            light_position[1] *= -1.0f;
            light_position[2] *= -1.0f;

            switch (light_target_face)
            {
                case SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT:
                case SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR:
                {
                    up_direction  [1] =  1.0f;
                    view_direction[2] = -1.0f;

                    break;
                }

                case SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_X:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[0] = -1.0f;

                    break;
                }

                case SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_Y:
                {
                    up_direction  [2] = -1.0f;
                    view_direction[1] = -1.0f;

                    break;
                }

                case SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_Z:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[2] = -1.0f;

                    break;
                }

                case SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_X:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[0] =  1.0f;

                    break;
                }

                case SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_Y:
                {
                    up_direction  [2] = 1.0f;
                    view_direction[1] = 1.0f;

                    break;
                }

                case SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_Z:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[2] =  1.0f;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized target face requested for point light SM");
                }
            }

            /* Move away from the light position in the view direction. */
            const float sm_lookat_world[3] =
            {
                light_position[0] - view_direction[0],
                light_position[1] - view_direction[1],
                light_position[2] - view_direction[2],
            };

            /* Set up the light's view matrix. */
            *out_view_matrix_ptr = system_matrix4x4_create_lookat_matrix(light_position,  /* eye_position */
                                                                         sm_lookat_world, /* lookat_point */
                                                                         up_direction);   /* up_vector    */

            break;
        }

        case SCENE_LIGHT_TYPE_SPOT:
        {
            ASSERT_DEBUG_SYNC(light_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D,
                              "Invalid target face requested for spot light SM");

            /* Calculate view matrix from light's transformation matrix */
            scene_graph_node light_owner_node            = nullptr;
            system_matrix4x4 light_transformation_matrix = nullptr;

            scene_light_get_property     (light,
                                          SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE,
                                         &light_owner_node);
            scene_graph_node_get_property(light_owner_node,
                                          SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                         &light_transformation_matrix);

            *out_view_matrix_ptr = system_matrix4x4_create();

            system_matrix4x4_set_from_matrix4x4(*out_view_matrix_ptr,
                                                light_transformation_matrix);
            system_matrix4x4_invert            (*out_view_matrix_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported light type encountered");
        }
    }

    /* Set up the light's projection matrix */
    scene_camera_set_property(current_camera,
                              SCENE_CAMERA_PROPERTY_VIEWPORT,
                              light_sm_size);

    switch (light_type)
    {
        case SCENE_LIGHT_TYPE_DIRECTIONAL:
        {
            float result_max[3];
            float result_min[3];

            _scene_renderer_sm_get_aabb_for_camera_frustum_and_scene_aabb(current_camera,
                                                                          time,
                                                                          aabb_min_world_ptr,
                                                                          aabb_max_world_ptr,
                                                                         *out_view_matrix_ptr,
                                                                          result_min,
                                                                          result_max);

            scene_camera_set_property(current_camera,
                                      SCENE_CAMERA_PROPERTY_VIEWPORT,
                                      zero_sm_size);

            /* Use the AABB data to compute the reuslt projection matrix.
             *
             * We're doing a bit of heuristical approach with far/near ranges here
             * but this solution seems to do the trick. For some reason, the naive
             * approach goes bollocks under some scenes - I'm probably missing 
             * an important location in obb_light_vertices but couldn't figure out
             * which it might be.
             */
            float max_z;

            if (fabs(result_min[2]) < fabs(result_max[2]) )
            {
                max_z = fabs(result_max[2]);
            }
            else
            {
                max_z = fabs(result_min[2]);
            }

            *out_projection_matrix_ptr = system_matrix4x4_create_ortho_projection_matrix(result_min[0],  /* left   */
                                                                                         result_max[0],  /* right  */
                                                                                         result_min[1],  /* bottom */
                                                                                         result_max[1],  /* top    */
                                                                                        -fabs(max_z),
                                                                                         fabs(max_z) );

            break;
        }

        case SCENE_LIGHT_TYPE_POINT:
        case SCENE_LIGHT_TYPE_SPOT:
        {
            /* Calculate min & max coordinates of an AABB that is an intersection of both
             * the camera frustum and the AABB of the visible part of the scene.
             *
             * Near plane for the shadow map should be made tweakable, since values that are
             * too low will break the algorithm, and values that are too high will cut out
             * geometry which should be taken into account when shading.
             * Far plane is a bit of a heuristical approach, again, but seems to work nicely
             * in a majority of the cases.
             *
             * NOTE: Projection matrix is NOT needed for point lights that use dual-paraboloid
             *       shadow mapping. However, we need to set the far plane for this algorithm,
             *       which means we still need to execute 90% of the code below.
             */
            float result_max[3];
            float result_min[3];
            float spotlight_sm_near_plane;

            _scene_renderer_sm_get_aabb_for_camera_frustum_and_scene_aabb(current_camera,
                                                                          time,
                                                                          aabb_min_world_ptr,
                                                                          aabb_max_world_ptr,
                                                                         *out_view_matrix_ptr,
                                                                          result_min,
                                                                          result_max);

            float min_len = system_math_vector_length3(result_min);
            float max_len = system_math_vector_length3(result_max);

            if (light_type == SCENE_LIGHT_TYPE_SPOT)
            {
                float           cone_angle_half;
                curve_container cone_angle_half_curve;

                scene_light_get_property(light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_SPOTLIGHT_NEAR_PLANE,
                                        &spotlight_sm_near_plane);
                scene_light_get_property (light,
                                          SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF,
                                         &cone_angle_half_curve);
                curve_container_get_value(cone_angle_half_curve,
                                          time,
                                          false, /* should_force */
                                          shadow_mapping_ptr->temp_variant_float);
                system_variant_get_float (shadow_mapping_ptr->temp_variant_float,
                                         &cone_angle_half);

                *out_projection_matrix_ptr = system_matrix4x4_create_perspective_projection_matrix(cone_angle_half * 2.0f,
                                                                                                   1.0f, /* ar - shadow maps are quads */
                                                                                                   spotlight_sm_near_plane,
                                                                                                   max_len + min_len);
            }
            else
            {
                if (light_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT ||
                    light_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR)
                {
                    scene_light_falloff light_falloff = SCENE_LIGHT_FALLOFF_UNKNOWN;

                    scene_light_get_property(light,
                                             SCENE_LIGHT_PROPERTY_FALLOFF,
                                            &light_falloff);

                    if (light_falloff != SCENE_LIGHT_FALLOFF_OFF)
                    {
                        /* If fall-off is defined, we can use light's range as far plane distance */
                        float           light_far_plane   = 0.0f;
                        curve_container light_range_curve = nullptr;

                        scene_light_get_property (light,
                                                  SCENE_LIGHT_PROPERTY_RANGE,
                                                 &light_range_curve);
                        curve_container_get_value(light_range_curve,
                                                  time,
                                                  false,
                                                  shadow_mapping_ptr->temp_variant_float);
                        system_variant_get_float (shadow_mapping_ptr->temp_variant_float,
                                                 &light_far_plane);

                        scene_light_set_property(light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,
                                                &light_far_plane);
                    }
                    else
                    {
                        float pointlight_sm_far_plane;
                        float pointlight_sm_near_plane;

                        scene_light_get_property(light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE,
                                                &pointlight_sm_near_plane);

                        pointlight_sm_far_plane = pointlight_sm_near_plane + max_len + min_len;

                        scene_light_set_property(light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,
                                                &pointlight_sm_far_plane);
                    }
                }
                else
                {
                    /* TODO: We should technically be passing projection matrices on a per-face basis,
                     *       since max_len and min_len are very likely to differ between faces.
                     *       However, this would increase the amount of data passed to the GPU.
                     *       If needed, please consider two solutions:
                     *
                     *       a) Use predefined min/max plane distances.
                     *       b) Expand ogl_uber & shaders_fragment_uber to take new data.
                     */
#ifdef _DEBUG
                    static bool has_logged_warning = false;

                    if (!has_logged_warning)
                    {
                        LOG_ERROR("NOTE: point light projection matrix calculation is potentially flaky."
                                  " See the comment attached to the source code if you're getting glitched shadows.");

                        has_logged_warning = true;
                    }
#endif
                    float pointlight_sm_far_plane;
                    float pointlight_sm_near_plane;

                    scene_light_get_property(light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE,
                                            &pointlight_sm_near_plane);

                    pointlight_sm_far_plane = pointlight_sm_near_plane + max_len + min_len;

                    scene_light_set_property(light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,
                                            &pointlight_sm_far_plane);

                     *out_projection_matrix_ptr = system_matrix4x4_create_perspective_projection_matrix(DEG_TO_RAD(90.0f),
                                                                                                        1.0f, /* ar - shadow maps are quads */
                                                                                                        pointlight_sm_near_plane,
                                                                                                        pointlight_sm_far_plane);
                }
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported light type encountered");
        }
    }
}

/** TODO */
PUBLIC EMERALD_API void scene_renderer_sm_get_property(const scene_renderer_sm    shadow_mapping,
                                                       scene_renderer_sm_property property,
                                                       void*                      out_result_ptr)
{
    const _scene_renderer_sm* shadow_mapping_ptr = reinterpret_cast<const _scene_renderer_sm*>(shadow_mapping);

    switch (property)
    {
        case SCENE_RENDERER_SM_PROPERTY_N_MAX_BLUR_TAPS:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = N_MAX_BLUR_TAPS;

            break;
        }

        case SCENE_RENDERER_SM_PROPERTY_N_MIN_BLUR_TAPS:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = N_MIN_BLUR_TAPS;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_renderer_sm_property value.");
        }
    }
}

/** TODO */
PUBLIC system_hashed_ansi_string scene_renderer_sm_get_special_material_shader_body(scene_renderer_sm_special_material_body_type body_type)
{
    system_hashed_ansi_string result = nullptr;

    switch (body_type)
    {
        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_FS:
        {
            static system_hashed_ansi_string depth_clip_fs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "void main()\n"
                "{\n"
                "}\n");

            result = depth_clip_fs;

            break;
        }

        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_VS:
        {
            static system_hashed_ansi_string depth_clip_and_squared_depth_clip_vs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "uniform VertexShaderProperties\n"
                "{\n"
                "    layout(row_major) mat4 model;\n"
                "    layout(row_major) mat4 vp;\n"
                "};\n"
                "\n"
                "in vec3 object_vertex;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_Position = vp * model * vec4(object_vertex, 1.0);\n"
                "}\n");

            result = depth_clip_and_squared_depth_clip_vs;

            break;
        }

        /* Variance Shadow Mapping for 2D Texture / Cube-map Texture Targets */
        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_FS:
        {
            static system_hashed_ansi_string depth_clip_and_squared_depth_clip_fs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "                     in  vec2 out_vs_depth;\n"
                "layout(location = 0) out vec2 result;\n"
                "\n"
                "uniform FragmentShaderProperties\n"
                "{\n"
                "    float max_variance;\n"
                "};\n"
                "\n"
                "void main()\n"
                "{\n"
                "    float dx               = dFdx(out_vs_depth).x;\n"
                "    float dy               = dFdy(out_vs_depth).x;\n"
                "    float normalized_depth = clamp(out_vs_depth.x / out_vs_depth.y * 0.5 + 0.5, 0.0, 1.0);\n"
                "\n"
                "    result = vec2(normalized_depth,\n"
                /* Use derivatives to account for necessary bias (as per the article in GPU Gems 3).
                 * Note that we parametrize the upper boundary. This turns out to be necessary for
                 * some scenes, where excessive variance causes the derivates to explode. */
                "                  clamp(normalized_depth * normalized_depth + 0.25*(dx * dx + dy * dy), 0.0, max_variance) );\n"
                "}\n");

            result = depth_clip_and_squared_depth_clip_fs;

            break;
        }

        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_VS:
        {
            static system_hashed_ansi_string depth_clip_and_squared_depth_clip_vs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "uniform VertexShaderProperties\n"
                "{\n"
                "    layout(row_major) mat4 model;\n"
                "    layout(row_major) mat4 vp;\n"
                "};\n"
                "\n"
                "in      vec3  object_vertex;\n"
                "out     vec2  out_vs_depth;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_Position  = vp * model * vec4(object_vertex, 1.0);\n"
                "    out_vs_depth = gl_Position.zw;\n"
                "}\n");

            result = depth_clip_and_squared_depth_clip_vs;

            break;
        }

        /* Variance Shadow Mapping for two 2D Texture Targets (dual-paraboloid) */
        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_FS:
        {
            static system_hashed_ansi_string dc_and_squared_dc_dp_fs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "                     in  vec2 out_vs_paraboloid_depth;\n"
                "                     in  vec2 out_vs_depth;\n"
                "layout(location = 0) out vec2 result;\n"
                "\n"
                "uniform FragmentShaderProperties\n"
                "{\n"
                "    float max_variance;\n"
                "};\n"
                "\n"
                "void main()\n"
                "{\n"
                "    float dx         = dFdx(out_vs_depth).x;\n"
                "    float dy         = dFdy(out_vs_depth).x;\n"
                "    float clip_depth = out_vs_depth.x / out_vs_depth.y;\n"
                "\n"
                "    if (clip_depth < 0.0) discard;\n"
                "\n"
                "    float normalized_depth = clamp(out_vs_paraboloid_depth.x / out_vs_paraboloid_depth.y * 0.5 + 0.5, 0.0, 1.0);\n"
                "\n"
                "    result = vec2(normalized_depth,\n"
                /* Use derivatives to account for necessary bias (as per the article in GPU Gems 3).
                 * Note that we parametrize the upper boundary. This turns out to be necessary for
                 * some scenes, where excessive variance causes the derivates to explode. */
                "                  clamp(normalized_depth * normalized_depth, 0.0, max_variance) );\n"
                "}\n");

            result = dc_and_squared_dc_dp_fs;

            break;
        }

        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_VS:
        {
            static system_hashed_ansi_string dc_and_squared_dc_dp_vs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "uniform VertexShaderProperties\n"
                "{\n"
                "                      float far_near_plane_diff;\n"
                "                      float flip_z;\n"
                "    layout(row_major) mat4  model;\n"
                "                      float near_plane;\n"
                "    layout(row_major) mat4  vp;\n"
                "};\n"
                "\n"
                "in      vec3  object_vertex;\n"
                "out     vec2  out_vs_depth;\n"
                "out     vec2  out_vs_paraboloid_depth;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec4 world_vertex = vp * model * vec4(object_vertex, 1.0);\n"
                "    vec4 clip_vertex  = world_vertex;\n"
                "    \n"
                "    clip_vertex.z *= flip_z;\n"
                "    \n"
                "    float light_to_vertex_vec_len = length(clip_vertex.xyz);\n"
                "    \n"
                "    clip_vertex.xyz /= vec3(light_to_vertex_vec_len);\n"
                "    out_vs_depth     = clip_vertex.zw;\n"
                "    clip_vertex.xy  /= vec2(clip_vertex.z + 1.0);\n"
                "    clip_vertex.z    = ((light_to_vertex_vec_len - near_plane) / far_near_plane_diff) * 2.0 - 1.0;\n"
                "    clip_vertex.w    = 1.0;\n"
                "    \n"
                "    gl_Position             = clip_vertex;\n"
                "    out_vs_paraboloid_depth = clip_vertex.zw;\n"
                "}\n");

            result = dc_and_squared_dc_dp_vs;

            break;
        }


        /* Plain Shadow Mapping for 2D texture / Cube-Map texture targets: */
        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_FS:
        {
            static system_hashed_ansi_string depth_clip_dual_paraboloid_fs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "in float clip_depth;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    if (clip_depth < 0.0) discard;\n"
                "}\n");

            result = depth_clip_dual_paraboloid_fs;

            break;
        }

        case SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_VS:
        {
            static system_hashed_ansi_string depth_clip_dual_paraboloid_vs = system_hashed_ansi_string_create(
                "#version 430 core\n"
                "\n"
                "uniform VertexShaderProperties\n"
                "{\n"
                "                      float far_near_plane_diff;\n"
                "                      float flip_z;\n"
                "    layout(row_major) mat4  model;\n"
                "                      float near_plane;\n"
                "    layout(row_major) mat4  vp;\n"
                "};\n"
                "\n"
                "out     float clip_depth;\n"
                "in      vec3  object_vertex;\n"
                "out     vec2  out_vs_depth;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec4 world_vertex = vp * model * vec4(object_vertex, 1.0);\n"
                "    vec4 clip_vertex  = world_vertex;\n"
                "    \n"
                "    clip_vertex.z *= flip_z;\n"
                "    \n"
                "    float light_to_vertex_vec_len = length(clip_vertex.xyz);\n"
                "    \n"
                "    clip_vertex.xyz /= vec3(light_to_vertex_vec_len);\n"
                "    clip_depth       = clip_vertex.z;\n"
                "    clip_vertex.xy  /= vec2(clip_vertex.z + 1.0);\n"
                "    clip_vertex.z    = ((light_to_vertex_vec_len - near_plane) / far_near_plane_diff) * 2.0 - 1.0;\n"
                "    clip_vertex.w    = 1.0;\n"
                "    \n"
                "    gl_Position = clip_vertex;\n"
                "}\n");

            result = depth_clip_dual_paraboloid_vs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_renderer_sm_special_material_body_type value");
        }
    }

    return result;
}

/** TODO */
PUBLIC void scene_renderer_sm_process_mesh_for_shadow_map_rendering(scene_mesh scene_mesh_instance,
                                                                    void*      renderer_raw_ptr)
{
    ral_context                   context                       = nullptr;
    mesh                          mesh_gpu                      = nullptr;
    mesh                          mesh_instantiation_parent_gpu = nullptr;
    bool                          mesh_is_shadow_caster         = false;
    _scene_renderer_sm_mesh_item* new_mesh_item                 = nullptr;
    scene_renderer                renderer                      = reinterpret_cast<scene_renderer>(renderer_raw_ptr);
    system_matrix4x4              renderer_current_model_matrix = nullptr;
    _scene_renderer_sm*           shadow_mapping_ptr            = nullptr;

    scene_renderer_get_property(renderer,
                                SCENE_RENDERER_PROPERTY_CONTEXT_RAL,
                               &context);
    scene_renderer_get_property(renderer,
                                SCENE_RENDERER_PROPERTY_SHADOW_MAPPING_MANAGER,
                               &shadow_mapping_ptr);
    scene_mesh_get_property    (scene_mesh_instance,
                                SCENE_MESH_PROPERTY_IS_SHADOW_CASTER,
                               &mesh_is_shadow_caster);

    if (!mesh_is_shadow_caster)
    {
        /* Do not render if not a shadow caster! */
        goto end;
    }

    scene_mesh_get_property(scene_mesh_instance,
                            SCENE_MESH_PROPERTY_MESH,
                           &mesh_gpu);
    mesh_get_property(mesh_gpu,
                      MESH_PROPERTY_INSTANTIATION_PARENT,
                     &mesh_instantiation_parent_gpu);

    if (mesh_instantiation_parent_gpu == nullptr)
    {
        mesh_instantiation_parent_gpu = mesh_gpu;
    }

    /* Perform frustum culling to make sure it actually makes sense to render
     * this mesh.
     *
     * NOTE: For point lights which use DPSM shadow mapping, we need to use
     *       a different culling behavior, since the projection is performed
     *       in VS for that specific technique.
     */
    if (shadow_mapping_ptr->current_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT ||
        shadow_mapping_ptr->current_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR)
    {
        /* This is a DPSM shadow map generation pass */
        float culling_behavior_data[6];

        scene_light_get_property(shadow_mapping_ptr->current_light,
                                 SCENE_LIGHT_PROPERTY_POSITION,
                                 culling_behavior_data);

        culling_behavior_data[3] = 0.0f;
        culling_behavior_data[4] = 0.0f;
        culling_behavior_data[5] = (shadow_mapping_ptr->current_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT) ? 1.0f : -1.0f;

        if (!scene_renderer_cull_against_frustum( (scene_renderer) renderer,
                                                  mesh_instantiation_parent_gpu,
                                                  SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_PASS_OBJECTS_IN_FRONT_OF_CAMERA,
                                                  culling_behavior_data) )
        {
            goto end;
        }
    }
    else
    {
        /* Not a DPSM shadow map generation pass */
        if (!scene_renderer_cull_against_frustum( (scene_renderer) renderer,
                                                  mesh_instantiation_parent_gpu,
                                                  SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES,
                                                  nullptr) ) /* behavior_data */
        {
            goto end;
        }
    }

    /* This is a visible mesh we should store. As opposed to the uber->mesh map we're
     * using for forward rendering, in "no rasterization" mode there is only one uber
     * that is used for the rendeirng process.
     * However, if we were to re-use the same map as in the other modes, we'd need
     * to clean it every scene_renderer_render_scene_graph() call which could
     * affect performance. It would also disrupt the shadow mapping baking.
     * Hence, for the "no rasterization mode", we use a renderer-level vector to store
     * visible meshes. This will work, as long the "no rasterization mode" does not
     * call graph rendeirng entry point from itself, which should never be the case.
     */
    new_mesh_item = reinterpret_cast<_scene_renderer_sm_mesh_item*>(system_resource_pool_get_from_pool(shadow_mapping_ptr->mesh_item_pool) );

    ASSERT_ALWAYS_SYNC(new_mesh_item != nullptr,
                       "Out of memory");

    scene_renderer_get_property(renderer,
                                SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX,
                               &renderer_current_model_matrix);

    new_mesh_item->mesh_instance = mesh_gpu;
    new_mesh_item->model_matrix  = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4(new_mesh_item->model_matrix,
                                        renderer_current_model_matrix);

    system_resizable_vector_push(shadow_mapping_ptr->meshes_to_render,
                                 new_mesh_item);

end:
    ;
}

/** Please see header for spec */
PUBLIC void scene_renderer_sm_release(scene_renderer_sm handler)
{
    _scene_renderer_sm* handler_ptr = reinterpret_cast<_scene_renderer_sm*>(handler);

    _scene_renderer_sm_deinit_samplers(handler_ptr);

    delete handler_ptr;
    handler_ptr = nullptr;
}

/** TODO */
PUBLIC ral_present_task scene_renderer_sm_render_shadow_map_meshes(scene_renderer_sm                shadow_mapping,
                                                                   scene_renderer                   renderer,
                                                                   scene                            scene,
                                                                   system_time                      frame_time,
                                                                   const ral_gfx_state_create_info* ref_gfx_state_create_info_ptr)
{
    scene_renderer_materials materials          = nullptr;
    ral_present_task         result_task        = nullptr;
    _scene_renderer_sm*      shadow_mapping_ptr = reinterpret_cast<_scene_renderer_sm*>(shadow_mapping);
    mesh_material            sm_material        = nullptr;
    scene_renderer_uber      sm_material_uber   = nullptr;

    demo_app_get_property(DEMO_APP_PROPERTY_MATERIAL_MANAGER,
                         &materials);

    /* Retrieve the material and associated uber, which
     * should be used for the rendering process. */
    scene_light_shadow_map_algorithm          sm_algo             = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
    scene_renderer_materials_special_material sm_special_material = SPECIAL_MATERIAL_UNKNOWN;

    scene_light_get_property(shadow_mapping_ptr->current_light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &sm_algo);

    if (shadow_mapping_ptr->current_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT ||
        shadow_mapping_ptr->current_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR)
    {
        if (sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN)
        {
            sm_special_material = SPECIAL_MATERIAL_DEPTH_CLIP_DUAL_PARABOLOID;
        }
        else
        {
            ASSERT_DEBUG_SYNC(sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM,
                              "Sanity check failed");

            sm_special_material = SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED_DUAL_PARABOLOID;
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN ||
                          sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM,
                          "Unrecognized SM algorithm type");

        if (sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN)
        {
            sm_special_material = SPECIAL_MATERIAL_DEPTH_CLIP;
        }
        else
        {
            sm_special_material = SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED;
        }
    }

    sm_material      = scene_renderer_materials_get_special_material(materials,
                                                                     shadow_mapping_ptr->context,
                                                                     sm_special_material);
    sm_material_uber = mesh_material_get_uber                       (sm_material,
                                                                     scene,
                                                                     false); /* use_shadow_maps */

    /* Configure the uber.
     *
     * NOTE: Since we're rendering a shadow map, far & near planes
     *       are configured relative to the light space.
     */
    scene_light_shadow_map_algorithm light_sm_algorithm;
    scene_light_type                 light_type;

    scene_light_get_property(shadow_mapping_ptr->current_light,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &light_sm_algorithm);
    scene_light_get_property(shadow_mapping_ptr->current_light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    if (light_sm_algorithm == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM)
    {
        float light_vsm_max_variance;

        scene_light_get_property(shadow_mapping_ptr->current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_MAX_VARIANCE,
                                &light_vsm_max_variance);

        scene_renderer_uber_set_shader_general_property(sm_material_uber,
                                                        SCENE_RENDERER_UBER_GENERAL_PROPERTY_VSM_MAX_VARIANCE,
                                                       &light_vsm_max_variance);
    }

    if (light_type == SCENE_LIGHT_TYPE_POINT)
    {
        scene_light_shadow_map_pointlight_algorithm sm_algorithm;

        scene_light_get_property(shadow_mapping_ptr->current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                &sm_algorithm);

        if (sm_algorithm == SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID)
        {
            float light_far_near_plane_diff;
            float light_far_plane;
            float light_flip_z = (shadow_mapping_ptr->current_target_face == SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR) ? -1.0f : 1.0f;
            float light_near_plane;

            scene_light_get_property(shadow_mapping_ptr->current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,
                                    &light_far_plane);
            scene_light_get_property(shadow_mapping_ptr->current_light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE,
                                    &light_near_plane);

            ASSERT_DEBUG_SYNC(light_far_plane >= light_near_plane,
                              "Light clipping planes are incorrect");

            light_far_near_plane_diff = light_far_plane - light_near_plane;

            ASSERT_DEBUG_SYNC(light_far_near_plane_diff > 0.0f,
                              "(Light far plane distance - near plane distance) is invalid.");

            scene_renderer_uber_set_shader_general_property(sm_material_uber,
                                                            SCENE_RENDERER_UBER_GENERAL_PROPERTY_FAR_NEAR_PLANE_DIFF,
                                                           &light_far_near_plane_diff);
            scene_renderer_uber_set_shader_general_property(sm_material_uber,
                                                            SCENE_RENDERER_UBER_GENERAL_PROPERTY_FLIP_Z,
                                                           &light_flip_z);
            scene_renderer_uber_set_shader_general_property(sm_material_uber,
                                                            SCENE_RENDERER_UBER_GENERAL_PROPERTY_NEAR_PLANE,
                                                           &light_near_plane);
        }
    }

    /* Set VP */
    system_matrix4x4 vp = nullptr;

    scene_renderer_get_property(renderer,
                                SCENE_RENDERER_PROPERTY_VP,
                               &vp);

    scene_renderer_uber_set_shader_general_property(sm_material_uber,
                                                    SCENE_RENDERER_UBER_GENERAL_PROPERTY_VP,
                                                    vp);

    /* Kick off the rendering */
    uint32_t                       n_meshes = 0;
    scene_renderer_uber_start_info uber_rendering_start_info;

    system_resizable_vector_get_property(shadow_mapping_ptr->meshes_to_render,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_meshes);

    ASSERT_DEBUG_SYNC(shadow_mapping_ptr->current_sm_color0_texture_view != nullptr ||
                      shadow_mapping_ptr->current_sm_depth_texture_view  != nullptr,
                      "About to start rendering shadow map meshes without render-targets assigned");

    uber_rendering_start_info.color_rt = shadow_mapping_ptr->current_sm_color0_texture_view;
    uber_rendering_start_info.depth_rt = shadow_mapping_ptr->current_sm_depth_texture_view;

    scene_renderer_uber_rendering_start(sm_material_uber,
                                       &uber_rendering_start_info);
    {
        for (uint32_t n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
        {
            _scene_renderer_sm_mesh_item* item_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(shadow_mapping_ptr->meshes_to_render,
                                                        n_mesh,
                                                       &item_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve a mesh descriptor at index [%d]",
                                  n_mesh);

                continue;
            }

            scene_renderer_uber_render_mesh(item_ptr->mesh_instance,
                                            item_ptr->model_matrix,
                                            nullptr,                    /* normal_matrix */
                                            sm_material_uber,
                                            nullptr,                    /* material */
                                            frame_time,
                                            ref_gfx_state_create_info_ptr);
        }
    }
    result_task = scene_renderer_uber_rendering_stop(sm_material_uber);

    /* Clean up */
    _scene_renderer_sm_mesh_item* item_ptr = nullptr;

    while (system_resizable_vector_pop(shadow_mapping_ptr->meshes_to_render,
                                      &item_ptr) )
    {
        if (item_ptr->model_matrix != nullptr)
        {
            system_matrix4x4_release(item_ptr->model_matrix);
        }

        system_resource_pool_return_to_pool(shadow_mapping_ptr->mesh_item_pool,
                                            (system_resource_pool_block) item_ptr);
    }

    return result_task;
}

/** Please see header for spec */
PUBLIC ral_present_task scene_renderer_sm_render_shadow_maps(scene_renderer_sm shadow_mapping,
                                                             scene_renderer    renderer,
                                                             scene             current_scene,
                                                             scene_camera      target_camera,
                                                             system_time       frame_time)
{
    scene_graph                     graph                          = nullptr;
    uint32_t                        n_lights                       = 0;
    uint32_t                        n_result_present_task_mappings = 0;
    ral_present_task*               render_present_tasks           = nullptr;
    ral_present_task                result_present_task            = nullptr;
    ral_present_task_group_mapping* result_present_task_mappings   = nullptr;
    _scene_renderer_sm*             shadow_mapping_ptr             = reinterpret_cast<_scene_renderer_sm*>(shadow_mapping);

    scene_get_property(current_scene,
                       SCENE_PROPERTY_GRAPH,
                      &graph);
    scene_get_property(current_scene,
                       SCENE_PROPERTY_N_LIGHTS,
                      &n_lights);

    ASSERT_DEBUG_SYNC(n_lights > 0,
                      "Can't render shadow maps if there's no lights in the scene");

    const uint32_t n_max_render_present_tasks  = n_lights                   * 6; /* all CM faces     - worst-case complexity */
    const uint32_t n_max_result_mappings       = n_max_render_present_tasks * 2; /* color + depth RT - worst-case scenario   */
    uint32_t       n_render_present_tasks_used = 0;

    render_present_tasks         = reinterpret_cast<ral_present_task*>              (_malloca(sizeof(ral_present_task)               * n_max_render_present_tasks));
    result_present_task_mappings = reinterpret_cast<ral_present_task_group_mapping*>(_malloca(sizeof(ral_present_task_group_mapping) * n_max_result_mappings));

    /* Stash target camera before continuing */
    shadow_mapping_ptr->current_camera = target_camera;

    /* Before we can proceed, we need to know what the AABB (expressed in the world space)
     * for the scene, as seen from the camera viewpoint, is. */
    float current_aabb_max_setting[3];
    float current_aabb_min_setting[3];

    static const float default_aabb_max_setting[] =
    {
        DEFAULT_AABB_MAX_VALUE,
        DEFAULT_AABB_MAX_VALUE,
        DEFAULT_AABB_MAX_VALUE
    };
    static const float default_aabb_min_setting[] =
    {
        DEFAULT_AABB_MIN_VALUE,
        DEFAULT_AABB_MIN_VALUE,
        DEFAULT_AABB_MIN_VALUE
    };

    scene_renderer_set_property(renderer,
                                SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX,
                                default_aabb_max_setting);
    scene_renderer_set_property(renderer,
                                SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN,
                                default_aabb_min_setting);

    scene_graph_traverse(graph,
                         scene_renderer_update_current_model_matrix,
                         nullptr, /* insert_camera_proc */
                         scene_renderer_update_light_properties,
                         _scene_renderer_sm_process_mesh_for_shadow_map_pre_pass,
                         renderer,
                         frame_time);

    /* Even if no meshes were found to be visible, we still need to clear the SM.
     * Zero out visible AABB so that scene_renderer routines do not get confused
     * later on. */
    scene_renderer_get_property(renderer,
                                SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX,
                                current_aabb_max_setting);
    scene_renderer_get_property(renderer,
                                SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN,
                                current_aabb_min_setting);

    if (current_aabb_max_setting[0] == DEFAULT_AABB_MAX_VALUE &&
        current_aabb_max_setting[1] == DEFAULT_AABB_MAX_VALUE &&
        current_aabb_max_setting[2] == DEFAULT_AABB_MAX_VALUE &&
        current_aabb_min_setting[0] == DEFAULT_AABB_MIN_VALUE &&
        current_aabb_min_setting[1] == DEFAULT_AABB_MIN_VALUE &&
        current_aabb_min_setting[2] == DEFAULT_AABB_MIN_VALUE)
    {
        static const float default_aabb_zero_setting[] =
        {
            0.0f,
            0.0f,
            0.0f
        };

        scene_renderer_set_property(renderer,
                                    SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX,
                                    default_aabb_zero_setting);
        scene_renderer_set_property(renderer,
                                    SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN,
                                    default_aabb_zero_setting);

        memcpy(current_aabb_max_setting,
               default_aabb_zero_setting,
               sizeof(current_aabb_max_setting) );
        memcpy(current_aabb_min_setting,
               default_aabb_zero_setting,
               sizeof(current_aabb_max_setting) );
    }

    /* Iterate over all lights defined for the scene. Focus only on those,
     * which act as shadow casters.. */
    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light                      current_light                  = scene_get_light_by_index(current_scene,
                                                                                                   n_light);
        bool                             current_light_is_shadow_caster = false;
        scene_light_shadow_map_algorithm current_light_sm_algorithm     = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
        scene_light_type                 current_light_type             = SCENE_LIGHT_TYPE_UNKNOWN;
        bool                             current_light_uses_color_rt    = false;
        bool                             current_light_uses_depth_rt    = false;

        ASSERT_DEBUG_SYNC(current_light != nullptr,
                          "Scene light is nullptr");

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_TYPE,
                                &current_light_type);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                                &current_light_sm_algorithm);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                &current_light_is_shadow_caster);

        if (current_light_is_shadow_caster)
        {
            /* For directional/spot lights, we only need a single iteration.
             * For point lights, the specific number is algorithm-specific.
             */
            const uint32_t n_sm_passes = _scene_renderer_sm_get_number_of_sm_passes(current_light,
                                                                                    current_light_type);

            for (unsigned int n_sm_pass = 0;
                              n_sm_pass < n_sm_passes;
                            ++n_sm_pass)
            {
                /* Determine which face we should be rendering to right now. */
                scene_renderer_sm_target_face current_target_face;

                if (current_light_type == SCENE_LIGHT_TYPE_POINT)
                {
                    scene_light_shadow_map_pointlight_algorithm current_light_point_sm_algorithm;

                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                            &current_light_point_sm_algorithm);

                    current_target_face = _scene_renderer_sm_get_target_face_for_point_light(current_light_point_sm_algorithm,
                                                                                             n_sm_pass);
                }
                else
                {
                    current_target_face = _scene_renderer_sm_get_target_face_for_nonpoint_light(n_sm_pass);
                }

                /* Record a command buffer which is going to render the depth textures */
                system_matrix4x4 light_projection_matrix = nullptr;
                system_matrix4x4 light_view_matrix       = nullptr;
                system_matrix4x4 light_vp_matrix         = nullptr;
                system_matrix4x4 sm_projection_matrix    = nullptr;
                system_matrix4x4 sm_view_matrix          = nullptr;

                ASSERT_DEBUG_SYNC(shadow_mapping_ptr->current_command_buffer == nullptr,
                                  "Another command buffer is already being recorded.");

                shadow_mapping_ptr->current_command_buffer = _scene_renderer_sm_start(shadow_mapping_ptr,
                                                                                      current_light,
                                                                                      current_target_face);
                {
                    if (n_sm_pass == 0)
                    {
                        ral_texture_view sm_color_texture_view = nullptr;
                        ral_texture_view sm_depth_texture_view = nullptr;

                        scene_light_get_property(current_light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_COLOR_RAL,
                                                &sm_color_texture_view);
                        scene_light_get_property(current_light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_DEPTH_RAL,
                                                &sm_depth_texture_view);

                        current_light_uses_color_rt = (sm_color_texture_view != nullptr);
                        current_light_uses_depth_rt = (sm_depth_texture_view != nullptr);
                    }

                    if (current_aabb_min_setting[0] != current_aabb_max_setting[0] ||
                        current_aabb_min_setting[1] != current_aabb_max_setting[1] ||
                        current_aabb_min_setting[2] != current_aabb_max_setting[2])
                    {
                        switch (current_light_type)
                        {
                            case SCENE_LIGHT_TYPE_DIRECTIONAL:
                            case SCENE_LIGHT_TYPE_POINT:
                            case SCENE_LIGHT_TYPE_SPOT:
                            {
                                /* NOTE: sm_projeciton_matrix may still be nullptr after this function leaves
                                 *       for some cases!
                                 */
                                scene_renderer_sm_get_matrices_for_light(shadow_mapping,
                                                                         current_light,
                                                                         current_target_face,
                                                                         target_camera,
                                                                         frame_time,
                                                                         current_aabb_min_setting,
                                                                         current_aabb_max_setting,
                                                                        &sm_view_matrix,
                                                                        &sm_projection_matrix);

                                break;
                            }

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Unsupported light type encountered.");
                            }
                        }

                        /* Update light's shadow VP matrix */
                        ASSERT_DEBUG_SYNC(sm_view_matrix != nullptr,
                                          "View matrix for shadow map rendering is nullptr");

                        if (current_target_face != SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT &&
                            current_target_face != SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR)
                        {
                            ASSERT_DEBUG_SYNC(sm_projection_matrix != nullptr,
                                              "Projection matrix for shadow map rendering is nullptr");
                        }
                        else
                        {
                            sm_projection_matrix = system_matrix4x4_create();

                            system_matrix4x4_set_to_identity(sm_projection_matrix);
                        }

                        scene_light_get_property(current_light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_VIEW,
                                                &light_view_matrix);
                        scene_light_get_property(current_light,
                                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP,
                                                &light_vp_matrix);

                        system_matrix4x4_set_from_matrix4x4(light_view_matrix,
                                                            sm_view_matrix);

                        if (sm_projection_matrix != nullptr)
                        {
                            scene_light_get_property(current_light,
                                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_PROJECTION,
                                                    &light_projection_matrix);

                            /* Projection matrix */
                            system_matrix4x4_set_from_matrix4x4(light_projection_matrix,
                                                                sm_projection_matrix);

                            /* VP matrix */
                            system_matrix4x4_set_from_matrix4x4   (light_vp_matrix,
                                                                   sm_projection_matrix);
                            system_matrix4x4_multiply_by_matrix4x4(light_vp_matrix,
                                                                   sm_view_matrix);
                        }
                        else
                        {
                            /* VP matrix is actually a view matrix in this case */
                            system_matrix4x4_set_from_matrix4x4(light_vp_matrix,
                                                                sm_view_matrix);
                        }

                        /* NOTE: This call is recursive (this function is called by exactly the same function,
                         *       but we're requesting no shadow maps this time AND the scene graph has already
                         *       been traversed, so it should be fairly inexpensive and focus solely on drawing
                         *       geometry.
                         *
                         * NOTE: This call will lead to a call back to scene_renderer_sm_render_shadow_map_meshes().
                         *       Since there's no way we could include additional info which would be rerouted to
                         *       that call-back, we store current target face in scene_renderer_sm instance.
                         */
                        shadow_mapping_ptr->current_light       = current_light;
                        shadow_mapping_ptr->current_target_face = current_target_face;

                        scene_renderer_render_scene_graph(renderer,
                                                          sm_view_matrix,
                                                          sm_projection_matrix,
                                                          target_camera,
                                                          RENDER_MODE_SHADOW_MAP,
                                                          false, /* apply_shadow_mapping */
                                                          HELPER_VISUALIZATION_NONE,
                                                          frame_time);
                   }

                   /* Clean up */
                   if (sm_projection_matrix != nullptr)
                   {
                        system_matrix4x4_release(sm_projection_matrix);
                        sm_projection_matrix = nullptr;
                   }

                   if (sm_view_matrix != nullptr)
                   {
                        system_matrix4x4_release(sm_view_matrix);
                        sm_view_matrix = nullptr;
                   }
                }

                if (current_light_uses_color_rt)
                {
                    ral_present_task_group_mapping& current_mapping = result_present_task_mappings[n_result_present_task_mappings];

                    current_mapping.n_present_task        = n_render_present_tasks_used;
                    current_mapping.present_task_io_index = 0;
                    current_mapping.unique_output_index   = n_light * 2 + 0;

                    n_result_present_task_mappings++;
                }

                if (current_light_uses_depth_rt)
                {
                    ral_present_task_group_mapping& current_mapping = result_present_task_mappings[n_result_present_task_mappings];

                    current_mapping.n_present_task        = n_render_present_tasks_used;
                    current_mapping.present_task_io_index = current_light_uses_color_rt ? 1 : 0;
                    current_mapping.unique_output_index   = n_light * 2 + 1;

                    n_result_present_task_mappings++;
                }

                render_present_tasks[n_render_present_tasks_used++] = _scene_renderer_sm_stop(shadow_mapping_ptr,
                                                                                              current_light);

                ASSERT_DEBUG_SYNC(n_render_present_tasks_used != n_max_render_present_tasks,
                                  "Bump up the n_max_render_present_tasks constant.");
                ASSERT_DEBUG_SYNC(n_result_present_task_mappings != n_max_result_mappings,
                                  "Bump up the n_max_result_mappings constant.");
            }
        }
    }

    /* Form the final present task */
    ral_present_task_group_create_info result_present_task_create_info;

    result_present_task_create_info.ingroup_connections                      = nullptr;
    result_present_task_create_info.n_ingroup_connections                    = 0;
    result_present_task_create_info.n_present_tasks                          = n_render_present_tasks_used;
    result_present_task_create_info.n_total_unique_inputs                    = 0;
    result_present_task_create_info.n_total_unique_outputs                   = n_lights * 2;
    result_present_task_create_info.n_unique_input_to_ingroup_task_mappings  = 0;
    result_present_task_create_info.n_unique_output_to_ingroup_task_mappings = n_result_present_task_mappings;
    result_present_task_create_info.present_tasks                            = render_present_tasks;
    result_present_task_create_info.unique_input_to_ingroup_task_mapping     = nullptr;
    result_present_task_create_info.unique_output_to_ingroup_task_mapping    = result_present_task_mappings;

    result_present_task = ral_present_task_create_group(&result_present_task_create_info);

    shadow_mapping_ptr->current_camera = nullptr;

    return result_present_task;
}
