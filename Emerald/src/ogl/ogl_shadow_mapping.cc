/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shader_constructor.h"
#include "ogl/ogl_textures.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_variant.h"
#include "system/system_window.h"
#include <string>
#include <sstream>


/** TODO */
typedef struct _ogl_shadow_mapping
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    GLuint         fbo_id;             /* FBO used to render depth data to light-specific depth texture. */
    system_variant temp_variant_float;

    /* Tells what ogl_texture is currently being used for SM rendering.
     *
     * This only makes sense if is_enabled is true.
     */
    ogl_texture current_sm_texture;

    /* Tells whether ogl_shadow_mapping has been toggled on.
     *
     * This is useful for point lights, for which we need to re-configure
     * the render-target at least twice to render a single shadow map.
     * When is_enabled is true and ogl_shadow_mapping_toggle() is called,
     * the SM texture set-up code is skipped.
     */
    bool is_enabled;

    _ogl_shadow_mapping()
    {
        context            = NULL;
        current_sm_texture = NULL;
        fbo_id             = 0;
        is_enabled         = false;
        temp_variant_float = system_variant_create(SYSTEM_VARIANT_FLOAT);
    }

} _ogl_shadow_mapping;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_shadow_mapping_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_shadow_mapping_verify_context_type(x)
#endif


/** TODO */
 PRIVATE void _ogl_shadow_mapping_get_aabb_for_camera_frustum_and_scene_aabb(__in            __notnull scene_camera         current_camera,
                                                                             __in                      system_timeline_time time,
                                                                             __in_ecount(3)  __notnull const float*         aabb_min_world,
                                                                             __in_ecount(3)  __notnull const float*         aabb_max_world,
                                                                             __in            __notnull system_matrix4x4     view_matrix,
                                                                             __out_ecount(3) __notnull float*               result_min,
                                                                             __out_ecount(3) __notnull float*               result_max)
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
        system_matrix4x4 current_camera_model_matrix      = NULL;
        scene_graph_node current_camera_node              = NULL;

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

    result_max[0] = obb_camera_frustum_vertices[0][0];
    result_max[1] = obb_camera_frustum_vertices[0][1];
    result_max[2] = obb_camera_frustum_vertices[0][2];
    result_min[0] = obb_camera_frustum_vertices[0][0];
    result_min[1] = obb_camera_frustum_vertices[0][1];
    result_min[2] = obb_camera_frustum_vertices[0][2];

    for (uint32_t n_obb_camera_frustum_vertex = 1; /* skip the first entry */
                  n_obb_camera_frustum_vertex < n_obb_camera_frustum_vertices;
                ++n_obb_camera_frustum_vertex)
    {
        if (result_max[0] < obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0])
        {
            result_max[0] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0];
        }
        if (result_min[0] > obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0])
        {
            result_min[0] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][0];
        }

        if (result_max[1] < obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1])
        {
            result_max[1] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1];
        }
        if (result_min[1] > obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1])
        {
            result_min[1] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][1];
        }

        if (result_max[2] < obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2])
        {
            result_max[2] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2];
        }
        if (result_min[2] > obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2])
        {
            result_min[2] = obb_camera_frustum_vertices[n_obb_camera_frustum_vertex][2];
        }
    } /* for (all vertices) */

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
        if (result_max[0] > obb_world_vertices[n_obb_world_vertex][0])
        {
            result_max[0] = obb_world_vertices[n_obb_world_vertex][0];
        }
        if (result_min[0] < obb_world_vertices[n_obb_world_vertex][0])
        {
            result_min[0] = obb_world_vertices[n_obb_world_vertex][0];
        }

        if (result_max[1] > obb_world_vertices[n_obb_world_vertex][1])
        {
            result_max[1] = obb_world_vertices[n_obb_world_vertex][1];
        }
        if (result_min[1] < obb_world_vertices[n_obb_world_vertex][1])
        {
            result_min[1] = obb_world_vertices[n_obb_world_vertex][1];
        }

        if (result_max[2] > obb_world_vertices[n_obb_world_vertex][2])
        {
            result_max[2] = obb_world_vertices[n_obb_world_vertex][2];
        }
        if (result_min[2] < obb_world_vertices[n_obb_world_vertex][2])
        {
            result_min[2] = obb_world_vertices[n_obb_world_vertex][2];
        }
    } /* for (all vertices) */

    /* Min/max values may have been shuffled. Make sure the order is in place. */
    if (result_min[0] > result_max[0])
    {
        float temp = result_max[0];

        result_max[0] = result_min[0];
        result_min[0] = temp;
    }
    if (result_min[1] > result_max[1])
    {
        float temp = result_max[1];

        result_max[1] = result_min[1];
        result_min[1] = temp;
    }
    if (result_min[2] > result_max[2])
    {
        float temp = result_max[2];

        result_max[2] = result_min[2];
        result_min[2] = temp;
    }

    ASSERT_DEBUG_SYNC(result_min[0] <= result_max[0] &&
                      result_min[1] <= result_max[1] &&
                      result_min[2] <= result_max[2],
                      "Something's nuts with the OBB min/max calcs");
}

