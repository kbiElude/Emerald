/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_shader.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "raGL/raGL_program.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_matrix4x4.h"
#include "main.h"


bool             triangle_test_is_first_frame             = true;
GLuint           triangle_test_position_bo_id             = -1;
GLuint           triangle_test_position_vao_id            = -1;
GLint            triangle_test_position_location          = -1;
GLint            triangle_test_program_id                 = -1;
system_matrix4x4 triangle_test_projection_matrix          = NULL;
GLint            triangle_test_projection_matrix_location = -1;
system_matrix4x4 triangle_test_view_matrix                = NULL;
GLint            triangle_test_view_matrix_location       = -1;

GLfloat triangle_test_position_data[] = { 0.0f,  0.5f, -1.0f, 1.0f,
                                          1.0f, -1.0f, -1.0f, 1.0f,
                                         -1.0f, -1.0f, -1.0f, 1.0f};


static void _on_render_frame_triangle_test_callback(ogl_context context,
                                                    uint32_t    n_frames_rendered,
                                                    system_time frame_time,
                                                    const int*  rendering_area_px_topdown,
                                                    void*)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Got to do a couple of things if this is the first frame */
    if (triangle_test_is_first_frame)
    {
        /* Set up the uniforms */
        entry_points->pGLProgramUniformMatrix4fv(triangle_test_program_id,
                                                 triangle_test_projection_matrix_location,
                                                 1,     /* count */
                                                 false, /* transpose */
                                                 system_matrix4x4_get_row_major_data(triangle_test_projection_matrix) );
        entry_points->pGLProgramUniformMatrix4fv(triangle_test_program_id,
                                                 triangle_test_view_matrix_location,
                                                 1,    /* count */
                                                 true, /* transpose */
                                                 system_matrix4x4_get_row_major_data(triangle_test_view_matrix) );

        /* Set up buffer object for position data */
        entry_points->pGLGenBuffers(1,
                                   &triangle_test_position_bo_id);
        ASSERT_NE                  (triangle_test_position_bo_id,
                                    -1);

        entry_points->pGLBindBuffer(GL_ARRAY_BUFFER,
                                    triangle_test_position_bo_id);
        entry_points->pGLBufferData(GL_ARRAY_BUFFER,
                                    sizeof(triangle_test_position_data),
                                    triangle_test_position_data,
                                    GL_STATIC_DRAW);

        /* Set up VAOs */
        entry_points->pGLGenVertexArrays(1,
                                        &triangle_test_position_vao_id);
        ASSERT_NE                       (triangle_test_position_vao_id,
                                         -1);

        entry_points->pGLBindVertexArray(triangle_test_position_vao_id);

        entry_points->pGLEnableVertexAttribArray(triangle_test_position_location);
        entry_points->pGLVertexAttribPointer    (triangle_test_position_location,
                                                 4,        /* size */
                                                 GL_FLOAT,
                                                 0,        /* normalized */
                                                 0,        /* stride */
                                                 0);       /* pointer */

        /* Good. */
        triangle_test_is_first_frame = false;
    }

    entry_points->pGLClearColor(1.0f,  /* red   */
                                0,     /* green */
                                1.0f,  /* blue  */
                                1);    /* alpha */

    entry_points->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entry_points->pGLBindVertexArray(triangle_test_position_vao_id);
    entry_points->pGLUseProgram     (triangle_test_program_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLES,
                                     0,  /* first */
                                     3); /* count */
}

static void _on_render_frame_creation_test_callback(ogl_context context,
                                                    uint32_t    n_frames_rendered,
                                                    system_time frame_time,
                                                    const int*  rendering_area_px_topdown,
                                                    void*)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLClearColor(0.75f, /* red */
                                0,     /* green */
                                0,     /* blue */
                                1.0f); /* alpha */
    entry_points->pGLClear     (GL_COLOR_BUFFER_BIT);
}

