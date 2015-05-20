/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_curvebackground.h"
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

    GLint colors_ub_offset;
    GLint positions_ub_offset;

    REFCOUNT_INSERT_VARIABLES

} _curve_editor_program_curvebackground;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_editor_program_curvebackground,
                               curve_editor_program_curvebackground,
                              _curve_editor_program_curvebackground);


/** Please see header for specification */
PRIVATE void _curve_editor_program_curvebackground_release(void* in)
{
    _curve_editor_program_curvebackground* program = (_curve_editor_program_curvebackground*) in;

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
PUBLIC curve_editor_program_curvebackground curve_editor_program_curvebackground_create(__in __notnull ogl_context               context,
                                                                                        __in __notnull system_hashed_ansi_string name)
{
    _curve_editor_program_curvebackground* result = new (std::nothrow) _curve_editor_program_curvebackground;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Out of memroy while instantiating curve background program object.");

    if (result != NULL)
    {
        /* Reset the structure */
        result->fragment_shader = NULL;
        result->program         = NULL;
        result->vertex_shader   = NULL;

        /* Create vertex shader */
        std::stringstream fp_body_stream;
        std::stringstream vp_body_stream;

        fp_body_stream << "#version 430 core\n"
                          "\n"
                          "in  vec4 color;\n"
                          "out vec4 result;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    result = color;\n"
                          "}\n";

        vp_body_stream << "#version 430 core\n"
                          "\n"
                          "uniform data\n"
                          "{\n"
                          "    vec4 positions[5];\n" /* 5 because we use the shader to draw the border - doh */
                          "    vec4 colors;\n"
                          "};\n"
                          "\n"
                          "out vec4 color;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "   gl_Position = positions[gl_VertexID];\n"
                          "   color       = (gl_VertexID < 2 ? colors : vec4(colors.xyz * 0.5, colors.w) );\n"
                          "}\n";

        /* Create the program */
        result->program = ogl_program_create(context,
                                             name,
                                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

        ASSERT_DEBUG_SYNC(result->program != NULL,
                          "ogl_program_create() failed");

        if (result->program == NULL)
        {
            LOG_ERROR("Could not create curve background program.");

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
            LOG_ERROR("Could not create curve background fragment / vertex shader.");

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
            LOG_ERROR("Could not set curve background fragment / vertex shader body.");

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
            LOG_ERROR("Could not attach shader(s) to curve background program.");

            goto end;
        }

        /* Link the program */
        b_result = ogl_program_link(result->program);

        ASSERT_DEBUG_SYNC(b_result,
                          "ogl_program_link() failed");
        if (!b_result)
        {
            LOG_ERROR("Could not link curve background program");

            goto end;
        }

        /* Retrieve uniform locations */
        const ogl_program_variable* colors_uniform_descriptor    = NULL;
        const ogl_program_variable* positions_uniform_descriptor = NULL;

        b_result  = ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("colors"),
                                                         &colors_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("positions[0]"),
                                                         &positions_uniform_descriptor);
        b_result &= ogl_program_get_uniform_block_by_name(result->program,
                                                          system_hashed_ansi_string_create("data"),
                                                         &result->program_ub);

        ASSERT_DEBUG_SYNC(b_result,
                          "Could not retrieve colors or positions uniform descriptor.");

        if (b_result)
        {
            result->colors_ub_offset    = colors_uniform_descriptor->block_offset;
            result->positions_ub_offset = positions_uniform_descriptor->block_offset;

            ASSERT_DEBUG_SYNC(result->colors_ub_offset    != -1 &&
                              result->positions_ub_offset != -1,
                              "At least one UB offset was -1");

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
                                                       _curve_editor_program_curvebackground_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_CURVEBACKGROUND,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (Curve Background)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_curvebackground) result;

end:
    if (result != NULL)
    {
        curve_editor_program_curvebackground_release( (curve_editor_program_curvebackground&) result);
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC void curve_editor_program_curvebackground_set_property(__in __notnull curve_editor_program_curvebackground          program,
                                                              __in           curve_editor_program_curvebackground_property property,
                                                              __in __notnull const void*                                   data)
{
    _curve_editor_program_curvebackground* program_ptr = (_curve_editor_program_curvebackground*) program;

    switch (property)
    {
        case CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_COLORS_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(program_ptr->program_ub,
                                                        program_ptr->colors_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            break;
        }

        case CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_POSITIONS_DATA:
        {
            ogl_program_ub_set_arrayed_uniform_value(program_ptr->program_ub,
                                                     program_ptr->positions_ub_offset,
                                                     data,
                                                     0, /* src_data_flags */
                                                     sizeof(float) * 4 * 5,
                                                     0,  /* dst_array_start_index */
                                                     5); /* dst_array_item_count */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_program_curvebackground_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void curve_editor_program_curvebackground_use(__in __notnull ogl_context                          context,
                                                     __in __notnull curve_editor_program_curvebackground curvebackground)
{
    _curve_editor_program_curvebackground* curvebackground_ptr = (_curve_editor_program_curvebackground*) curvebackground;
    const ogl_context_gl_entrypoints*      entry_points        = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_program_ub_sync(curvebackground_ptr->program_ub);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                     curvebackground_ptr->program_ub_bo_id,
                                     curvebackground_ptr->program_ub_bo_start_offset,
                                     curvebackground_ptr->program_ub_bo_size);
    entry_points->pGLUseProgram     (ogl_program_get_id(curvebackground_ptr->program) );
}
