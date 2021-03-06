/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_static.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "layout(binding = 0, std140) uniform dataFS\n"
    "{\n"
    "    vec4 color;\n"
    "};\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = color;\n"
    "}\n";

/** Internal type definition */
typedef struct
{
    ral_context context;
    ral_shader  shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_static;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_static,
                               shaders_fragment_static,
                              _shaders_fragment_static);


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_static instance.
 **/
PRIVATE void _shaders_fragment_static_release(void* ptr)
{
    _shaders_fragment_static* data_ptr = reinterpret_cast<_shaders_fragment_static*>(ptr);

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
PUBLIC EMERALD_API shaders_fragment_static shaders_fragment_static_create(ral_context               context,
                                                                          system_hashed_ansi_string name)
{
    _shaders_fragment_static* result_object_ptr = nullptr;
    ral_shader                shader            = nullptr;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(fragment_shader_body) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_FRAGMENT);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a RAL shader instance");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_fragment_static;

    ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                      "Out of memory while instantiating _shaders_fragment_static object.");

    if (result_object_ptr == nullptr)
    {
        LOG_ERROR("Out of memory while creating Static object instance.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_fragment_static_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_STATIC,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Static Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return reinterpret_cast<shaders_fragment_static>(result_object_ptr);

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
PUBLIC EMERALD_API ral_shader shaders_fragment_static_get_shader(shaders_fragment_static shader)
{
    return (((_shaders_fragment_static*)shader)->shader);
}