TEST(ShaderTest, CreationTest)
{
    /* Create the window */
    ral_shader                      test_shader              = NULL;
    demo_window                     window                   = NULL;
    ral_context                     window_context           = NULL;
    const system_hashed_ansi_string window_name              = system_hashed_ansi_string_create("Test window");
    const uint32_t                  window_resolution[]      = {320, 240};
    const uint32_t                  window_target_frame_rate = ~0;
    const bool                      window_visible           = false;

    ASSERT_NE( (window = demo_app_create_window(window_name,
                                                RAL_BACKEND_TYPE_GL)),
               (demo_window) NULL);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_resolution);
    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE,
                            &window_target_frame_rate);
    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_VISIBLE,
                            &window_visible);

    /* Show the window */
    ASSERT_TRUE(demo_window_show(window) );

    /* Create the test shader */
    const system_hashed_ansi_string shader_body_has    = system_hashed_ansi_string_create("void main()\n"
                                                                                          "{\n"
                                                                                          "  gl_FragColor.xyz = vec3(0, 0.25, 0.5);\n"
                                                                                          "}");
    const ral_shader_create_info    shader_create_info =
    {
        system_hashed_ansi_string_create("test shader"),
        RAL_SHADER_TYPE_FRAGMENT,
    };

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &window_context);

    ASSERT_NE(window_context,
              (ral_context) NULL);

    ral_context_create_shaders(window_context,
                               1, /* n_create_info_items */
                              &shader_create_info,
                              &test_shader);

    ASSERT_TRUE(test_shader != NULL);

    ral_shader_set_property(test_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body_has);

    ral_context_delete_objects(window_context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               (const void**) &test_shader);

    /* Destroy the window */
    ASSERT_TRUE(demo_app_destroy_window(window_name) );
}

