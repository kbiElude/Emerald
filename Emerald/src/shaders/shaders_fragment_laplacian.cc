/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_convolution3x3.h"
#include "shaders/shaders_fragment_laplacian.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type definition */
typedef struct
{
    shaders_fragment_convolution3x3 shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_laplacian;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_laplacian,
                               shaders_fragment_laplacian,
                              _shaders_fragment_laplacian);


/* Internal variables */
const float laplacian[9] = {1,  1, 1,
                            1, -8, 1,
                            1,  1, 1};
                           

/** Function called back when reference counter drops to zero. Releases the laplacian shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_sobel instance.
 **/
PRIVATE void _shaders_fragment_laplacian_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_fragment_laplacian* data_ptr = (_shaders_fragment_laplacian*) ptr;

    if (data_ptr->shader != NULL)
    {
        shaders_fragment_convolution3x3_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_laplacian shaders_fragment_laplacian_create(__in __notnull ogl_context               context,
                                                                                __in __notnull system_hashed_ansi_string name)
{
    _shaders_fragment_laplacian* result_object = NULL;
    shaders_fragment_laplacian   result_shader = NULL;

    /* Create the shader */
    shaders_fragment_convolution3x3 shader = shaders_fragment_convolution3x3_create(context,
                                                                                    laplacian,
                                                                                    name);

    ASSERT_DEBUG_SYNC(shader != NULL,
                      "shaders_fragment_convolution3x3_create() failed");

    if (shader == NULL)
    {
        LOG_ERROR("Could not create shader object.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_laplacian;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_laplacian object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Laplacian object instance.");

        goto end;
    }

    result_object->shader = shader;

    /* Return the object */
    return (shaders_fragment_laplacian) result_object;

end:
    if (shader != NULL)
    {
        shaders_fragment_convolution3x3_release(shader);

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
PUBLIC EMERALD_API ogl_shader shaders_fragment_laplacian_get_shader(__in __notnull shaders_fragment_laplacian shader)
{
    return shaders_fragment_convolution3x3_get_shader(((_shaders_fragment_laplacian*)shader)->shader);
}
