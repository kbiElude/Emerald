/**
 *
 * Compute shader + SSBO test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include <algorithm>

#ifdef _WIN32
    #undef min
#endif

ral_buffer      _bo                   = NULL;
ral_context     _context              = NULL;
ral_framebuffer _read_fbo             = NULL;
int             _local_workgroup_size = 0;
ral_program     _program              = NULL;
ral_texture     _texture              = NULL;
system_event    _window_closed_event  = system_event_create(true); /* manual_reset */
int             _window_size[2]       = {1280, 720};

const char* _cs_body_preamble = "#version 430 core\n"
                                "\n"
                                "#extension GL_ARB_compute_shader               : require\n"
                                "#extension GL_ARB_shader_storage_buffer_object : require\n"
                                "\n";

const char* _cs_body_core     = "layout (local_size_x = LOCAL_SIZE, local_size_y = LOCAL_SIZE) in;\n"
                                "\n"
                                "layout(rgba8) restrict writeonly uniform image2D result;\n"
                                "\n"
                                "layout(std140) restrict readonly buffer dataBlock\n"
                                "{\n"
                                "    uint test_counter;"
                                "};\n"
                                "\n"
                                "void main()\n"
                                "{\n"
                                "    const uvec2 current_xy   = gl_GlobalInvocationID.xy;\n"
                                "    const uvec2 image_size   = imageSize(result);\n"
                                "           vec4 result_value = vec4(float (test_counter + current_xy.x                 % image_size.x)                / float(image_size.x),\n"
                                "                                    float (test_counter + current_xy.y                 % image_size.y)                / float(image_size.y),\n"
                                "                                    float((test_counter + current_xy.x + current_xy.y) % image_size.x + image_size.y) / float(image_size.x + image_size.y),\n"
                                "                                    1.0);\n"
                                "\n"
                                "    imageStore(result, ivec2(current_xy), result_value);\n"
                                "}";

/** TODO */
system_hashed_ansi_string _get_cs_body(ogl_context context_gl)
{
    const ogl_context_gl_limits*    limits_ptr                       = NULL;
    const GLint*                    max_local_work_group_dimensions  = NULL;
          GLint                     max_local_work_group_invocations = 0;
          system_hashed_ansi_string result                           = NULL;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    max_local_work_group_invocations = limits_ptr->max_compute_work_group_invocations;
    max_local_work_group_dimensions  = limits_ptr->max_compute_work_group_size;

    /* Form the body */
    const unsigned int max_dimension_size    = std::min(max_local_work_group_dimensions[0],
                                                        max_local_work_group_dimensions[1]);
          char         definitions_part[1024];

    _local_workgroup_size = (int) sqrt( (float) max_local_work_group_invocations);

    if (_local_workgroup_size > max_dimension_size)
    {
        _local_workgroup_size = max_dimension_size;
    }

    snprintf(definitions_part,
             sizeof(definitions_part),
             "#define LOCAL_SIZE %d\n",
             _local_workgroup_size);

    /* Form the body */
    const char* body_strings[] =
    {
        _cs_body_preamble,
        definitions_part,
        _cs_body_core
    };
    const unsigned int n_body_strings = sizeof(body_strings) /
                                        sizeof(body_strings[0]);

    result = system_hashed_ansi_string_create_by_merging_strings(n_body_strings,
                                                                 body_strings);

    return result;
}

