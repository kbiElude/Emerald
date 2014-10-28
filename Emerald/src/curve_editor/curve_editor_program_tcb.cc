/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_tcb.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
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

    GLuint delta_time_location;
    GLuint delta_x_location;
    GLuint node_indexes_location;
    GLuint should_round_location;
    GLuint start_time_location;
    GLuint start_x_location;
    GLuint val_range_location;

    GLint nodes_buffer_uniform_block_index;

    REFCOUNT_INSERT_VARIABLES

} _curve_editor_program_tcb;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_editor_program_tcb, curve_editor_program_tcb, _curve_editor_program_tcb);


/** Please see header for specification */
PRIVATE void _curve_editor_program_tcb_release(void* in)
{
    _curve_editor_program_tcb* program = (_curve_editor_program_tcb*) in;

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


/** TODO */
PRIVATE void _curve_editor_program_tcb_create_renderer_callback(__in __notnull ogl_context context,
                                                                               void*       curve_editor_program_tcb)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _curve_editor_program_tcb*        result_ptr   = (_curve_editor_program_tcb*) curve_editor_program_tcb;
    GLuint                            program_id   = ogl_program_get_id(result_ptr->program);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    result_ptr->nodes_buffer_uniform_block_index = entry_points->pGLGetUniformBlockIndex(program_id,
                                                                                         "NodesBuffer");

    ASSERT_DEBUG_SYNC(result_ptr->nodes_buffer_uniform_block_index != -1,
                      "Could not retrieve uniform block index for NodesBuffer");

    /* Configure the binding for the program */
    entry_points->pGLUseProgram         (program_id);
    entry_points->pGLUniformBlockBinding(program_id,
                                         result_ptr->nodes_buffer_uniform_block_index,
                                         0);
    entry_points->pGLUseProgram         (0);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                      "GL error in _curve_editor_program_tcb_create_renderer_callback()");
}

