/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_lerp.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_types.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include <sstream>


/* Type definitions */
typedef struct
{
    ral_context    context;
    ral_program    program;
    ogl_program_ub program_ub;
    ral_buffer     program_ub_bo;
    GLuint         program_ub_bo_size;

    GLint pos1_ub_offset;
    GLint pos2_ub_offset;

    REFCOUNT_INSERT_VARIABLES

} _curve_editor_program_lerp;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_editor_program_lerp,
                               curve_editor_program_lerp,
                              _curve_editor_program_lerp);


/** Please see header for specification */
PRIVATE void _curve_editor_program_lerp_release(void* in)
{
    _curve_editor_program_lerp* lerp_ptr = (_curve_editor_program_lerp*) in;

    /* Release all objects */
    if (lerp_ptr->program != NULL)
    {
        ral_context_delete_objects(lerp_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                  &lerp_ptr->program);
    }
}



/** Please see header for specification */
PUBLIC curve_editor_program_lerp curve_editor_program_lerp_create(ral_context               context,
                                                                  system_hashed_ansi_string name)
{
    _curve_editor_program_lerp* lerp_ptr = new (std::nothrow) _curve_editor_program_lerp;

    ASSERT_DEBUG_SYNC(lerp_ptr != NULL,
                      "Out of memroy while instantiating lerp program object.");

    if (lerp_ptr != NULL)
    {
        /* Reset the structure */
        lerp_ptr->context    = context;
        lerp_ptr->program    = NULL;
        lerp_ptr->program_ub = NULL;

        /* Create vertex shader */
        std::stringstream fp_body_stream;
        std::stringstream vp_body_stream;

        fp_body_stream << "#version 430 core\n"
                          "\n"
                          "out vec4 color;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    color = vec4(1, 1, 0, 1);\n"
                          "}\n";

        vp_body_stream << "#version 430 core\n"
                          "\n"
                          "uniform data\n"
                          "{\n"
                          "    vec4 pos1;\n"
                          "    vec4 pos2;\n"
                          "};\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "   gl_Position = (gl_VertexID == 0 ? pos1 : pos2);\n"
                          "}\n";

        /* Create the program */
        ral_program_create_info program_create_info =
        {
            name
        };

        ral_context_create_programs(context,
                                    1, /* n_create_info_items */
                                   &program_create_info,
                                   &lerp_ptr->program);


        if (lerp_ptr->program == NULL)
        {
            ASSERT_DEBUG_SYNC(lerp_ptr->program != NULL,
                              "RAL program creation failed");

            goto end;
        }

        /* Create the shaders */
        ral_shader_create_info shader_create_info_items[] =
        {
            {
                system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                        " FS"),
                RAL_SHADER_TYPE_FRAGMENT
            },
            {
                system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                        " VS"),
                RAL_SHADER_TYPE_VERTEX
            }
        };
        const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
        ral_shader     result_shaders[n_shader_create_info_items];

        if (!ral_context_create_shaders(lerp_ptr->context,
                                        n_shader_create_info_items,
                                        shader_create_info_items,
                                        result_shaders) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL shader creation failed");

            goto end;
        }

        /* Set the shaders' bodies */
        system_hashed_ansi_string fs_shader_body = system_hashed_ansi_string_create(fp_body_stream.str().c_str() );
        system_hashed_ansi_string vs_shader_body = system_hashed_ansi_string_create(vp_body_stream.str().c_str() );

        ral_shader_set_property(result_shaders[0],
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &fs_shader_body);
        ral_shader_set_property(result_shaders[1],
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &vs_shader_body);

        /* Attach shaders to the program */
        if (!ral_program_attach_shader(lerp_ptr->program,
                                       result_shaders[0],
                                       false /* relink_needed */) ||
            !ral_program_attach_shader(lerp_ptr->program,
                                       result_shaders[1],
                                       true /* relink_needed */))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not attach shaders to lerp curve program");

            goto end;
        }

        /* Release the shaders */
        ral_context_delete_objects(lerp_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   n_shader_create_info_items,
                                   result_shaders);

        /* Retrieve uniform locations */
        const ral_program_variable* pos1_uniform_ptr = NULL;
        const ral_program_variable* pos2_uniform_ptr = NULL;
        raGL_program                program_raGL     = ral_context_get_program_gl(context,
                                                                                  lerp_ptr->program);

        raGL_program_get_uniform_by_name      (program_raGL,
                                              system_hashed_ansi_string_create("pos1"),
                                             &pos1_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                              system_hashed_ansi_string_create("pos2"),
                                             &pos2_uniform_ptr);
        raGL_program_get_uniform_block_by_name(program_raGL,
                                              system_hashed_ansi_string_create("data"),
                                             &lerp_ptr->program_ub);

        lerp_ptr->pos1_ub_offset = pos1_uniform_ptr->block_offset;
        lerp_ptr->pos2_ub_offset = pos2_uniform_ptr->block_offset;

        ogl_program_ub_get_property(lerp_ptr->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &lerp_ptr->program_ub_bo);
        ogl_program_ub_get_property(lerp_ptr->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &lerp_ptr->program_ub_bo_size);

        ASSERT_DEBUG_SYNC(lerp_ptr->pos1_ub_offset != -1 &&
                          lerp_ptr->pos2_ub_offset != -1,
                          "At least one UB offset is -1");

        /* Add to the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(lerp_ptr,
                                                       _curve_editor_program_lerp_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_LERP,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (LERP)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_lerp) lerp_ptr;

end:
    if (lerp_ptr != NULL)
    {
        curve_editor_program_lerp_release( (curve_editor_program_lerp&) lerp_ptr);
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC void curve_editor_program_lerp_set_property(curve_editor_program_lerp          lerp,
                                                   curve_editor_program_lerp_property property,
                                                   const void*                        data)
{
    _curve_editor_program_lerp* lerp_ptr = (_curve_editor_program_lerp*) lerp;

    switch (property)
    {
        case CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS1:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(lerp_ptr->program_ub,
                                                        lerp_ptr->pos1_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            break;
        }

        case CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS2:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(lerp_ptr->program_ub,
                                                        lerp_ptr->pos2_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_program_lerp_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void curve_editor_program_lerp_use(ral_context               context,
                                          curve_editor_program_lerp lerp)
{
    const ogl_context_gl_entrypoints* entry_points_ptr           = NULL;
    _curve_editor_program_lerp*       lerp_ptr                   = (_curve_editor_program_lerp*) lerp;
    raGL_program                      program_raGL               = ral_context_get_program_gl(lerp_ptr->context,
                                                                                              lerp_ptr->program);
    GLuint                            program_raGL_id            = 0;
    GLuint                            program_ub_bo_id           = 0;
    raGL_buffer                       program_ub_bo_raGL         = NULL;
    uint32_t                          program_ub_bo_start_offset = -1;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);
    ogl_context_get_property(ral_context_get_gl_context(context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points_ptr);

    program_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                   lerp_ptr->program_ub_bo);

    ogl_program_ub_sync(lerp_ptr->program_ub);

    raGL_buffer_get_property(program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_ub_bo_id);
    raGL_buffer_get_property(program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_ub_bo_start_offset);

    entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         0, /* index */
                                         program_ub_bo_id,
                                         program_ub_bo_start_offset,
                                         lerp_ptr->program_ub_bo_size);
    entry_points_ptr->pGLUseProgram     (program_raGL_id);
}
