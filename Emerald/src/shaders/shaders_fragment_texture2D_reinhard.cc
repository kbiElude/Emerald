/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_texture2D_reinhard.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>


/* Internal variables */
const char* tex2D_fragment_reinhardt_shader_body_not_reverted =
    "#version 430 core\n"
    "\n"
    "in vec2 uv;\n"
    "\n"
    "uniform data\n"
    "{\n"
    "    float exposure;\n"
    "};\n"
    "\n"
    "uniform sampler2D tex;\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec3 tex = texture(tex, uv).xyz * vec3(exposure);\n"
    "\n"
    "    result = vec4(pow(tex / (1 + tex), vec3(1/2.2) ), 1);\n"
    "}\n";

const char* tex2D_fragment_reinhardt_shader_body_reverted =
    "#version 430 core\n"
    "\n"
    "in vec2 uv;\n"
    "\n"
    "uniform data\n"
    "{\n"
    "    float exposure;\n"
    "};\n"
    "\n"
    "uniform sampler2D tex;\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec3 tex = texture(tex, vec2(uv.x, 1-uv.y)).xyz * vec3(exposure);\n"
    "\n"
    "    result = vec4(pow(tex / (1 + tex), vec3(1/2.2) ), 1);\n"
    "}\n";

/** Internal type definition */
typedef struct
{
    ral_context context;
    ral_shader  shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_texture2D_reinhardt;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_texture2D_reinhardt,
                               shaders_fragment_texture2D_reinhardt,
                               _shaders_fragment_texture2D_reinhardt);


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_texture2D_reinhardt instance.
 **/
PRIVATE void _shaders_fragment_texture2D_reinhardt_release(void* ptr)
{
    _shaders_fragment_texture2D_reinhardt* data_ptr = reinterpret_cast<_shaders_fragment_texture2D_reinhardt*>(ptr);

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
PUBLIC EMERALD_API shaders_fragment_texture2D_reinhardt shaders_fragment_texture2D_reinhardt_create(ral_context               context,
                                                                                                    bool                      should_revert_y,
                                                                                                    system_hashed_ansi_string name)
{
    _shaders_fragment_texture2D_reinhardt* result_object_ptr = nullptr;
    ral_shader                             shader            = nullptr;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(should_revert_y ? tex2D_fragment_reinhardt_shader_body_reverted
                                                                                                  : tex2D_fragment_reinhardt_shader_body_not_reverted) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_FRAGMENT);

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
    result_object_ptr = new (std::nothrow) _shaders_fragment_texture2D_reinhardt;

    if (result_object_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                          "Out of memory while instantiating _shaders_fragment_texture2D_reinhardt object.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_fragment_texture2D_reinhardt_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_REINHARDT,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Texture2D Reinhardt Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_texture2D_reinhardt) result_object_ptr;

end:
    if (shader != nullptr)
    {
        ral_context_delete_objects(result_object_ptr->context,
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
PUBLIC EMERALD_API ral_shader shaders_fragment_texture2D_reinhardt_get_shader(shaders_fragment_texture2D_reinhardt shader)
{
    return (((_shaders_fragment_texture2D_reinhardt*)shader)->shader);
}
