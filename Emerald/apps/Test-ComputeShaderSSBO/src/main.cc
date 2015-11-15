/**
 *
 * Compute shader + SSBO test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ssb.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include <algorithm>

#ifdef _WIN32
    #undef min
#endif

GLuint       _bo_id                = 0;
ogl_context  _context              = NULL;
GLuint       _read_fbo_id          = 0;
int          _local_workgroup_size = 0;
ogl_program  _program              = NULL;
ogl_texture  _texture              = NULL;
system_event _window_closed_event  = system_event_create(true); /* manual_reset */
int          _window_size[2]       = {1280, 720};

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
system_hashed_ansi_string _get_cs_body()
{
    const ogl_context_gl_limits*    limits_ptr                       = NULL;
    const GLint*                    max_local_work_group_dimensions  = NULL;
          GLint                     max_local_work_group_invocations = 0;
          system_hashed_ansi_string result                           = NULL;

    ogl_context_get_property(_context,
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

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                             &entry_points);

    if (!has_initialized)
    {
        /* Set up the data buffer object */
        const int data_bo_contents = 0;

        entry_points->pGLGenBuffers(1,
                                   &_bo_id);
        entry_points->pGLBindBuffer(GL_SHADER_STORAGE_BUFFER,
                                    _bo_id);

        entry_points->pGLBufferData(GL_SHADER_STORAGE_BUFFER,
                                    sizeof(data_bo_contents),
                                   &data_bo_contents,
                                    GL_DYNAMIC_DRAW);

        /* Set up the compute shader object */
        ogl_shader cs = ogl_shader_create(context,
                                          RAL_SHADER_TYPE_COMPUTE,
                                          system_hashed_ansi_string_create("Compute shader object") );

        ogl_shader_set_body(cs,
                            _get_cs_body() );

        /* Set up the compute program object */
        _program = ogl_program_create(context,
                                      system_hashed_ansi_string_create("Program object") );

        ogl_program_attach_shader(_program,
                                  cs);

        ogl_shader_release(cs);
        cs = NULL;

        if (!ogl_program_link(_program))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Test program object failed to link");
        }

        /* Set up the texture object we will have the CS write to. The texture mip-map will later be
         * used as a source for the blit operation which is going to fill the back buffer with data.
         */
        _texture = ogl_texture_create_and_initialize(_context,
                                                     system_hashed_ansi_string_create("Test texture"),
                                                     RAL_TEXTURE_TYPE_2D,
                                                     RAL_TEXTURE_FORMAT_RGBA8_UNORM,
                                                     false,  /* use_full_mipmap_chain */
                                                     _window_size[0],
                                                     _window_size[1],
                                                     1,      /* base_mipmap_depth */
                                                     1,      /* n_samples */
                                                     false); /* fixed_sample_locations */

        entry_points->pGLBindImageTexture(0, /* index */
                                          _texture,
                                          0,        /* level */
                                          GL_FALSE, /* layered */
                                          0,        /* layer */
                                          GL_WRITE_ONLY,
                                          GL_RGBA8);

        /* Set up the read FBO we are going to use for the blit ops. */
        entry_points->pGLGenFramebuffers(1,
                                        &_read_fbo_id);
        entry_points->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                        _read_fbo_id);

        entry_points->pGLFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                                              GL_COLOR_ATTACHMENT0,
                                              GL_TEXTURE_2D,
                                              _texture,
                                              0); /* level */

        has_initialized = true;
    } /* if (!has_initialized) */

    /* Run the compute shader invocations.
     *
     * NOTE: We're doing a ceiling division here to ensure the whole texture mipmap
     *       is filled with data. */
    unsigned int n_global_invocations[] =
    {
        (_window_size[0] + _local_workgroup_size - 1) / _local_workgroup_size,
        (_window_size[1] + _local_workgroup_size - 1) / _local_workgroup_size
    };

    entry_points->pGLUseProgram     (ogl_program_get_id(_program) );
    entry_points->pGLDispatchCompute(n_global_invocations[0],
                                     n_global_invocations[1],
                                     1); /* num_groups_z */

    /* Update the test counter value */
    entry_points->pGLBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                                    0,
                                    _bo_id);
    entry_points->pGLBufferSubData(GL_SHADER_STORAGE_BUFFER,
                                   0, /* offset */
                                   sizeof(n_frames_rendered),
                                  &n_frames_rendered);

    /* Copy the result data to the back buffer */
    entry_points->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                     _read_fbo_id);

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
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (_bo_id != 0)
    {
        entry_points->pGLDeleteBuffers(1,
                                      &_bo_id);

        _bo_id = 0;
    }

    if (_program != NULL)
    {
        ogl_program_release(_program);

        _program = NULL;
    }

    if (_read_fbo_id != 0)
    {
        entry_points->pGLDeleteFramebuffers(1,
                                           &_read_fbo_id);

        _read_fbo_id = 0;
    }

    if (_texture != NULL)
    {
        ogl_texture_release(_texture);

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
    system_screen_mode    screen_mode              = NULL;
    system_window         window                   = NULL;
    system_pixel_format   window_pf                = NULL;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                           0); /* stencil_buffer_bits     */

#if 0
    window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                             screen_mode,
                                             true, /* vsync_enabled */
                                             window_pf);
#else
    _window_size[0] /= 2;
    _window_size[1] /= 2;

    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Test window"),
                                                 false, /* scalable */
                                                 true,  /* vsync_enabled */
                                                 true,  /* visible */
                                                 window_pf);
#endif

    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,
                                                                            _rendering_handler,
                                                                            NULL);

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        (void*) _rendering_lbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    system_window_close (window);
    system_event_release(_window_closed_event);

    return 0;
}