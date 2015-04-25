/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_static.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include <sstream>


/* Type definitions */
typedef struct
{
    ogl_shader  fragment_shader;
    ogl_shader  vertex_shader;
    ogl_program program;

    GLint pos1_location;
    GLint pos2_location;

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
                          "uniform vec4 pos1;\n"
                          "uniform vec4 pos2;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "   gl_Position = (gl_VertexID == 0 ? pos1 : pos2);\n"
                          "}\n";

        /* Create the program */
        result->program = ogl_program_create(context,
                                             name);

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

        b_result  = ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("pos1"),
                                                   &pos1_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("pos2"),
                                                   &pos2_uniform_descriptor);

        ASSERT_DEBUG_SYNC(b_result,
                          "Could not retrieve pos1/pos2 uniform descriptor.");

        if (b_result)
        {
            result->pos1_location = pos1_uniform_descriptor->location;
            result->pos2_location = pos2_uniform_descriptor->location;
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

/** Please see header for specification */
PUBLIC GLint curve_editor_program_static_get_pos1_uniform_location(__in __notnull curve_editor_program_static program)
{
    return ((_curve_editor_program_static*) program)->pos1_location;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_static_get_pos2_uniform_location(__in __notnull curve_editor_program_static program)
{
    return ((_curve_editor_program_static*) program)->pos2_location;
}

/** Please see header for specification */
PUBLIC GLuint curve_editor_program_static_get_id(__in __notnull curve_editor_program_static program)
{
    return ogl_program_get_id( ((_curve_editor_program_static*) program)->program);
}
