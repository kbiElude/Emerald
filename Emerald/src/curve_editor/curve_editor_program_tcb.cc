/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_tcb.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
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
    ogl_shader     fragment_shader;
    ogl_shader     vertex_shader;
    ogl_program    program;
    ogl_program_ub program_data_ub;
    GLuint         program_data_ub_bo_id;
    GLuint         program_data_ub_bo_size;
    GLuint         program_data_ub_bo_start_offset;

    GLuint delta_time_ub_offset;
    GLuint delta_x_ub_offset;
    GLuint node_indexes_ub_offset;
    GLuint should_round_ub_offset;
    GLuint start_time_ub_offset;
    GLuint start_x_ub_offset;
    GLuint val_range_ub_offset;

    REFCOUNT_INSERT_VARIABLES

} _curve_editor_program_tcb;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_editor_program_tcb,
                               curve_editor_program_tcb,
                              _curve_editor_program_tcb);


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
        result->fragment_shader         = NULL;
        result->program                 = NULL;
        result->program_data_ub         = NULL;
        result->vertex_shader           = NULL;

        /* Create vertex shader */
        std::stringstream fp_body_stream;
        std::stringstream vp_body_stream;

        fp_body_stream << "#version 420 core\n"
                          "\n"
                          "out vec4 color;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    color = vec4(1, 1, 0, 1);\n"
                          "}\n";

        vp_body_stream << "#version 420 core\n"
                          "\n"
                          "const int max_nodes = 64;\n"
                          "\n"
                          "layout(binding = 1) uniform data\n"
                          "{\n"
                          "    float start_x;\n"
                          "    float start_time;\n"
                          "    float delta_x;\n"
                          "    float delta_time;\n"
                          "    vec2  val_range;\n"
                          "    ivec4 node_indexes;\n"
                          "    bool  should_round;\n"
                          "};\n"
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
        result->program = ogl_program_create(context,
                                             name,
                                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

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

        if (result->fragment_shader == NULL ||
            result->vertex_shader   == NULL)
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

        b_result  = ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("delta_time"),
                                                         &delta_time_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("delta_x"),
                                                         &delta_x_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("node_indexes"),
                                                         &node_indexes_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("should_round"),
                                                         &should_round_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("start_time"),
                                                         &start_time_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("start_x"),
                                                         &start_x_uniform_descriptor);
        b_result &= ogl_program_get_uniform_by_name      (result->program,
                                                          system_hashed_ansi_string_create("val_range"),
                                                         &val_range_uniform_descriptor);
        b_result &= ogl_program_get_uniform_block_by_name(result->program,
                                                          system_hashed_ansi_string_create("data"),
                                                         &result->program_data_ub);

        ASSERT_DEBUG_SYNC(b_result,
                          "Could not retrieve uniform or uniform block descriptor(s).");

        if (b_result)
        {
            result->delta_time_ub_offset   = delta_time_uniform_descriptor->ub_offset;
            result->delta_x_ub_offset      = delta_x_uniform_descriptor->ub_offset;
            result->node_indexes_ub_offset = node_indexes_uniform_descriptor->ub_offset;
            result->should_round_ub_offset = should_round_uniform_descriptor->ub_offset;
            result->start_time_ub_offset   = start_time_uniform_descriptor->ub_offset;
            result->start_x_ub_offset      = start_x_uniform_descriptor->ub_offset;
            result->val_range_ub_offset    = val_range_uniform_descriptor->ub_offset;

            ASSERT_DEBUG_SYNC(result->delta_time_ub_offset   != -1 &&
                              result->delta_x_ub_offset      != -1 &&
                              result->node_indexes_ub_offset != -1 &&
                              result->should_round_ub_offset != -1 &&
                              result->start_time_ub_offset   != -1 &&
                              result->start_x_ub_offset      != -1 &&
                              result->val_range_ub_offset    != -1,
                              "At least one uniform has an UB offset of -1");

            ogl_program_ub_get_property(result->program_data_ub,
                                        OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                       &result->program_data_ub_bo_size);
            ogl_program_ub_get_property(result->program_data_ub,
                                        OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                       &result->program_data_ub_bo_id);
            ogl_program_ub_get_property(result->program_data_ub,
                                        OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                       &result->program_data_ub_bo_start_offset);
        }

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

/* Please see header for spec */
PUBLIC void curve_editor_program_tcb_set_property(__in __notnull curve_editor_program_tcb          tcb,
                                                  __in           curve_editor_program_tcb_property property,
                                                  __in __notnull const void*                       data)
{
    _curve_editor_program_tcb* tcb_ptr = (_curve_editor_program_tcb*) tcb;

    switch (property)
    {
        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_DELTA_TIME_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->delta_time_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) );

            break;
        }

        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_DELTA_X_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->delta_x_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) );

            break;
        }

        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_NODE_INDEXES_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->node_indexes_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(GLuint) * 4);

            break;
        }

        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_SHOULD_ROUND_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->should_round_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(GLboolean) );

            break;
        }

        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_START_TIME_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->start_time_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(GLfloat) );

            break;
        }

        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_START_X_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->start_x_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(GLfloat) );

            break;
        }

        case CURVE_EDITOR_PROGRAM_TCB_PROPERTY_VAL_RANGE_DATA:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(tcb_ptr->program_data_ub,
                                                        tcb_ptr->val_range_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(GLfloat) * 2);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_program_tcb_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void curve_editor_program_tcb_use(__in __notnull ogl_context              context,
                                         __in __notnull curve_editor_program_tcb tcb)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _curve_editor_program_tcb*        tcb_ptr      = (_curve_editor_program_tcb*) tcb;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_program_ub_sync(tcb_ptr->program_data_ub);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     1, /* index */
                                     tcb_ptr->program_data_ub_bo_id,
                                     tcb_ptr->program_data_ub_bo_start_offset,
                                     tcb_ptr->program_data_ub_bo_size);
    entry_points->pGLUseProgram     (ogl_program_get_id(tcb_ptr->program) );
}
