/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_static.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_types.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
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

} _curve_editor_program_static;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_editor_program_static,
                               curve_editor_program_static,
                              _curve_editor_program_static);


/** Please see header for specification */
PRIVATE void _curve_editor_program_static_release(void* in)
{
    _curve_editor_program_static* program_ptr = (_curve_editor_program_static*) in;

    /* Release all objects */
    if (program_ptr->program != NULL)
    {
        ral_context_delete_objects(program_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                  &program_ptr->program);

        program_ptr->program = NULL;
    }
}



/** Please see header for specification */
PUBLIC curve_editor_program_static curve_editor_program_static_create(ral_context               context,
                                                                      system_hashed_ansi_string name)
{
    _curve_editor_program_static* program_ptr = new (std::nothrow) _curve_editor_program_static;

    ASSERT_DEBUG_SYNC(program_ptr != NULL,
                      "Out of memroy while instantiating static program object.");

    if (program_ptr != NULL)
    {
        /* Reset the structure */
        program_ptr->context    = context;
        program_ptr->program    = NULL;
        program_ptr->program_ub = NULL;

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
        const ral_program_create_info program_create_info =
        {
            name
        };

        if (!ral_context_create_programs(context,
                                         1, /* n_create_info_items */
                                        &program_create_info,
                                        &program_ptr->program) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create static curve program.");

            goto end;
        }

        /* Create the shaders */
        const ral_shader_create_info fs_create_info =
        {
            system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                    " FS"),
            RAL_SHADER_TYPE_FRAGMENT
        };
        const ral_shader_create_info vs_create_info =
        {
            system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                    " VS"),
            RAL_SHADER_TYPE_VERTEX
        };

        const ral_shader_create_info shader_create_info_items[] =
        {
            fs_create_info,
            vs_create_info
        };
        const uint32_t n_shader_create_info_items                 = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
        ral_shader     result_shaders[n_shader_create_info_items] = { NULL };

        if (!ral_context_create_shaders(context,
                                        n_shader_create_info_items,
                                        shader_create_info_items,
                                        result_shaders) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL Shader creation failed");

            goto end;
        }

        /* Set the shader bodies */
        system_hashed_ansi_string fs_shader_body = system_hashed_ansi_string_create(fp_body_stream.str().c_str() );
        system_hashed_ansi_string vs_shader_body = system_hashed_ansi_string_create(vp_body_stream.str().c_str() );

        ral_shader_set_property(result_shaders[0],
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                                &fs_shader_body);
        ral_shader_set_property(result_shaders[1],
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                                &vs_shader_body);

        /* Attach shaders to the program */
        if (!ral_program_attach_shader(program_ptr->program,
                                       result_shaders[0],
                                       false /* relink_needed */) ||
            !ral_program_attach_shader(program_ptr->program,
                                       result_shaders[1],
                                       true /* relink_needed */) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not attach shader(s) to static curve program.");

            goto end;
        }

        /* Release the shaders */
        ral_context_delete_objects(program_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   n_shader_create_info_items,
                                   result_shaders);

        /* Retrieve uniform locations */
        const ral_program_variable* pos1_uniform_ptr = NULL;
        const ral_program_variable* pos2_uniform_ptr = NULL;
        raGL_program                program_raGL     = ral_context_get_program_gl(context,
                                                                                  program_ptr->program);

        raGL_program_get_uniform_by_name      (program_raGL,
                                               system_hashed_ansi_string_create("pos1"),
                                              &pos1_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                               system_hashed_ansi_string_create("pos2"),
                                              &pos2_uniform_ptr);
        raGL_program_get_uniform_block_by_name(program_raGL,
                                               system_hashed_ansi_string_create("data"),
                                              &program_ptr->program_ub);

        ASSERT_DEBUG_SYNC(pos1_uniform_ptr        != NULL &&
                          pos2_uniform_ptr        != NULL &&
                          program_ptr->program_ub != NULL,
                          "Could not retrieve uniform / uniform block descriptors.");

        program_ptr->pos1_ub_offset = pos1_uniform_ptr->block_offset;
        program_ptr->pos2_ub_offset = pos2_uniform_ptr->block_offset;

        ASSERT_DEBUG_SYNC(program_ptr->pos1_ub_offset != -1 &&
                          program_ptr->pos2_ub_offset != -1,
                          "At least one UB offset is -1");

        ogl_program_ub_get_property(program_ptr->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &program_ptr->program_ub_bo_size);
        ogl_program_ub_get_property(program_ptr->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &program_ptr->program_ub_bo);

        /* Add to the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(program_ptr,
                                                       _curve_editor_program_static_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_STATIC,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (Static)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_static) program_ptr;

end:
    if (program_ptr != NULL)
    {
        curve_editor_program_static_release( (curve_editor_program_static&) program_ptr);
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC void curve_editor_program_static_set_property(curve_editor_program_static          instance,
                                                     curve_editor_program_static_property property,
                                                     const void*                          data)
{
    _curve_editor_program_static* instance_ptr = (_curve_editor_program_static*) instance;

    switch (property)
    {
        case CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS1_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(instance_ptr->program_ub,
                                                        instance_ptr->pos1_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            break;
        }

        case CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS2_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(instance_ptr->program_ub,
                                                        instance_ptr->pos2_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_program_static_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void curve_editor_program_static_use(ral_context                 context,
                                            curve_editor_program_static instance)
{
    const ogl_context_gl_entrypoints* entry_points_ptr           = NULL;
    GLuint                            program_ub_bo_id           = 0;
    raGL_buffer                       program_ub_bo_raGL         = NULL;
    uint32_t                          program_ub_bo_start_offset = -1;
    _curve_editor_program_static*     static_ptr                 = (_curve_editor_program_static*) instance;

    const raGL_program                program_raGL               = ral_context_get_program_gl(context,
                                                                                              static_ptr->program);
    GLuint                            program_raGL_id            = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ogl_context_get_property(ral_context_get_gl_context(context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points_ptr);

    program_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                   static_ptr->program_ub_bo);

    ogl_program_ub_sync(static_ptr->program_ub);

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
                                         static_ptr->program_ub_bo_size);
    entry_points_ptr->pGLUseProgram     (program_raGL_id);

}

