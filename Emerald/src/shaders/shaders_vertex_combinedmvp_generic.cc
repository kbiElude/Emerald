/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_vertex_combinedmvp_generic.h"
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
} _shaders_vertex_combinedmvp_generic;

static const char* shader_body =
    "#version 430 core\n"
    "\n"
    "in  vec4 in_color;\n"
    "in  vec4 in_position;\n"
    "in  vec2 in_uv;\n"
    "out vec4 color;\n"
    "out vec2 uv;\n"
    "\n"
    "uniform dataVS\n"
    "{\n"
    "    mat4 mvp;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = mvp * in_position;\n"
    "   color       = in_color;\n"
    "   uv          = in_uv;\n"
    "}\n";

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_combinedmvp_generic,
                               shaders_vertex_combinedmvp_generic,
                              _shaders_vertex_combinedmvp_generic);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to _shaders_vertex_combinedmvp_generic instance.
 **/
PRIVATE void _shaders_vertex_combinedmvp_generic_release(void* ptr)
{
    _shaders_vertex_combinedmvp_generic* data_ptr = (_shaders_vertex_combinedmvp_generic*) ptr;

    if (data_ptr->shader != nullptr)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &data_ptr->shader);

        data_ptr->shader = nullptr;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_generic shaders_vertex_combinedmvp_generic_create(ral_context               context,
                                                                                                system_hashed_ansi_string name)
{
    bool                                 result            = false;
    _shaders_vertex_combinedmvp_generic* result_object_ptr = nullptr;
    ral_shader                           shader            = nullptr;

    /* Create the shader */
    system_hashed_ansi_string shader_body_has   (system_hashed_ansi_string_create(shader_body) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_VERTEX);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(result,
                          "Could not create a new RAL shader instance.");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body_has);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_vertex_combinedmvp_generic;

    if (result_object_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                          "Out of memory while instantiating _shaders_vertex_combinedmvp_generic object.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_vertex_combinedmvp_generic_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_GENERIC,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Combined MVP Generic Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_combinedmvp_generic) result_object_ptr;

end:
    if (shader != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &shader);

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
PUBLIC EMERALD_API ral_shader shaders_vertex_combinedmvp_generic_get_shader(shaders_vertex_combinedmvp_generic shader)
{
    return ((_shaders_vertex_combinedmvp_generic*)shader)->shader;
}