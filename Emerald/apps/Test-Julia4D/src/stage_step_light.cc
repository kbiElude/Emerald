/**
 *
 * Julia 4D demo test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_light.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "raGL/raGL_buffer.h"
#include "shaders/shaders_fragment_static.h"
#include "shaders/shaders_vertex_combinedmvp_generic.h"
#include "system/system_matrix4x4.h"
#include <string.h>

ogl_program      _light_program                        =  0;
ogl_program_ub   _light_program_datafs_ub              = NULL;
raGL_buffer      _light_program_datafs_ub_bo           =  0;
GLuint           _light_program_datafs_ub_bo_size      =  0;
GLuint           _light_program_datafs_ub_bp           = -1;
ogl_program_ub   _light_program_datavs_ub              = NULL;
raGL_buffer      _light_program_datavs_ub_bo           =  0;
GLuint           _light_program_datavs_ub_bo_size      =  0;
GLuint           _light_program_datavs_ub_bp           = -1;
GLuint           _light_color_ub_offset                = -1;
GLuint           _light_in_position_attribute_location = -1;
GLuint           _light_mvp_ub_offset                  = -1;
GLuint           _light_vao_id                         = -1;
GLuint           _light_vertex_attribute_location      = -1;
system_matrix4x4 _light_view_matrix                    = NULL;

/** TODO */
static void _stage_step_light_execute(ogl_context context,
                                      uint32_t    frame_index,
                                      system_time time,
                                      const int*  rendering_area_px_topdown,
                                      void*       not_used)
{
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    ogl_flyby                         flyby       = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_FLYBY,
                            &flyby);

    entrypoints->pGLUseProgram     (ogl_program_get_id(_light_program) );
    entrypoints->pGLBindVertexArray(_light_vao_id);

    /* Calculate MVP matrix */
    float            camera_location[3];
    system_matrix4x4 mvp             = NULL;

    if (_light_view_matrix == NULL)
    {
        _light_view_matrix = system_matrix4x4_create();
    }

    ogl_flyby_get_property(flyby,
                           OGL_FLYBY_PROPERTY_CAMERA_LOCATION,
                           camera_location);
    ogl_flyby_get_property(flyby,
                           OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                          &_light_view_matrix);

    mvp = system_matrix4x4_create_by_mul(main_get_projection_matrix(),
                                         _light_view_matrix);

    /* Set up uniforms */
    static float  gpu_light_position[3] = {0};
    const  float* light_color           = main_get_light_color();
    const  float* light_position        = main_get_light_position();

    ogl_program_ub_set_nonarrayed_uniform_value(_light_program_datafs_ub,
                                                _light_color_ub_offset,
                                                light_color,
                                                0, /* src_data_flags */
                                                sizeof(float) * 4);

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

    ogl_program_ub_set_nonarrayed_uniform_value(_light_program_datavs_ub,
                                                _light_mvp_ub_offset,
                                                system_matrix4x4_get_column_major_data(mvp),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);

    system_matrix4x4_release(mvp);

    ogl_program_ub_sync(_light_program_datafs_ub);
    ogl_program_ub_sync(_light_program_datavs_ub);

    GLuint   light_program_datafs_ub_bo_id           = 0;
    uint32_t light_program_datafs_ub_bo_start_offset = -1;
    GLuint   light_program_datavs_ub_bo_id           = 0;
    uint32_t light_program_datavs_ub_bo_start_offset = -1;

    raGL_buffer_get_property(_light_program_datafs_ub_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &light_program_datafs_ub_bo_id);
    raGL_buffer_get_property(_light_program_datafs_ub_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &light_program_datafs_ub_bo_start_offset);
    raGL_buffer_get_property(_light_program_datavs_ub_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &light_program_datavs_ub_bo_id);
    raGL_buffer_get_property(_light_program_datavs_ub_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &light_program_datavs_ub_bo_start_offset);

    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    _light_program_datafs_ub_bp,
                                     light_program_datafs_ub_bo_id,
                                     light_program_datafs_ub_bo_start_offset,
                                    _light_program_datafs_ub_bo_size);
    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    _light_program_datavs_ub_bp,
                                     light_program_datavs_ub_bo_id,
                                     light_program_datavs_ub_bo_start_offset,
                                    _light_program_datavs_ub_bo_size);

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
                                                                system_hashed_ansi_string_create("light program"),
                                                                OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);
    fragment_shader = shaders_fragment_static_create           (context,
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
    const ogl_program_variable*             color_uniform_data         = NULL;
    const ogl_program_attribute_descriptor* in_position_attribute_data = NULL;
    const ogl_program_variable*             mvp_uniform_data           = NULL;

    ogl_program_get_attribute_by_name(_light_program,
                                      system_hashed_ansi_string_create("in_position"),
                                     &in_position_attribute_data);
    ogl_program_get_uniform_by_name  (_light_program,
                                      system_hashed_ansi_string_create("color"),
                                     &color_uniform_data);
    ogl_program_get_uniform_by_name  (_light_program,
                                      system_hashed_ansi_string_create("mvp"),
                                     &mvp_uniform_data);

    _light_color_ub_offset                = (color_uniform_data         != NULL) ? color_uniform_data->block_offset     : -1;
    _light_in_position_attribute_location = (in_position_attribute_data != NULL) ? in_position_attribute_data->location : -1;
    _light_mvp_ub_offset                  = (mvp_uniform_data           != NULL) ? mvp_uniform_data->block_offset       : -1;

    /* Retrieve uniform block properties */
    ogl_program_get_uniform_block_by_name(_light_program,
                                          system_hashed_ansi_string_create("dataFS"),
                                         &_light_program_datafs_ub);
    ogl_program_get_uniform_block_by_name(_light_program,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &_light_program_datavs_ub);

    ogl_program_ub_get_property(_light_program_datafs_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_light_program_datafs_ub_bo_size);
    ogl_program_ub_get_property(_light_program_datafs_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &_light_program_datafs_ub_bo);
    ogl_program_ub_get_property(_light_program_datafs_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &_light_program_datafs_ub_bp);

    ogl_program_ub_get_property(_light_program_datavs_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_light_program_datavs_ub_bo_size);
    ogl_program_ub_get_property(_light_program_datavs_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &_light_program_datavs_ub_bo);
    ogl_program_ub_get_property(_light_program_datavs_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &_light_program_datavs_ub_bp);

    /* Generate VAO */
    const ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLGenVertexArrays(1, /* n */
                                   &_light_vao_id);

    /* Add ourselves to the pipeline */
    ogl_pipeline_stage_step_declaration light_preview_stage_step;

    light_preview_stage_step.name              = system_hashed_ansi_string_create("Light preview");
    light_preview_stage_step.pfn_callback_proc = _stage_step_light_execute;
    light_preview_stage_step.user_arg          = NULL;

    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                               &light_preview_stage_step);

    /* Set up matrices */
    _light_view_matrix = system_matrix4x4_create();
}
