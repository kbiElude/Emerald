/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_saturate.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* saturate_fragment_shader_body = "#version 430 core\n"
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
    ogl_shader shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_saturate;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_saturate, shaders_fragment_saturate, _shaders_fragment_saturate);


/* Internal variables */
                          

/** Function called back when reference counter drops to zero. Releases the laplacian shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_sobel instance.
 **/
PRIVATE void _shaders_fragment_saturate_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_fragment_saturate* data_ptr = (_shaders_fragment_saturate*) ptr;

    if (data_ptr->shader != NULL)
    {
        ogl_shader_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_saturate shaders_fragment_saturate_create(__in __notnull ogl_context context, __in __notnull system_hashed_ansi_string name)
{
    _shaders_fragment_saturate* result_object = NULL;
    shaders_fragment_saturate   result_shader = NULL;

    /* Create the shader */
    ogl_shader shader = ogl_shader_create(context, SHADER_TYPE_FRAGMENT, name);

    ASSERT_DEBUG_SYNC(shader != NULL, "Could not create a fragment shader.");
    if (shader == NULL)
    {
        LOG_ERROR("Could not create a fragment shader for Saturate shader object.");

        goto end;
    }

    /* Attach body to the shader */
    if (!ogl_shader_set_body(shader, system_hashed_ansi_string_create(saturate_fragment_shader_body) ))
    {
        LOG_ERROR("Could not set body of saturation fragment shader.");
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }
    
    /* Compile the shader */
    /*
    if (!ogl_shader_compile(shader) )
    {
        LOG_ERROR("Could not compile saturation fragment shader.");
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }
    */

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_saturate;

    ASSERT_DEBUG_SYNC(result_object != NULL, "Out of memory while instantiating _shaders_fragment_saturate object.");
    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Laplacian object instance.");

        goto end;
    }

    result_object->shader = shader;

    /* Return the object */
    return (shaders_fragment_saturate) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_fragment_saturate_get_shader(__in __notnull shaders_fragment_saturate shader)
{
    return (((_shaders_fragment_saturate*)shader)->shader);
}
