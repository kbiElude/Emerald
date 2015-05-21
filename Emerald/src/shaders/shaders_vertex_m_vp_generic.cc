/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_vertex_m_vp_generic.h"
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
} _shaders_vertex_m_vp_generic;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_m_vp_generic, shaders_vertex_m_vp_generic, _shaders_vertex_m_vp_generic);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to shaders_vertex_m_vp_generic instance.
 **/
PRIVATE void _shaders_vertex_m_vp_generic_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_vertex_m_vp_generic* data_ptr = (_shaders_vertex_m_vp_generic*) ptr;

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_m_vp_generic shaders_vertex_m_vp_generic_create(__in __notnull ogl_context context, __in __notnull system_hashed_ansi_string name)
{
    _shaders_vertex_m_vp_generic* result_object = NULL;
    shaders_vertex_m_vp_generic   result_shader = NULL;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << "#version 430 core\n"
                   "\n"
                   "in      vec4 in_position;\n"
                   "in      vec2 in_uv;\n"
                   "uniform mat4 model;\n"
                   "uniform mat4 vp;\n"
                   "out     vec2 uv;\n"
                   "\n"
                   "void main()\n"
                   "{\n"
                   "   gl_Position = model * vp * in_position;\n"
                   "   uv          = in_uv;\n"
                   "}\n";

    /* Create the shader */
    ogl_shader vertex_shader = ogl_shader_create(context, SHADER_TYPE_VERTEX, name);

    ASSERT_DEBUG_SYNC(vertex_shader != NULL, "ogl_shader_create() failed");
    if (vertex_shader == NULL)
    {
        LOG_ERROR("Could not create m+vp generic vertex shader.");

        goto end;
    }

    /* Set the shader's body */
    system_hashed_ansi_string shader_body = system_hashed_ansi_string_create(body_stream.str().c_str() );
    bool                      result      = ogl_shader_set_body(vertex_shader, shader_body);

    ASSERT_DEBUG_SYNC(result, "ogl_shader_set_body() failed");
    if (!result)
    {
        LOG_ERROR("Could not set m+vp generic vertex shader body.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_vertex_m_vp_generic;

    ASSERT_DEBUG_SYNC(result_object != NULL, "Out of memory while instantiating _shaders_vertex_m_vp_generic object.");
    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating m+vp generic vertex shader object instance.");

        goto end;
    }

    result_object->body          = shader_body;
    result_object->vertex_shader = vertex_shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object, 
                                                   _shaders_vertex_m_vp_generic_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_M_VP_GENERIC,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\M+VP Generic Vertex Shaders\\", system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_m_vp_generic) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_vertex_m_vp_generic_get_shader(__in __notnull shaders_vertex_m_vp_generic shader)
{
    return ((_shaders_vertex_m_vp_generic*)shader)->vertex_shader;
}