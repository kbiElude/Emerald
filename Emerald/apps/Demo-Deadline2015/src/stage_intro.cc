#include "stage_intro.h"
#include "demo/demo_app.h"
#include "demo/demo_loader.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_video_segment.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "postprocessing/postprocessing_motion_blur.h"
#include "procedural/procedural_mesh_circle.h"
#include "system/system_math_other.h"
#include "spinner.h"

/* Forward declarations */
PRIVATE                        void _init_overlay_po                                      (ogl_context context);
PRIVATE RENDERING_CONTEXT_CALL void _stage_intro_blur_spinner                             (ogl_context context,
                                                                                           uint32_t    frame_index,
                                                                                           system_time frame_time,
                                                                                           const int*  rendering_area_px_topdown,
                                                                                           void*       unused);
PRIVATE                        void _stage_intro_deinit_rendering_callback                (void*       unused);
PRIVATE                        void _stage_intro_draw_spinner_animation_rendering_callback(ogl_context context,
                                                                                           uint32_t    n_frame,
                                                                                           system_time frame_time,
                                                                                           const int*  rendering_area_px_topdown,
                                                                                           void*       user_arg);
PRIVATE                        void _stage_intro_draw_spinner_frame_rendering_callback    (ogl_context context,
                                                                                           void*       user_arg);
PRIVATE                        void _stage_intro_init_rendering_callback                  (ogl_context context,
                                                                                           void*       unused);
PRIVATE RENDERING_CONTEXT_CALL void _stage_intro_render_overlay                           (ogl_context context,
                                                                                           uint32_t    frame_index,
                                                                                           system_time frame_time,
                                                                                           const int*  rendering_area_px_topdown,
                                                                                           void*       unused);
PRIVATE RENDERING_CONTEXT_CALL void _stage_intro_render_spinner                           (ogl_context context,
                                                                                           uint32_t    frame_index,
                                                                                           system_time frame_time,
                                                                                           const int*  rendering_area_px_topdown,
                                                                                           void*       ununsed);


/* Private variables */
PRIVATE GLuint                      _blit_fbo_id               = 0;
PRIVATE postprocessing_motion_blur  _motion_blur               = NULL;
PRIVATE procedural_mesh_circle      _overlay_circle            = NULL;
PRIVATE unsigned int                _overlay_circle_n_vertices = 0;
PRIVATE ogl_program                 _overlay_po                = NULL;
PRIVATE GLuint                      _overlay_vao_id            = 0;
PRIVATE unsigned int                _resolution[2] = { 0 };
PRIVATE ogl_texture                 _spinner_blurred_to        = NULL;
PRIVATE demo_timeline_video_segment _video_segment             = NULL;


/** Blurs the spinner. */
PRIVATE RENDERING_CONTEXT_CALL void _stage_intro_blur_spinner(ogl_context context,
                                                              uint32_t    frame_index,
                                                              system_time frame_time,
                                                              const int*  rendering_area_px_topdown,
                                                              void*       unused)
{
    ogl_texture color_to = NULL;
    ogl_texture velocity_to = NULL;

    spinner_get_property(SPINNER_PROPERTY_RESULT_COLOR_TO,
                        &color_to);
    spinner_get_property(SPINNER_PROPERTY_RESULT_VELOCITY_TO,
                        &velocity_to);

    /* Apply motion blur to the rendered contents */
    postprocessing_motion_blur_execute(_motion_blur,
                                       color_to,
                                       velocity_to,
                                       _spinner_blurred_to);
}

/** TODO */
PRIVATE void _stage_intro_configure_timeline(demo_timeline timeline,
                                             void*         unused)
{
    const demo_timeline_video_segment_pass intro_segment_passes[]  =
    {
        {
            system_hashed_ansi_string_create("Spinner rendering"),
            _stage_intro_render_spinner,
            NULL
        },
        {
            system_hashed_ansi_string_create("Motion-blur of the spinner"),
            _stage_intro_blur_spinner,
            NULL /* user_arg */
        },
        {
            system_hashed_ansi_string_create("Overlay"),
            _stage_intro_render_overlay,
            NULL /* user_arg */
        }
    };

    demo_timeline_add_video_segment(timeline,
                                    system_hashed_ansi_string_create("Intro video segment"),
                                    system_time_get_time_for_s   (0),    /* start_time */
                                    system_time_get_time_for_msec(5650), /* end_time */
                                    1280.0f / 720.0f,                    /* aspect_ratio */
                                    NULL,                                /* out_op_segment_id_ptr */
                                    &_video_segment);

    demo_timeline_video_segment_add_passes(_video_segment,
                                           sizeof(intro_segment_passes) / sizeof(intro_segment_passes[0]),
                                           intro_segment_passes);
}

