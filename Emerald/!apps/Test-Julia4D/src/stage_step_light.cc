/**
 *
 * Julia 4D demo test app
 *
 */

#include "shared.h"
#include "include\main.h"
#include "stage_step_light.h"
#include "ogl\ogl_context.h"
#include "ogl\ogl_flyby.h"
#include "ogl\ogl_pipeline.h"
#include "ogl\ogl_program.h"
#include "ogl\ogl_shader.h"
#include "shaders\shaders_fragment_static.h"
#include "shaders\shaders_vertex_combinedmvp_generic.h"
#include "system\system_matrix4x4.h"

ogl_program      _light_program                        = 0;
GLuint           _light_color_uniform_location         = -1;
GLuint           _light_in_position_attribute_location = -1;
GLuint           _light_mvp_uniform_location           = -1;
GLuint           _light_vao_id                         = -1;
GLuint           _light_vertex_attribute_location      = -1;
system_matrix4x4 _light_view_matrix                    = NULL;

/** TODO */
static void _stage_step_light_execute(ogl_context          context,
                                      system_timeline_time time,
                                      void*                not_used)
{
    const ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLUseProgram     (ogl_program_get_id(_light_program) );
    entrypoints->pGLBindVertexArray(_light_vao_id);

    /* Calculate MVP matrix */
    float            camera_location[3];
    system_matrix4x4 mvp             = NULL;

    if (_light_view_matrix == NULL)
    {
        _light_view_matrix = system_matrix4x4_create();
    }

    ogl_flyby_get_property(context,
                           OGL_FLYBY_PROPERTY_CAMERA_LOCATION,
                           camera_location);
    ogl_flyby_get_property(context,
                           OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                          &_light_view_matrix);

    mvp = system_matrix4x4_create_by_mul(main_get_projection_matrix(),
                                         _light_view_matrix);

    /* Set up uniforms */
    static float  gpu_light_position[3] = {0};
    static float  gpu_light_color   [4] = {0, 1};
    const  float* light_color           = main_get_light_color();
    const  float* light_position        = main_get_light_position();

    if (memcmp(gpu_light_color,
               light_color,
               sizeof(float) * 3) != 0)
    {
        entrypoints->pGLProgramUniform4fv(ogl_program_get_id(_light_program),
                                          _light_color_uniform_location,
                                          1, /* count */
                                          light_color);

        memcpy(gpu_light_color,
               light_color,
               sizeof(float) * 3);
    }

    if (memcmp(gpu_light_position,
               light_position,
               sizeof(float) * 3) != 0)
    {
        entrypoints->pGLVertexAttrib3fv(_light_in_position_attribute_location,
                                        light_position);

        memcpy(gpu_light_position,
               light_position,
               sizeof(float) * 3);
    }

    entrypoints->pGLProgramUniformMatrix4fv(ogl_program_get_id(_light_program),
                                            _light_mvp_uniform_location,
                                            1,       /* count */
                                            GL_TRUE, /* transpose */
                                            system_matrix4x4_get_row_major_data(mvp) );

    system_matrix4x4_release(mvp);

    /* Draw light representation*/
    entrypoints->pGLPointSize (16.0f);
    entrypoints->pGLEnable    (GL_DEPTH_TEST);
    entrypoints->pGLDepthFunc (GL_LESS);
    {
        entrypoints->pGLDrawArrays(GL_POINTS,
                                   0,  /* first */
                                   1); /* count */
    }
    entrypoints->pGLDisable(GL_DEPTH_TEST);
}

/* Please see header for specification */
PUBLIC void stage_step_light_deinit(ogl_context context)
{
    ogl_program_release     (_light_program);
    system_matrix4x4_release(_light_view_matrix);
}

/* Please see header for specification */
PUBLIC void stage_step_light_init(ogl_context  context,
                                  ogl_pipeline pipeline,
                                  uint32_t     stage_id)
{
    /* Link  program */
    shaders_fragment_static            fragment_shader = NULL;
    shaders_vertex_combinedmvp_generic vertex_shader   = NULL;

    _light_program  = ogl_program_create                       (context,
                                                                system_hashed_ansi_string_create("light program") );
    fragment_shader = shaders_fragment_static_create           (context,
                                                                FRAGMENT_STATIC_UNIFORM,
                                                                system_hashed_ansi_string_create("light fragment") );
    vertex_shader   = shaders_vertex_combinedmvp_generic_create(context,
                                                                system_hashed_ansi_string_create("light vertex") );

    ogl_program_attach_shader(_light_program,
                              shaders_fragment_static_get_shader(fragment_shader) );
    ogl_program_attach_shader(_light_program,
                              shaders_vertex_combinedmvp_generic_get_shader(vertex_shader) );

    ogl_program_link(_light_program);

    shaders_fragment_static_release           (fragment_shader);
    shaders_vertex_combinedmvp_generic_release(vertex_shader);

    /* Retrieve attribute/uniform locations */
    const ogl_program_uniform_descriptor*   color_uniform_data         = NULL;
    const ogl_program_attribute_descriptor* in_position_attribute_data = NULL;
    const ogl_program_uniform_descriptor*   mvp_uniform_data           = NULL;

    ogl_program_get_attribute_by_name(_light_program,
                                      system_hashed_ansi_string_create("in_position"),
                                     &in_position_attribute_data);
    ogl_program_get_uniform_by_name  (_light_program,
                                      system_hashed_ansi_string_create("color"),
                                     &color_uniform_data);
    ogl_program_get_uniform_by_name  (_light_program,
                                      system_hashed_ansi_string_create("mvp"),
                                     &mvp_uniform_data);

    _light_color_uniform_location         = (color_uniform_data         != NULL) ? color_uniform_data->location         : -1;
    _light_in_position_attribute_location = (in_position_attribute_data != NULL) ? in_position_attribute_data->location : -1;
    _light_mvp_uniform_location           = (mvp_uniform_data           != NULL) ? mvp_uniform_data->location           : -1;

    /* Generate VAO */
    const ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLGenVertexArrays(1, /* n */
                                   &_light_vao_id);

    /* Add ourselves to the pipeline */
    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                                system_hashed_ansi_string_create("Light preview"),
                                _stage_step_light_execute,
                                NULL); /* step_callback_user_arg */

    /* Set up matrices */
    _light_view_matrix = system_matrix4x4_create();
}
