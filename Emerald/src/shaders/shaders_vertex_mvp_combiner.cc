/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_vertex_mvp_combiner.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type defintiion */
typedef struct
{
    system_hashed_ansi_string body;
    ogl_shader                vertex_shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_vertex_mvp_combiner;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_mvp_combiner, shaders_vertex_mvp_combiner, _shaders_vertex_mvp_combiner);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to shaders_vertex_mvp_combiner instance.
 **/
PRIVATE void _shaders_vertex_mvp_combiner_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_vertex_mvp_combiner* data_ptr = (_shaders_vertex_mvp_combiner*) ptr;

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_mvp_combiner shaders_vertex_mvp_combiner_create(__in __notnull ogl_context context, __in __notnull system_hashed_ansi_string name)
{
    _shaders_vertex_mvp_combiner* result_object = NULL;
    shaders_vertex_mvp_combiner   result_shader = NULL;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << "#version 430 core\n"
                   "\n"
                   "in vec3 in_color;\n"
                   "in vec4 in_translation;\n"
                   "in vec4 in_rotation; /* x, y, z, angle */ \n"
                   "in vec3 in_scale;\n"
                   "\n"
                   "uniform mat4 vp;\n"
                   "\n"
                   "out result\n"
                   "{\n"
                   "    mat4 mvp;\n"
                   "} Out;\n"
                   "\n"
                   "void main()\n"
                   "{\n"
                   "    /* Translate */\n"
                   "    mat4 translation_matrix=mat4(vec4(1, 0, 0, 0) ,\n"
                   "                                 vec4(0, 1, 0, 0) ,\n"
                   "                                 vec4(0, 0, 1, 0) ,\n"
                   "                                 vec4(in_translation.x, in_translation.y, in_translation.z, in_translation.w) );\n"
                   "\n"
                   "    /* Scale */\n"
                   "    mat4 scale_matrix = mat4(vec4(in_scale.x, 0,          0,          0), \n"
                   "                             vec4(0,          in_scale.y, 0,          0), \n"
                   "                             vec4(0,          0,          in_scale.z, 0), \n"
                   "                             vec4(0,          0,          0,          1) );\n"
                   "\n"
                   "    /* Rotate */\n"
                   "    float      cos_angle       = cos(in_rotation.w);\n"
                   "    float      sin_angle       = sin(in_rotation.w);\n"
                   "    float      inv_cos_angle   = 1.0 - cos_angle;\n"
                   "    float      inv_sin_angle   = 1.0 - sin_angle;\n"
                   "    float      x_pow_2         = pow(in_rotation.x, 2);\n"
                   "    float      y_pow_2         = pow(in_rotation.y, 2);\n"
                   "    float      z_pow_2         = pow(in_rotation.z, 2);\n"
                   "    float      x_y             = in_rotation.x * in_rotation.y;\n"
                   "    float      x_z             = in_rotation.x * in_rotation.z;\n"
                   "    float      y_z             = in_rotation.y * in_rotation.z;\n"
                   "\n"
                   "    mat4 rotation_matrix = mat4(vec4(x_pow_2 + (1 - x_pow_2) * cos_angle,             inv_cos_angle * x_y + in_rotation.z * sin_angle, inv_cos_angle * x_z - in_rotation.y * sin_angle, 0), \n"
                   "                                vec4(inv_cos_angle * x_y - in_rotation.z * sin_angle, y_pow_2 + (1 - y_pow_2) * cos_angle,             inv_cos_angle * y_z + in_rotation.x * sin_angle, 0), \n"
                   "                                vec4(inv_cos_angle * x_z + in_rotation.y * sin_angle, inv_cos_angle * y_z - in_rotation.x * sin_angle, z_pow_2 + (1 - z_pow_2) * cos_angle,             0), \n"
                   "                                vec4(0,                                               0,                                               0,                                               1) );\n"
                   "\n"
                   "   Out.mvp = vp * rotation_matrix * translation_matrix * scale_matrix;\n"
                   "}\n";

    /* Create the shader */
    ogl_shader vertex_shader = ogl_shader_create(context, SHADER_TYPE_VERTEX, name);

    ASSERT_DEBUG_SYNC(vertex_shader != NULL, "ogl_shader_create() failed");
    if (vertex_shader == NULL)
    {
        LOG_ERROR("Could not create mvp combiner vertex shader.");

        goto end;
    }

    /* Set the shader's body */
    system_hashed_ansi_string shader_body = system_hashed_ansi_string_create(body_stream.str().c_str() );
    bool                      result      = ogl_shader_set_body(vertex_shader, shader_body);

    ASSERT_DEBUG_SYNC(result, "ogl_shader_set_body() failed");
    if (!result)
    {
        LOG_ERROR("Could not set mvp combiner vertex shader body.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_vertex_mvp_combiner;

    ASSERT_DEBUG_SYNC(result_object != NULL, "Out of memory while instantiating _shaders_vertex_mvp_combiner object.");
    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating mvp combiner vertex shader object instance.");

        goto end;
    }

    result_object->body          = shader_body;
    result_object->vertex_shader = vertex_shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object, 
                                                   _shaders_vertex_mvp_combiner_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_MVP_COMBINER,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\MVP Combiner Vertex Shaders\\", system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_mvp_combiner) result_object;

end:
    if (vertex_shader != NULL)
    {
        ogl_shader_release(vertex_shader);

        vertex_shader = NULL;
    }

    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_vertex_mvp_combiner_get_shader(__in __notnull shaders_vertex_mvp_combiner shader)
{
    return ((_shaders_vertex_mvp_combiner*)shader)->vertex_shader;
}