/** TODO */
PRIVATE void _stage_intro_deinit_rendering_callback(void* unused)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (_blit_fbo_id != 0)
    {
        entrypoints_ptr->pGLDeleteFramebuffers(1,
                                             &_blit_fbo_id);

        _blit_fbo_id = 0;
    }

    if (_motion_blur != NULL)
    {
        postprocessing_motion_blur_release(_motion_blur);

        _motion_blur = NULL;
    }

    if (_spinner_blurred_to != NULL)
    {
        ogl_texture_release(_spinner_blurred_to);

        _spinner_blurred_to = NULL;
    }

    if (_overlay_circle != NULL)
    {
        procedural_mesh_circle_release(_overlay_circle);

        _overlay_circle = NULL;
    } /* if (_overlay_circle != NULL) */

    if (_overlay_po != NULL)
    {
        ogl_program_release(_overlay_po);

        _overlay_po = NULL;
    } /* if (_overlay_po != NULL) */

    if (_overlay_vao_id != 0)
    {
        entrypoints_ptr->pGLDeleteVertexArrays(1,
                                              &_overlay_vao_id);

        _overlay_vao_id = 0;
    } /* if (_overlay_vao_id != 0) */
}

/** TODO */
PRIVATE void _stage_intro_draw_spinner_animation_rendering_callback(ogl_context context,
                                                                    uint32_t    n_frame,
                                                                    system_time frame_time,
                                                                    const int*  rendering_area_px_topdown,
                                                                    void*       user_arg)
{
    static const spinner_stage stage_animation = SPINNER_STAGE_ANIMATION;

    spinner_set_property(SPINNER_PROPERTY_CURRENT_STAGE,
                        &user_arg);
    {
        demo_timeline_video_segment_render(_video_segment,
                                           n_frame,
                                           frame_time,
                                           rendering_area_px_topdown);
    }
    spinner_set_property(SPINNER_PROPERTY_CURRENT_STAGE,
                        &stage_animation);
}

/** TODO */
PRIVATE void _stage_intro_draw_spinner_frame_rendering_callback(ogl_context context,
                                                                void*       user_arg)
{
    static const spinner_stage stage_animation = SPINNER_STAGE_ANIMATION;

    spinner_set_property(SPINNER_PROPERTY_CURRENT_STAGE,
                        &user_arg);
    {
        demo_timeline_video_segment_render(_video_segment,
                                           0,    /* frame_index               - irrelevant */
                                           0,    /* frame_time                - unused */
                                           NULL);/* rendering_area_px_topdown - unused */
    }
    spinner_set_property(SPINNER_PROPERTY_CURRENT_STAGE,
                        &stage_animation);
}

