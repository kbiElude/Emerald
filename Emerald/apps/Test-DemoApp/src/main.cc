/**
 *
 * Test demo application @2015
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_shader.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "raGL/raGL_framebuffer.h"
#include "ral/ral_context.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include "app_config.h"
#include "state.h"
#include "ui.h"

ral_context           _context             = NULL;
ogl_rendering_handler _rendering_handler   = NULL;
demo_window           _window              = NULL;
system_event          _window_closed_event = system_event_create(true); /* manual_reset */
GLuint                _vao_id              = 0;


/** Rendering handler */
void _rendering_handler_entrypoint(ogl_context context,
                                   uint32_t    n_frames_rendered,
                                   system_time frame_time,
                                   const int*  rendering_area_px_topdown,
                                   void*       unused)
{
    ral_framebuffer                   default_fb         = NULL;
    raGL_framebuffer                  default_fb_raGL    = NULL;
    GLuint                            default_fb_raGL_id = -1;
    const ogl_context_gl_entrypoints* entry_points       = NULL;
    static bool                       ui_initialized     = false;

    state_set_current_frame_time(frame_time);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (!ui_initialized)
    {
        /* Set up UI */
        ui_init();

        ui_initialized = true;
    }

    ral_context_get_property(_context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS,
                            &default_fb);

    default_fb_raGL = ral_context_get_framebuffer_gl(_context,
                                                     default_fb);

    raGL_framebuffer_get_property(default_fb_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &default_fb_raGL_id);

    entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                     default_fb_raGL_id);
    entry_points->pGLClearColor     (0.0f,
                                     0.0f,
                                     0.5f,
                                     0.0f);
    entry_points->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entry_points->pGLEnable         (GL_DEPTH_TEST);
    entry_points->pGLEnable         (GL_CULL_FACE);

    /* Render the scene */
    ogl_pipeline_draw_stage(state_get_pipeline(),
                            state_get_pipeline_stage_id(),
                            n_frames_rendered,
                            frame_time,
                            rendering_area_px_topdown);
}

PUBLIC void _render_scene(ral_context context,
                          uint32_t    frame_index,
                          system_time time,
                          const int*  rendering_area_px_topdown,
                          void*       not_used)
{
    scene_camera       current_camera   = NULL;
    ogl_scene_renderer current_renderer = NULL;
    scene              current_scene    = NULL;
    system_time        frame_time       = 0;

    state_get_current_frame_properties(&current_scene,
                                       &current_camera,
                                       &current_renderer,
                                       &frame_time);

    /* Update projection matrix */
    float            new_zfar   = CAMERA_SETTING_Z_FAR;
    float            new_znear  = 0.1f;
    system_matrix4x4 projection = NULL;
    float            yfov_value;

    scene_camera_get_property(current_camera,
                              SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,
                              time,
                              &yfov_value);

    scene_camera_set_property(current_camera,
                              SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                             &new_zfar);
    scene_camera_set_property(current_camera,
                              SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                             &new_znear);

    projection = system_matrix4x4_create_perspective_projection_matrix(yfov_value,
                                                                       float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
                                                                       new_znear,
                                                                       new_zfar);

    /* Proceed with frame rendering.
     *
     * First, traverse the scene graph to compute node transformation matrices.
     */
    scene_graph scene_renderer_graph = NULL;

    ogl_scene_renderer_get_property(current_renderer,
                                    OGL_SCENE_RENDERER_PROPERTY_GRAPH,
                                   &scene_renderer_graph);

    scene_graph_lock(scene_renderer_graph);
    {
        scene_graph_compute(scene_renderer_graph,
                            frame_time);
    }
    scene_graph_unlock(scene_renderer_graph);

    /* Retrieve node that contains the transformation matrix for the camera */
    scene_graph_node scene_camera_node                  = NULL;
    system_matrix4x4 scene_camera_transformation_matrix = NULL;

    scene_camera_get_property(current_camera,
                              SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                              0, /* time - irrelevant for the owner graph node */
                             &scene_camera_node);

    scene_graph_node_get_property(scene_camera_node,
                                  SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                 &scene_camera_transformation_matrix);

    /* Configure view matrix, using the selected camera's transformation matrix */
    system_matrix4x4 view = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4 (view,
                                         scene_camera_transformation_matrix);
    system_matrix4x4_invert             (view);

    /* Render the scene graph */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(ral_context_get_gl_context(context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_scene_renderer_render_scene_graph(current_renderer,
                                          view,
                                          projection,
                                          current_camera,
                                          RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
                                          true,  /* apply_shadow_mapping */
                                          HELPER_VISUALIZATION_BOUNDING_BOXES,
                                          frame_time
                                         );

    /* Render UI */
    ui_draw();

    /* All done */
    system_matrix4x4_release(view);
    system_matrix4x4_release(projection);
}

void _rendering_rbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

bool _rendering_window_closed_callback_handler(system_window window,
                                               void*         unused)
{
    system_event_set(_window_closed_event);

    return true;
}

void _rendering_window_closing_callback_handler(system_window window,
                                                void*         unused)
{
    ui_deinit   ();
    state_deinit();
}

/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
#else
    int main()
#endif
{
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler_entrypoint;
    system_screen_mode                      screen_mode        = NULL;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Demo test app");
    const uint32_t                          window_size[2]     = {1280, 720};
    int                                     window_x1y1x2y2[4] = {0};

    _window = demo_app_create_window(window_name,
                                     RAL_BACKEND_TYPE_GL,
                                     false /* use_timeline */);

    demo_window_set_property(_window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_size);

    demo_window_show(_window);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &_rendering_handler);

    ogl_rendering_handler_set_property(_rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func      (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    demo_window_add_callback_func      (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _rendering_window_closed_callback_handler,
                                        NULL);
    demo_window_add_callback_func      (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _rendering_window_closing_callback_handler,
                                        NULL);

    /* Initialize data required to run the demo */
    state_init();

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    demo_app_destroy_window(window_name);
    system_event_release   (_window_closed_event);

    return 0;
}