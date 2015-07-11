/**
 *
 * Emerald (kbi/elude @2012-2015
 *
 */
#include "test_webcam.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "config.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_matrix4x4.h"
#include "system/system_time.h"
#include "system/system_window.h"
#include "webcam/webcam_device.h"
#include "webcam/webcam_manager.h"
#include "webcam/webcam_device_streamer.h"

#if defined(INCLUDE_WEBCAM_MANAGER) && defined(_WIN32)

uint32_t              cam_data_width                             = 0;
uint32_t              cam_data_height                            = 0;
ogl_texture           cam_to                                     = 0;
uint32_t              n_frames_rendered                          = 0;
GLint                 fullscreen_quad_cam_texture_location       = -1;
GLuint                fullscreen_quad_position_bo_id             = -1;
GLint                 fullscreen_quad_position_location          = -1;
GLint                 fullscreen_quad_program_id                 = -1;
system_matrix4x4      fullscreen_quad_projection_matrix          = NULL;
GLint                 fullscreen_quad_projection_matrix_location = -1;
GLint                 fullscreen_quad_uv_location                = -1;
GLint                 fullscreen_quad_uv_bo_id                   = -1;
GLuint                fullscreen_quad_vao_id                     = -1;
system_matrix4x4      fullscreen_quad_view_matrix                = NULL;
GLint                 fullscreen_quad_view_matrix_location       = -1;
ogl_rendering_handler rendering_handler                          = NULL;

GLfloat fullscreen_quad_position_data[] = { 0.0f,   0.0f,   0.0f, 1.0f,
                                            320.0f, 0.0f,   0.0f, 1.0f,
                                            320.0f, 240.0f, 0.0f, 1.0f,
                                            0.0f,   240.0f, 0.0f, 1.0f};
GLfloat fullscreen_quad_uv_data[]       = { 0.0f, 0.0f,
                                            1.0f, 0.0f,
                                            1.0f, 1.0f,
                                            0.0f, 1.0f};

typedef struct
{
    const BYTE* buffer_ptr;
    long        buffer_length;

} _creation_test_on_new_frame_data_callback_arg;


static void _creation_test_on_new_texture_data_available(ogl_context context,
                                                         void*       in_arg)
{
    _creation_test_on_new_frame_data_callback_arg* arg          = (_creation_test_on_new_frame_data_callback_arg*) in_arg;
    const ogl_context_gl_entrypoints*              entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                   cam_to); /* texture */
    entry_points->pGLTexSubImage2D(GL_TEXTURE_2D,
                                   0, /* level */
                                   0, /* xoffset */
                                   0, /* yoffset */
                                   cam_data_width,
                                   cam_data_height,
                                   GL_RGB,
                                   GL_UNSIGNED_BYTE,
                                   arg->buffer_ptr);

    ASSERT_EQ(entry_points->pGLGetError(),
              GL_NO_ERROR);
}