/** TODO */
PRIVATE void _stage_intro_init_rendering_callback(ogl_context context,
                                                  void*       unused)
{
    static const char* overlay_fs = "#version 430 core\n"
                                    "\n"
                                    "layout(binding = 0) uniform sampler2D data;\n"
                                    "\n"
                                    "in vec2 fs_vertex;\n"
                                    "\n"
                                    "out vec4 result;\n"
                                    "\n"
                                    "void main()\n"
                                    "{\n"
                                    /* Add a vignette on the exteriors of the circle. */
                                    "    float fade_factor = 1.0 - (clamp(length(fs_vertex) - 0.9, 0.0, 0.1) / 0.1);\n"
                                    "\n"
                                    "    result = vec4( (texelFetch(data, ivec2(gl_FragCoord.xy), 0) + vec4(1.0, 0.0, 0.0, 0.0)).xyz, fade_factor);\n"
                                    "}\n";
    static const char* overlay_vs = "#version 430 core\n"
                                    "\n"
                                    "layout(location = 0) in vec2 vertex;\n"
                                    "\n"
                                    "out vec2 fs_vertex;\n"
                                    "\n"
                                    "void main()\n"
                                    "{\n"
                                    "    gl_Position = vec4(vertex, 0.0, 1.0);\n"
                                    "    fs_vertex   = vertex;\n"
                                    "}";

    ogl_shader fs = NULL;
    ogl_shader vs = NULL;

    /* Set up the overlay program object */
    _overlay_po = ogl_program_create(context,
                                     system_hashed_ansi_string_create("Overlay PO (intro)"),
                                     OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    fs = ogl_shader_create(context,
                           SHADER_TYPE_FRAGMENT,
                           system_hashed_ansi_string_create("Overlay FS (intro)") );
    vs = ogl_shader_create(context,
                           SHADER_TYPE_VERTEX,
                           system_hashed_ansi_string_create("Overlay VS (intro)") );

    ogl_shader_set_body(fs,
                        system_hashed_ansi_string_create(overlay_fs) );
    ogl_shader_set_body(vs,
                        system_hashed_ansi_string_create(overlay_vs) );

    ogl_program_attach_shader(_overlay_po,
                              fs);
    ogl_program_attach_shader(_overlay_po,
                              vs);

    ogl_program_link(_overlay_po);

    /* Instantiate circle mesh instance. */
    GLuint       circle_data_bo_id           = 0;
    unsigned int circle_data_bo_start_offset = -1;

    _overlay_circle = procedural_mesh_circle_create(context,
                                                    DATA_BO_ARRAYS,
                                                    128, /* n_segments */
                                                    system_hashed_ansi_string_create("Overlay circle") );

    procedural_mesh_circle_get_property(_overlay_circle,
                                        PROCEDURAL_MESH_CIRCLE_PROPERTY_ARRAYS_BO_ID,
                                       &circle_data_bo_id);
    procedural_mesh_circle_get_property(_overlay_circle,
                                        PROCEDURAL_MESH_CIRCLE_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET,
                                       &circle_data_bo_start_offset);
    procedural_mesh_circle_get_property(_overlay_circle,
                                        PROCEDURAL_MESH_CIRCLE_PROPERTY_N_VERTICES,
                                       &_overlay_circle_n_vertices);

    /* Set up the VAO we will use to render the mesh */
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLGenVertexArrays(1, /* n */
                                       &_overlay_vao_id);
    entrypoints_ptr->pGLBindVertexArray(_overlay_vao_id);

    entrypoints_ptr->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                circle_data_bo_id);
    entrypoints_ptr->pGLVertexAttribPointer    (0, /* index */
                                                2, /* size */
                                                GL_FLOAT,
                                                GL_FALSE,          /* normalized */
                                                sizeof(float) * 2, /* stride */
                                                (void*) circle_data_bo_start_offset);
    entrypoints_ptr->pGLEnableVertexAttribArray(0); /* index */

    /* Clean up */
    ogl_shader_release(fs);
    fs = NULL;

    ogl_shader_release(vs);
    vs = NULL;

    /* Set up texture objects */
    _spinner_blurred_to = ogl_texture_create_empty(context,
                                                   system_hashed_ansi_string_create("Blurred spinner color TO"));

    entrypoints_ptr->pGLBindTexture (GL_TEXTURE_2D,
                                     _spinner_blurred_to);
    entrypoints_ptr->pGLTexStorage2D(GL_TEXTURE_2D,
                                     1, /* levels */
                                     GL_RGBA8,
                                     _resolution[0],
                                     _resolution[1]);

    /* Set up framebuffer objects */
    entrypoints_ptr->pGLGenFramebuffers     (1,
                                            &_blit_fbo_id);
    entrypoints_ptr->pGLBindFramebuffer     (GL_READ_FRAMEBUFFER,
                                             _blit_fbo_id);
    entrypoints_ptr->pGLFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                                             GL_COLOR_ATTACHMENT0,
                                             GL_TEXTURE_2D,
                                             _spinner_blurred_to,
                                             0); /* level */

    ASSERT_DEBUG_SYNC(entrypoints_ptr->pGLCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                      "Spinner read FBO is incomplete");

    /* Init motion blur post-processor */
    _motion_blur = postprocessing_motion_blur_create(context,
                                                     POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RGBA8,
                                                     POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG32F,
                                                     POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D,
                                                     system_hashed_ansi_string_create("Spinner motion blur") );

}

