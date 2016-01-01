/**
 *
 * Pipeline test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "main.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_buffer.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_texture.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"

ral_context   _context             = NULL;
ogl_pipeline  _pipeline            = NULL;
uint32_t      _pipeline_stage_id   = -1;
demo_window   _window              = NULL;
system_event  _window_closed_event = system_event_create(true); /* manual_reset */
int           _window_size[2]      = {0};

ral_framebuffer _fbo                              = NULL;
ogl_program     _generation_po                    = NULL;
GLuint          _generation_po_time_ub_offset     = -1;
ogl_program_ub  _generation_po_ub                 = NULL;
ral_buffer      _generation_po_ub_bo              = NULL;
ogl_program     _modification_po                  = NULL;
GLuint          _modification_po_input_location   = -1; /* set to GL_TEXTURE0 */
ral_texture     _to_1                             = 0;
ral_texture     _to_2                             = 0;
GLuint          _vao_id                           = 0;

const char* fragment_shader_generation = "#version 430 core\n"
                                         "\n"
                                         "in  vec2 uv;\n"
                                         "out vec4 result;\n"
                                         "\n"
                                         "uniform data\n"
                                         "{\n"
                                         "    float time;\n"
                                         "};\n"
                                         "\n"
                                         "void main()\n"
                                         "{\n"
                                         "    result += vec4(cos(uv + vec2(time) ) * 0.5 + 0.5, sin(time) * 0.5 + 0.5, 1);\n"
                                         "}\n";
const char* fragment_shader_modification = "#version 430 core\n"
                                           "\n"
                                           "in      vec2      uv;\n"
                                           "out     vec4      result;\n"
                                           "uniform sampler2D in_data;\n"
                                           "\n"
                                           "void main()\n"
                                           "{\n"
                                           "    vec4 input_data = texture2D(in_data, uv);\n"
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
void _deinit_gl(ral_context context)
{
    ral_texture tos[] = 
    {
        _to_1,
        _to_2
    };

    ral_context_delete_framebuffers(context,
                                    1, /* n_framebuffers */
                                   &_fbo);

    ogl_program_release(_generation_po);
    ogl_program_release(_modification_po);

    ral_context_delete_textures(context,
                                sizeof(tos) / sizeof(tos[0]),
                                tos);

    _fbo = NULL;
}

