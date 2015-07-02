/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_vertex_fullscreen.h"
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
} _shaders_vertex_fullscreen;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_fullscreen,
                               shaders_vertex_fullscreen,
                              _shaders_vertex_fullscreen);


/** Function called back when reference counter drops to zero. Releases the 3x3 convolution shader object.
 *
 *  @param ptr Pointer to _shaders_convolution3x3 instance.
 **/
PRIVATE void _shaders_vertex_fullscreen_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_vertex_fullscreen* data_ptr = (_shaders_vertex_fullscreen*) ptr;

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_fullscreen shaders_vertex_fullscreen_create(__in __notnull ogl_context               context,
                                                                              __in           bool                      export_uv,
                                                                              __in __notnull system_hashed_ansi_string name)
{
    bool                        result        = false;
    _shaders_vertex_fullscreen* result_object = NULL;
    shaders_vertex_fullscreen   result_shader = NULL;
    system_hashed_ansi_string   shader_body   = NULL;
    ogl_shader                  vertex_shader = NULL;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << "#version 430 core\n"
                   "\n";

    if (export_uv)
    {
        body_stream << "out vec2 uv;\n"
                       "\n";
    }

    body_stream << "void main()\n"
                   "{\n"
                   "   gl_Position = vec4(gl_VertexID < 2 ? -1.0 : 1.0, (gl_VertexID == 0 || gl_VertexID == 3) ? -1.0 : 1.0, 0, 1);\n";

    if (export_uv)
    {
        body_stream << "   uv          = vec2(gl_VertexID < 2 ?  0.0 : 1.0, (gl_VertexID == 0 || gl_VertexID == 3) ? 0.0 : 1.0);\n";
    }

    body_stream << "}\n";

    /* Create the shader */
    vertex_shader = ogl_shader_create(context,
                                      SHADER_TYPE_VERTEX,
                                      name);

    ASSERT_DEBUG_SYNC(vertex_shader != NULL,
                      "ogl_shader_create() failed");

    if (vertex_shader == NULL)
    {
        LOG_ERROR("Could not create fullscreen vertex shader.");

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
        LOG_ERROR("Could not set fullscreen vertex shader body.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_vertex_fullscreen;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_vertex_fullscreen object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating fullscreen vertex shader object instance.");

        goto end;
    }

    result_object->body          = shader_body;
    result_object->vertex_shader = vertex_shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_vertex_fullscreen_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_FULLSCREEN,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Full-screen Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_fullscreen) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_vertex_fullscreen_get_shader(__in __notnull shaders_vertex_fullscreen shader)
{
    return ((_shaders_vertex_fullscreen*)shader)->vertex_shader;
}