/** TODO */
PRIVATE void _ogl_shadow_mapping_get_texture_targets_from_target_face(__in            ogl_shadow_mapping_target_face target_face,
                                                                      __out __notnull GLenum*                        out_general_texture_target,
                                                                      __out __notnull GLenum*                        out_detailed_texture_target)
{
    switch (target_face)
    {
        case OGL_SHADOW_MAPPING_TARGET_FACE_2D:
        {
            *out_general_texture_target  = GL_TEXTURE_2D;
            *out_detailed_texture_target = GL_TEXTURE_2D;

            break;
        }

        case OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_X:
        {
            *out_general_texture_target  = GL_TEXTURE_CUBE_MAP;
            *out_detailed_texture_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;

            break;
        }

        case OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_Y:
        {
            *out_general_texture_target  = GL_TEXTURE_CUBE_MAP;
            *out_detailed_texture_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;

            break;
        }

        case OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_Z:
        {
            *out_general_texture_target  = GL_TEXTURE_CUBE_MAP;
            *out_detailed_texture_target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;

            break;
        }

        case OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_X:

        {
            *out_general_texture_target  = GL_TEXTURE_CUBE_MAP;
            *out_detailed_texture_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

            break;
        }

        case OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_Y:

        {
            *out_general_texture_target  = GL_TEXTURE_CUBE_MAP;
            *out_detailed_texture_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;

            break;
        }

        case OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_Z:
        {
            *out_general_texture_target  = GL_TEXTURE_CUBE_MAP;
            *out_detailed_texture_target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_shadow_mapping_target_face value");
        }
    } /* switch (target_face) */
}

/** TODO */
PRIVATE void _ogl_shadow_mapping_init_renderer_callback(__in __notnull ogl_context context,
                                                        __in __notnull void*       shadow_mapping)
{
    ogl_context_gl_entrypoints* entry_points       = NULL;
    _ogl_shadow_mapping*        shadow_mapping_ptr = (_ogl_shadow_mapping*) shadow_mapping;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Generate a FBO id. */
    entry_points->pGLGenFramebuffers(1,
                                    &shadow_mapping_ptr->fbo_id);

    /* Set up the FBO state */
    entry_points->pGLBindFramebuffer(GL_FRAMEBUFFER,
                                     shadow_mapping_ptr->fbo_id);
    entry_points->pGLDrawBuffer     (GL_NONE);

    entry_points->pGLBindFramebuffer(GL_FRAMEBUFFER,
                                     0);
}

/** TODO */
PRIVATE void _ogl_shadow_mapping_release_renderer_callback(__in __notnull ogl_context context,
                                                           __in __notnull void*       shadow_mapping)
{
    ogl_context_gl_entrypoints* entry_points       = NULL;
    _ogl_shadow_mapping*        shadow_mapping_ptr = (_ogl_shadow_mapping*) shadow_mapping;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Release the FBO */
    if (shadow_mapping_ptr->fbo_id != 0)
    {
        entry_points->pGLDeleteFramebuffers(1,
                                           &shadow_mapping_ptr->fbo_id);

        shadow_mapping_ptr->fbo_id = 0;
    }
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_shadow_mapping_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_shadow_mapping is only supported under GL contexts")
    }
#endif


