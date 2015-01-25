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
PUBLIC void ogl_shadow_mapping_get_matrices_for_directional_light(__in            __notnull scene_light          light,
                                                                  __in            __notnull scene_camera         sm_user_camera,
                                                                  __in                      system_timeline_time frame_time,
                                                                  __out           __notnull system_matrix4x4*    out_view_matrix,
                                                                  __out           __notnull system_matrix4x4*    out_projection_matrix,
                                                                  __out_ecount(3) __notnull float*               out_camera_position)
{
    /* Retrieve camera's frustum properties */
    float frustum_centroid_camera_view[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_fbl_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_fbr_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_ftl_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_ftr_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_nbl_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_nbr_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_ntl_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_ntr_camera_view     [4] = {0.0f, 0.0f, 0.0f, 1.0f};

    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_CENTROID,
                              frame_time,
                              frustum_centroid_camera_view);

    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT,
                              frame_time,
                              frustum_fbl_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT,
                              frame_time,
                              frustum_fbr_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT,
                              frame_time,
                              frustum_nbl_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT,
                              frame_time,
                              frustum_nbr_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT,
                              frame_time,
                              frustum_ftl_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT,
                              frame_time,
                              frustum_ftr_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT,
                              frame_time,
                              frustum_ntl_camera_view);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT,
                              frame_time,
                              frustum_ntr_camera_view);

    /* Calculate camera's far Z setting */
    float            camera_far_z;
    scene_graph_node camera_owner_node            = NULL;
    system_matrix4x4 camera_transformation_matrix = NULL;

    scene_camera_get_property    (sm_user_camera,
                                  SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                  frame_time,
                                 &camera_far_z);
    scene_camera_get_property    (sm_user_camera,
                                  SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                  frame_time,
                                 &camera_owner_node);
    scene_graph_node_get_property(camera_owner_node,
                                  SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                 &camera_transformation_matrix);

    /* Now retrieve the light direction vector */
    float light_direction_vector[3] = {0};

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_DIRECTION,
                             light_direction_vector);

    ASSERT_DEBUG_SYNC(fabs(system_math_vector_length3(light_direction_vector) - 1.0f) < 1e-5f,
                      "Light direction vector is not normalized");

    /* Move away from the frustum centroid in the light direction, using the max z
     * value we've retrieved.
     */
    float sm_camera_location_camera_view[4] =
    {
        frustum_centroid_camera_view[0] - light_direction_vector[0] * camera_far_z,
        frustum_centroid_camera_view[1] - light_direction_vector[1] * camera_far_z,
        frustum_centroid_camera_view[2] - light_direction_vector[2] * camera_far_z,
        1.0f
    };

    memcpy(out_camera_position,
           sm_camera_location_camera_view,
           sizeof(float) * 3);

    /* Set up the light's view matrix */
    static const float up_vector[3] = {0.0f, 1.0f, 0.0f};

    *out_view_matrix = system_matrix4x4_create_lookat_matrix(sm_camera_location_camera_view, /* eye_position */
                                                             frustum_centroid_camera_view,   /* lookat_point */
                                                             up_vector);                     /* up_vector */

    /* Calculate light view frustum vertex locations */
    float frustum_fbl_light_view      [4];
    float frustum_fbr_light_view      [4];
    float frustum_ftl_light_view      [4];
    float frustum_ftr_light_view      [4];
    float frustum_nbl_light_view      [4];
    float frustum_nbr_light_view      [4];
    float frustum_ntl_light_view      [4];
    float frustum_ntr_light_view      [4];
    float frustum_x_max_light_view;
    float frustum_x_min_light_view;
    float frustum_y_max_light_view;
    float frustum_y_min_light_view;
    float frustum_z_max_light_view;
    float frustum_z_min_light_view;

    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_fbl_camera_view,
                                         frustum_fbl_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_fbr_camera_view,
                                         frustum_fbr_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_ftl_camera_view,
                                         frustum_ftl_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_ftr_camera_view,
                                         frustum_ftr_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_nbl_camera_view,
                                         frustum_nbl_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_nbr_camera_view,
                                         frustum_nbr_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_ntl_camera_view,
                                         frustum_ntl_light_view);
    system_matrix4x4_multiply_by_vector4(*out_view_matrix,
                                         frustum_ntr_camera_view,
                                         frustum_ntr_light_view);

    float* frustum_vertices_light_view[] =
    {
        frustum_fbl_light_view,
        frustum_fbr_light_view,
        frustum_ftl_light_view,
        frustum_ftr_light_view,
        frustum_nbl_light_view,
        frustum_nbr_light_view,
        frustum_ntl_light_view,
        frustum_ntr_light_view,
    };
    const uint32_t n_frustum_vertices = sizeof(frustum_vertices_light_view) / sizeof(frustum_vertices_light_view[0]);

    frustum_x_max_light_view = frustum_vertices_light_view[0][0];
    frustum_x_min_light_view = frustum_vertices_light_view[0][0];
    frustum_y_max_light_view = frustum_vertices_light_view[0][1];
    frustum_y_min_light_view = frustum_vertices_light_view[0][1];
    frustum_z_max_light_view = frustum_vertices_light_view[0][2];
    frustum_z_min_light_view = frustum_vertices_light_view[0][2];

    for (uint32_t n_frustum_vertex = 1; /* skip the first entry */
                  n_frustum_vertex < n_frustum_vertices;
                ++n_frustum_vertex)
    {
        if (frustum_x_max_light_view < frustum_vertices_light_view[n_frustum_vertex][0])
        {
            frustum_x_max_light_view = frustum_vertices_light_view[n_frustum_vertex][0];
        }
        if (frustum_x_min_light_view > frustum_vertices_light_view[n_frustum_vertex][0])
        {
            frustum_x_min_light_view = frustum_vertices_light_view[n_frustum_vertex][0];
        }

        if (frustum_y_max_light_view < frustum_vertices_light_view[n_frustum_vertex][1])
        {
            frustum_y_max_light_view = frustum_vertices_light_view[n_frustum_vertex][1];
        }
        if (frustum_y_min_light_view > frustum_vertices_light_view[n_frustum_vertex][1])
        {
            frustum_y_min_light_view = frustum_vertices_light_view[n_frustum_vertex][1];
        }

        if (frustum_z_max_light_view < frustum_vertices_light_view[n_frustum_vertex][2])
        {
            frustum_z_max_light_view = frustum_vertices_light_view[n_frustum_vertex][2];
        }
        if (frustum_z_min_light_view > frustum_vertices_light_view[n_frustum_vertex][2])
        {
            frustum_z_min_light_view = frustum_vertices_light_view[n_frustum_vertex][2];
        }
    } /* for (all frustum Z's) */

    /* Set up projection matrix */
    *out_projection_matrix = system_matrix4x4_create_ortho_projection_matrix(frustum_x_min_light_view, /* left */
                                                                             frustum_x_max_light_view, /* right */
                                                                             frustum_y_min_light_view, /* bottom */
                                                                             frustum_y_max_light_view, /* top */
                                                                             0.0f,
                                                                             frustum_z_max_light_view + frustum_z_min_light_view);
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
        entry_points->pGLDepthFunc (GL_LESS);
        entry_points->pGLEnable    (GL_DEPTH_TEST);
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