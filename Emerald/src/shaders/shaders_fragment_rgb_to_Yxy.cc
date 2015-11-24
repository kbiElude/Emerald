/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_rgb_to_Yxy.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type definition */
typedef struct
{
    ogl_shader shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_rgb_to_Yxy;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_rgb_to_Yxy,
                               shaders_fragment_rgb_to_Yxy,
                              _shaders_fragment_rgb_to_Yxy);

/* Internal variables */
const char* shader_body_wo_log = "#version 430 core\n"
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

const char* shader_body_w_log = "#version 430 core\n"
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
    _shaders_fragment_rgb_to_Yxy* data_ptr = (_shaders_fragment_rgb_to_Yxy*) ptr;

    if (data_ptr->shader != NULL)
    {
        ogl_shader_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_rgb_to_Yxy shaders_fragment_rgb_to_Yxy_create(ral_context               context,
                                                                                  system_hashed_ansi_string name,
                                                                                  bool                      convert_to_log_Yxy)
{
    _shaders_fragment_rgb_to_Yxy* result_object = NULL;
    shaders_fragment_rgb_to_Yxy   result_shader = NULL;

    /* Create the shader */
    ogl_shader shader = ogl_shader_create(context,
                                          RAL_SHADER_TYPE_FRAGMENT,
                                          name);

    ASSERT_DEBUG_SYNC(shader != NULL,
                      "Could not create a fragment shader.");

    if (shader == NULL)
    {
        LOG_ERROR("Could not create a fragment shader for RGB=>Yxy shader object.");

        goto end;
    }

    /* Attach body to the shader */
    if (!ogl_shader_set_body(shader,
                             system_hashed_ansi_string_create(convert_to_log_Yxy ? shader_body_w_log : shader_body_wo_log)) )
    {
        LOG_ERROR        ("Could not set body of RGB=>Yxy fragment shader.");
        ASSERT_DEBUG_SYNC(false,
                          "");

        goto end;
    }
    
    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_rgb_to_Yxy;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_rgb_to_yxy object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating RGB=>Yxy shader object instance.");

        goto end;
    }

    result_object->shader = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object, 
                                                   _shaders_fragment_rgb_to_Yxy_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_RGB_TO_YXY,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\RGB to Yxy Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_rgb_to_Yxy) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_fragment_rgb_to_Yxy_get_shader(shaders_fragment_rgb_to_Yxy shader)
{
    return (((_shaders_fragment_rgb_to_Yxy*)shader)->shader);
}