/** Adds the overlay over the blurred spinner and blits stuff into Emerald-default FBO. */
PRIVATE RENDERING_CONTEXT_CALL void _stage_intro_render_overlay(ogl_context context,
                                                                uint32_t    frame_index,
                                                                system_time frame_time,
                                                                const int*  rendering_area_px_topdown,
                                                                void*       unused)
{
    GLuint                            default_fbo_id  = -1;
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    GLint                             spinner_viewport[4];

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                            &default_fbo_id);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Blit the blurred version of the spinner into the default FBO */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                            &default_fbo_id);

    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        default_fbo_id);
    entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                        _blit_fbo_id);

    entrypoints_ptr->pGLMemoryBarrier  (GL_FRAMEBUFFER_BARRIER_BIT);
    entrypoints_ptr->pGLBlitFramebuffer(0,              /* srcX0 */
                                        0,              /* srcY0 */
                                        _resolution[0], /* srcX1 */
                                        _resolution[1], /* srcY1 */
                                        0,              /* dstX0 */
                                        0,              /* dstY0 */
                                        _resolution[0], /* dstX1 */
                                        _resolution[1], /* dstY1 */
                                        GL_COLOR_BUFFER_BIT,
                                        GL_NEAREST);

    /* Draw the overlay circle on top of the spinner */
    GLint    cached_viewport[4];
    float    current_size = 0.0f;
    uint32_t frame_time_msec;

    system_time_get_msec_for_time(frame_time,
                                 &frame_time_msec);

    if (frame_time_msec >= 3500)
    {
        current_size = system_math_smoothstep(float(frame_time_msec),
                                              3500.0f,
                                              7500.0f) * 1.6f;
    }

    if (current_size > 1.0f)
    {
        current_size = 1.0f;
    }

    spinner_get_property(SPINNER_PROPERTY_VIEWPORT,
                         spinner_viewport);

    float spinner_viewport_x1_y1_size_resized[3];
    float spinner_viewport_x1_y1_size[] =
    {
        float(spinner_viewport[0]),
        float(spinner_viewport[1]),
        float(spinner_viewport[2] - spinner_viewport[0])
    };

    system_math_other_resize_quad2d_by_diagonal(spinner_viewport_x1_y1_size,
                                                current_size,
                                                spinner_viewport_x1_y1_size_resized);

    entrypoints_ptr->pGLGetIntegerv(GL_VIEWPORT,        /* <- this will use Emerald's internal state cache */
                                    cached_viewport);

    entrypoints_ptr->pGLViewport(GLint(spinner_viewport_x1_y1_size_resized[0]),
                                 GLint(spinner_viewport_x1_y1_size_resized[1]),
                                 GLint(spinner_viewport_x1_y1_size_resized[2]),
                                 GLint(spinner_viewport_x1_y1_size_resized[2]) );
    {
        entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_overlay_po) );
        entrypoints_ptr->pGLBindVertexArray(_overlay_vao_id);
        entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                            default_fbo_id);

        entrypoints_ptr->pGLBlendFunc(GL_SRC_ALPHA,
                                      GL_ONE_MINUS_SRC_ALPHA);
        entrypoints_ptr->pGLEnable   (GL_BLEND);
        {
            /* TODO: We really should be setting all these states prior to linking. */
            entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                           0, /* first */
                                           _overlay_circle_n_vertices);
        }
        entrypoints_ptr->pGLDisable(GL_BLEND);
    }
    entrypoints_ptr->pGLViewport(cached_viewport[0],
                                 cached_viewport[1],
                                 cached_viewport[2] - cached_viewport[0],
                                 cached_viewport[3] - cached_viewport[1]);
}

/** Renders the spinner. */
PRIVATE RENDERING_CONTEXT_CALL void _stage_intro_render_spinner(ogl_context context,
                                                                uint32_t    frame_index,
                                                                system_time frame_time,
                                                                const int*  rendering_area_px_topdown,
                                                                void*       unused)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Generate spinner and rasterize it into color & velocity textures.
     */
    spinner_render      (frame_index,
                         frame_time);
}


/** Please see header for specification */
PUBLIC void stage_intro_enqueue(demo_loader   loader,
                                spinner_stage stage)
{
    switch (stage)
    {
        case SPINNER_STAGE_FIRST_SPIN:
        case SPINNER_STAGE_SECOND_SPIN:
        {
            demo_loader_op_render_animation render_animation_arg;

            render_animation_arg.pfn_rendering_callback_proc = _stage_intro_draw_spinner_animation_rendering_callback;
            render_animation_arg.user_arg                    = (void*) stage;

            demo_loader_enqueue_operation(loader,
                                          DEMO_LOADER_OP_RENDER_ANIMATION,
                                         &render_animation_arg);

            break;
        }

        case SPINNER_STAGE_START:
        {
            demo_loader_op_render_frame render_frame_arg;

            render_frame_arg.pfn_rendering_callback_proc = _stage_intro_draw_spinner_frame_rendering_callback;
            render_frame_arg.user_arg                    = (void*) stage;

            demo_loader_enqueue_operation(loader,
                                          DEMO_LOADER_OP_RENDER_FRAME,
                                         &render_frame_arg);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized spinner stage requested");
        }
    } /* switch (stage) */
}

/** Please see header for specification */
PUBLIC void stage_intro_init(demo_app    app,
                             demo_loader loader)
{
    demo_loader_op_configure_timeline                configure_timeline_op;
    demo_loader_op_request_rendering_thread_callback init_callback_op;

    /* Cache the resolution */
    demo_app_get_property(app,
                          DEMO_APP_PROPERTY_RESOLUTION,
                          _resolution);

    /* Enqueue loader operations */
    configure_timeline_op.pfn_configure_timeline_proc = _stage_intro_configure_timeline;
    configure_timeline_op.user_arg                    = NULL;

    init_callback_op.pfn_rendering_callback_proc = _stage_intro_init_rendering_callback;
    init_callback_op.pfn_teardown_func_ptr       = _stage_intro_deinit_rendering_callback;
    init_callback_op.should_swap_buffers         = false;
    init_callback_op.teardown_callback_user_arg  = NULL;
    init_callback_op.user_arg                    = NULL;

    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_CONFIGURE_TIMELINE,
                                 &configure_timeline_op);
    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK,
                                 &init_callback_op);
}

