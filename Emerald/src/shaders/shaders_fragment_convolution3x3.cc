/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_convolution3x3.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type defintion */
typedef struct
{
    ral_context context;
    ral_shader  fragment_shader;
    float       mask[3*3]; 

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
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &data_ptr->fragment_shader);

        data_ptr->fragment_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_convolution3x3 shaders_fragment_convolution3x3_create(ral_context               context,
                                                                                          const float*              input_mask,
                                                                                          system_hashed_ansi_string name)
{
    ral_shader                        convolution_shader = NULL;
    bool                              result             = false;
    _shaders_fragment_convolution3x3* result_object_ptr  = NULL;

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
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(body_stream.str().c_str() ));
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_FRAGMENT);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info */
                                   &shader_create_info,
                                   &convolution_shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create 3x3 convolution fragment shader.");

        goto end;
    }

    ral_shader_set_property(convolution_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_fragment_convolution3x3;

    ASSERT_DEBUG_SYNC(result_object_ptr != NULL,
                      "Out of memory while instantiating _shaders_fragment_convolution3x3 object.");

    memcpy(result_object_ptr->mask,
           input_mask,
           sizeof(float) * 9);

    result_object_ptr->context         = context;
    result_object_ptr->fragment_shader = convolution_shader;

end:
    /* Return the object */
    return (shaders_fragment_convolution3x3) result_object_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_fragment_convolution3x3_get_shader(shaders_fragment_convolution3x3 shader)
{
    return ((_shaders_fragment_convolution3x3*)shader)->fragment_shader;
}