/* GL initialization */
void _init_gl(ogl_context context,
              void*       arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    ral_texture                                               tos[2]           = {0};
    ral_texture_create_info                                   to_create_info_items[2];

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ral_context_create_framebuffers(_context,
                                    1, /* n_framebuffers */
                                   &_fbo);

    entry_points->pGLGenVertexArrays(1,
                                    &_vao_id);

    to_create_info_items[0].base_mipmap_depth      = 1;
    to_create_info_items[0].base_mipmap_height     = _window_size[1];
    to_create_info_items[0].base_mipmap_width      = _window_size[0];
    to_create_info_items[0].fixed_sample_locations = true;
    to_create_info_items[0].format                 = RAL_TEXTURE_FORMAT_RGBA8_UNORM;
    to_create_info_items[0].name                   = system_hashed_ansi_string_create("TO 1");
    to_create_info_items[0].n_layers               = 1;
    to_create_info_items[0].n_samples              = 1;
    to_create_info_items[0].type                   = RAL_TEXTURE_TYPE_2D;
    to_create_info_items[0].usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT;
    to_create_info_items[0].use_full_mipmap_chain  = false;

    to_create_info_items[1].base_mipmap_depth = 1;
    to_create_info_items[1].base_mipmap_height = _window_size[1];
    to_create_info_items[1].base_mipmap_width  = _window_size[0];
    to_create_info_items[1].fixed_sample_locations = true;
    to_create_info_items[1].format                 = RAL_TEXTURE_FORMAT_DEPTH32_SNORM;
    to_create_info_items[1].name                   = system_hashed_ansi_string_create("TO 2");
    to_create_info_items[1].n_layers               = 1;
    to_create_info_items[1].n_samples              = 1;
    to_create_info_items[1].type                   = RAL_TEXTURE_TYPE_2D;
    to_create_info_items[1].usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT;

    ral_context_create_textures(_context,
                                sizeof(to_create_info_items) / sizeof(to_create_info_items[0]),
                                to_create_info_items,
                                tos);

    _to_1 = tos[0];
    _to_2 = tos[1];

    /* Set up texture storage */
    raGL_texture to_1_raGL    = NULL;
    GLuint       to_1_raGL_id = 0;
    raGL_texture to_2_raGL    = NULL;
    GLuint       to_2_raGL_id = 0;

    to_1_raGL = ral_context_get_texture_gl(_context,
                                          _to_1);
    to_2_raGL = ral_context_get_texture_gl(_context,
                                          _to_2);

    raGL_texture_get_property(to_1_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &to_1_raGL_id);
    raGL_texture_get_property(to_2_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &to_2_raGL_id);

    ASSERT_DEBUG_SYNC(_window_size[0] != 0,
                      "Window width is 0");
    ASSERT_DEBUG_SYNC(_window_size[1] != 0,
                      "Window height is 0");

    dsa_entry_points->pGLTextureParameteriEXT(to_1_raGL_id,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE_MIN_FILTER,
                                              GL_LINEAR);
    dsa_entry_points->pGLTextureParameteriEXT(to_2_raGL_id,
                                              GL_TEXTURE_2D,
                                              GL_TEXTURE_MIN_FILTER,
                                              GL_LINEAR);

    /* Set up fbo */
    ral_framebuffer_set_attachment_2D(_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      _to_1,
                                      0 /* n_mipmap */);
    ral_framebuffer_set_attachment_2D(_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL,
                                      0, /* index */
                                      _to_2,
                                      0 /* n_mipmap */);

    /* Set up programs */
    shaders_vertex_fullscreen general_vs      = shaders_vertex_fullscreen_create(_context,
                                                                                 true, /* export_uv */
                                                                                 system_hashed_ansi_string_create("general") );
    ogl_shader                generation_fs   = ogl_shader_create               (_context,
                                                                                 RAL_SHADER_TYPE_FRAGMENT,
                                                                                 system_hashed_ansi_string_create("creation") );
    ogl_shader                modification_fs = ogl_shader_create               (_context,
                                                                                 RAL_SHADER_TYPE_FRAGMENT,
                                                                                 system_hashed_ansi_string_create("modification") );

    _generation_po   = ogl_program_create(_context,
                                          system_hashed_ansi_string_create("generation"),
                                          OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);
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
    const ogl_program_variable* input_descriptor = NULL;
    const ogl_program_variable* time_descriptor  = NULL;

    ogl_program_get_uniform_by_name(_generation_po,
                                    system_hashed_ansi_string_create("time"),
                                   &time_descriptor);
    ogl_program_get_uniform_by_name(_modification_po,
                                    system_hashed_ansi_string_create("in_data"),
                                   &input_descriptor);

    _generation_po_time_ub_offset   = time_descriptor->block_offset;
    _modification_po_input_location = input_descriptor->location;

    /* Retrieve uniform block data */
    ogl_program_get_uniform_block_by_name(_generation_po,
                                          system_hashed_ansi_string_create("data"),
                                         &_generation_po_ub);

    ogl_program_ub_get_property(_generation_po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                &_generation_po_ub_bo);
}

/* Stage step 1: sample data generation */
static void _stage_step_generate_data(ral_context context,
                                      uint32_t    frame_index,
                                      system_time time,
                                      const int*  rendering_area_px_topdown,
                                      void*       not_used)
{
    ogl_context                                               context_gl               = ral_context_get_gl_context(context);
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points         = NULL;
    const ogl_context_gl_entrypoints*                         entry_points             = NULL;
    raGL_buffer                                               generation_po_ub_bo_raGL = NULL;
    uint32_t                                                  time_ms                  = 0;
    float                                                     time_ms_as_s             = 0;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    generation_po_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                         _generation_po_ub_bo);

    system_time_get_msec_for_time(time,
                                 &time_ms);

    time_ms_as_s = ((float) time_ms) / 1000.0f;

    entry_points->pGLBindVertexArray(_vao_id);
    entry_points->pGLUseProgram      (ogl_program_get_id(_generation_po) );

    ogl_program_ub_set_nonarrayed_uniform_value(_generation_po_ub,
                                                _generation_po_time_ub_offset,
                                               &time_ms_as_s,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_sync                        (_generation_po_ub);

    raGL_framebuffer fbo_raGL                              = NULL;
    GLuint           fbo_raGL_id                           = -1;
    GLuint           generation_po_ub_bo_raGL_id           =  0;
    uint32_t         generation_po_ub_bo_raGL_start_offset = -1;
    uint32_t         generation_po_ub_bo_ral_size          = -1;
    uint32_t         generation_po_ub_bo_ral_start_offset  = -1;

    fbo_raGL = ral_context_get_framebuffer_gl(context,
                                              _fbo);

    raGL_framebuffer_get_property(fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &fbo_raGL_id);

    raGL_buffer_get_property(generation_po_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &generation_po_ub_bo_raGL_id);
    raGL_buffer_get_property(generation_po_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &generation_po_ub_bo_raGL_start_offset);
    ral_buffer_get_property (_generation_po_ub_bo,
                             RAL_BUFFER_PROPERTY_SIZE,
                            &generation_po_ub_bo_ral_size);
    ral_buffer_get_property (_generation_po_ub_bo,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &generation_po_ub_bo_ral_start_offset);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                      generation_po_ub_bo_raGL_id,
                                      generation_po_ub_bo_raGL_start_offset + generation_po_ub_bo_ral_start_offset,
                                      generation_po_ub_bo_ral_size);

    entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                     fbo_raGL_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0,  /* first */
                                     4); /* count */
}