TEST(ShaderTest, FullViewportTriangleTest)
{
    /* Create the window */
    demo_timeline_segment           video_segment            = NULL;
    demo_timeline_segment_id        video_segment_id         = -1;
    demo_window                     window                   = NULL;
    ral_context                     window_context           = NULL;
    ral_texture_format              window_context_texture_format;
    const system_hashed_ansi_string window_name              = system_hashed_ansi_string_create("Test window");
    ogl_rendering_handler           window_rendering_handler = NULL;
    const uint32_t                  window_resolution[]      = {320, 240};
    const uint32_t                  window_target_frame_rate = ~0;

    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_resolution);
    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE,
                            &window_target_frame_rate);

    ASSERT_TRUE(demo_window_show(window) );

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &window_context);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &window_rendering_handler);

    ASSERT_NE(window_context,
              (ral_context) NULL);

    ral_context_get_property(window_context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_ATTACHMENT_TEXTURE_FORMAT,
                            &window_context_texture_format);

    /* Create the test vertex shader */
    ral_shader                   test_vertex_shader             = NULL;
    const ral_shader_create_info test_vertex_shader_create_info =
    {
        system_hashed_ansi_string_create("test vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };
    const system_hashed_ansi_string test_vertex_shader_has = system_hashed_ansi_string_create("#version 430 core\n"
                                                                                              "\n"
                                                                                              "uniform mat4 view_matrix;\n"
                                                                                              "uniform mat4 projection_matrix;\n"
                                                                                              "\n"
                                                                                              "in vec4 position;\n"
                                                                                              "\n"
                                                                                              "void main()\n"
                                                                                              "{\n"
                                                                                              "gl_Position = projection_matrix * view_matrix * position;"
                                                                                              "}\n");

    ral_context_create_shaders(window_context,
                               1, /* n_create_info_items */
                              &test_vertex_shader_create_info,
                              &test_vertex_shader);

    ASSERT_TRUE(test_vertex_shader != NULL);

    ral_shader_set_property(test_vertex_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &test_vertex_shader_has);

    /* Create the test fragment shader */
    ral_shader                   test_fragment_shader             = NULL;
    const ral_shader_create_info test_fragment_shader_create_info =
    {
        system_hashed_ansi_string_create("test fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const system_hashed_ansi_string test_fragment_shader_has = system_hashed_ansi_string_create("void main()\n"
                                                                                                "{\n"
                                                                                                "  gl_FragColor.xyz = vec3((gl_FragCoord.x - 0.5)/320.0, (gl_FragCoord.y - 0.5) / 240.0, 0.5);\n"
                                                                                                "}");

    ral_context_create_shaders(window_context,
                               1, /* n_create_info_items */
                              &test_fragment_shader_create_info,
                              &test_fragment_shader);

    ASSERT_TRUE(test_fragment_shader != NULL);

    ral_shader_set_property(test_fragment_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &test_fragment_shader_has);

    /* Create the test program */
    ral_program                   test_program = NULL;
    const ral_program_create_info test_program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("test program")
    };

    ral_context_create_programs(window_context,
                                1, /* n_create_info_items */
                               &test_program_create_info,
                               &test_program);

    ASSERT_TRUE(test_program != NULL);

    ASSERT_TRUE(ral_program_attach_shader(test_program,
                                          test_vertex_shader) );
    ASSERT_TRUE(ral_program_attach_shader(test_program,
                                          test_fragment_shader) );

    /* Retrieve test program id */
    const raGL_program test_program_raGL = ral_context_get_program_gl(window_context,
                                                                      test_program);

    raGL_program_get_property(test_program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                              &triangle_test_program_id);

    ASSERT_NE(triangle_test_program_id,
              -1);

    /* Retrieve uniform locations */
    const ral_program_variable* projection_matrix_uniform_ptr = NULL;
    const ral_program_variable* view_matrix_uniform_ptr       = NULL; 

    ASSERT_TRUE(raGL_program_get_uniform_by_name(test_program_raGL,
                                                 system_hashed_ansi_string_create("view_matrix"),
                                                &view_matrix_uniform_ptr) );
    ASSERT_TRUE(raGL_program_get_uniform_by_name(test_program_raGL,
                                                 system_hashed_ansi_string_create("projection_matrix"),
                                                &projection_matrix_uniform_ptr) );

    triangle_test_projection_matrix_location = projection_matrix_uniform_ptr->location;
    triangle_test_view_matrix_location       = view_matrix_uniform_ptr->location;

    ASSERT_NE(triangle_test_projection_matrix_location,
              -1);
    ASSERT_NE(triangle_test_view_matrix_location,
              -1);

    /* Retrieve attribute location */
    const ral_program_attribute* position_attribute_ptr = NULL;

    ASSERT_TRUE(raGL_program_get_vertex_attribute_by_name(test_program_raGL,
                                                          system_hashed_ansi_string_create("position"),
                                                         &position_attribute_ptr) );

    triangle_test_position_location = position_attribute_ptr->location;

    ASSERT_NE(triangle_test_position_location,
              -1);

    /* Create a projection matrix */
    triangle_test_projection_matrix = system_matrix4x4_create_perspective_projection_matrix(53.0f,           /* fov_y */
                                                                                            320.0f / 240.0f, /* ar */
                                                                                            0.05f,           /* z_near */
                                                                                            25);             /* z_far */

    ASSERT_TRUE(triangle_test_projection_matrix != NULL);

    /* Create a view matrix */
    triangle_test_view_matrix = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(triangle_test_view_matrix);

    /* Use a little hack to re-route the rendering calls to our custom rendering handler. */
    static const PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc = _on_render_frame_triangle_test_callback;

    ogl_rendering_handler_set_property(window_rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    /* Let's render a couple of frames. */
    ASSERT_TRUE(demo_window_start_rendering(window,
                                            0) ); /* rendering_start_time */

#ifdef _WIN32
    ::Sleep(1000);
#else
    sleep(1);
#endif

    /* Stop playing so that we can release */
    ASSERT_TRUE(demo_window_stop_rendering(window) );

    /* Release */
    const ral_shader test_shaders[] =
    {
        test_fragment_shader,
        test_vertex_shader
    };

    ral_context_delete_objects(window_context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &test_program);
    ral_context_delete_objects(window_context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               sizeof(test_shaders) / sizeof(test_shaders[0]),
                               (const void**) &test_shaders);

    system_matrix4x4_release(triangle_test_projection_matrix);
    system_matrix4x4_release(triangle_test_view_matrix);

    /* Destroy the window */
    ASSERT_TRUE(demo_app_destroy_window(window_name) );
}

