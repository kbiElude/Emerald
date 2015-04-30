/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_quadselector.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include <sstream>


/* Type definitions */
typedef struct
{
    ogl_shader     fragment_shader;
    ogl_shader     vertex_shader;
    ogl_program    program;
    ogl_program_ub program_ub;
    GLuint         program_ub_bo_id;
    GLuint         program_ub_bo_size;
    GLuint         program_ub_bo_start_offset;

    GLint alpha_ub_offset;
    GLint positions_ub_offset;

    REFCOUNT_INSERT_VARIABLES

} _curve_editor_program_quadselector;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_editor_program_quadselector,
                               curve_editor_program_quadselector,
                              _curve_editor_program_quadselector);


/** Please see header for specification */
PRIVATE void _curve_editor_program_quadselector_release(void* in)
{
    _curve_editor_program_quadselector* program = (_curve_editor_program_quadselector*) in;

    /* Release all objects */
    if (program->fragment_shader != NULL)
    {
        ogl_shader_release(program->fragment_shader);
    }

    if (program->vertex_shader != NULL)
    {
        ogl_shader_release(program->vertex_shader);
    }

    if (program->program != NULL)
    {
        ogl_program_release(program->program);
    }
}



/** Please see header for specification */
PUBLIC curve_editor_program_quadselector curve_editor_program_quadselector_create(__in __notnull ogl_context               context,
                                                                                  __in __notnull system_hashed_ansi_string name)
{
    _curve_editor_program_quadselector* result = new (std::nothrow) _curve_editor_program_quadselector;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Out of memroy while instantiating quad selector program object.");

    if (result != NULL)
    {
        /* Reset the structure */
        result->fragment_shader = NULL;
        result->program         = NULL;
        result->vertex_shader   = NULL;

        /* Create vertex shader */
        std::stringstream fp_body_stream;
        std::stringstream vp_body_stream;

        fp_body_stream << "#version 330\n"
                          "\n"
                          "uniform data\n"
                          "{\n"
                          "    float alpha;\n"
                          "    vec4 positions[4];\n"
                          "};\n"
                          "\n"
                          "out vec4 color;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    color = vec4(1, 1, 1, alpha);\n"
                          "}\n";

        vp_body_stream << "#version 330\n"
                          "\n"
                          "uniform data\n"
                          "{\n"
                          "    float alpha;\n"
                          "    vec4  positions[4];\n"
                          "};\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "   gl_Position = positions[gl_VertexID];\n"
                          "}\n";

        /* Create the program */
        result->program = ogl_program_create(context,
                                             name,
                                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

        ASSERT_DEBUG_SYNC(result->program != NULL,
                          "ogl_program_create() failed");

        if (result->program == NULL)
        {
            LOG_ERROR("Could not create quad selector curve program.");

            goto end;
        }

        /* Create the shaders */
        result->fragment_shader = ogl_shader_create(context,
                                                    SHADER_TYPE_FRAGMENT,
                                                    system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                            " FP"));
        result->vertex_shader   = ogl_shader_create(context,
                                                    SHADER_TYPE_VERTEX,
                                                    system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                            " VP"));

        ASSERT_DEBUG_SYNC(result->fragment_shader != NULL &&
                          result->vertex_shader   != NULL,
                          "ogl_shader_create() failed");

        if (result->fragment_shader == NULL ||
            result->vertex_shader   == NULL)
        {
            LOG_ERROR("Could not create quad selector curve fragment / vertex shader.");

            goto end;
        }

        /* Set the shaders' bodies */
        system_hashed_ansi_string fp_shader_body = system_hashed_ansi_string_create(fp_body_stream.str().c_str() );
        system_hashed_ansi_string vp_shader_body = system_hashed_ansi_string_create(vp_body_stream.str().c_str() );
        bool                      b_result       = false;

        b_result  = ogl_shader_set_body(result->fragment_shader,
                                        fp_shader_body);
        b_result &= ogl_shader_set_body(result->vertex_shader,
                                        vp_shader_body);

        ASSERT_DEBUG_SYNC(b_result,
                          "ogl_shader_set_body() failed");

        if (!b_result)
        {
            LOG_ERROR("Could not set quad selector curve fragment / vertex shader body.");

            goto end;
        }

        /* Attach shaders to the program */
        b_result  = ogl_program_attach_shader(result->program,
                                              result->fragment_shader);
        b_result &= ogl_program_attach_shader(result->program,
                                              result->vertex_shader);

        ASSERT_DEBUG_SYNC(b_result,
                          "ogl_program_attach_shader() failed");

        if (!b_result)
        {
            LOG_ERROR("Could not attach shader(s) to quad selector curve program.");

            goto end;
        }

        /* Link the program */
        b_result = ogl_program_link(result->program);

        ASSERT_DEBUG_SYNC(b_result,
                          "ogl_program_link() failed");

        if (!b_result)
        {
            LOG_ERROR("Could not link quad selector curve program");

            goto end;
        }

        /* Retrieve uniform locations */
        const ogl_program_uniform_descriptor* alpha_uniform_descriptor     = NULL;
        const ogl_program_uniform_descriptor* positions_uniform_descriptor = NULL;

        b_result  = ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("alpha"),
                                                         &alpha_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("positions[0]"),
                                                         &positions_uniform_descriptor);
        b_result &= ogl_program_get_uniform_block_by_name(result->program,
                                                          system_hashed_ansi_string_create("data"),
                                                          &result->program_ub);

        ASSERT_DEBUG_SYNC(b_result,
                          "Could not retrieve alpha/positions uniform descriptor or the data uniform block descriptor.");

        if (b_result)
        {
            result->alpha_ub_offset     = alpha_uniform_descriptor->ub_offset;
            result->positions_ub_offset = positions_uniform_descriptor->ub_offset;

            ASSERT_DEBUG_SYNC(result->alpha_ub_offset     != -1 &&
                              result->positions_ub_offset != -1,
                              "At least one UB offset is -1");

            ogl_program_ub_get_property(result->program_ub,
                                        OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                       &result->program_ub_bo_size);
            ogl_program_ub_get_property(result->program_ub,
                                        OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                       &result->program_ub_bo_id);
            ogl_program_ub_get_property(result->program_ub,
                                        OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                       &result->program_ub_bo_start_offset);
        }

        /* Add to the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _curve_editor_program_quadselector_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_QUADSELECTOR,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (Quad Selector)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_quadselector) result;

end:
    if (result != NULL)
    {
        curve_editor_program_quadselector_release( (curve_editor_program_quadselector&) result);
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC void curve_editor_program_quadselector_set_property(__in __notnull curve_editor_program_quadselector          program,
                                                           __in           curve_editor_program_quadselector_property property,
                                                           __in __notnull const void*                                data)
{
    _curve_editor_program_quadselector* program_ptr = (_curve_editor_program_quadselector*) program;

    switch (property)
    {
        case CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_ALPHA_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(program_ptr->program_ub,
                                                        program_ptr->alpha_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) );

            break;
        }

        case CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_POSITIONS_DATA:
        {
            ogl_program_ub_set_arrayed_uniform_value(program_ptr->program_ub,
                                                     program_ptr->positions_ub_offset,
                                                     data,
                                                     0, /* src_data_flags */
                                                     sizeof(float) * 16,
                                                     0, /* dst_array_start_index */
                                                     4);/* dst_array_item_count */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_program_quadselector_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void curve_editor_program_quadselector_use(__in __notnull ogl_context                       context,
                                                  __in __notnull curve_editor_program_quadselector quadselector)
{
    const ogl_context_gl_entrypoints*   entry_points     = NULL;
    _curve_editor_program_quadselector* quadselector_ptr = (_curve_editor_program_quadselector*) quadselector;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_program_ub_sync(quadselector_ptr->program_ub);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                     quadselector_ptr->program_ub_bo_id,
                                     quadselector_ptr->program_ub_bo_start_offset,
                                     quadselector_ptr->program_ub_bo_size);
    entry_points->pGLUseProgram     (ogl_program_get_id(quadselector_ptr->program) );
}