/** Please see header for specification */
PUBLIC curve_editor_program_tcb curve_editor_program_tcb_create(__in __notnull ogl_context               context,
                                                                __in __notnull system_hashed_ansi_string name)
{
    _curve_editor_program_tcb* result = new (std::nothrow) _curve_editor_program_tcb;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Out of memroy while instantiating TCB program object.");

    if (result != NULL)
    {
        /* Reset the structure */
        result->delta_time_location = -1;
        result->delta_x_location    = -1;
        result->fragment_shader     = NULL;
        result->program             = NULL;
        result->start_time_location = -1;
        result->start_x_location    = -1;
        result->vertex_shader       = NULL;

        /* Create vertex shader */
        std::stringstream fp_body_stream;
        std::stringstream vp_body_stream;

        fp_body_stream << "#version 420\n"
                          "\n"
                          "out vec4 color;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    color = vec4(1, 1, 0, 1);\n"
                          "}\n";

        vp_body_stream << "#version 420\n"
                          "\n"
                          "const int max_nodes = 64;\n"
                          "\n"
                          "uniform float start_x;\n"
                          "uniform float start_time;\n"
                          "uniform float delta_x;\n"
                          "uniform float delta_time;\n"
                          "uniform vec2  val_range;\n"
                          "uniform ivec4 node_indexes;\n"
                          "uniform bool  should_round;\n"
                          "\n"
                          "struct Node\n"
                          "{\n"
                          "    float bias;\n"
                          "    float continuity;\n"
                          "    float tension;\n"
                          "    float time;\n"
                          "    float value;\n"
                          "    float pad1;\n"
                          "    float pad2;\n"
                          "    float pad3;\n"
                          "};\n"
                          "layout(std140, binding=0) uniform NodesBuffer\n"
                          "{\n"
                          "   Node nodes[max_nodes];\n"
                          "};\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    float x    = start_x    + delta_x    * gl_VertexID;\n"
                          "    float time = start_time + delta_time * gl_VertexID;\n"
                          "\n"
                          "    float a = (1.0 - nodes[node_indexes.y].tension) * (1.0 + nodes[node_indexes.y].continuity) * (1.0 + nodes[node_indexes.y].bias);\n"
                          "    float b = (1.0 - nodes[node_indexes.y].tension) * (1.0 - nodes[node_indexes.y].continuity) * (1.0 - nodes[node_indexes.y].bias);\n"
                          "    float d = nodes[node_indexes.z].value - nodes[node_indexes.y].value;\n"
                          "    float t;\n"
                          "    float in_tangent;\n"
                          "    float out_tangent;\n"
                          "\n"
                          "    if (node_indexes.x != -1)\n"
                          "    {\n"
                          "        t           = (nodes[node_indexes.z].time - nodes[node_indexes.y].time) / (nodes[node_indexes.z].time - nodes[node_indexes.x].time);\n"
                          "        out_tangent = t * (a * (nodes[node_indexes.y].value - nodes[node_indexes.x].value) + b * d);\n"
                          "    }\n"
                          "    else\n"
                          "    {\n"
                          "        out_tangent = b * d;\n"
                          "    }\n"
                          "\n"
                          "    a = (1.0 - nodes[node_indexes.z].tension) * (1.0 - nodes[node_indexes.z].continuity) * (1.0 + nodes[node_indexes.z].bias);\n"
                          "    b = (1.0 - nodes[node_indexes.z].tension) * (1.0 + nodes[node_indexes.z].continuity) * (1.0 - nodes[node_indexes.z].bias);\n"
                          "\n"
                          "    if (node_indexes.w != -1)\n"
                          "    {\n"
                          "        t          = (nodes[node_indexes.w].time - nodes[node_indexes.y].time) / (nodes[node_indexes.w].time - nodes[node_indexes.y].time);\n"
                          "        in_tangent = t * (b * (nodes[node_indexes.w].value - nodes[node_indexes.z].value) + a * d);\n"
                          "    }\n"
                          "    else\n"
                          "    {\n"
                          "        in_tangent = a * d;\n"
                          "    }\n"
                          "\n"
                          "   float curve_time = (time - nodes[node_indexes.y].time) / (nodes[node_indexes.z].time - nodes[node_indexes.y].time);\n"
                          "   float t2         = pow(curve_time, 2);\n"
                          "   float t3         = pow(curve_time, 3);\n"
                          "   float h2         = 3.0 * t2 - t3 - t3;\n"
                          "   float h1         = 1.0 - h2;\n"
                          "   float h4         = t3 - t2;\n"
                          "   float h3         = h4 - t2 + curve_time;\n"
                          "\n"
                          "   float value    = h1 * nodes[node_indexes.y].value + h2 * nodes[node_indexes.z].value + h3 * out_tangent + h4 * in_tangent;\n"
                          "\n"
                          "   if (should_round)\n"
                          "   {\n"
                          "      value = float(int(value));\n"
                          "   }\n"
                          "   float value_ss = 2.0 / (val_range.y - val_range.x) * (value - val_range.x) - 1;\n"
                          "\n"
                          "   gl_Position = vec4(x, value_ss, 0, 1);\n"
                          "}\n";
        
        /* Create the program */
        result->program = ogl_program_create(context, name);

        ASSERT_DEBUG_SYNC(result->program != NULL,
                          "ogl_program_create() failed");

        if (result->program == NULL)
        {
            LOG_ERROR("Could not create tcb curve program.");

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

        if (result->fragment_shader == NULL || result->vertex_shader == NULL)
        {
            LOG_ERROR("Could not create tcb curve fragment / vertex shader.");

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
            LOG_ERROR("Could not set tcb curve fragment / vertex shader body.");

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
            LOG_ERROR("Could not attach shader(s) to lerp curve program.");

            goto end;
        }

        /* Link the program */
        b_result = ogl_program_link(result->program);

        ASSERT_DEBUG_SYNC(b_result,
                          "ogl_program_link() failed");

        if (!b_result)
        {
            LOG_ERROR("Could not link tcb curve program");

            goto end;
        }

        /* Retrieve uniform locations */
        const ogl_program_uniform_descriptor* delta_time_uniform_descriptor   = NULL;
        const ogl_program_uniform_descriptor* delta_x_uniform_descriptor      = NULL;
        const ogl_program_uniform_descriptor* node_indexes_uniform_descriptor = NULL;
        const ogl_program_uniform_descriptor* should_round_uniform_descriptor = NULL;
        const ogl_program_uniform_descriptor* start_time_uniform_descriptor   = NULL;
        const ogl_program_uniform_descriptor* start_x_uniform_descriptor      = NULL;
        const ogl_program_uniform_descriptor* val_range_uniform_descriptor    = NULL;

        b_result  = ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("delta_time"),
                                                   &delta_time_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("delta_x"),
                                                   &delta_x_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("node_indexes"),
                                                   &node_indexes_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("should_round"),
                                                   &should_round_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("start_time"),
                                                   &start_time_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("start_x"),
                                                   &start_x_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name(result->program,
                                                    system_hashed_ansi_string_create("val_range"),
                                                   &val_range_uniform_descriptor);

        ASSERT_DEBUG_SYNC(b_result,
                          "Could not retrieve delta_time/delta_x/start_time/start_x uniform descriptor.");

        if (b_result)
        {
            result->delta_time_location   = delta_time_uniform_descriptor->location;
            result->delta_x_location      = delta_x_uniform_descriptor->location;
            result->node_indexes_location = node_indexes_uniform_descriptor->location;
            result->should_round_location = should_round_uniform_descriptor->location;
            result->start_time_location   = start_time_uniform_descriptor->location;
            result->start_x_location      = start_x_uniform_descriptor->location;
            result->val_range_location    = val_range_uniform_descriptor->location;
        }

        /* Configure uniform block binding */
        ogl_context_request_callback_from_context_thread(context,
                                                         _curve_editor_program_tcb_create_renderer_callback,
                                                         result);
        
        /* Add to the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _curve_editor_program_tcb_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_TCB,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (TCB)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_tcb) result;

end:
    if (result != NULL)
    {
        curve_editor_program_tcb_release( (curve_editor_program_tcb&) result);
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_delta_time_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->delta_time_location;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_delta_x_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->delta_x_location;
}

/** Please see header for specification */
PUBLIC GLuint curve_editor_program_tcb_get_id(__in __notnull curve_editor_program_tcb program)
{
    return ogl_program_get_id( ((_curve_editor_program_tcb*) program)->program);
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_node_indexes_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->node_indexes_location;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_should_round_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->should_round_location;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_start_time_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->start_time_location;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_start_x_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->start_x_location;
}

/** Please see header for specification */
PUBLIC GLint curve_editor_program_tcb_get_val_range_uniform_location(__in __notnull curve_editor_program_tcb program)
{
    return ((_curve_editor_program_tcb*) program)->val_range_location;
}