static void _creation_test_on_render_frame_callback(ogl_context context,
                                                    uint32_t    n_frames_rendered,
                                                    system_time frame_time,
                                                    void*)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (n_frames_rendered == 0)
    {
        /* first frame. Set up textuer object for the frame. */
        cam_to = ogl_texture_create_empty(context,
                                          system_hashed_ansi_string_create("cam to") );

        entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                       cam_to);
        entry_points->pGLTexImage2D   (GL_TEXTURE_2D,
                                       0, /* level */
                                       GL_RGB,
                                       cam_data_width,
                                       cam_data_height,
                                       0, /* border */
                                       GL_RGB,
                                       GL_UNSIGNED_BYTE,
                                       0); /* pixels */
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_WRAP_S,
                                       GL_CLAMP_TO_EDGE);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_WRAP_T,
                                       GL_CLAMP_TO_EDGE);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_MAG_FILTER,
                                       GL_LINEAR);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_MIN_FILTER,
                                       GL_LINEAR);

        /* Set up rectangle VAAs */
        entry_points->pGLUseProgram(fullscreen_quad_program_id);

        /* Set up the uniforms */
        entry_points->pGLProgramUniformMatrix4fv(fullscreen_quad_program_id,
                                                 fullscreen_quad_projection_matrix_location,
                                                 1,     /* count */
                                                 false, /* transpose */
                                                 system_matrix4x4_get_row_major_data(fullscreen_quad_projection_matrix) );
        entry_points->pGLProgramUniformMatrix4fv(fullscreen_quad_program_id,
                                                 fullscreen_quad_view_matrix_location,
                                                 1,    /* count */
                                                 true, /* transpose */
                                                 system_matrix4x4_get_row_major_data(fullscreen_quad_view_matrix) );

        ASSERT_EQ(entry_points->pGLGetError(),
                  GL_NO_ERROR);

        /* Set up buffer object for position data */
        GLuint bo_ids[2] = {0, 0};

        entry_points->pGLGenBuffers(2,
                                    bo_ids);

        fullscreen_quad_position_bo_id = bo_ids[0];
        fullscreen_quad_uv_bo_id       = bo_ids[1];

        ASSERT_NE(fullscreen_quad_position_bo_id,
                  -1);
        ASSERT_NE(fullscreen_quad_uv_bo_id,
                  -1);
        ASSERT_EQ(entry_points->pGLGetError(),
                  GL_NO_ERROR);

        /* Set up VAOs */
        entry_points->pGLGenVertexArrays(1,
                                        &fullscreen_quad_vao_id);

        ASSERT_NE(fullscreen_quad_vao_id,
                  -1);

        entry_points->pGLBindVertexArray(fullscreen_quad_vao_id);

        entry_points->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                 fullscreen_quad_position_bo_id);
        entry_points->pGLBufferData             (GL_ARRAY_BUFFER,
                                                 sizeof(fullscreen_quad_position_data),
                                                 fullscreen_quad_position_data,
                                                 GL_STATIC_DRAW);
        entry_points->pGLVertexAttribPointer    (fullscreen_quad_position_location,
                                                 4,        /* size */
                                                 GL_FLOAT,
                                                 0,        /* normalized */
                                                 0,        /* stride */
                                                 0);       /* pointer */
        entry_points->pGLEnableVertexAttribArray(fullscreen_quad_position_location);

        ASSERT_EQ(entry_points->pGLGetError(),
                  GL_NO_ERROR);

        entry_points->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                 fullscreen_quad_uv_bo_id);
        entry_points->pGLBufferData             (GL_ARRAY_BUFFER,
                                                 sizeof(fullscreen_quad_uv_data),
                                                 fullscreen_quad_uv_data,
                                                 GL_STATIC_DRAW);
        entry_points->pGLVertexAttribPointer    (fullscreen_quad_uv_location,
                                                 2,        /* size */
                                                 GL_FLOAT,
                                                 0,        /* normalized */
                                                 0,        /* stride */
                                                 0);       /* pointer */
        entry_points->pGLEnableVertexAttribArray(fullscreen_quad_uv_location);

        entry_points->pGLActiveTexture   (GL_TEXTURE0);
        entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                          cam_to);
        entry_points->pGLProgramUniform1i(fullscreen_quad_program_id,
                                          fullscreen_quad_cam_texture_location,
                                          0);

        ASSERT_EQ(entry_points->pGLGetError(),
                  GL_NO_ERROR);
    }

    entry_points->pGLBindVertexArray(fullscreen_quad_vao_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0,  /* first */
                                     4); /* count */

    ASSERT_TRUE(entry_points->pGLGetError() == GL_NO_ERROR);
}

static void _creation_test_on_new_frame_data_callback(double      sample_time,
                                                      const BYTE* buffer_ptr,
                                                            long  buffer_length)
{
    _creation_test_on_new_frame_data_callback_arg arg;

    arg.buffer_length = buffer_length;
    arg.buffer_ptr    = buffer_ptr;

    ogl_rendering_handler_request_callback_from_context_thread(rendering_handler,
                                                               _creation_test_on_new_texture_data_available,
                                                              &arg);
}

