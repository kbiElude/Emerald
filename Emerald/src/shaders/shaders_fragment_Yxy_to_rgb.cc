/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_Yxy_to_rgb.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type definition */
typedef struct
{
    ogl_shader shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_Yxy_to_rgb;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_Yxy_to_rgb,
                               shaders_fragment_Yxy_to_rgb,
                              _shaders_fragment_Yxy_to_rgb);

/* Internal variables */
const char* yxy_to_rgb_shader_body = "#version 420 core\n"
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
PRIVATE void _shaders_fragment_Yxy_to_rgb_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_fragment_Yxy_to_rgb* data_ptr = (_shaders_fragment_Yxy_to_rgb*) ptr;

    if (data_ptr->shader != NULL)
    {
        ogl_shader_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_Yxy_to_rgb shaders_fragment_Yxy_to_rgb_create(__in __notnull ogl_context               context,
                                                                                  __in __notnull system_hashed_ansi_string name)
{
    _shaders_fragment_Yxy_to_rgb* result_object = NULL;
    shaders_fragment_Yxy_to_rgb   result_shader = NULL;

    /* Create the shader */
    ogl_shader shader = ogl_shader_create(context,
                                          SHADER_TYPE_FRAGMENT,
                                          name);

    ASSERT_DEBUG_SYNC(shader != NULL,
                      "Could not create a fragment shader.");

    if (shader == NULL)
    {
        LOG_ERROR("Could not create a fragment shader for Yxy=>RGB shader object.");

        goto end;
    }

    /* Attach body to the shader */
    if (!ogl_shader_set_body(shader,
                             system_hashed_ansi_string_create(yxy_to_rgb_shader_body)) )
    {
        LOG_ERROR("Could not set body of Yxy=>RGB fragment shader.");

        ASSERT_DEBUG_SYNC(false,
                          "");

        goto end;
    }
    
    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_Yxy_to_rgb;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_Yxy_to_rgb object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Yxy=>RGB shader object instance.");

        goto end;
    }

    result_object->shader = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_fragment_Yxy_to_rgb_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_YXY_TO_RGB,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Yxy to RGB Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_Yxy_to_rgb) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_fragment_Yxy_to_rgb_get_shader(__in __notnull shaders_fragment_Yxy_to_rgb shader)
{
    return (((_shaders_fragment_Yxy_to_rgb*)shader)->shader);
}