/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_adjust_fragment_uber_code(__in  __notnull ogl_shader_constructor           shader_constructor_fs,
                                                         __in            uint32_t                         n_light,
                                                         __in            scene_light_shadow_map_bias      sm_bias,
                                                         __in            _uniform_block_id                ub_fs,
                                                         __in            shaders_fragment_uber_light_type light_type,
                                                         __in            system_hashed_ansi_string        light_vector_norm_var_name,
                                                         __in            system_hashed_ansi_string        light_vector_non_norm_var_name,
                                                         __out __notnull system_hashed_ansi_string*       out_visibility_var_name)
{
    const bool        is_point_light                            = (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
                                                                   light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT);
    std::stringstream camera_eye_to_light_eye_var_name_sstream;
    std::stringstream light_projection_matrix_var_name_sstream;
    std::stringstream light_shadow_coord_var_name_sstream;

    if (!is_point_light)
    {
        /* Add the light-specific shadow coordinate input variable.
         *
         * shadow_coord is only used for non-point lights. see _adjust_vertex_uber_code() for details.
         */
        bool                      is_light_shadow_coord_var_added = false;
        system_hashed_ansi_string light_shadow_coord_var_name_has = NULL;

        light_shadow_coord_var_name_sstream << "light_"
                                            << n_light
                                            << "_shadow_coord";

        light_shadow_coord_var_name_has = system_hashed_ansi_string_create                (light_shadow_coord_var_name_sstream.str().c_str() );
        is_light_shadow_coord_var_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_fs,
                                                                                           0, /* uniform_block */
                                                                                           light_shadow_coord_var_name_has);

        ASSERT_DEBUG_SYNC(!is_light_shadow_coord_var_added,
                          "Light shadow coordinate input variable already added!");

        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_fs,
                                                          VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC4,
                                                          0,     /* array_size */
                                                          0,     /* uniform_block */
                                                          light_shadow_coord_var_name_has,
                                                          NULL); /* out_variable_id */
    }
    else
    {
        bool                      is_camera_eye_to_light_eye_var_added = false;
        bool                      is_light_projection_matrix_var_added = false;
        system_hashed_ansi_string camera_eye_to_light_eye_var_name_has = NULL;
        system_hashed_ansi_string light_projection_matrix_var_name_has = NULL;

        /* Add camera_eye_to_light_eye matrix uniform */
        camera_eye_to_light_eye_var_name_sstream << "light"
                                                  << n_light
                                                  << "_camera_eye_to_light_eye";

        camera_eye_to_light_eye_var_name_has = system_hashed_ansi_string_create                (camera_eye_to_light_eye_var_name_sstream.str().c_str() );
        is_camera_eye_to_light_eye_var_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_fs,
                                                                                                ub_fs,
                                                                                                camera_eye_to_light_eye_var_name_has);

        ASSERT_DEBUG_SYNC(!is_camera_eye_to_light_eye_var_added,
                          "Camera eye->light eye matrix already added for light");

        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_fs,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_ROW_MAJOR,
                                                          TYPE_MAT4,
                                                          0,     /* array_size */
                                                          ub_fs, /* uniform_block */
                                                          camera_eye_to_light_eye_var_name_has,
                                                          NULL); /* out_variable_id */

        /* Add light_projection matrix uniform */
        light_projection_matrix_var_name_sstream << "light"
                                                 << n_light
                                                 << "_projection";

        light_projection_matrix_var_name_has = system_hashed_ansi_string_create                (light_projection_matrix_var_name_sstream.str().c_str() );
        is_light_projection_matrix_var_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_fs,
                                                                                                ub_fs,
                                                                                                light_projection_matrix_var_name_has);

        ASSERT_DEBUG_SYNC(!is_light_projection_matrix_var_added,
                          "Light projection matrix already added for light");

        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_fs,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_ROW_MAJOR,
                                                          TYPE_MAT4,
                                                          0,     /* array_size */
                                                          ub_fs, /* uniform_block */
                                                          light_projection_matrix_var_name_has,
                                                          NULL); /* out_variable_id */
    }

    /* Add the light-specific shadow map texture sampler */
    bool                      is_shadow_map_sampler_var_added = false;
    system_hashed_ansi_string shadow_map_sampler_var_name_has = NULL;
    std::stringstream         shadow_map_sampler_var_name_sstream;

    shadow_map_sampler_var_name_sstream << "light"
                                        << n_light
                                        << "_shadow_map";

    shadow_map_sampler_var_name_has = system_hashed_ansi_string_create                (shadow_map_sampler_var_name_sstream.str().c_str() );
    is_shadow_map_sampler_var_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_fs,
                                                                                       0, /* uniform_block */
                                                                                       shadow_map_sampler_var_name_has);

    ASSERT_DEBUG_SYNC(!is_shadow_map_sampler_var_added,
                      "Light-specific shadow map texture sampler already added!");

    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_fs,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      is_point_light ? TYPE_SAMPLERCUBESHADOW
                                                                     : TYPE_SAMPLER2DSHADOW,
                                                      0,     /* array_size */
                                                      0,     /* uniform_block */
                                                      shadow_map_sampler_var_name_has,
                                                      NULL); /* out_variable_id */

    /* Add helper calculations for point lights */
    system_hashed_ansi_string code_snippet_has     = NULL;
    std::stringstream         code_snippet_sstream;
    std::stringstream         light_vertex_depth_window_var_name_sstream;

    if (is_point_light)
    {
        std::stringstream light_vertex_abs_var_name_sstream;
        std::stringstream light_vertex_abs_max_var_name_sstream;
        std::stringstream light_vertex_clip_var_name_sstream;
        std::stringstream light_vertex_var_name_sstream;

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

        code_snippet_sstream << "vec4 "
                             << light_vertex_var_name_sstream.str()
#if 1
                             << " = vec4(world_vertex, 1.0);\n"
#else
                             << " = vec4(world_vertex, 1.0) * "
                             << camera_eye_to_light_eye_var_name_sstream.str()
                             << ";\n"
#endif
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
                             << light_projection_matrix_var_name_sstream.str()
                             << " * vec4(0.0, 0.0, " << light_vertex_abs_max_var_name_sstream.str() << ", 1.0);\n"
                             << "float "
                             << light_vertex_depth_window_var_name_sstream.str()
                             << " = "
                             << light_vertex_clip_var_name_sstream.str()
                             << ".z / "
                             << light_vertex_clip_var_name_sstream.str()
                             << ".w * 0.5 + 0.5;\n";
    }

    /* Add the light-specific visibility calculations */
    std::stringstream         light_visibility_var_name_sstream;
    bool                      uses_texturelod                   = false; /* takes a LOD argument */
    bool                      uses_textureprojlod               = false; /* P takes an additional divisor component */

    light_visibility_var_name_sstream << "light_"
                                      << n_light
                                      << "_visibility";

    code_snippet_sstream << "float "
                             << light_visibility_var_name_sstream.str()
                             << " = 0.1 + 0.9 * ";

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL)
    {
        code_snippet_sstream << "textureLod("
                             << shadow_map_sampler_var_name_sstream.str()
                             << ", vec3("
                             << light_shadow_coord_var_name_sstream.str()
                             << ".xy, "
                             << light_shadow_coord_var_name_sstream.str()
                             << ".z";

        uses_texturelod = true;
    }
    else
    {
        /* All other light use shadow maps constructed with perspective projection matrices */
        if (is_point_light)
        {
            code_snippet_sstream << "texture("
                                 << shadow_map_sampler_var_name_sstream.str()
                                 << ", vec4("
                                 << "normalize(light_1_vertex.xyz)"
                                 << ", "
                                 << light_vertex_depth_window_var_name_sstream.str();
        } /* if (point light) */
        else
        {
            code_snippet_sstream << "textureProjLod("
                                 << shadow_map_sampler_var_name_sstream.str()
                                 << ", vec4("
                                 << light_shadow_coord_var_name_sstream.str()
                                 << ".xy, "
                                 << light_shadow_coord_var_name_sstream.str()
                                 << ".z ";

            uses_texturelod     = true;
            uses_textureprojlod = true;
        }
    }

    switch (sm_bias)
    {
        case SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE:
        {
            code_snippet_sstream << " - clamp(0.001 * tan(acos(light"
                                 << n_light
                                 << "_LdotN_clamped)), 0.0, 1.0)\n";

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE_FAST:
        {
            code_snippet_sstream << " - 0.001 * acos(clamp(light"
                                 << n_light
                                 << "_LdotN_clamped, 0.0, 1.0))\n";

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_CONSTANT:
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO");
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_NONE:
        {
            code_snippet_sstream << "\n";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized shadow map bias requested");
        }
    } /* switch (sm_bias) */

    if (uses_textureprojlod)
    {
        code_snippet_sstream << ", "
                             << light_shadow_coord_var_name_sstream.str()
                             << ".w";
    }

    code_snippet_sstream << ")";

    if (uses_texturelod)
    {
        code_snippet_sstream << ", 0.0";
    }

    code_snippet_sstream << ");\n";

    code_snippet_has = system_hashed_ansi_string_create(code_snippet_sstream.str().c_str());

    ogl_shader_constructor_append_to_function_body(shader_constructor_fs,
                                                   0, /* function_id */
                                                   code_snippet_has);

    /* Tell the caller what the name of the visibility variable is. */
    *out_visibility_var_name = system_hashed_ansi_string_create(light_visibility_var_name_sstream.str().c_str() );
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_adjust_vertex_uber_code(__in __notnull ogl_shader_constructor           shader_constructor_vs,
                                                       __in           uint32_t                         n_light,
                                                       __in           shaders_fragment_uber_light_type light_type,
                                                       __in           _uniform_block_id                ub_vs,
                                                       __in __notnull system_hashed_ansi_string        world_vertex_vec4_variable_name)
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
        bool is_depth_bias_matrix_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_vs,
                                                                                           0, /* uniform_block */
                                                                                           depth_bias_matrix_var_name);

        if (!is_depth_bias_matrix_added)
        {
            uint32_t     matrix_variable_data_size = 0;
            _variable_id matrix_variable_id        = -1;

            ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_vs,
                                                              VARIABLE_TYPE_CONST,
                                                              LAYOUT_QUALIFIER_NONE,
                                                              TYPE_MAT4,
                                                              0, /* array_size */
                                                              0, /* uniform_block */
                                                              depth_bias_matrix_var_name,
                                                             &matrix_variable_id);

            ogl_shader_constructor_set_general_variable_default_value(shader_constructor_vs,
                                                                      0,                         /* uniform_block */
                                                                      matrix_variable_id,
                                                                      NULL,                      /* data */
                                                                     &matrix_variable_data_size);

            ASSERT_DEBUG_SYNC(matrix_variable_data_size == sizeof(depth_bias_matrix_data),
                              "ogl_shader_constructor glitch detected");

            ogl_shader_constructor_set_general_variable_default_value(shader_constructor_vs ,
                                                                      0,                      /* uniform_block */
                                                                      matrix_variable_id,
                                                                      depth_bias_matrix_data,
                                                                      NULL);                  /* n_bytes_to_read */
        }
        /* Add the light-specific depth VP matrix. */
        system_hashed_ansi_string depth_vp_matrix_name_has     = NULL;
        std::stringstream         depth_vp_matrix_name_sstream;
        bool                      is_depth_vp_matrix_added     = false;

        depth_vp_matrix_name_sstream << "light"
                                     << n_light
                                     << "_depth_vp";

        depth_vp_matrix_name_has = system_hashed_ansi_string_create                (depth_vp_matrix_name_sstream.str().c_str() );
        is_depth_vp_matrix_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_vs,
                                                                                    ub_vs,
                                                                                    depth_vp_matrix_name_has);

        ASSERT_DEBUG_SYNC(!is_depth_vp_matrix_added,
                          "Depth VP already added to VS UB for light [%d]",
                          n_light);

        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_vs,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_ROW_MAJOR,
                                                          TYPE_MAT4,
                                                          0, /* array_size */
                                                          ub_vs,
                                                          depth_vp_matrix_name_has,
                                                          NULL); /* out_variable_id */

        /* Add an output variable that calculates the vertex position in light-view space */
        bool                      is_light_shadow_coord_added = false;
        system_hashed_ansi_string light_shadow_coord_name_has = NULL;
        std::stringstream         light_shadow_coord_name_sstream;

        light_shadow_coord_name_sstream << "light_"
                                        << n_light
                                        << "_shadow_coord";

        light_shadow_coord_name_has = system_hashed_ansi_string_create                (light_shadow_coord_name_sstream.str().c_str() );
        is_light_shadow_coord_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_vs,
                                                                                       ub_vs,
                                                                                       light_shadow_coord_name_has);

        ASSERT_DEBUG_SYNC(!is_light_shadow_coord_added,
                          "Light shadow coord for light [%d] already added",
                          n_light);

        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_vs,
                                                          VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC4,
                                                          0, /* array_size */
                                                          0, /* ub_id */
                                                          light_shadow_coord_name_has,
                                                          NULL); /* out_variable_id */

        /* Add new code snippet to main() */
        const char*               world_vertex_vec4_name_raw = system_hashed_ansi_string_get_buffer(world_vertex_vec4_variable_name);
        system_hashed_ansi_string vs_code_has                = NULL;
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

        ogl_shader_constructor_append_to_function_body(shader_constructor_vs,
                                                       0, /* function_id */
                                                       vs_code_has);
    } /* if (!is_point_light) */
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context)
{
    _ogl_shadow_mapping* new_instance = new (std::nothrow) _ogl_shadow_mapping;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    if (new_instance != NULL)
    {
        new_instance->context    = context;
        new_instance->is_enabled = false;

        /* NOTE: We cannot make an usual ogl_context renderer callback request at this point,
         *       since shadow mapping handler is initialized by ogl_context itself. Assume
         *       we're already within a GL rendering context.. */
        _ogl_shadow_mapping_init_renderer_callback(context,
                                                   new_instance);
    } /* if (new_instance != NULL) */

    return (ogl_shadow_mapping) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_get_matrices_for_light(__in            __notnull ogl_shadow_mapping             shadow_mapping,
                                                      __in            __notnull scene_light                    light,
                                                      __in                      ogl_shadow_mapping_target_face light_target_face,
                                                      __in_ecount(3)  __notnull scene_camera                   current_camera,
                                                      __in                      system_timeline_time           time,
                                                      __in_ecount(3)  __notnull const float*                   aabb_min_world,
                                                      __in_ecount(3)  __notnull const float*                   aabb_max_world,
                                                      __out           __notnull system_matrix4x4*              out_view_matrix,
                                                      __out           __notnull system_matrix4x4*              out_projection_matrix)
{
    _ogl_shadow_mapping* shadow_mapping_ptr = (_ogl_shadow_mapping*) shadow_mapping;

    /* First, we spawn a view matrix.
     *
     * For directional light, the view matrix presents stuff from a fake position of the light.
     * The actual position doesn't really matter, since the real nitty gritty happens when projection
     * matrix is formed.
     * For point lights, we use a look-at matrix, where the eye position is set to the light's potion,
     * and the look-at point is set according to @param light_target_face.
     * For spot light, we use an inverse of the light's transformation matrix for the view matrix.
     * We then proceed with forming the perspective projection matrix, using the cone angle information we have.
     *
     * For projection matrices, we transfer both frustum AABBs AND the AABB we're provided to the light's eye space (which
     * usually does funky stuff with the AABB coordinates, making it an OBB!). We then calculate an AABB by:
     *
     * 1. Computing a max & min AABB for the camera frustum.
     * 2. Intersecting the AABB from 1) with the AABB of the visible part of the scene,
     *    as seen from the camera.
     *
     * We use the result to compute a projection matrix.
     *
     * Once we have the data, we output both the view and projection matrices, so they can be used to render the
     * shadow maps for the light.
     */
    scene_light_type light_type = SCENE_LIGHT_TYPE_UNKNOWN;

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_TYPE,
                            &light_type);

    /* sanity checks */
    ASSERT_DEBUG_SYNC(aabb_min_world[0] <= aabb_max_world[0] &&
                      aabb_min_world[1] <= aabb_max_world[1] &&
                      aabb_min_world[2] <= aabb_max_world[2],
                      "AABB corruption");

    /* Set up camera & lookat positions */
    switch (light_type)
    {
        case SCENE_LIGHT_TYPE_DIRECTIONAL:
        {
            ASSERT_DEBUG_SYNC(light_target_face == OGL_SHADOW_MAPPING_TARGET_FACE_2D,
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
                aabb_min_world[0] + (aabb_max_world[0] - aabb_min_world[0]) * 0.5f,
                aabb_min_world[1] + (aabb_max_world[1] - aabb_min_world[1]) * 0.5f,
                aabb_min_world[2] + (aabb_max_world[2] - aabb_min_world[2]) * 0.5f
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

            *out_view_matrix = system_matrix4x4_create_lookat_matrix(sm_eye_world,    /* eye_position */
                                                                     sm_lookat_world, /* lookat_point */
                                                                     up_vector);      /* up_vector */
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

            switch (light_target_face)
            {
                case OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_X:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[0] = -1.0f;

                    break;
                }

                case OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_Y:
                {
                    up_direction  [2] = -1.0f;
                    view_direction[1] = -1.0f;

                    break;
                }

                case OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_Z:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[2] = -1.0f;

                    break;
                }

                case OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_X:
                {
                    up_direction  [1] = -1.0f;
                    view_direction[0] =  1.0f;

                    break;
                }

                case OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_Y:
                {
                    up_direction  [2] = 1.0f;
                    view_direction[1] = 1.0f;

                    break;
                }

                case OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_Z:
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
            } /* switch (light_target_face) */

            /* Move away from the light position in the view direction. */
            const float sm_lookat_world[3] =
            {
                light_position[0] - view_direction[0],
                light_position[1] - view_direction[1],
                light_position[2] - view_direction[2],
            };

            /* Set up the light's view matrix. */
            *out_view_matrix = system_matrix4x4_create_lookat_matrix(light_position,  /* eye_position */
                                                                     sm_lookat_world, /* lookat_point */
                                                                     up_direction);   /* up_vector */
            break;
        }

        case SCENE_LIGHT_TYPE_SPOT:
        {
            ASSERT_DEBUG_SYNC(light_target_face == OGL_SHADOW_MAPPING_TARGET_FACE_2D,
                              "Invalid target face requested for spot light SM");

            /* Calculate view matrix from light's transformation matrix */
            scene_graph_node light_owner_node            = NULL;
            system_matrix4x4 light_transformation_matrix = NULL;

            scene_light_get_property     (light,
                                          SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE,
                                         &light_owner_node);
            scene_graph_node_get_property(light_owner_node,
                                          SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                         &light_transformation_matrix);

            *out_view_matrix = system_matrix4x4_create();

            system_matrix4x4_set_from_matrix4x4(*out_view_matrix,
                                                light_transformation_matrix);
            system_matrix4x4_invert            (*out_view_matrix);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported light type encountered");
        }
    } /* switch (light_type) */

    /* Set up the light's projection matrix */
    switch (light_type)
    {
        case SCENE_LIGHT_TYPE_DIRECTIONAL:
        {
            float result_max[3];
            float result_min[3];

            _ogl_shadow_mapping_get_aabb_for_camera_frustum_and_scene_aabb(current_camera,
                                                                           time,
                                                                           aabb_min_world,
                                                                           aabb_max_world,
                                                                           *out_view_matrix,
                                                                           result_min,
                                                                           result_max);

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

            *out_projection_matrix = system_matrix4x4_create_ortho_projection_matrix(result_min[0],  /* left   */
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
             */
            float result_max[3];
            float result_min[3];
            float spotlight_sm_near_plane;

            _ogl_shadow_mapping_get_aabb_for_camera_frustum_and_scene_aabb(current_camera,
                                                                           time,
                                                                           aabb_min_world,
                                                                           aabb_max_world,
                                                                           *out_view_matrix,
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

                *out_projection_matrix = system_matrix4x4_create_perspective_projection_matrix(cone_angle_half * 2.0f,
                                                                                               1.0f, /* ar - shadow maps are quads */
                                                                                               spotlight_sm_near_plane,
                                                                                               max_len + min_len);
            }
            else
            {
                *out_projection_matrix = system_matrix4x4_create_perspective_projection_matrix(DEG_TO_RAD(90.0f),
                                                                                               1.0f, /* ar - shadow maps are quads */
                                                                                               0.1f,
                                                                                               100.0f);
                                                                                               //max_len + min_len); TODO: projection matrix should be the same for all faces
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported light type encountered");
        }
    } /* switch (light_type) */
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_release(__in __notnull __post_invalid ogl_shadow_mapping handler)
{
    _ogl_shadow_mapping* handler_ptr = (_ogl_shadow_mapping*) handler;

    if (handler_ptr->temp_variant_float != NULL)
    {
        system_variant_release(handler_ptr->temp_variant_float);

        handler_ptr->temp_variant_float = NULL;
    }

    ogl_context_request_callback_from_context_thread(handler_ptr->context,
                                                     _ogl_shadow_mapping_release_renderer_callback,
                                                     handler_ptr);

    delete handler_ptr;
    handler_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_shadow_mapping_toggle(__in __notnull ogl_shadow_mapping             handler,
                                                             __in __notnull scene_light                    light,
                                                             __in           bool                           should_enable,
                                                             __in           ogl_shadow_mapping_target_face target_face)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    _ogl_shadow_mapping*                                      handler_ptr      = (_ogl_shadow_mapping*) handler;

    ogl_context_get_property(handler_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(handler_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    if (should_enable)
    {
        /* If ogl_shadow_mapping is not already enabled, grab a texture from the texture pool.
         * It will serve as storage for the shadow map. After the SM is rendered, the ownership
         * is assumed to be passed to the caller.
         */
        GLenum           light_shadow_map_texture_target_detailed = GL_ZERO;
        GLenum           light_shadow_map_texture_target_general  = GL_ZERO;
        scene_light_type light_type                               = SCENE_LIGHT_TYPE_UNKNOWN;

        scene_light_get_property                                (light,
                                                                 SCENE_LIGHT_PROPERTY_TYPE,
                                                                &light_type);
        _ogl_shadow_mapping_get_texture_targets_from_target_face(target_face,
                                                                &light_shadow_map_texture_target_general,
                                                                &light_shadow_map_texture_target_detailed);

        if (!handler_ptr->is_enabled)
        {
            bool                             light_shadow_map_cull_front_faces        = false;
            scene_light_shadow_map_filtering light_shadow_map_filtering               = SCENE_LIGHT_SHADOW_MAP_FILTERING_UNKNOWN;
            GLenum                           light_shadow_map_internalformat          = GL_NONE;
            uint32_t                         light_shadow_map_size[2]                 = {0};

            /* Set up the shadow map */
            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_CULL_FRONT_FACES,
                                    &light_shadow_map_cull_front_faces);
            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING,
                                    &light_shadow_map_filtering);
            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT,
                                    &light_shadow_map_internalformat);
            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,
                                     light_shadow_map_size);

            ASSERT_DEBUG_SYNC(light_shadow_map_size[0] > 0 &&
                              light_shadow_map_size[1] > 0,
                              "Invalid shadow map size requested for a scene_light instance.");
            ASSERT_DEBUG_SYNC(handler_ptr->current_sm_texture == NULL,
                              "SM texture is already active");

            handler_ptr->current_sm_texture = ogl_textures_get_texture_from_pool(handler_ptr->context,
                                                                                 (light_type == SCENE_LIGHT_TYPE_POINT) ? OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP
                                                                                                                        : OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D,
                                                                                 1, /* n_mipmaps */
                                                                                 light_shadow_map_internalformat,
                                                                                 light_shadow_map_size[0],
                                                                                 light_shadow_map_size[1],
                                                                                 1,      /* base_mipmap_depth */
                                                                                 0,      /* n_samples */
                                                                                 false); /* fixed_samples_location */
            ASSERT_DEBUG_SYNC(handler_ptr->current_sm_texture != NULL,
                             "Could not retrieve a shadow map texture from the texture pool.");

            switch (light_shadow_map_filtering)
            {
                case SCENE_LIGHT_SHADOW_MAP_FILTERING_PCF:
                {
                    dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                              light_shadow_map_texture_target_general,
                                                              GL_TEXTURE_MAG_FILTER,
                                                              GL_LINEAR);

                    break;
                }

                case SCENE_LIGHT_SHADOW_MAP_FILTERING_PLAIN:
                {
                    dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                              light_shadow_map_texture_target_general,
                                                              GL_TEXTURE_MAG_FILTER,
                                                              GL_NEAREST);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized shadow map filtering mode");
                }
            } /* switch (light_shadow_map_filtering) */

            const float border_color[] = {1, 0, 0, 0};

            dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_MIN_FILTER,
                                                      GL_NEAREST);
            dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_COMPARE_FUNC,
                                                      GL_LESS);
            dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_COMPARE_MODE,
                                                      GL_COMPARE_REF_TO_TEXTURE);
            dsa_entry_points->pGLTextureParameterfvEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_BORDER_COLOR,
                                                      border_color);
            dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_WRAP_R,
                                                      GL_CLAMP_TO_BORDER);
            dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_WRAP_S,
                                                      GL_CLAMP_TO_BORDER);
            dsa_entry_points->pGLTextureParameteriEXT(handler_ptr->current_sm_texture,
                                                      light_shadow_map_texture_target_general,
                                                      GL_TEXTURE_WRAP_T,
                                                      GL_CLAMP_TO_BORDER);

            scene_light_set_property(light,
                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                    &handler_ptr->current_sm_texture);

            /* Set up color & depth masks */
            entry_points->pGLColorMask(GL_FALSE,
                                       GL_FALSE,
                                       GL_FALSE,
                                       GL_FALSE);
            entry_points->pGLDepthMask(GL_TRUE);

            /* render back-facing faces only: THIS WON'T WORK FOR NON-CONVEX GEOMETRY */
            if (light_shadow_map_cull_front_faces)
            {
                entry_points->pGLCullFace  (GL_FRONT);
                entry_points->pGLEnable    (GL_CULL_FACE);
            }
            else
            {
                /* :C */
                entry_points->pGLDisable(GL_CULL_FACE);
            }

            /* Set up depth function */
            entry_points->pGLDepthFunc (GL_LESS);
            entry_points->pGLEnable    (GL_DEPTH_TEST);

            /* Adjust viewport to match shadow map size */
            entry_points->pGLViewport  (0, /* x */
                                        0, /* y */
                                        light_shadow_map_size[0],
                                        light_shadow_map_size[1]);
        } /* if (!handler_ptr->is_enabled) */
        else
        {
            ASSERT_DEBUG_SYNC(handler_ptr->current_sm_texture != NULL,
                              "ogl_shadow_mapping is enabled but no SM texture is associated with the instance");
        }

        /* Set up the draw FBO */
        entry_points->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER,
                                              handler_ptr->fbo_id);
        entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              light_shadow_map_texture_target_detailed,
                                              handler_ptr->current_sm_texture,
                                              0); /* level */

        /* Clear the depth buffer */
        entry_points->pGLClearDepth(1.0);
        entry_points->pGLClear     (GL_DEPTH_BUFFER_BIT);
    } /* if (should_enable) */
    else
    {
        system_window context_window        = NULL;
        int32_t       context_window_height = 0;
        int32_t       context_window_width  = 0;

        entry_points->pGLColorMask(GL_TRUE,
                                   GL_TRUE,
                                   GL_TRUE,
                                   GL_TRUE);
        entry_points->pGLDepthMask(GL_TRUE);
        entry_points->pGLDisable  (GL_DEPTH_TEST);

        /* Bring back the face culling setting */
        entry_points->pGLCullFace(GL_BACK);

        ogl_context_get_property    (handler_ptr->context,
                                     OGL_CONTEXT_PROPERTY_WINDOW,
                                    &context_window);
        system_window_get_dimensions(context_window,
                                    &context_window_width,
                                    &context_window_height);

        entry_points->pGLViewport(0, /* x */
                                  0, /* y */
                                  context_window_width,
                                  context_window_height);

        /* Unbind the SM FBO */
        entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                         0);

        /* "Unbind" the SM texture from the ogl_shadow_mapping instance */
        ASSERT_DEBUG_SYNC(handler_ptr->current_sm_texture != NULL,
                          "ogl_shadow_mapping_toggle() called to disable SM rendering without an active SM");

        handler_ptr->current_sm_texture = NULL;
    }

    handler_ptr->is_enabled = should_enable;
}
