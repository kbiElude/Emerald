/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_texture2D_filmic.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* tex2D_fragment_filmic_shader_body_not_reverted = "#version 430 core\n"
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
                                                             "    vec3 tex = max(vec3(0.0), texture(tex, uv).xyz * vec3(exposure) - 0.004);\n"
                                                             "\n"
                                                             "    result = vec4((tex * (6.2 * tex + 0.5)) / (tex * (6.2 * tex + 1.7) + 0.06), 1);\n"
                                                             "}\n";

const char* tex2D_fragment_filmic_shader_body_reverted = "#version 430 core\n"
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
                                                         "    vec3 tex = max(vec3(0.0), texture(tex, vec2(uv.x, 1 - uv.y) ).xyz * vec3(exposure) - 0.004);\n"
                                                         "\n"
                                                         "    result = vec4((tex * (6.2 * tex + 0.5)) / (tex * (6.2 * tex + 1.7) + 0.06), 1);\n"
                                                         "}\n";

/** Internal type definition */
typedef struct
{
    ogl_shader shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_texture2D_filmic;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_texture2D_filmic,
                               shaders_fragment_texture2D_filmic,
                              _shaders_fragment_texture2D_filmic);


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_texture2D_filmic instance.
 **/
PRIVATE void _shaders_fragment_texture2D_filmic_release(void* ptr)
{
    _shaders_fragment_texture2D_filmic* data_ptr = (_shaders_fragment_texture2D_filmic*) ptr;

    if (data_ptr->shader != NULL)
    {
        ogl_shader_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_texture2D_filmic shaders_fragment_texture2D_filmic_create(ral_context               context,
                                                                                              bool                      should_revert_y,
                                                                                              system_hashed_ansi_string name)
{
    _shaders_fragment_texture2D_filmic* result_object = NULL;
    shaders_fragment_texture2D_filmic   result_shader = NULL;

    /* Create the shader */
    ogl_shader shader = ogl_shader_create(context,
                                          RAL_SHADER_TYPE_FRAGMENT,
                                          name);

    ASSERT_DEBUG_SYNC(shader != NULL,
                      "Could not create a fragment shader.");

    if (shader == NULL)
    {
        LOG_ERROR("Could not create a fragment shader for Filmic Texture2D shader object.");

        goto end;
    }

    /* Attach body to the shader */
    if (!ogl_shader_set_body(shader,
                             system_hashed_ansi_string_create(should_revert_y ? tex2D_fragment_filmic_shader_body_reverted
                                                                              : tex2D_fragment_filmic_shader_body_not_reverted) ))
    {
        LOG_ERROR("Could not set body of Filmic Texture2D fragment shader.");

        ASSERT_DEBUG_SYNC(false,
                          "");

        goto end;
    }
    
    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_texture2D_filmic;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_texture2D_filmic object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Filmic Texture2D object instance.");

        goto end;
    }

    result_object->shader = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_fragment_texture2D_filmic_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_FILMIC,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Texture2D Filmic Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_texture2D_filmic) result_object;

end:
    if (shader != NULL)
    {
        ogl_shader_release(shader);

        shader = NULL;
    }

    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_fragment_texture2D_filmic_get_shader(shaders_fragment_texture2D_filmic shader)
{
    return (((_shaders_fragment_texture2D_filmic*)shader)->shader);
}
