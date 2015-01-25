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
    float frustum_centroid_model[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frustum_centroid_mv   [4] = {0.0f, 0.0f, 0.0f, 1.0f};

    float frustum_fbl_model     [3];
    float frustum_fbr_model     [3];
    float frustum_ftl_model     [3];
    float frustum_ftr_model     [3];
    float frustum_nbl_model     [3];
    float frustum_nbr_model     [3];
    float frustum_ntl_model     [3];
    float frustum_ntr_model     [3];
    float frustum_x_max_model;
    float frustum_x_min_model;
    float frustum_y_max_model;
    float frustum_y_min_model;
    float frustum_z_max_model;
    float frustum_z_min_model;

    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_CENTROID,
                              frame_time,
                              frustum_centroid_model);

    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT,
                              frame_time,
                              frustum_fbl_model);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT,
                              frame_time,
                              frustum_fbr_model);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT,
                              frame_time,
                              frustum_nbl_model);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT,
                              frame_time,
                              frustum_nbr_model);

    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT,
                              frame_time,
                              frustum_ftl_model);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT,
                              frame_time,
                              frustum_ftr_model);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT,
                              frame_time,
                              frustum_ntl_model);
    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT,
                              frame_time,
                              frustum_ntr_model);

    /* Calculate camera's frustum centroid in world space */
    scene_graph_node camera_owner_node            = NULL;
    system_matrix4x4 camera_transformation_matrix = NULL;

    scene_camera_get_property(sm_user_camera,
                              SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                              frame_time,
                             &camera_owner_node);

    ASSERT_DEBUG_SYNC(camera_owner_node != NULL,
                      "Camera's scene graph owner node is NULL");

    scene_graph_node_get_property(camera_owner_node,
                                  SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                 &camera_transformation_matrix);

    system_matrix4x4_multiply_by_vector4(camera_transformation_matrix,
                                         frustum_centroid_model,
                                         frustum_centroid_mv);

    /* Obtain min & max values of all 3 components for the frustum vertices */
    const float* frustum_vertices[] =
    {
        frustum_fbl_model,
        frustum_fbr_model,
        frustum_ftl_model,
        frustum_ftr_model,
        frustum_nbl_model,
        frustum_nbr_model,
        frustum_ntl_model,
        frustum_ntr_model,
    };
    const uint32_t n_frustum_vertices = sizeof(frustum_vertices) / sizeof(frustum_vertices);

    frustum_x_max_model = frustum_vertices[0][0];
    frustum_x_min_model = frustum_vertices[0][0];
    frustum_y_max_model = frustum_vertices[0][1];
    frustum_y_min_model = frustum_vertices[0][1];
    frustum_z_max_model = frustum_vertices[0][2];
    frustum_z_min_model = frustum_vertices[0][2];

    for (uint32_t n_frustum_vertex = 1; /* skip the first entry */
                  n_frustum_vertex < n_frustum_vertices;
                ++n_frustum_vertex)
    {
        if (frustum_x_max_model < frustum_vertices[n_frustum_vertex][0])
        {
            frustum_x_max_model = frustum_vertices[n_frustum_vertex][0];
        }
        if (frustum_x_min_model > frustum_vertices[n_frustum_vertex][0])
        {
            frustum_x_min_model = frustum_vertices[n_frustum_vertex][0];
        }

        if (frustum_y_max_model < frustum_vertices[n_frustum_vertex][1])
        {
            frustum_y_max_model = frustum_vertices[n_frustum_vertex][1];
        }
        if (frustum_y_min_model > frustum_vertices[n_frustum_vertex][1])
        {
            frustum_y_min_model = frustum_vertices[n_frustum_vertex][1];
        }

        if (frustum_z_max_model < frustum_vertices[n_frustum_vertex][2])
        {
            frustum_z_max_model = frustum_vertices[n_frustum_vertex][2];
        }
        if (frustum_z_min_model > frustum_vertices[n_frustum_vertex][2])
        {
            frustum_z_min_model = frustum_vertices[n_frustum_vertex][2];
        }
    } /* for (all frustum Z's) */

    /* Now retrieve the light direction vector */
    float light_direction_vector[3] = {0};

    scene_light_get_property(light,
                             SCENE_LIGHT_PROPERTY_DIRECTION,
                             light_direction_vector);

    ASSERT_DEBUG_SYNC(fabs(system_math_vector_length3(light_direction_vector) - 1.0f) < 1e-5f,
                      "Light direction vector is not normalized");

    /* We will need a vector that is orthogonal to the light direction. Now, for a single 3D vector
     * there's an infinite number of orthogonal vectors, so use a cross product to come up with one.
     */
    const float light_arbitrary_vector           [3] = {light_direction_vector[1],
                                                        light_direction_vector[2],
                                                        light_direction_vector[0]};
          float light_direction_orthogonal_vector[3] = {0};

    system_math_vector_cross3(light_direction_vector,
                              light_arbitrary_vector,
                              light_direction_orthogonal_vector);

    /* Move away from the frustum centroid in the light direction, using the max z
     * value we've retrieved.
     */
    float sm_camera_location[3] =
    {
        frustum_centroid_model[0] - light_direction_vector[0] * frustum_z_max_model,
        frustum_centroid_model[1] - light_direction_vector[1] * frustum_z_max_model,
        frustum_centroid_model[2] - light_direction_vector[2] * frustum_z_max_model,
    };

    memcpy(out_camera_position,
           sm_camera_location,
           sizeof(sm_camera_location) );

    /* Set up projection matrix */
    *out_projection_matrix = system_matrix4x4_create_ortho_projection_matrix(frustum_x_min_model,                        /* left */
                                                                             frustum_x_max_model,                        /* right */
                                                                             frustum_y_min_model,                        /* bottom */
                                                                             frustum_y_max_model,                        /* top */
                                                                             0.0001f,                                    /* near z */
                                                                             frustum_z_max_model - frustum_z_min_model); /* far z */

    /* Set up the view matrix */
    *out_view_matrix = system_matrix4x4_create_lookat_matrix(sm_camera_location,                 /* eye_position */
                                                             frustum_centroid_mv,                /* lookat_point */
                                                             light_direction_orthogonal_vector); /* up_vector */
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
        ogl_texture light_shadow_map = NULL;

        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,
                                &light_shadow_map);

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
    }
    else
    {
        entry_points->pGLColorMask(GL_TRUE,
                                   GL_TRUE,
                                   GL_TRUE,
                                   GL_TRUE);
        entry_points->pGLDepthMask(GL_TRUE);

        /* Unbind the SM FBO */
        entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                         0);
    }
}
