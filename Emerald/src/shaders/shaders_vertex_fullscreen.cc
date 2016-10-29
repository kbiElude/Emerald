/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type defintiion */
typedef struct
{
    ral_context context;
    ral_shader  shader;

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
PRIVATE void _shaders_vertex_fullscreen_release(void* ptr)
{
    _shaders_vertex_fullscreen* data_ptr = reinterpret_cast<_shaders_vertex_fullscreen*>(ptr);

    if (data_ptr->shader != nullptr)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&data_ptr->shader) );

        data_ptr->shader = nullptr;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_fullscreen shaders_vertex_fullscreen_create(ral_context               context,
                                                                              bool                      export_uv,
                                                                              system_hashed_ansi_string name)
{
    _shaders_vertex_fullscreen* result_object_ptr = nullptr;
    ral_shader                  shader            = nullptr;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << "#version 430 core\n"
                   "\n";

    if (export_uv)
    {
        body_stream << "layout(location = 0) out vec2 uv;\n"
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
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(body_stream.str().c_str() ));
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_VERTEX);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a new RAL shader instance.");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_vertex_fullscreen;

    if (result_object_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                          "Out of memory while instantiating _shaders_vertex_fullscreen object.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_vertex_fullscreen_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_FULLSCREEN,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Full-screen Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return reinterpret_cast<shaders_vertex_fullscreen>(result_object_ptr);

end:
    if (shader != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&shader) );

        shader = nullptr;
    }

    if (result_object_ptr != nullptr)
    {
        delete result_object_ptr;

        result_object_ptr = nullptr;
    }

    return nullptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_vertex_fullscreen_get_shader(shaders_vertex_fullscreen shader)
{
    return ((_shaders_vertex_fullscreen*)shader)->shader;
}