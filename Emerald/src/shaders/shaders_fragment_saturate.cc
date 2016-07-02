/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_saturate.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* saturate_fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "in vec2 uv;\n"
    "\n"
    "uniform sampler2D in_texture;\n"
    "uniform float     strength;\n"
    "\n"
    "out vec4 color;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4  in_texel = texture(in_texture, uv);\n"
    "    float avg      = (in_texel.x + in_texel.y + in_texel.z) / 3.0;\n"
    "\n"
    "    color = vec4(avg + strength * (in_texel.rgb - avg), 1.0);\n"
    "}\n";

/** Internal type definition */
typedef struct
{
    ral_context context;
    ral_shader  shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_saturate;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_saturate,
                               shaders_fragment_saturate,
                              _shaders_fragment_saturate);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_saturate instance.
 **/
PRIVATE void _shaders_fragment_saturate_release(void* ptr)
{
    _shaders_fragment_saturate* data_ptr = reinterpret_cast<_shaders_fragment_saturate*>(ptr);

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
PUBLIC EMERALD_API shaders_fragment_saturate shaders_fragment_saturate_create(ral_context               context,
                                                                              system_hashed_ansi_string name)
{
    _shaders_fragment_saturate* result_object_ptr = nullptr;
    ral_shader                  shader            = nullptr;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(saturate_fragment_shader_body) );
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
    result_object_ptr = new (std::nothrow) _shaders_fragment_saturate;

    ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                      "Out of memory while instantiating _shaders_fragment_saturate object.");

    if (result_object_ptr == nullptr)
    {
        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    /* Return the object */
    return (shaders_fragment_saturate) result_object_ptr;

end:
    if (shader != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &shader);

        shader = nullptr;
    }

    return nullptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_fragment_saturate_get_shader(shaders_fragment_saturate shader)
{
    return (((_shaders_fragment_saturate*)shader)->shader);
}