/* Stage step 2: sample data modification */
static void _stage_step_modify_data(ral_context context,
                                    uint32_t    frame_index,
                                    system_time time,
                                    const int*  rendering_area_px_topdown,
                                    void*       not_used)
{
    ogl_context                                               context_gl          = ral_context_get_gl_context(context);
    raGL_framebuffer                                          default_fbo_raGL    = NULL;
    GLuint                                                    default_fbo_raGL_id = 0;
    ral_framebuffer                                           default_fbo_ral     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points    = NULL;
    const ogl_context_gl_entrypoints*                         entry_points        = NULL;
    uint32_t                                                  time_ms             = 0;
    float                                                     time_ms_as_s        = ((float) time_ms) / 1000.0f;
    raGL_texture                                              to_1_raGL           = NULL;
    GLuint                                                    to_1_raGL_id        = 0;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS,
                            &default_fbo_ral);

    default_fbo_raGL = ral_context_get_framebuffer_gl(context,
                                                      default_fbo_ral);

    raGL_framebuffer_get_property(default_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &default_fbo_raGL_id);

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    system_time_get_msec_for_time(time,
                                 &time_ms);

    to_1_raGL = ral_context_get_texture_gl(context,
                                           _to_1);

    raGL_texture_get_property(to_1_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &to_1_raGL_id);

    entry_points->pGLUseProgram             (ogl_program_get_id(_generation_po) );
    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                             GL_TEXTURE_2D,
                                             to_1_raGL_id);

    entry_points->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                     default_fbo_raGL_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0, /* first */
                                     4);/* count */
}


/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
                        void*       renderer)
{
    ogl_pipeline_draw_stage(_pipeline,
                            _pipeline_stage_id,
                            n_frames_rendered,
                            frame_time,
                            rendering_area_px_topdown);
}

void _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    ral_context context = NULL;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                              &context);

    _deinit_gl          (context);
    ogl_pipeline_release(_pipeline);
}

/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler;
    ogl_rendering_handler                   rendering_handler  = NULL;
    system_screen_mode                      screen_mode        = NULL;
    system_window                           window             = NULL;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Marching cubes demo");

    _window_size[0] = 640;
    _window_size[1] = 480;

    _window = demo_app_create_window(window_name,
                                     RAL_BACKEND_TYPE_GL,
                                     false /* use_timeline */);

    demo_window_set_property(_window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             _window_size);

    demo_window_show(_window);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _window_closing_callback_handler,
                                  NULL);

    /* Create and configure pipeline object */
    ogl_pipeline_stage_step_declaration data_generation_stage_step;
    ogl_pipeline_stage_step_declaration data_update_stage_step;

    uint32_t                   pipeline_stage_generate_data_step = -1;
    uint32_t                   pipeline_stage_modify_data_step   = -1;
    PFNOGLPIPELINECALLBACKPROC stage_1_callback                  = _stage_step_generate_data;
    PFNOGLPIPELINECALLBACKPROC stage_2_callback                  = _stage_step_modify_data;

    _pipeline          = ogl_pipeline_create   (_context,
                                                true,  /* should_overlay_performance_info */
                                                system_hashed_ansi_string_create("pipeline") );
    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    data_generation_stage_step.name              = system_hashed_ansi_string_create("Data generation");
    data_generation_stage_step.pfn_callback_proc = stage_1_callback;
    data_generation_stage_step.user_arg          = NULL;

    data_update_stage_step.name              = system_hashed_ansi_string_create("Data update");
    data_update_stage_step.pfn_callback_proc = stage_2_callback;
    data_update_stage_step.user_arg          = NULL;

    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                               &data_generation_stage_step);
    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                               &data_update_stage_step);

    /* Initialize GL objects */
    ogl_rendering_handler_request_callback_from_context_thread(rendering_handler,
                                                               _init_gl,
                                                               NULL); /* callback_user_arg */

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    demo_window_stop_rendering(_window);
    demo_app_destroy_window   (window_name);

    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}