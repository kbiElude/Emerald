/**
 *
 * Pipeline test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_window.h"

ogl_context   _context             = NULL;
ogl_pipeline  _pipeline            = NULL;
uint32_t      _pipeline_stage_id   = -1;
system_window _window              = NULL;
system_event  _window_closed_event = system_event_create(true,   /* manual_reset */
                                                         false); /* start_state */
int           _window_size[2]      = {0};

GLuint      _fbo_id                         = -1;
ogl_program _generation_po                  = NULL;
GLuint      _generation_po_time_location    = -1;
ogl_program _modification_po                = NULL;
GLuint      _modification_po_input_location = -1; /* set to GL_TEXTURE0 */
ogl_texture _to_1                           = 0;
ogl_texture _to_2                           = 0;
GLuint      _vao_id                         = 0;

const char* fragment_shader_generation = "#version 330\n"
                                         "\n"
                                         "in      vec2  uv;\n"
                                         "out     vec4  result;\n"
                                         "uniform float time;\n"
                                         "\n"
                                         "void main()\n"
                                         "{\n"
                                         "    result += vec4(cos(uv + vec2(time) ) * 0.5 + 0.5, sin(time) * 0.5 + 0.5, 1);\n"
                                         "}\n";
const char* fragment_shader_modification = "#version 330\n"
                                           "\n"
                                           "in      vec2      uv;\n"
                                           "out     vec4      result;\n"
                                           "uniform sampler2D input;\n"
                                           "\n"
                                           "void main()\n"
                                           "{\n"
                                           "    vec4 input_data = texture2D(input, uv);\n"
                                           "\n"
                                           "    if (uv.x >= 0.5)\n"
                                           "    {\n"
                                           "        result = vec4(vec3(1.0) - input_data.xyz, 1);\n"
                                           "    }\n"
                                           "    else\n"
                                           "    {\n"
                                           "        result = input_data;\n"
                                           "    }\n"
                                           "}\n";
/* GL deinitialization */
void _deinit_gl(ogl_context context,
                void*       arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLDeleteFramebuffers(1,
                                       &_fbo_id);

    ogl_program_release(_generation_po);
    ogl_program_release(_modification_po);

    ogl_texture_release(_to_1);
    ogl_texture_release(_to_2);

    _fbo_id = 0;
}

/* GL initialization */
void _init_gl(ogl_context context,
              void*       arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    ogl_texture                                               tos[2]           = {0};

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLGenFramebuffers(1,
                                    &_fbo_id);
    entry_points->pGLGenVertexArrays(1,
                                    &_vao_id);

    tos[0] = ogl_texture_create_empty(context,
                                      system_hashed_ansi_string_create("1"));
    tos[1] = ogl_texture_create_empty(context,
                                      system_hashed_ansi_string_create("2"));

    _to_1 = tos[0];
    _to_2 = tos[1];

    /* Set up texture storage */
    ASSERT_DEBUG_SYNC(_window_size[0] != 0,
                      "Window width is 0");
    ASSERT_DEBUG_SYNC(_window_size[1] != 0,
                      "Window height is 0");

    dsa_entry_points->pGLTextureStorage2DEXT (_to_1,
                                              GL_TEXTURE_2D,
                                              1, /* levels */
                                              GL_RGBA8,
                                              _window_size[0],
                                              _window_size[1]);
    dsa_entry_points->pGLTextureStorage2DEXT (_to_2,
                                              GL_TEXTURE_2D,
                                              1, /* levels */
                                              GL_DEPTH_COMPONENT32,
                                              _window_size[0],
                                              _window_size[1]);

    dsa_entry_points->pGLTextureParameteriEXT(_to_1,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE_MIN_FILTER,
                                              GL_LINEAR);
    dsa_entry_points->pGLTextureParameteriEXT(_to_2,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE_MIN_FILTER,
                                              GL_LINEAR);

    /* Set up fbo */
    dsa_entry_points->pGLNamedFramebufferTexture2DEXT(_fbo_id,
                                                      GL_COLOR_ATTACHMENT0,
                                                      GL_TEXTURE_2D,
                                                      _to_1,
                                                      0); /* level */
    dsa_entry_points->pGLNamedFramebufferTexture2DEXT(_fbo_id,
                                                      GL_DEPTH_ATTACHMENT,
                                                      GL_TEXTURE_2D,
                                                      _to_2,
                                                      0); /* level */

    /* Set up programs */
    shaders_vertex_fullscreen general_vs      = shaders_vertex_fullscreen_create(_context,
                                                                                 true, /* export_uv */
                                                                                 system_hashed_ansi_string_create("general") );
    ogl_shader                generation_fs   = ogl_shader_create               (_context,
                                                                                 SHADER_TYPE_FRAGMENT,
                                                                                 system_hashed_ansi_string_create("creation") );
    ogl_shader                modification_fs = ogl_shader_create               (_context,
                                                                                 SHADER_TYPE_FRAGMENT,
                                                                                 system_hashed_ansi_string_create("modification") );

    _generation_po   = ogl_program_create(_context,
                                          system_hashed_ansi_string_create("generation") );
    _modification_po = ogl_program_create(_context,
                                          system_hashed_ansi_string_create("modification") );

    ogl_shader_set_body(generation_fs,
                        system_hashed_ansi_string_create(fragment_shader_generation) );
    ogl_shader_set_body(modification_fs,
                        system_hashed_ansi_string_create(fragment_shader_modification) );

    ogl_program_attach_shader(_generation_po,
                              generation_fs);
    ogl_program_attach_shader(_generation_po,
                              shaders_vertex_fullscreen_get_shader(general_vs) );

    ogl_program_attach_shader(_modification_po,
                              modification_fs);
    ogl_program_attach_shader(_modification_po,
                              shaders_vertex_fullscreen_get_shader(general_vs) );

    ogl_program_link(_generation_po);
    ogl_program_link(_modification_po);

    ogl_shader_release               (generation_fs);
    ogl_shader_release               (modification_fs);
    shaders_vertex_fullscreen_release(general_vs);

    /* Retrieve uniform locations */
    const ogl_program_uniform_descriptor* input_descriptor = NULL;
    const ogl_program_uniform_descriptor* time_descriptor  = NULL;

    ogl_program_get_uniform_by_name(_generation_po,
                                    system_hashed_ansi_string_create("time"),
                                   &time_descriptor);
    ogl_program_get_uniform_by_name(_modification_po,
                                    system_hashed_ansi_string_create("input"),
                                   &input_descriptor);

    _generation_po_time_location    = time_descriptor->location;
    _modification_po_input_location = input_descriptor->location;
}

