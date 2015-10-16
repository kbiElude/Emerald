#include "stage_intro.h"
#include "demo/demo_loader.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "procedural/procedural_mesh_circle.h"
#include "system/system_math_other.h"
#include "spinner.h"

/* Forward declarations */
PRIVATE                       void _init_overlay_po                      (ogl_context context);
PRIVATE                       void _stage_intro_deinit_rendering_callback(void*       unused);
PRIVATE                       void _stage_intro_init_rendering_callback  (ogl_context context,
                                                                          void*       unused);
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_render                    (ogl_context context,
                                                                          uint32_t    frame_index,
                                                                          system_time frame_time,
                                                                          const int*  rendering_area_px_topdown,
                                                                          void*       unused);


/* Private variables */
PRIVATE procedural_mesh_circle _overlay_circle            = NULL;
PRIVATE unsigned int           _overlay_circle_n_vertices = 0;
PRIVATE ogl_program            _overlay_po                = NULL;
PRIVATE GLuint                 _overlay_vao_id            = 0;


/** TODO */
PRIVATE void _stage_intro_configure_timeline(demo_timeline timeline,
                                             void*         unused)
{
    demo_timeline_segment_pass intro_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Intro video segment (main pass)"),
            stage_intro_render,
            NULL /* user_arg */
        }
    };

    demo_timeline_add_video_segment(timeline,
                                    system_hashed_ansi_string_create("Intro video segment"),
                                    system_time_get_time_for_s   (0),    /* start_time */
                                    system_time_get_time_for_msec(5650), /* end_time */
                                    1280.0f / 720.0f,                    /* aspect_ratio */
                                    1,                                   /* n_passes */
                                    intro_segment_passes,
                                    NULL);                               /* out_op_segment_id_ptr */
}

/** TODO */
PRIVATE void _stage_intro_deinit_rendering_callback(void* unused)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

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
}

/** Renders frame contents.
 *
 *  @param context                   Rendering context.
 *  @param frame_time                Frame time (relative to the beginning of the timeline).
 *  @param rendering_area_px_topdown Rendering area, to which frame contents should be rendered.
 *                                   The four ints (x1/y1/x2/y2) are calculated, based on the aspect
 *                                   ratio defined for the video segment.
 *                                   It is your responsibility to update the scissor area / viewport.
 *  @param unused                    Rendering call-back user argument. We're not using this, so
 *                                   the argument is always NULL.
 **/
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_render(ogl_context context,
                                                      uint32_t    frame_index,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused)
{
    GLuint                            default_fbo_id  = -1;
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                            &default_fbo_id);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Generate blurred spinner visuals. We will be rendering data over this content so ask the
     * spinner module to blit the rendered contents to the Emerald's default FBO 
     */
    ogl_texture spinner_blurred_to = NULL;
    GLint       spinner_viewport[4];

    spinner_get_property(SPINNER_PROPERTY_RESULT_BLURRED_COLOR_TO,
                        &spinner_blurred_to);

    spinner_render(frame_index,
                   true /* should_blit_to_default_fbo */);

    /* Draw the overlay circle on top of the spinner */
    GLint    cached_viewport[4];
    float    current_size = 0.0f;
    uint32_t frame_time_msec;

    system_time_get_msec_for_time(frame_time,
                                 &frame_time_msec);

    if (frame_time_msec >= 3500)
    {
#if 0
        current_size = 0.1f + system_math_smoothstep(float(frame_time_msec),
                                                     3500.0f,
                                                     6000.0f) * 0.65f;
#else
        current_size = 0.1f + system_math_smoothstep(float(frame_time_msec),
                                                     3400.0f,
                                                     7500.0f);
#endif
    }
    else
    {
        current_size = float(frame_time_msec) / 3500.0f * 0.1f;
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
        spinner_viewport[0],
        spinner_viewport[1],
        spinner_viewport[2] - spinner_viewport[0]
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


/** Please see header for specification */
PUBLIC void stage_intro_deinit(ogl_context context)
{
    /* Nothing to deinit at the moment */
}

/** Please see header for specification */
PUBLIC void stage_intro_init(demo_loader loader)
{
    demo_loader_op_configure_timeline                configure_timeline_op;
    demo_loader_op_request_rendering_thread_callback init_callback_op;

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

