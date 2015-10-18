/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_vertex_combinedmvp_simplified_twopoint.h"
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
} _shaders_vertex_combinedmvp_simplified_twopoint;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_combinedmvp_simplified_twopoint,
                               shaders_vertex_combinedmvp_simplified_twopoint,
                              _shaders_vertex_combinedmvp_simplified_twopoint);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to _shaders_vertex_combinedmvp instance.
 **/
PRIVATE void _shaders_vertex_combinedmvp_simplified_twopoint_release(void* ptr)
{
    _shaders_vertex_combinedmvp_simplified_twopoint* data_ptr = (_shaders_vertex_combinedmvp_simplified_twopoint*) ptr;

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_simplified_twopoint shaders_vertex_combinedmvp_simplified_twopoint_create(ogl_context               context,
                                                                                                                        system_hashed_ansi_string name)
{
    bool                                             result        = false;
    _shaders_vertex_combinedmvp_simplified_twopoint* result_object = NULL;
    shaders_vertex_combinedmvp_simplified_twopoint   result_shader = NULL;
    system_hashed_ansi_string                        shader_body   = NULL;
    ogl_shader                                       vertex_shader = NULL;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << "#version 430 core\n"
                   "\n"
                   "in vec2 in_uv;\n"
                   "\n"
                   "uniform dataVS\n"
                   "{\n"
                   "    vec4 a;\n"
                   "    vec4 b;\n"
                   "    mat4 mvp;\n"
                   "};\n"
                   "\n"
                   "out vec2 uv;\n"
                   "\n"
                   "void main()\n"
                   "{\n"
                   "   gl_Position = mvp * (gl_VertexID == 0 ? a : b);\n"
                   "   uv          = in_uv;\n"
                   "}\n";

    /* Create the shader */
    vertex_shader = ogl_shader_create(context,
                                      RAL_SHADER_TYPE_VERTEX,
                                      name);

    ASSERT_DEBUG_SYNC(vertex_shader != NULL,
                      "ogl_shader_create() failed");

    if (vertex_shader == NULL)
    {
        LOG_ERROR("Could not create combinedmvp vertex shader.");

        goto end;
    }

    /* Set the shader's body */
    shader_body = system_hashed_ansi_string_create(body_stream.str().c_str() );
    result      = ogl_shader_set_body             (vertex_shader,
                                                   shader_body);

    ASSERT_DEBUG_SYNC(result,
                      "ogl_shader_set_body() failed");

    if (!result)
    {
        LOG_ERROR("Could not set combinedmvp vertex shader body.");

        goto end;
    }

    /* Instantiate the object */
    result_object = new (std::nothrow) _shaders_vertex_combinedmvp_simplified_twopoint;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_vertex_combinedmvp_simplified_twopoint object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating combinedmvp vertex shader object instance.");

        goto end;
    }

    result_object->body          = shader_body;
    result_object->vertex_shader = vertex_shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_vertex_combinedmvp_simplified_twopoint_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_SIMPLIFIED_TWO_POINT,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Combined MVP Simplified 2-point Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_combinedmvp_simplified_twopoint) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_vertex_combinedmvp_simplified_twopoint_get_shader(shaders_vertex_combinedmvp_simplified_twopoint shader)
{
    return ((_shaders_vertex_combinedmvp_simplified_twopoint*)shader)->vertex_shader;
}