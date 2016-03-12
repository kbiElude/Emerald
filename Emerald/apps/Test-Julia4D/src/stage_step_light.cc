/**
 *
 * Julia 4D demo test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_light.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_static.h"
#include "shaders/shaders_vertex_combinedmvp_generic.h"
#include "system/system_matrix4x4.h"
#include <string.h>

ral_program              _light_program                        =  0;
ral_program_block_buffer _light_program_datafs_ub              = NULL;
GLuint                   _light_program_datafs_ub_bo_size      =  0;
GLuint                   _light_program_datafs_ub_bp           = -1;
ral_program_block_buffer _light_program_datavs_ub              = NULL;
GLuint                   _light_program_datavs_ub_bo_size      =  0;
GLuint                   _light_program_datavs_ub_bp           = -1;
GLuint                   _light_color_ub_offset                = -1;
GLuint                   _light_in_position_attribute_location = -1;
GLuint                   _light_mvp_ub_offset                  = -1;
GLuint                   _light_vao_id                         = -1;
GLuint                   _light_vertex_attribute_location      = -1;
system_matrix4x4         _light_view_matrix                    = NULL;

/** TODO */
static void _stage_step_light_execute(ral_context context,
                                      uint32_t    frame_index,
                                      system_time time,
                                      const int*  rendering_area_px_topdown,
                                      void*       not_used)
{
    ogl_context                       context_gl       = ral_context_get_gl_context(context);
    const ogl_context_gl_entrypoints* entrypoints      = NULL;
    const raGL_program                light_po_raGL    = ral_context_get_program_gl(context,
                                                                                    _light_program);
    GLuint                            light_po_raGL_id = 0;

    ogl_context_get_property (context_gl,
                              OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                             &entrypoints);
    raGL_program_get_property(light_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &light_po_raGL_id);

    entrypoints->pGLUseProgram     (light_po_raGL_id);
    entrypoints->pGLBindVertexArray(_light_vao_id);

    /* Calculate MVP matrix */
    float            camera_location[3];
    system_matrix4x4 mvp             = NULL;

    if (_light_view_matrix == NULL)
    {
        _light_view_matrix = system_matrix4x4_create();
    }

    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_CAMERA_LOCATION,
                            camera_location);
    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
                           &_light_view_matrix);

    mvp = system_matrix4x4_create_by_mul(main_get_projection_matrix(),
                                         _light_view_matrix);

    /* Set up uniforms */
    static float  gpu_light_position[3] = {0};
    const  float* light_color           = main_get_light_color();
    const  float* light_position        = main_get_light_position();

    ral_program_block_buffer_set_nonarrayed_variable_value(_light_program_datafs_ub,
                                                           _light_color_ub_offset,
                                                           light_color,
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

    ral_program_block_buffer_set_nonarrayed_variable_value(_light_program_datavs_ub,
                                                           _light_mvp_ub_offset,
                                                           system_matrix4x4_get_column_major_data(mvp),
                                                           sizeof(float) * 16);

    system_matrix4x4_release(mvp);

    ral_program_block_buffer_sync(_light_program_datafs_ub);
    ral_program_block_buffer_sync(_light_program_datavs_ub);

    raGL_buffer light_program_datafs_ub_bo_raGL              = NULL;
    GLuint      light_program_datafs_ub_bo_raGL_id           = 0;
    uint32_t    light_program_datafs_ub_bo_raGL_start_offset = -1;
    ral_buffer  light_program_datafs_ub_bo_ral               = NULL;
    uint32_t    light_program_datafs_ub_bo_ral_start_offset  = -1;
    raGL_buffer light_program_datavs_ub_bo_raGL              = NULL;
    GLuint      light_program_datavs_ub_bo_raGL_id           = 0;
    uint32_t    light_program_datavs_ub_bo_raGL_start_offset = -1;
    ral_buffer  light_program_datavs_ub_bo_ral               = NULL;
    uint32_t    light_program_datavs_ub_bo_ral_start_offset  = -1;

    ral_program_block_buffer_get_property(_light_program_datafs_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &light_program_datafs_ub_bo_ral);
    ral_program_block_buffer_get_property(_light_program_datavs_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &light_program_datavs_ub_bo_ral);

    light_program_datafs_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                                light_program_datafs_ub_bo_ral);
    light_program_datavs_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                                light_program_datavs_ub_bo_ral);

    ral_buffer_get_property(light_program_datafs_ub_bo_ral,
                            RAL_BUFFER_PROPERTY_START_OFFSET,
                           &light_program_datafs_ub_bo_ral_start_offset);
    ral_buffer_get_property(light_program_datavs_ub_bo_ral,
                            RAL_BUFFER_PROPERTY_START_OFFSET,
                           &light_program_datavs_ub_bo_ral_start_offset);

    raGL_buffer_get_property(light_program_datafs_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &light_program_datafs_ub_bo_raGL_id);
    raGL_buffer_get_property(light_program_datafs_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &light_program_datafs_ub_bo_raGL_start_offset);
    raGL_buffer_get_property(light_program_datavs_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &light_program_datavs_ub_bo_raGL_id);
    raGL_buffer_get_property(light_program_datavs_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &light_program_datavs_ub_bo_raGL_start_offset);

    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    _light_program_datafs_ub_bp,
                                     light_program_datafs_ub_bo_raGL_id,
                                     light_program_datafs_ub_bo_raGL_start_offset + light_program_datafs_ub_bo_ral_start_offset,
                                    _light_program_datafs_ub_bo_size);
    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    _light_program_datavs_ub_bp,
                                     light_program_datavs_ub_bo_raGL_id,
                                     light_program_datavs_ub_bo_raGL_start_offset + light_program_datavs_ub_bo_ral_start_offset,
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
PUBLIC void stage_step_light_deinit(ral_context context)
{
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &_light_program);

    if (_light_program_datafs_ub != NULL)
    {
        ral_program_block_buffer_release(_light_program_datafs_ub);

        _light_program_datafs_ub = NULL;
    }

    if (_light_program_datavs_ub != NULL)
    {
        ral_program_block_buffer_release(_light_program_datavs_ub);

        _light_program_datavs_ub = NULL;
    }

    system_matrix4x4_release(_light_view_matrix);
}