TEST(WebcamTest, CreationTest)
{
    /* Enumerate available webcams. Carry on only if at least one is available */
    webcam_device webcam = webcam_manager_get_device_by_index(0);

    if (webcam != NULL)
    {
        /* Pick the highest resolution available */
        webcam_device_media_info* picked_media_info = NULL;
        uint32_t                  counter           = 0;

        while (true)
        {
            webcam_device_media_info* media_info = webcam_device_get_media_info(webcam, counter);

            if (media_info != NULL)
            {
                if ((picked_media_info  == NULL ||
                     picked_media_info  != NULL && picked_media_info->width * picked_media_info->height <  media_info->width * media_info->height) && 
                     media_info->format == WEBCAM_DEVICE_MEDIA_FORMAT_RGB24)
                {
                    picked_media_info = media_info;
                }
            }
            else
            {
                break;
            }

            counter++;
        }

        /* Cache stream data */
        cam_data_width  = picked_media_info->width;
        cam_data_height = picked_media_info->height;

        /* Set up the streamer */
        webcam_device_streamer webcam_streamer = webcam_device_open_streamer(webcam, picked_media_info, _creation_test_on_new_frame_data_callback);
        
        /* Create the window */
        const int     xywh[]            = {0, 0, 320, 240};
        system_window window_handle     = system_window_create_not_fullscreen(xywh, system_hashed_ansi_string_create("webcam test"), false, 0, true);
        HWND          window_sys_handle = 0;

        ASSERT_TRUE(window_handle != NULL);

        ASSERT_TRUE(system_window_get_handle(window_handle, &window_sys_handle) );
        ASSERT_EQ  (::IsWindow(window_sys_handle),          TRUE);

        /* Set up rendering handler */
        rendering_handler = ogl_rendering_handler_create_with_fps_policy(30, _creation_test_on_render_frame_callback, 0);
        ASSERT_TRUE(rendering_handler != NULL);

        /* Bind the handler to the window */
        ASSERT_TRUE(system_window_set_rendering_handler(window_handle, rendering_handler) );

        /* Create the vertex shader */
        ogl_context gl_context    = NULL;
        ogl_shader  vertex_shader = NULL;

        ASSERT_TRUE(system_window_get_context(window_handle, &gl_context) );

        vertex_shader = ogl_shader_create(gl_context, SHADER_TYPE_VERTEX);
        ASSERT_TRUE(vertex_shader != NULL);

        ogl_shader_set_body(vertex_shader,
                            system_hashed_ansi_string_create("#version 430 core\n"
                                                             "\n"
                                                             "uniform mat4 view_matrix;\n"
                                                             "uniform mat4 projection_matrix;\n"
                                                             "\n"
                                                             "in  vec4 position;\n"
                                                             "in  vec2 texcoord;\n"
                                                             "out vec2 f_texcoord;\n"
                                                             "\n"
                                                             "void main()\n"
                                                             "{\n"
                                                             "gl_Position  = projection_matrix * view_matrix * position;\n"
                                                             "f_texcoord   = texcoord;\n"
                                                             "}\n"
                                                            )
                           );
        ASSERT_TRUE(ogl_shader_compile(vertex_shader) );

        /* Create the fragment shader */
        ogl_shader fragment_shader = ogl_shader_create(gl_context, SHADER_TYPE_FRAGMENT);
        ASSERT_TRUE(fragment_shader != NULL);

        ogl_shader_set_body(fragment_shader,
                            system_hashed_ansi_string_create("in      vec2      f_texcoord;\n"
                                                             "uniform sampler2D cam_texture;\n"
                                                             "\n"
                                                             "void main()\n"
                                                             "{\n"
                                                             "   gl_FragColor.xyz = texture(cam_texture, f_texcoord).rgb;\n"
                                                             "}")
                           );
        ASSERT_TRUE(ogl_shader_compile(fragment_shader) );

        /* Create the test program */
        ogl_program program = ogl_program_create(gl_context);
        ASSERT_TRUE(program != NULL);

        ASSERT_TRUE(ogl_program_attach_shader(program, vertex_shader) );
        ASSERT_TRUE(ogl_program_attach_shader(program, fragment_shader) );
        ASSERT_TRUE(ogl_program_link(program) );

        /* Retrieve test program id */
        fullscreen_quad_program_id = ogl_program_get_id(program);
        ASSERT_NE(fullscreen_quad_program_id, -1);

        /* Retrieve uniform locations */
        const ogl_program_variable* projection_matrix_uniform_descriptor = NULL;
        const ogl_program_variable* cam_texture_uniform_descriptor       = NULL;
        const ogl_program_variable* view_matrix_uniform_descriptor       = NULL; 

        ASSERT_TRUE(ogl_program_get_uniform_by_name(program, system_hashed_ansi_string_create("cam_texture"),       &cam_texture_uniform_descriptor)       );
        ASSERT_TRUE(ogl_program_get_uniform_by_name(program, system_hashed_ansi_string_create("view_matrix"),       &view_matrix_uniform_descriptor)       );
        ASSERT_TRUE(ogl_program_get_uniform_by_name(program, system_hashed_ansi_string_create("projection_matrix"), &projection_matrix_uniform_descriptor) );

        fullscreen_quad_cam_texture_location       = cam_texture_uniform_descriptor->location;
        fullscreen_quad_projection_matrix_location = projection_matrix_uniform_descriptor->location;
        fullscreen_quad_view_matrix_location       = view_matrix_uniform_descriptor->location;

        ASSERT_NE(fullscreen_quad_cam_texture_location,       -1);
        ASSERT_NE(fullscreen_quad_projection_matrix_location, -1);
        ASSERT_NE(fullscreen_quad_view_matrix_location,       -1);

        /* Retrieve attribute location */
        const ogl_program_attribute_descriptor* position_attribute_descriptor = NULL;
        const ogl_program_attribute_descriptor* uv_attribute_descriptor       = NULL;

        ASSERT_TRUE(ogl_program_get_attribute_by_name(program, system_hashed_ansi_string_create("position"), &position_attribute_descriptor) );
        ASSERT_TRUE(ogl_program_get_attribute_by_name(program, system_hashed_ansi_string_create("texcoord"), &uv_attribute_descriptor) );

        fullscreen_quad_position_location = position_attribute_descriptor->location;
        fullscreen_quad_uv_location       = uv_attribute_descriptor->location;

        ASSERT_NE(fullscreen_quad_position_location, -1);
        ASSERT_NE(fullscreen_quad_uv_location,       -1);

        /* Create a projection matrix */
        fullscreen_quad_projection_matrix = system_matrix4x4_create_ortho_projection_matrix(0, 320, 240, 0, -1, 1);
        ASSERT_TRUE(fullscreen_quad_projection_matrix != NULL);

        /* Create a view matrix */
        fullscreen_quad_view_matrix = system_matrix4x4_create();
        system_matrix4x4_set_to_identity(fullscreen_quad_view_matrix);

        /* Start up the streamer */
        ASSERT_TRUE(webcam_device_streamer_play(webcam_streamer) );

        /* Let's render a couple of frames. */
        ASSERT_TRUE(ogl_rendering_handler_play(rendering_handler, 0) );

        system_time start_time = system_time_now();
        system_time end_time   = start_time + system_time_get_timeline_time_for_s(5);

        while (true)
        {
            system_time curr_time = system_time_now();

            if (curr_time < end_time)
            {
                ::Sleep(1000);
            }
            else
            {
                break;
            }
        }

        /* Stop the streamer */
        ASSERT_TRUE(webcam_device_streamer_stop(webcam_streamer) );

        /* Release stuff */
        system_matrix4x4_release(fullscreen_quad_projection_matrix);
        system_matrix4x4_release(fullscreen_quad_view_matrix);
        ogl_program_release     (program);
        ogl_shader_release      (fragment_shader);
        ogl_shader_release      (vertex_shader);

        /* Destroy the window */
        ASSERT_TRUE(system_window_close(window_handle) );
        ASSERT_EQ  (::IsWindow(window_sys_handle), FALSE);
    }
}

#endif /* INCLUDE_WEBCAM_MANAGER */