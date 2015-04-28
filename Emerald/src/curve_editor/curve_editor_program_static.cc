/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_static.h"
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
    _curve_editor_program_static* program = (_curve_editor_program_static*) in;

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
PUBLIC curve_editor_program_static curve_editor_program_static_create(__in __notnull ogl_context               context,
                                                                      __in __notnull system_hashed_ansi_string name)
{
    _curve_editor_program_static* result = new (std::nothrow) _curve_editor_program_static;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Out of memroy while instantiating static program object.");

    if (result != NULL)
    {
        /* Reset the structure */
        result->fragment_shader = NULL;
        result->program         = NULL;
        result->program_ub      = NULL;
        result->vertex_shader   = NULL;

        /* Create vertex shader */
        std::stringstream fp_body_stream;
        std::stringstream vp_body_stream;

        fp_body_stream << "#version 330\n"
                          "\n"
                          "out vec4 color;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    color = vec4(1, 1, 0, 1);\n"
                          "}\n";

        vp_body_stream << "#version 330\n"
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
        result->program = ogl_program_create(context,
                                             name,
                                             true); /* use_syncable_ubs */

        ASSERT_DEBUG_SYNC(result->program != NULL,
                          "ogl_program_create() failed");

        if (result->program == NULL)
        {
            LOG_ERROR("Could not create static curve program.");

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
            LOG_ERROR("Could not create static curve fragment / vertex shader.");

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
            LOG_ERROR("Could not set static curve fragment / vertex shader body.");

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
            LOG_ERROR("Could not attach shader(s) to static curve program.");

            goto end;
        }

        /* Link the program */
        b_result = ogl_program_link(result->program);

        ASSERT_DEBUG_SYNC(b_result,
                          "ogl_program_link() failed");

        if (!b_result)
        {
            LOG_ERROR("Could not link static curve program");

            goto end;
        }

        /* Retrieve uniform locations */
        const ogl_program_uniform_descriptor* pos1_uniform_descriptor = NULL;
        const ogl_program_uniform_descriptor* pos2_uniform_descriptor = NULL;

        b_result  = ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("pos1"),
                                                         &pos1_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("pos2"),
                                                         &pos2_uniform_descriptor);
        b_result &= ogl_program_get_uniform_block_by_name(result->program,
                                                          system_hashed_ansi_string_create("data"),
                                                         &result->program_ub);

        ASSERT_DEBUG_SYNC(b_result,
                          "Could not retrieve pos1/pos2 uniform descriptor.");

        if (b_result)
        {
            result->pos1_ub_offset = pos1_uniform_descriptor->ub_offset;
            result->pos2_ub_offset = pos2_uniform_descriptor->ub_offset;

            ASSERT_DEBUG_SYNC(result->pos1_ub_offset != -1 &&
                              result->pos2_ub_offset != -1,
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
                                                       _curve_editor_program_static_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_STATIC,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (Static)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_static) result;

end:
    if (result != NULL)
    {
        curve_editor_program_static_release( (curve_editor_program_static&) result);
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC void curve_editor_program_static_set_property(__in __notnull curve_editor_program_static          instance,
                                                     __in           curve_editor_program_static_property property,
                                                     __in __notnull const void*                          data)
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
PUBLIC void curve_editor_program_static_use(__in __notnull ogl_context                 context,
                                            __in __notnull curve_editor_program_static instance)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _curve_editor_program_static*     static_ptr   = (_curve_editor_program_static*) instance;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_program_ub_sync(static_ptr->program_ub);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                     static_ptr->program_ub_bo_id,
                                     static_ptr->program_ub_bo_start_offset,
                                     static_ptr->program_ub_bo_size);
    entry_points->pGLUseProgram     (ogl_program_get_id(static_ptr->program) );

}