/* Please see header for specification */
PUBLIC void stage_step_light_init(ral_context  context,
                                  ogl_pipeline pipeline,
                                  uint32_t     stage_id)
{
    /* Link  program */
    shaders_fragment_static            fragment_shader = NULL;
    shaders_vertex_combinedmvp_generic vertex_shader   = NULL;

    const ral_program_create_info light_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("light program")
    };

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &light_po_create_info,
                               &_light_program);

    fragment_shader = shaders_fragment_static_create           (context,
                                                                system_hashed_ansi_string_create("light fragment") );
    vertex_shader   = shaders_vertex_combinedmvp_generic_create(context,
                                                                system_hashed_ansi_string_create("light vertex") );

    ral_program_attach_shader(_light_program,
                              shaders_fragment_static_get_shader(fragment_shader),
                              true /* async */);
    ral_program_attach_shader(_light_program,
                              shaders_vertex_combinedmvp_generic_get_shader(vertex_shader),
                              true /* async */);

    shaders_fragment_static_release           (fragment_shader);
    shaders_vertex_combinedmvp_generic_release(vertex_shader);

    /* Generate VAO */
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(ral_context_get_gl_context(context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLGenVertexArrays(1, /* n */
                                       &_light_vao_id);

    /* Retrieve attribute/uniform locations */
    const ral_program_variable*    color_uniform_ral_ptr          = NULL;
    const _raGL_program_attribute* in_position_attribute_raGL_ptr = NULL;
    const ral_program_variable*    mvp_uniform_ral_ptr            = NULL;

    const raGL_program light_po_raGL = ral_context_get_program_gl(context,
                                                                  _light_program);
    raGL_program_get_vertex_attribute_by_name(light_po_raGL,
                                              system_hashed_ansi_string_create("in_position"),
                                             &in_position_attribute_raGL_ptr);
    ral_program_get_block_variable_by_name   (_light_program,
                                              system_hashed_ansi_string_create("dataFS"),
                                              system_hashed_ansi_string_create("color"),
                                             &color_uniform_ral_ptr);
    ral_program_get_block_variable_by_name   (_light_program,
                                              system_hashed_ansi_string_create("dataVS"),
                                              system_hashed_ansi_string_create("mvp"),
                                             &mvp_uniform_ral_ptr);

    _light_color_ub_offset                = (color_uniform_ral_ptr          != NULL) ? color_uniform_ral_ptr->block_offset      : -1;
    _light_in_position_attribute_location = (in_position_attribute_raGL_ptr != NULL) ? in_position_attribute_raGL_ptr->location : -1;
    _light_mvp_ub_offset                  = (mvp_uniform_ral_ptr            != NULL) ? mvp_uniform_ral_ptr->block_offset        : -1;

    /* Retrieve uniform block properties */
    ral_buffer datafs_ub_bo_ral = NULL;
    ral_buffer datavs_ub_bo_ral = NULL;

    _light_program_datafs_ub = ral_program_block_buffer_create(context,
                                                               _light_program,
                                                               system_hashed_ansi_string_create("dataFS") );
    _light_program_datavs_ub = ral_program_block_buffer_create(context,
                                                               _light_program,
                                                               system_hashed_ansi_string_create("dataVS") );

    ral_program_block_buffer_get_property(_light_program_datafs_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &datafs_ub_bo_ral);
    ral_program_block_buffer_get_property(_light_program_datavs_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &datavs_ub_bo_ral);

    ral_buffer_get_property                (datafs_ub_bo_ral,
                                            RAL_BUFFER_PROPERTY_SIZE,
                                           &_light_program_datafs_ub_bo_size);
    raGL_program_get_block_property_by_name(light_po_raGL,
                                            system_hashed_ansi_string_create("dataFS"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &_light_program_datafs_ub_bp);

    ral_buffer_get_property                (datavs_ub_bo_ral,
                                            RAL_BUFFER_PROPERTY_SIZE,
                                           &_light_program_datavs_ub_bo_size);
    raGL_program_get_block_property_by_name(light_po_raGL,
                                            system_hashed_ansi_string_create("dataVS"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &_light_program_datavs_ub_bp);

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
