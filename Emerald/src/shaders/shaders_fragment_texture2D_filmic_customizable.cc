/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_texture2D_filmic_customizable.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* tex2D_fragment_filmic_customizable_shader_body_not_reverted = "#version 430 core\n"
                                                                          "\n"
                                                                          "in vec2 uv;\n"
                                                                          "\n"
                                                                          "uniform data\n"
                                                                          "{\n"
                                                                          "    float a, b,c, d, e, f, w;\n"
                                                                          "    float exposure;\n"
                                                                          "    float exposure_bias;\n"
                                                                          "};\n"
                                                                          "\n"
                                                                          "uniform sampler2D tex;\n"
                                                                          "\n"
                                                                          "out vec4 result;\n"
                                                                          "\n"
                                                                          "vec3 tone_map(vec3 x)\n"
                                                                          "{\n"
                                                                          "   return ((x*(a*x + c*b) + d*e) / (x*(a*x + b) + d*f)) - e/f;\n"
                                                                          "}\n"
                                                                          "\n"
                                                                          "void main()\n"
                                                                          "{\n"
                                                                          "    vec3 tex = vec3(exposure) * texture(tex, uv).xyz;\n"
                                                                          "\n"
                                                                          "    vec3 current     =              tone_map(exposure_bias * tex);\n"
                                                                          "    vec3 white_scale = vec3(1.0f) / tone_map(vec3(w));\n"
                                                                          "\n"
                                                                          "    result = vec4(pow(current * white_scale, vec3(1.0/2.2)), 1);\n"
                                                                          "}\n";

const char* tex2D_fragment_filmic_customizable_shader_body_reverted = "#version 430 core\n"
                                                                      "\n"
                                                                      "in vec2 uv;\n"
                                                                      "\n"
                                                                      "uniform data\n"
                                                                      "{\n"
                                                                      "    float a, b,c, d, e, f, w;\n"
                                                                      "    float exposure;\n"
                                                                      "    float exposure_bias;\n"
                                                                      "};\n"
                                                                      "\n"
                                                                      "uniform sampler2D tex;\n"
                                                                      "\n"
                                                                      "out vec4 result;\n"
                                                                      "\n"
                                                                      "vec3 tone_map(vec3 x)\n"
                                                                      "{\n"
                                                                      "   return ((x*(a*x + c*b) + d*e) / (x*(a*x + b) + d*f)) - e/f;\n"
                                                                      "}\n"
                                                                      "\n"
                                                                      "void main()\n"
                                                                      "{\n"
                                                                      "    vec3 tex = vec3(exposure) * texture(tex, vec2(uv.x, 1-uv.y) ).xyz;\n"
                                                                      "\n"
                                                                      "    vec3 current     =              tone_map(exposure_bias * tex);\n"
                                                                      "    vec3 white_scale = vec3(1.0f) / tone_map(vec3(w));\n"
                                                                      "\n"
                                                                      "    result = vec4(pow(current * white_scale, vec3(1.0/2.2)), 1);\n"
                                                                      "}\n";

/** Internal type definition */
typedef struct
{
    ral_context context;
    ral_shader  shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_texture2D_filmic_customizable;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_texture2D_filmic_customizable,
                               shaders_fragment_texture2D_filmic_customizable,
                               _shaders_fragment_texture2D_filmic_customizable);


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_texture2D_filmic_customizable instance.
 **/
PRIVATE void _shaders_fragment_texture2D_filmic_customizable_release(void* ptr)
{
    _shaders_fragment_texture2D_filmic_customizable* data_ptr = (_shaders_fragment_texture2D_filmic_customizable*) ptr;

    if (data_ptr->shader != NULL)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                  &data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_texture2D_filmic_customizable shaders_fragment_texture2D_filmic_customizable_create(ral_context               context,
                                                                                                                        bool                      should_revert_y,
                                                                                                                        system_hashed_ansi_string name)
{
    _shaders_fragment_texture2D_filmic_customizable* result_object_ptr = NULL;
    ral_shader                                       shader            = NULL;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(should_revert_y ? tex2D_fragment_filmic_customizable_shader_body_reverted
                                                                                                  : tex2D_fragment_filmic_customizable_shader_body_not_reverted) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_FRAGMENT);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Error creating a new RAL shader instance.");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_fragment_texture2D_filmic_customizable;

    if (result_object_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(result_object_ptr != NULL,
                          "Out of memory while instantiating _shaders_fragment_texture2D_filmic_customizable object.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_fragment_texture2D_filmic_customizable_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Texture2D Customizable Filmic Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_texture2D_filmic_customizable) result_object_ptr;

end:
    if (shader != NULL)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                  &shader);
    }

    if (result_object_ptr != NULL)
    {
        delete result_object_ptr;

        result_object_ptr = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_fragment_texture2D_filmic_customizable_get_shader(shaders_fragment_texture2D_filmic_customizable shader)
{
    return (((_shaders_fragment_texture2D_filmic_customizable*)shader)->shader);
}