/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
                        void*       renderer)
{
    const  ogl_context_gl_entrypoints* entry_points    = NULL;
    static bool                        has_initialized = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                             &entry_points);

    if (!has_initialized)
    {
        /* Set up the data buffer object */
        ral_buffer_create_info bo_create_info;
        const int              data_bo_contents = 0;

        bo_create_info.mappability_bits = 0;
        bo_create_info.parent_buffer    = NULL;
        bo_create_info.property_bits    = 0;
        bo_create_info.size             = sizeof(data_bo_contents);
        bo_create_info.start_offset     = 0;
        bo_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT;
        bo_create_info.user_queue_bits  = RAL_QUEUE_COMPUTE_BIT | RAL_QUEUE_GRAPHICS_BIT;

        ral_context_create_buffers(_context,
                                   1, /* n_buffers */
                                  &bo_create_info,
                                  &_bo);

        /* Set up the compute shader object */
        ral_shader                      cs             = NULL;
        const system_hashed_ansi_string cs_body        = _get_cs_body(context);
        const ral_shader_create_info    cs_create_info =
        {
            system_hashed_ansi_string_create("Compute shader object"),
            RAL_SHADER_TYPE_COMPUTE
        };

        ral_context_create_shaders(_context,
                                   1, /* n_create_info_items */
                                  &cs_create_info,
                                  &cs);

        ral_shader_set_property(cs,
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &cs_body);

        /* Set up the compute program object */
        const ral_program_create_info po_create_info =
        {
            RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
            system_hashed_ansi_string_create("Program object")
        };

        ral_context_create_programs(_context,
                                    1, /* n_create_info_items */
                                   &po_create_info,
                                   &_program);

        ral_program_attach_shader(_program,
                                  cs);

        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &cs);

        /* Set up the texture object we will have the CS write to. The texture mip-map will later be
         * used as a source for the blit operation which is going to fill the back buffer with data.
         */
        ral_texture_create_info texture_create_info;
        raGL_texture            texture_gl    = NULL;
        GLuint                  texture_gl_id = 0;

        texture_create_info.base_mipmap_depth      = 1;
        texture_create_info.base_mipmap_height     = _window_size[1];
        texture_create_info.base_mipmap_width      = _window_size[0];
        texture_create_info.fixed_sample_locations = true;
        texture_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA8_UNORM;
        texture_create_info.n_layers               = 1;
        texture_create_info.n_samples              = 1;
        texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
        texture_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_SRC_BIT |
                                                     RAL_TEXTURE_USAGE_IMAGE_STORE_OPS_BIT;
        texture_create_info.use_full_mipmap_chain  = false;

        ral_context_create_textures(_context,
                                    1, /* n_textures */
                                   &texture_create_info,
                                   &_texture);

        texture_gl = ral_context_get_texture_gl(_context,
                                                _texture);

        raGL_texture_get_property(texture_gl,
                                  RAGL_TEXTURE_PROPERTY_ID,
                                 &texture_gl_id);

        entry_points->pGLBindImageTexture(0, /* index */
                                          texture_gl_id,
                                          0,        /* level */
                                          GL_FALSE, /* layered */
                                          0,        /* layer */
                                          GL_WRITE_ONLY,
                                          GL_RGBA8);

        /* Set up the read FBO we are going to use for the blit ops. */
        ral_context_create_framebuffers  (_context,
                                          1, /* n_framebuffers */
                                         &_read_fbo);
        ral_framebuffer_set_attachment_2D(_read_fbo,
                                          RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                          0, /* index */
                                          _texture,
                                          0 /* n_mipmap */);

        has_initialized = true;
    } /* if (!has_initialized) */

    /* Update the test counter value */
    raGL_buffer                           bo_raGL              = NULL;
    GLuint                                bo_raGL_id           = 0;
    uint32_t                              bo_raGL_start_offset = -1;
    uint32_t                              bo_ral_start_offset  = -1;
    ral_buffer_client_sourced_update_info update;

    bo_raGL = ral_context_get_buffer_gl(_context,
                                        _bo);

    raGL_buffer_get_property(bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &bo_raGL_id);
    raGL_buffer_get_property(bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &bo_raGL_start_offset);
    ral_buffer_get_property (_bo,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &bo_ral_start_offset);

    entry_points->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                     0,
                                     bo_raGL_id,
                                     bo_ral_start_offset + bo_raGL_start_offset,
                                     sizeof(n_frames_rendered) );

    update.data         = &n_frames_rendered;
    update.data_size    = sizeof(n_frames_rendered);
    update.start_offset = 0;

    ral_buffer_set_data_from_client_memory(_bo,
                                           1, /* n_updates */
                                          &update);

    /* Run the compute shader invocations.
     *
     * NOTE: We're doing a ceiling division here to ensure the whole texture mipmap
     *       is filled with data. */
    const raGL_program program_raGL           = ral_context_get_program_gl(_context,
                                                                           _program);
    GLuint             program_raGL_id        = 0;
    unsigned int       n_global_invocations[] =
    {
        (_window_size[0] + _local_workgroup_size - 1) / _local_workgroup_size,
        (_window_size[1] + _local_workgroup_size - 1) / _local_workgroup_size
    };

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    entry_points->pGLUseProgram     (program_raGL_id);
    entry_points->pGLDispatchCompute(n_global_invocations[0],
                                     n_global_invocations[1],
                                     1); /* num_groups_z */

    /* Copy the result data to the back buffer */
    raGL_framebuffer read_fbo_gl    = NULL;
    GLuint           read_fbo_gl_id = 0;

    read_fbo_gl = ral_context_get_framebuffer_gl(_context,
                                                 _read_fbo);

    raGL_framebuffer_get_property(read_fbo_gl,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &read_fbo_gl_id);

    entry_points->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                     read_fbo_gl_id);

    entry_points->pGLMemoryBarrier  (GL_FRAMEBUFFER_BARRIER_BIT);
    entry_points->pGLBlitFramebuffer(0,
                                     0,
                                     _window_size[0],
                                     _window_size[1],
                                     0,
                                     0,
                                     _window_size[0],
                                     _window_size[1],
                                     GL_COLOR_BUFFER_BIT,
                                     GL_NEAREST);
}

/** Event callback handlers */
bool _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    if (_bo != NULL)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   (const void**) &_bo);

        _bo = NULL;
    }

    if (_program != NULL)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &_program);

        _program = NULL;
    }

    if (_read_fbo != NULL)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                                   1, /* n_objects */
                                   (const void**) &_read_fbo);

        _read_fbo = NULL;
    }

    if (_texture != NULL)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_textures */
                                   (const void**) &_texture);

        _texture = NULL;
    }

    system_event_set(_window_closed_event);
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
    demo_window                             window             = NULL;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Compute shader SSBO test app");
    int                                     window_x1y1x2y2[4] = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    _window_size[0] /= 2;
    _window_size[1] /= 2;


    window_create_info.resolution[0] = _window_size[0];
    window_create_info.resolution[1] = _window_size[1];

    window = demo_app_create_window(window_name,
                                    window_create_info,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _window_closing_callback_handler,
                                  NULL);

    demo_window_start_rendering(window,
                                0 /* rendering_start_time */);
    system_event_wait_single   (_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    demo_app_destroy_window(window_name);

    system_event_release(_window_closed_event);
    return 0;
}