/* Stage step 1: sample data generation */
static void _stage_step_generate_data(ogl_context          context,
                                      system_timeline_time time,
                                      void*                not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    uint32_t                                                  time_ms          = 0;
    float                                                     time_ms_as_s     = 0;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    system_time_get_msec_for_timeline_time(time,
                                          &time_ms);

    time_ms_as_s = ((float) time_ms) / 1000.0f;

    entry_points->pGLBindVertexArray(_vao_id);

    entry_points->pGLUseProgram      (ogl_program_get_id(_generation_po) );
    entry_points->pGLProgramUniform1f(ogl_program_get_id(_generation_po),
                                      _generation_po_time_location,
                                      time_ms_as_s);

    entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                     _fbo_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0,  /* first */
                                     4); /* count */
}

/* Stage step 2: sample data modification */
static void _stage_step_modify_data(ogl_context context, system_timeline_time time, void* not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    uint32_t                                                  time_ms          = 0;
    float                                                     time_ms_as_s     = ((float) time_ms) / 1000.0f;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    system_time_get_msec_for_timeline_time(time,
                                          &time_ms);

    entry_points->pGLUseProgram             (ogl_program_get_id(_generation_po) );
    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                             GL_TEXTURE_2D,
                                             _to_1);

    entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                     0);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0, /* first */
                                     4);/* count */
}


/** Rendering handler */
void _rendering_handler(ogl_context          context,
                        uint32_t             n_frames_rendered,
                        system_timeline_time frame_time,
                        void*                renderer)
{
    ogl_pipeline_draw_stage(_pipeline,
                            _pipeline_stage_id,
                            frame_time);
}

void _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle,
                   HINSTANCE,
                   LPTSTR,
                   int)
{
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    _window_size[0] = 640;
    _window_size[1] = 480;

    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    _window                  = system_window_create_not_fullscreen         (OGL_CONTEXT_TYPE_GL,
                                                                            window_x1y1x2y2,
                                                                            system_hashed_ansi_string_create("Test window"),
                                                                            false, /* scalable */
                                                                            0,     /* n_multisampling_samples */
                                                                            false, /* vsync_enabled */
                                                                            false, /* multisampling_supported */
                                                                            true); /* visible */
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            30,                 /* desired_fps */
                                                                            _rendering_handler,
                                                                            NULL);              /* user_arg */

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);

    system_window_set_rendering_handler(_window,
                                        window_rendering_handler);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        _rendering_lbm_callback_handler,
                                        NULL);

    /* Create and configure pipeline object */
    uint32_t                   pipeline_stage_generate_data_step = -1;
    uint32_t                   pipeline_stage_modify_data_step   = -1;
    PFNOGLPIPELINECALLBACKPROC stage_1_callback                  = _stage_step_generate_data;
    PFNOGLPIPELINECALLBACKPROC stage_2_callback                  = _stage_step_modify_data;

    _pipeline          = ogl_pipeline_create   (_context,
                                                true,  /* should_overlay_performance_info */
                                                system_hashed_ansi_string_create("pipeline") );
    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                                system_hashed_ansi_string_create("Data generation"),
                                stage_1_callback,
                                NULL); /* step_callback_user_arg */
    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                                system_hashed_ansi_string_create("Data update"),
                                stage_2_callback,
                                NULL); /* step_callback_user_arg */

    /* Initialize GL objects */
    ogl_rendering_handler_request_callback_from_context_thread(window_rendering_handler,
                                                               _init_gl,
                                                               NULL); /* callback_user_arg */

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single_infinite(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    ogl_rendering_handler_request_callback_from_context_thread(window_rendering_handler,
                                                               _deinit_gl,
                                                               NULL);

    ogl_pipeline_release(_pipeline);
    system_window_close (_window);
    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}