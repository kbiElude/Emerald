/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_convolution3x3.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type defintiion */
typedef struct
{
    system_hashed_ansi_string body;
    ogl_shader                fragment_shader;
    float                     mask[3*3]; 

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_convolution3x3;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_convolution3x3,
                               shaders_fragment_convolution3x3,
                              _shaders_fragment_convolution3x3);


/* Internal variables */
const char* convolution3x3_fragment_shader_body_prefix = "#version 430 core\n"
                                                         "\n"
                                                         "in vec2 uv;\n"
                                                         "\n"
                                                         "uniform sampler2D texture;\n"
                                                         "\n"
                                                         "out vec4 color;\n"
                                                         "\n"
                                                         "void main()\n"
                                                         "{\n"
                                                         "    color = ";

const char* convolution3x3_fragment_shader_body_suffix = ";\n"
                                                         "}\n";


/** Function called back when reference counter drops to zero. Releases the 3x3 convolution shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_convolution3x3 instance.
 **/
PRIVATE void _shaders_fragment_convolution3x3_release(void* ptr)
{
    _shaders_fragment_convolution3x3* data_ptr = (_shaders_fragment_convolution3x3*) ptr;

    if (data_ptr->fragment_shader != NULL)
    {
        ogl_shader_release(data_ptr->fragment_shader);

        data_ptr->fragment_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_convolution3x3 shaders_fragment_convolution3x3_create(ral_context               context,
                                                                                          const float*              input_mask,
                                                                                          system_hashed_ansi_string name)
{
    ogl_shader                        convolution_shader = NULL;
    bool                              result             = false;
    _shaders_fragment_convolution3x3* result_object      = NULL;
    shaders_fragment_convolution3x3   result_shader      = NULL;
    system_hashed_ansi_string         shader_body        = NULL;

    /* Create the body */
    std::stringstream body_stream;

    body_stream << convolution3x3_fragment_shader_body_prefix;

    for (unsigned int n = 0;
                      n < 3 * 3;
                    ++n)
    {
        int dx = ((n % 3) - 1);
        int dy = ((n / 3) - 1);

        if (n != 0)
        {
            body_stream << " + ";
        }

        body_stream << "textureOffset(texture, uv, ivec2(" << dx << ", " << dy << ")) * " << input_mask[n];
    }

    body_stream << convolution3x3_fragment_shader_body_suffix;

    /* Create the shader */
    convolution_shader = ogl_shader_create(context,
                                           RAL_SHADER_TYPE_FRAGMENT,
                                           name);

    ASSERT_DEBUG_SYNC(convolution_shader != NULL,
                      "ogl_shader_create() failed");

    if (convolution_shader == NULL)
    {
        LOG_ERROR("Could not create 3x3 convolution fragment shader.");

        goto end;
    }

    /* Set the shader's body */
    shader_body = system_hashed_ansi_string_create(body_stream.str().c_str() );
    result      = ogl_shader_set_body             (convolution_shader,
                                                   shader_body);

    ASSERT_DEBUG_SYNC(result,
                      "ogl_shader_set_body() failed");

    if (!result)
    {
        LOG_ERROR("Could not set 3x3 convolution fragment shader body.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_convolution3x3;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_convolution3x3 object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating convolution3x3 object instance.");

        goto end;
    }

    memcpy(result_object->mask,
           input_mask,
           sizeof(float) * 9);

    result_object->body            = shader_body;
    result_object->fragment_shader = convolution_shader;

    /* Return the object */
    return (shaders_fragment_convolution3x3) result_object;

end:
    if (convolution_shader != NULL)
    {
        ogl_shader_release(convolution_shader);

        convolution_shader = NULL;
    }

    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_fragment_convolution3x3_get_shader(shaders_fragment_convolution3x3 shader)
{
    return ((_shaders_fragment_convolution3x3*)shader)->fragment_shader;
}