/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_program_tcb.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_types.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
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
    ogl_program_ub program_data_ub;
    ral_buffer     program_data_ub_bo;
    GLuint         program_data_ub_bo_size;

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
    _curve_editor_program_tcb* program_ptr = (_curve_editor_program_tcb*) in;

    /* Release all objects */
    if (program_ptr->program != NULL)
    {
        ral_context_delete_objects(program_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                  &program_ptr->program);
    }
}


/** Please see header for specification */
PUBLIC curve_editor_program_tcb curve_editor_program_tcb_create(ral_context               context,
                                                                system_hashed_ansi_string name)
{
    _curve_editor_program_tcb* result_ptr = new (std::nothrow) _curve_editor_program_tcb;

    ASSERT_DEBUG_SYNC(result_ptr != NULL,
                      "Out of memroy while instantiating TCB program object.");

    if (result_ptr != NULL)
    {
        /* Reset the structure */
        result_ptr->context         = context;
        result_ptr->program         = NULL;
        result_ptr->program_data_ub = NULL;

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
        const ral_program_create_info program_create_info =
        {
            name
        };

        if (!ral_context_create_programs(context,
                                         1, /* n_create_info_items */
                                        &program_create_info,
                                        &result_ptr->program) )
        {
            ASSERT_DEBUG_SYNC(result_ptr->program != NULL,
                              "RAL program creation failed");

            goto end;
        }

        /* Create the shaders */
        const ral_shader_create_info shader_create_info_items[] =
        {
            {
                system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                        " FS"),
                RAL_SHADER_TYPE_FRAGMENT
            },
            {
                system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                        " VP"),
                RAL_SHADER_TYPE_VERTEX
            }
        };
        const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
        ral_shader     result_shaders[n_shader_create_info_items];

        if (!ral_context_create_shaders(context,
                                        n_shader_create_info_items,
                                        shader_create_info_items,
                                        result_shaders) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create tcb curve fragment / vertex shader.");

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
        if (!ral_program_attach_shader(result_ptr->program,
                                       result_shaders[0],
                                       false /* relink_needed */) ||
            !ral_program_attach_shader(result_ptr->program,
                                       result_shaders[1],
                                       true /* relink_needed */) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not attach shader(s) to lerp curve program.");

            goto end;
        }

        /* Release the shaders */
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   n_shader_create_info_items,
                                   result_shaders);

        /* Retrieve uniform locations */
        const ral_program_variable* delta_time_uniform_ptr   = NULL;
        const ral_program_variable* delta_x_uniform_ptr      = NULL;
        const ral_program_variable* node_indexes_uniform_ptr = NULL;
        const ral_program_variable* should_round_uniform_ptr = NULL;
        const ral_program_variable* start_time_uniform_ptr   = NULL;
        const ral_program_variable* start_x_uniform_ptr      = NULL;
        const ral_program_variable* val_range_uniform_ptr    = NULL;

        const raGL_program          program_raGL = ral_context_get_program_gl(context,
                                                                              result_ptr->program);

        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("delta_time"),
                                              &delta_time_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("delta_x"),
                                              &delta_x_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("node_indexes"),
                                              &node_indexes_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("should_round"),
                                              &should_round_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("start_time"),
                                              &start_time_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("start_x"),
                                              &start_x_uniform_ptr);
        raGL_program_get_uniform_by_name      (program_raGL,
                                                system_hashed_ansi_string_create("val_range"),
                                              &val_range_uniform_ptr);
        raGL_program_get_uniform_block_by_name(program_raGL,
                                               system_hashed_ansi_string_create("data"),
                                              &result_ptr->program_data_ub);

        result_ptr->delta_time_ub_offset   = delta_time_uniform_ptr->block_offset;
        result_ptr->delta_x_ub_offset      = delta_x_uniform_ptr->block_offset;
        result_ptr->node_indexes_ub_offset = node_indexes_uniform_ptr->block_offset;
        result_ptr->should_round_ub_offset = should_round_uniform_ptr->block_offset;
        result_ptr->start_time_ub_offset   = start_time_uniform_ptr->block_offset;
        result_ptr->start_x_ub_offset      = start_x_uniform_ptr->block_offset;
        result_ptr->val_range_ub_offset    = val_range_uniform_ptr->block_offset;

        ASSERT_DEBUG_SYNC(result_ptr->delta_time_ub_offset   != -1 &&
                          result_ptr->delta_x_ub_offset      != -1 &&
                          result_ptr->node_indexes_ub_offset != -1 &&
                          result_ptr->should_round_ub_offset != -1 &&
                          result_ptr->start_time_ub_offset   != -1 &&
                          result_ptr->start_x_ub_offset      != -1 &&
                          result_ptr->val_range_ub_offset    != -1,
                          "At least one uniform has an UB offset of -1");

        ogl_program_ub_get_property(result_ptr->program_data_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &result_ptr->program_data_ub_bo_size);
        ogl_program_ub_get_property(result_ptr->program_data_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &result_ptr->program_data_ub_bo);

        /* Add to the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                       _curve_editor_program_tcb_release,
                                                       OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_TCB,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Editor Programs (TCB)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (curve_editor_program_tcb) result_ptr;

end:
    if (result_ptr != NULL)
    {
        curve_editor_program_tcb_release( (curve_editor_program_tcb&) result_ptr);
    }

    return NULL;
}

/* Please see header for spec */
PUBLIC void curve_editor_program_tcb_set_property(curve_editor_program_tcb          tcb,
                                                  curve_editor_program_tcb_property property,
                                                  const void*                       data)
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
PUBLIC void curve_editor_program_tcb_use(ral_context              context,
                                         curve_editor_program_tcb tcb)
{
    const ogl_context_gl_entrypoints* entry_points_ptr                = NULL;
    GLuint                            program_data_ub_bo_id           = 0;
    raGL_buffer                       program_data_ub_bo_raGL         = NULL;
    uint32_t                          program_data_ub_bo_start_offset = -1;
    _curve_editor_program_tcb*        tcb_ptr                         = (_curve_editor_program_tcb*) tcb;

    const raGL_program program_raGL    = ral_context_get_program_gl(tcb_ptr->context,
                                                                    tcb_ptr->program);
    GLuint             program_raGL_id = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ogl_context_get_property(ral_context_get_gl_context(context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points_ptr);

    ogl_program_ub_sync(tcb_ptr->program_data_ub);

    program_data_ub_bo_raGL = ral_context_get_buffer_gl(tcb_ptr->context,
                                                        tcb_ptr->program_data_ub_bo);

    raGL_buffer_get_property(program_data_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_data_ub_bo_id);
    raGL_buffer_get_property(program_data_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_data_ub_bo_start_offset);

    entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         1, /* index */
                                         program_data_ub_bo_id,
                                         program_data_ub_bo_start_offset,
                                         tcb_ptr->program_data_ub_bo_size);
    entry_points_ptr->pGLUseProgram     (program_raGL_id);
}
