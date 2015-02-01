/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shader_constructor.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
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

    GLuint fbo_id; /* FBO used to render depth data to light-specific depth texture. */


    _ogl_shadow_mapping()
    {
        context = NULL;
        fbo_id  = 0;
    }

} _ogl_shadow_mapping;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_shadow_mapping_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_shadow_mapping_verify_context_type(x)
#endif


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
PUBLIC void ogl_shadow_mapping_adjust_fragment_uber_code(__in  __notnull ogl_shader_constructor     shader_constructor_fs,
                                                         __in            uint32_t                   n_light,
                                                         __in            _uniform_block_id          ub_fs,
                                                         __out __notnull system_hashed_ansi_string* out_visibility_var_name)
{
    /* Add the light-specific shadow coordinate input variable */
    bool                      is_light_shadow_coord_var_added = false;
    system_hashed_ansi_string light_shadow_coord_var_name_has = NULL;
    std::stringstream         light_shadow_coord_var_name_sstream;

    light_shadow_coord_var_name_sstream << "light_"
                                        << n_light
                                        << "_shadow_coord";

    light_shadow_coord_var_name_has = system_hashed_ansi_string_create                (light_shadow_coord_var_name_sstream.str().c_str() );
    is_light_shadow_coord_var_added = ogl_shader_constructor_is_general_variable_in_ub(shader_constructor_fs,
                                                                                       0, /* uniform_block */
                                                                                       light_shadow_coord_var_name_has);

    ASSERT_DEBUG_SYNC(!is_light_shadow_coord_var_added,
                      "light shadow coordinate input variable already added!");

    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor_fs,
                                                      VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_VEC4,
                                                      0,     /* array_size */
                                                      0,     /* uniform_block */
                                                      light_shadow_coord_var_name_has,
                                                      NULL); /* out_variable_id */

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
                                                      TYPE_SAMPLER2D,
                                                      0,     /* array_size */
                                                      0,     /* uniform_block */
                                                      shadow_map_sampler_var_name_has,
                                                      NULL); /* out_variable_id */

    /* Add the light-specific visibility calculations */
    system_hashed_ansi_string code_snippet_has = NULL;
    std::stringstream         code_snippet_sstream;
    std::stringstream         light_visibility_var_name_sstream;

    light_visibility_var_name_sstream << "light_"
                                      << n_light
                                      << "_visibility";

    code_snippet_sstream << "float "
                         << light_visibility_var_name_sstream.str()
                         << " = (textureLod("
                         << shadow_map_sampler_var_name_sstream.str()
                         << ", "
                         << light_shadow_coord_var_name_sstream.str()
                         << ".xy, 0).x < ("
                         << light_shadow_coord_var_name_sstream.str()
                         << ".z - clamp(0.001 * tan(acos(light" << n_light << "_LdotN_clamped)), 0.0, 1.0)) ? 0.0 : 1.0);\n";

    code_snippet_has = system_hashed_ansi_string_create(code_snippet_sstream.str().c_str());

    ogl_shader_constructor_append_to_function_body(shader_constructor_fs,
                                                   0, /* function_id */
                                                   code_snippet_has);

    /* Tell the caller what the name of the visibility variable is. */
    *out_visibility_var_name = system_hashed_ansi_string_create(light_visibility_var_name_sstream.str().c_str() );
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_adjust_vertex_uber_code(__in __notnull ogl_shader_constructor    shader_constructor_vs,
                                                       __in           uint32_t                  n_light,
                                                       __in           _uniform_block_id         ub_vs,
                                                       __in __notnull system_hashed_ansi_string world_vertex_vec4_variable_name)
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

    /* Add the light-specific depth VP matrix */
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
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context)
{
    _ogl_shadow_mapping* new_instance = new (std::nothrow) _ogl_shadow_mapping;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    if (new_instance != NULL)
    {
        new_instance->context = context;

        /* NOTE: We cannot make an usual ogl_context renderer callback request at this point,
         *       since shadow mapping handler is initialized by ogl_context itself. Assume
         *       we're already within a GL rendering context.. */
        _ogl_shadow_mapping_init_renderer_callback(context,
                                                   new_instance);
    } /* if (new_instance != NULL) */

    return (ogl_shadow_mapping) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_get_matrices_for_directional_light(__in            __notnull scene_light       light,
                                                                  __in_ecount(3)  __notnull const float*      aabb_min_world,
                                                                  __in_ecount(3)  __notnull const float*      aabb_max_world,
                                                                  __out           __notnull system_matrix4x4* out_view_matrix,
                                                                  __out           __notnull system_matrix4x4* out_projection_matrix,
                                                                  __out_ecount(3) __notnull float*            out_camera_position)
{
    /* What we take as input is AABB of the part of the scene that's visible from the camera viewpoint, which is great.
     * We use this data to spawn a view matrix that presents stuff from a fake position of the directional light.
     * Once we have the view matrix, we transfer the AABB to the light's eye space (which usually does funky stuff with
     * the AABB coordinates, making it an OBB!). We then calculate an AABB out of this OBB and use this to compute
     * a projection matrix.
     *
     * Once we have the data, we output both the view and projection matrices, so they can be used to render the
     * shadow maps for the light.
     */

    /* Retrieve the light direction vector */
    float light_direction_vector[3] = {0};

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_DIRECTION,
                             light_direction_vector);

    ASSERT_DEBUG_SYNC(fabs(system_math_vector_length3(light_direction_vector) - 1.0f) < 1e-5f,
                      "Light direction vector is not normalized");

    /* sanity checks */
    ASSERT_DEBUG_SYNC(aabb_min_world[0] < aabb_max_world[0] &&
                      aabb_min_world[1] < aabb_max_world[1] &&
                      aabb_min_world[2] < aabb_max_world[2],
                      "AABB corruption");

    /* Move away from the frustum centroid in the light direction, using the max z
     * value we've retrieved.
     */
    const float frustum_centroid_world[3] =
    {
        /* Position is not really relevant here */
        0.0f,
        0.0f,
        0.0f,
    };
    const float sm_camera_location_world[4] =
    {
        -light_direction_vector[0],
        -light_direction_vector[1],
        -light_direction_vector[2],
    };

    memcpy(out_camera_position,
           sm_camera_location_world,
           sizeof(float) * 3);

    /* Set up the light's view matrix */
    static const float up_vector[3] = {
        0.0f,
        1.0f,
        0.0f
    };

    *out_view_matrix = system_matrix4x4_create_lookat_matrix(sm_camera_location_world, /* eye_position */
                                                             frustum_centroid_world,   /* lookat_point */
                                                             up_vector);               /* up_vector */

    /* Transfer the AABB to the light's eye space */
    const float  aabb_vec4_fbl_world[4] = {aabb_min_world[0], aabb_max_world[1], aabb_max_world[2], 1.0f};
    const float  aabb_vec4_fbr_world[4] = {aabb_max_world[0], aabb_max_world[1], aabb_max_world[2], 1.0f};
    const float  aabb_vec4_ftl_world[4] = {aabb_min_world[0], aabb_min_world[1], aabb_max_world[2], 1.0f};
    const float  aabb_vec4_ftr_world[4] = {aabb_max_world[0], aabb_min_world[1], aabb_max_world[2], 1.0f};
    const float  aabb_vec4_nbl_world[4] = {aabb_min_world[0], aabb_max_world[1], aabb_min_world[2], 1.0f};
    const float  aabb_vec4_nbr_world[4] = {aabb_max_world[0], aabb_max_world[1], aabb_min_world[2], 1.0f};
    const float  aabb_vec4_ntl_world[4] = {aabb_min_world[0], aabb_min_world[1], aabb_min_world[2], 1.0f};
    const float  aabb_vec4_ntr_world[4] = {aabb_max_world[0], aabb_min_world[1], aabb_min_world[2], 1.0f};
          float  obb_vec4_fbl_light[4];
          float  obb_vec4_fbr_light[4];
          float  obb_vec4_ftl_light[4];
          float  obb_vec4_ftr_light[4];
          float  obb_vec4_nbl_light[4];
          float  obb_vec4_nbr_light[4];
          float  obb_vec4_ntl_light[4];
          float  obb_vec4_ntr_light[4];

    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_fbl_world,
                                         obb_vec4_fbl_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_fbr_world,
                                         obb_vec4_fbr_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_ftl_world,
                                         obb_vec4_ftl_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_ftr_world,
                                         obb_vec4_ftr_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_nbl_world,
                                         obb_vec4_nbl_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_nbr_world,
                                         obb_vec4_nbr_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_ntl_world,
                                         obb_vec4_ntl_light);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         aabb_vec4_ntr_world,
                                         obb_vec4_ntr_light);

    /* This gives us coordinates of an OBB. Great. Compute an AABB for this data */
    const float* obb_light_vertices[]  =
    {
        obb_vec4_fbl_light,
        obb_vec4_fbr_light,
        obb_vec4_ftl_light,
        obb_vec4_ftr_light,
        obb_vec4_nbl_light,
        obb_vec4_nbr_light,
        obb_vec4_ntl_light,
        obb_vec4_ntr_light
    };
    float          obb_max_light[3]     = {obb_vec4_fbl_light[0], obb_vec4_fbl_light[1], obb_vec4_fbl_light[2]};
    float          obb_min_light[3]     = {obb_vec4_fbl_light[0], obb_vec4_fbl_light[1], obb_vec4_fbl_light[2]};
    const uint32_t n_obb_light_vertices = sizeof(obb_light_vertices) / sizeof(obb_light_vertices[0]);

    for (uint32_t n_obb_light_vertex = 1; /* skip the first entry */
                  n_obb_light_vertex < n_obb_light_vertices;
                ++n_obb_light_vertex)
    {
        if (obb_max_light[0] < obb_light_vertices[n_obb_light_vertex][0])
        {
            obb_max_light[0] = obb_light_vertices[n_obb_light_vertex][0];
        }
        if (obb_min_light[0] > obb_light_vertices[n_obb_light_vertex][0])
        {
            obb_min_light[0] = obb_light_vertices[n_obb_light_vertex][0];
        }

        if (obb_max_light[1] < obb_light_vertices[n_obb_light_vertex][1])
        {
            obb_max_light[1] = obb_light_vertices[n_obb_light_vertex][1];
        }
        if (obb_min_light[1] > obb_light_vertices[n_obb_light_vertex][1])
        {
            obb_min_light[1] = obb_light_vertices[n_obb_light_vertex][1];
        }

        if (obb_max_light[2] < obb_light_vertices[n_obb_light_vertex][2])
        {
            obb_max_light[2] = obb_light_vertices[n_obb_light_vertex][2];
        }
        if (obb_min_light[2] > obb_light_vertices[n_obb_light_vertex][2])
        {
            obb_min_light[2] = obb_light_vertices[n_obb_light_vertex][2];
        }
    } /* for (all frustum Z's) */

    ASSERT_DEBUG_SYNC(obb_min_light[0] < obb_max_light[0] &&
                      obb_min_light[1] < obb_max_light[1] &&
                      obb_min_light[2] < obb_max_light[2],
                      "Something's nuts with the OBB min/max calcs");

    /* Use the AABB data to compute the reuslt projection matrix */
    *out_projection_matrix = system_matrix4x4_create_ortho_projection_matrix(obb_min_light[0],  /* left   */
                                                                             obb_max_light[0],  /* right  */
                                                                             obb_min_light[1],  /* bottom */
                                                                             obb_max_light[1],  /* top    */
                                                                             obb_min_light[2],  /* near   */
                                                                             obb_max_light[2]); /* far    */
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_release(__in __notnull __post_invalid ogl_shadow_mapping handler)
{
    _ogl_shadow_mapping* handler_ptr = (_ogl_shadow_mapping*) handler;

    ogl_context_request_callback_from_context_thread(handler_ptr->context,
                                                     _ogl_shadow_mapping_release_renderer_callback,
                                                     handler_ptr);

    delete handler_ptr;
    handler_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_shadow_mapping_toggle(__in __notnull ogl_shadow_mapping handler,
                                                             __in __notnull scene_light        light,
                                                             __in           bool               should_enable)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _ogl_shadow_mapping*              handler_ptr     = (_ogl_shadow_mapping*) handler;

    /* Configure color/depth masks according to whether SM is being brought up or down */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(handler_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (should_enable)
    {
        ogl_texture light_shadow_map         = NULL;
        uint32_t    light_shadow_map_size[2] = {0};

        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                &light_shadow_map);
        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,
                                 light_shadow_map_size);

        ASSERT_DEBUG_SYNC(light_shadow_map != NULL,
                          "Shadow map is NULL");

        /* Set up color & depth masks */
        entry_points->pGLColorMask(GL_FALSE,
                                   GL_FALSE,
                                   GL_FALSE,
                                   GL_FALSE);
        entry_points->pGLDepthMask(GL_TRUE);

        /* Set up draw FBO */
        entry_points->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER,
                                              handler_ptr->fbo_id);
        entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                              GL_DEPTH_ATTACHMENT,
                                              GL_TEXTURE_2D,
                                              light_shadow_map,
                                              0); /* level */

        /* Clear the depth buffer */
        entry_points->pGLClearDepth(1.0);
        entry_points->pGLClear     (GL_DEPTH_BUFFER_BIT);

        /* render back-facing faces only: THIS WON'T WORK FOR NON-CONVEX GEOMETRY */
        entry_points->pGLCullFace  (GL_FRONT);
        entry_points->pGLEnable    (GL_CULL_FACE);

        /* Set up depth function */
        entry_points->pGLDepthFunc (GL_LESS);
        entry_points->pGLEnable    (GL_DEPTH_TEST);

        /* Adjust viewport to match shadow map size */
        entry_points->pGLViewport  (0, /* x */
                                    0, /* y */
                                    light_shadow_map_size[0],
                                    light_shadow_map_size[1]);
    }
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
    }
}
