/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_vertex_combinedmvp_ubo.h"
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
} _shaders_vertex_combinedmvp_ubo;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_combinedmvp_ubo,
                               shaders_vertex_combinedmvp_ubo,
                              _shaders_vertex_combinedmvp_ubo);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to _shaders_vertex_combinedmvp_ubo instance.
 **/
PRIVATE void _shaders_vertex_combinedmvp_ubo_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_vertex_combinedmvp_ubo* data_ptr = (_shaders_vertex_combinedmvp_ubo*) ptr;

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_ubo shaders_vertex_combinedmvp_ubo_create(__in __notnull ogl_context               context,
                                                                                        __in           uint32_t                  n_matrices,
                                                                                        __in __notnull system_hashed_ansi_string name)
{
    _shaders_vertex_combinedmvp_ubo* result_object = NULL;
    shaders_vertex_combinedmvp_ubo   result_shader = NULL;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << "#version 430 core\n"
                   "\n"
                   "layout(std140) uniform Matrices\n"
                   "{\n"
                   "    mat4 mvp[" << n_matrices << "];\n"
                   "};\n" 
                   "\n"
                   "in       vec4 in_position;\n"
                   "in       vec2 in_uv;\n"
                   "flat out int  instance_id;\n"
                   "out      vec2 uv;\n"
                   "flat out int  vertex_id;\n"
                   "\n"
                   "void main()\n"
                   "{\n"
                   "   gl_Position = mvp[gl_InstanceID] * in_position;\n"
                   "   instance_id = gl_InstanceID;\n"
                   "   vertex_id   = gl_VertexID;\n"
                   "   uv          = in_uv;\n"
                   "}\n";

    /* Create the shader */
    ogl_shader vertex_shader = ogl_shader_create(context,
                                                 SHADER_TYPE_VERTEX,
                                                 name);

    ASSERT_DEBUG_SYNC(vertex_shader != NULL,
                      "ogl_shader_create() failed");

    if (vertex_shader == NULL)
    {
        LOG_ERROR("Could not create UBO combinedmvp vertex shader.");

        goto end;
    }

    /* Set the shader's body */
    system_hashed_ansi_string shader_body = system_hashed_ansi_string_create(body_stream.str().c_str() );
    bool                      result      = ogl_shader_set_body             (vertex_shader,
                                                                             shader_body);

    ASSERT_DEBUG_SYNC(result,
                      "ogl_shader_set_body() failed");

    if (!result)
    {
        LOG_ERROR("Could not set UBO combinedmvp vertex shader body.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_vertex_combinedmvp_ubo;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_vertex_combinedmvp_ubo object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating combinedmvp UBO vertex shader object instance.");

        goto end;
    }

    result_object->body          = shader_body;
    result_object->vertex_shader = vertex_shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_vertex_combinedmvp_ubo_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_UBO,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Combined MVP UBO Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_combinedmvp_ubo) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_vertex_combinedmvp_ubo_get_shader(__in __notnull shaders_vertex_combinedmvp_ubo shader)
{
    return ((_shaders_vertex_combinedmvp_ubo*)shader)->vertex_shader;
}