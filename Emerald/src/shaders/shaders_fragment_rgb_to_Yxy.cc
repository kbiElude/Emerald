/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_rgb_to_Yxy.h"
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
} _shaders_fragment_rgb_to_Yxy;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_rgb_to_Yxy,
                               shaders_fragment_rgb_to_Yxy,
                              _shaders_fragment_rgb_to_Yxy);

/* Internal variables */
const char* shader_body_wo_log =
    "#version 430 core\n"
    "\n"
    "in vec2 uv;\n"
    "\n"
    "uniform sampler2D tex;\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    /* sRGB=>Yxy */\n"
    "    const vec3 X_vector = vec3(0.4125, 0.3576, 0.1805);\n"
    "    const vec3 Y_vector = vec3(0.2126, 0.7152, 0.0722);\n"
    "    const vec3 Z_vector = vec3(0.0193, 0.1192, 0.9505);\n"
    "\n"
    "    vec4 input = texture2D(tex, uv);\n"
    "\n"
    "    float X = dot(X_vector, input.xyz);\n"
    "    float Y = dot(Y_vector, input.xyz);\n"
    "    float Z = dot(Z_vector, input.xyz);\n"
    "\n"
    "    result = vec4(Y, X / (X + Y + Z) , Y / (X + Y + Z), input.w);\n"
    "}\n";

const char* shader_body_w_log =
    "#version 430 core\n"
    "\n"
    "in vec2 uv;\n"
    "\n"
    "uniform sampler2D tex;\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    /* sRGB=>Yxy */\n"
    "    const vec3 R_vector = vec3(0.4125, 0.3576, 0.1805);\n"
    "    const vec3 G_vector = vec3(0.2126, 0.7152, 0.0722);\n"
    "    const vec3 B_vector = vec3(0.0193, 0.1192, 0.9505);\n"
    "\n"
    "    vec4 input = texture2D(tex, uv);\n"
    "\n"
    "    vec3  XYZ     = vec3(dot(R_vector, input.xyz), dot(G_vector, input.xyz), dot(B_vector, input.xyz) );\n"
    "    float dot_XYZ = dot(vec3(1.0), XYZ);\n"
    "\n"
    "    result = vec4(log(XYZ.y), XYZ.x / dot_XYZ , XYZ.y / dot_XYZ, input.w);\n"
    "}\n";


/** Function called back when reference counter drops to zero. Releases the sobel shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_sobel instance.
 **/
PRIVATE void _shaders_fragment_rgb_to_Yxy_release(void* ptr)
{
    _shaders_fragment_rgb_to_Yxy* data_ptr = reinterpret_cast<_shaders_fragment_rgb_to_Yxy*>(ptr);

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
PUBLIC EMERALD_API shaders_fragment_rgb_to_Yxy shaders_fragment_rgb_to_Yxy_create(ral_context               context,
                                                                                  system_hashed_ansi_string name,
                                                                                  bool                      convert_to_log_Yxy)
{
    _shaders_fragment_rgb_to_Yxy* result_object_ptr = nullptr;
    ral_shader                    shader            = nullptr;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(convert_to_log_Yxy ? shader_body_w_log : shader_body_wo_log) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_FRAGMENT);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_fragment_rgb_to_Yxy;

    ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                      "Out of memory while instantiating _shaders_fragment_rgb_to_Yxy object.");

    if (result_object_ptr == nullptr)
    {
        LOG_ERROR("Out of memory while creating RGB=>Yxy shader object instance.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_fragment_rgb_to_Yxy_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_RGB_TO_YXY,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\RGB to Yxy Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return reinterpret_cast<shaders_fragment_rgb_to_Yxy>(result_object_ptr);

end:
    if (shader != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&shader) );

        shader = nullptr;
    }

    return nullptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_fragment_rgb_to_Yxy_get_shader(shaders_fragment_rgb_to_Yxy shader)
{
    return (((_shaders_fragment_rgb_to_Yxy*)shader)->shader);
}
