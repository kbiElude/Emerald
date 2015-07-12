/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_static.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* fragment_shader_body = "#version 430 core\n"
                                   "\n"
                                   "uniform dataFS\n"
                                   "{\n"
                                   "    vec4 color;\n"
                                   "};\n"
                                   "\n"
                                   "out vec4 result;\n"
                                   "\n"
                                   "void main()\n"
                                   "{\n"
                                   "    result = color;\n"
                                   "}\n";

/** Internal type definition */
typedef struct
{
    ogl_shader shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_static;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_static,
                               shaders_fragment_static,
                              _shaders_fragment_static);


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_static instance.
 **/
PRIVATE void _shaders_fragment_static_release(void* ptr)
{
    _shaders_fragment_static* data_ptr = (_shaders_fragment_static*) ptr;

    if (data_ptr->shader != NULL)
    {
        ogl_shader_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_static shaders_fragment_static_create(ogl_context               context,
                                                                          system_hashed_ansi_string name)
{
    _shaders_fragment_static* result_object = NULL;
    shaders_fragment_static   result_shader = NULL;

    /* Create the shader */
    ogl_shader shader = ogl_shader_create(context,
                                          SHADER_TYPE_FRAGMENT,
                                          name);

    ASSERT_DEBUG_SYNC(shader != NULL,
                      "Could not create a fragment shader.");

    if (shader == NULL)
    {
        LOG_ERROR("Could not create a fragment shader for Static shader object.");

        goto end;
    }

    /* Attach body to the shader */
    if (!ogl_shader_set_body(shader,
                             system_hashed_ansi_string_create(fragment_shader_body)) )
    {
        LOG_ERROR("Could not set body of static fragment shader.");

        ASSERT_DEBUG_SYNC(false,
                          "");

        goto end;
    }
    
    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_static;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_static object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Static object instance.");

        goto end;
    }

    result_object->shader = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_fragment_static_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_STATIC,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Static Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_static) result_object;

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
PUBLIC EMERALD_API ogl_shader shaders_fragment_static_get_shader(shaders_fragment_static shader)
{
    return (((_shaders_fragment_static*)shader)->shader);
}
