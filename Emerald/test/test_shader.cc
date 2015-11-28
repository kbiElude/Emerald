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
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_matrix4x4.h"

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
        entry_points->pGLUseProgram(triangle_test_program_id);

        /* Set up the uniforms */
        entry_points->pGLProgramUniformMatrix4fv(triangle_test_program_id,
                                                 triangle_test_projection_matrix_location,
                                                 1,     /* count */
                                                 false, /* transpose */
                                                 system_matrix4x4_get_row_major_data(triangle_test_projection_matrix) );
        entry_points->pGLProgramUniformMatrix4fv(triangle_test_program_id, triangle_test_view_matrix_location,
                                                 1,    /* count */
                                                 true, /* transpose */
                                                 system_matrix4x4_get_row_major_data(triangle_test_view_matrix) );
        ASSERT_EQ                               (entry_points->pGLGetError(),
                                                 GL_NO_ERROR);

        /* Set up buffer object for position data */
        entry_points->pGLGenBuffers(1,
                                   &triangle_test_position_bo_id);
        ASSERT_NE                  (triangle_test_position_bo_id,
                                    -1);
        ASSERT_EQ                  (entry_points->pGLGetError(),
                                    GL_NO_ERROR);

        entry_points->pGLBindBuffer(GL_ARRAY_BUFFER,
                                    triangle_test_position_bo_id);
        ASSERT_EQ                  (entry_points->pGLGetError(),
                                    GL_NO_ERROR);
        entry_points->pGLBufferData(GL_ARRAY_BUFFER,
                                    sizeof(triangle_test_position_data),
                                    triangle_test_position_data,
                                    GL_STATIC_DRAW);
        ASSERT_EQ                  (entry_points->pGLGetError(),
                                    GL_NO_ERROR);

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

        /* Set up clear color */
        entry_points->pGLClearColor(1.0f,  /* red   */
                                    0,     /* green */
                                    1.0f,  /* blue  */
                                    1);    /* alpha */

        /* Good. */
        triangle_test_is_first_frame = false;
    }

    entry_points->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entry_points->pGLBindVertexArray(triangle_test_position_vao_id);
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
    ogl_shader                      test_shader              = NULL;
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
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &window_context);

    ASSERT_NE(window_context,
              (ral_context) NULL);

    test_shader = ogl_shader_create(window_context,
                                    RAL_SHADER_TYPE_FRAGMENT,
                                    system_hashed_ansi_string_create("test shader") );

    ASSERT_TRUE(test_shader != NULL);

    ogl_shader_set_body(test_shader,
                        system_hashed_ansi_string_create("void main()\n"
                                                         "{\n"
                                                         "  gl_FragColor.xyz = vec3(0, 0.25, 0.5);\n"
                                                         "}")
                       );
    ASSERT_TRUE(ogl_shader_compile(test_shader) );

    ogl_shader_release(test_shader);

    /* Destroy the window */
    ASSERT_TRUE(demo_app_destroy_window(window_name) );
}

TEST(ShaderTest, FullViewportTriangleTest)
{
    /* Create the window */
    demo_window                     window                   = NULL;
    ral_context                     window_context           = NULL;
    const system_hashed_ansi_string window_name              = system_hashed_ansi_string_create("Test window");
    const uint32_t                  window_resolution[]      = {320, 240};
    const uint32_t                  window_target_frame_rate = ~0;

    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_GL);

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

    ASSERT_NE(window_context,
              (ral_context) NULL);

    /* Create the test vertex shader */
    ogl_shader test_vertex_shader = NULL;

    test_vertex_shader = ogl_shader_create(window_context,
                                           RAL_SHADER_TYPE_VERTEX,
                                           system_hashed_ansi_string_create("test vertex shader") );

    ASSERT_TRUE(test_vertex_shader != NULL);

    ogl_shader_set_body(test_vertex_shader,
                        system_hashed_ansi_string_create("#version 430 core\n"
                                                         "\n"
                                                         "uniform mat4 view_matrix;\n"
                                                         "uniform mat4 projection_matrix;\n"
                                                         "\n"
                                                         "in vec4 position;\n"
                                                         "\n"
                                                         "void main()\n"
                                                         "{\n"
                                                         "gl_Position = projection_matrix * view_matrix * position;"
                                                         "}\n"
                                                        )
                       );

    ASSERT_TRUE(ogl_shader_compile(test_vertex_shader) );

    /* Create the test fragment shader */
    ogl_shader test_fragment_shader = ogl_shader_create(window_context,
                                                        RAL_SHADER_TYPE_FRAGMENT,
                                                        system_hashed_ansi_string_create("test fragment shader") );

    ASSERT_TRUE(test_fragment_shader != NULL);

    ogl_shader_set_body(test_fragment_shader,
                        system_hashed_ansi_string_create("void main()\n"
                                                         "{\n"
                                                         "  gl_FragColor.xyz = vec3((gl_FragCoord.x - 0.5)/320.0, (gl_FragCoord.y - 0.5) / 240.0, 0.5);\n"
                                                         "}")
                       );

    ASSERT_TRUE(ogl_shader_compile(test_fragment_shader) );

    /* Create the test program */
    ogl_program test_program = ogl_program_create(window_context,
                                                  system_hashed_ansi_string_create("test program") );
    ASSERT_TRUE(test_program != NULL);

    ASSERT_TRUE(ogl_program_attach_shader(test_program,
                                          test_vertex_shader) );
    ASSERT_TRUE(ogl_program_attach_shader(test_program,
                                          test_fragment_shader) );
    ASSERT_TRUE(ogl_program_link         (test_program) );

    /* Retrieve test program id */
    triangle_test_program_id = ogl_program_get_id(test_program);

    ASSERT_NE(triangle_test_program_id, -1);

    /* Retrieve uniform locations */
    const ogl_program_variable* projection_matrix_uniform_descriptor = NULL;
    const ogl_program_variable* view_matrix_uniform_descriptor       = NULL; 

    ASSERT_TRUE(ogl_program_get_uniform_by_name(test_program,
                                                system_hashed_ansi_string_create("view_matrix"),
                                               &view_matrix_uniform_descriptor) );
    ASSERT_TRUE(ogl_program_get_uniform_by_name(test_program,
                                                system_hashed_ansi_string_create("projection_matrix"),
                                               &projection_matrix_uniform_descriptor) );

    triangle_test_projection_matrix_location = projection_matrix_uniform_descriptor->location;
    triangle_test_view_matrix_location       = view_matrix_uniform_descriptor->location;

    ASSERT_NE(triangle_test_projection_matrix_location,
              -1);
    ASSERT_NE(triangle_test_view_matrix_location,
              -1);

    /* Retrieve attribute location */
    const ogl_program_attribute_descriptor* position_attribute_descriptor = NULL;

    ASSERT_TRUE(ogl_program_get_attribute_by_name(test_program,
                                                  system_hashed_ansi_string_create("position"),
                                                 &position_attribute_descriptor) );

    triangle_test_position_location = position_attribute_descriptor->location;

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
    ogl_program_release     (test_program);
    ogl_shader_release      (test_fragment_shader);
    ogl_shader_release      (test_vertex_shader);
    system_matrix4x4_release(triangle_test_projection_matrix);
    system_matrix4x4_release(triangle_test_view_matrix);

    /* Destroy the window */
    ASSERT_TRUE(demo_app_destroy_window(window_name) );
}

