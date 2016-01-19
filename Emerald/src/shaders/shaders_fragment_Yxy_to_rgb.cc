/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_Yxy_to_rgb.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type definition */
typedef struct
{
    ral_context context;
    ral_shader  shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_Yxy_to_rgb;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_Yxy_to_rgb,
                               shaders_fragment_Yxy_to_rgb,
                              _shaders_fragment_Yxy_to_rgb);

/* Internal variables */
const char* yxy_to_rgb_shader_body = "#version 430 core\n"
                                     "\n"
                                     "in vec2 uv;\n"
                                     "\n"
                                     "uniform sampler2D tex;\n"
                                     "\n"
                                     "out vec4 result;\n"
                                     "\n"
                                     "void main()\n"
                                     "{\n"
                                     "    const float R_vector[3] = { 3.2405f, -1.5371f, -0.4985f};\n"
                                     "    const float G_vector[3] = {-0.9693f,  1.8760f,  0.0416f};\n"
                                     "    const float B_vector[3] = { 0.0556f, -0.2040f,  1.0572f};\n"
                                     "\n"
                                     "    vec4 in = texture2D(tex, uv);\n"
                                     "\n"
                                     "    float X = in.y * (in.x / in.z);\n"
                                     "    float Y = in.x;\n"
                                     "    float Z = (1 - in.y - in.z) * (in.x / in.z);\n"
                                     "\n"
                                     "    result = vec4(dot(R_vector, in.xyz), dot(G_vector, in.xyz), dot(B_vector, in.xyz), in.w);\n"
                                     "}\n";


/** Function called back when reference counter drops to zero. Releases the sobel shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_sobel instance.
 **/
PRIVATE void _shaders_fragment_Yxy_to_rgb_release(void* ptr)
{
    _shaders_fragment_Yxy_to_rgb* data_ptr = (_shaders_fragment_Yxy_to_rgb*) ptr;

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
PUBLIC EMERALD_API shaders_fragment_Yxy_to_rgb shaders_fragment_Yxy_to_rgb_create(ral_context               context,
                                                                                  system_hashed_ansi_string name)
{
    _shaders_fragment_Yxy_to_rgb* result_object_ptr = NULL;
    ral_shader                    shader            = NULL;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(yxy_to_rgb_shader_body) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_FRAGMENT);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a new RAL shader instance");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_fragment_Yxy_to_rgb;

    if (result_object_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(result_object_ptr != NULL,
                          "Out of memory while instantiating _shaders_fragment_Yxy_to_rgb object.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_fragment_Yxy_to_rgb_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_YXY_TO_RGB,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Yxy to RGB Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_Yxy_to_rgb) result_object_ptr;

end:
    if (shader != NULL)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                  &shader);

        shader = NULL;
    }

    if (result_object_ptr != NULL)
    {
        delete result_object_ptr;

        result_object_ptr = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_fragment_Yxy_to_rgb_get_shader(shaders_fragment_Yxy_to_rgb shader)
{
    return (((_shaders_fragment_Yxy_to_rgb*)shader)->shader);
}
