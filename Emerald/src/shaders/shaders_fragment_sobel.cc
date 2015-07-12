/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_convolution3x3.h"
#include "shaders/shaders_fragment_sobel.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type definition */
typedef struct
{
    shaders_fragment_convolution3x3 dx_shader;
    shaders_fragment_convolution3x3 dy_shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_sobel;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_sobel,
                               shaders_fragment_sobel,
                              _shaders_fragment_sobel);


/* Internal variables */
const float sobel_dx[9] = {-1,  0,  1,
                           -2,  0,  2,
                           -1,  0,  1};
                           
const float sobel_dy[9] = {-1, -2, -1,
                            0,  0,  0,
                            1,  2,  1};



/** Function called back when reference counter drops to zero. Releases the sobel shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_sobel instance.
 **/
PRIVATE void _shaders_fragment_sobel_release(void* ptr)
{
    _shaders_fragment_sobel* data_ptr = (_shaders_fragment_sobel*) ptr;

    if (data_ptr->dx_shader != NULL)
    {
        shaders_fragment_convolution3x3_release(data_ptr->dx_shader);

        data_ptr->dx_shader = NULL;
    }

    if (data_ptr->dy_shader != NULL)
    {
        shaders_fragment_convolution3x3_release(data_ptr->dy_shader);

        data_ptr->dy_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_sobel shaders_fragment_sobel_create(ogl_context               context,
                                                                        system_hashed_ansi_string name)
{
    shaders_fragment_convolution3x3 dy_shader     = NULL;
    _shaders_fragment_sobel*        result_object = NULL;
    shaders_fragment_sobel          result_shader = NULL;

    /* Create the shaders */
    shaders_fragment_convolution3x3 dx_shader = shaders_fragment_convolution3x3_create(context,
                                                                                       sobel_dx,
                                                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                                               " DX")
                                                                                      );

    ASSERT_DEBUG_SYNC(dx_shader != NULL,
                      "shaders_fragment_convolution3x3_create() failed");

    if (dx_shader == NULL)
    {
        LOG_ERROR("Could not create DX sobel object.");

        goto end;
    }

    dy_shader = shaders_fragment_convolution3x3_create(context,
                                                       sobel_dy,
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                               " DY")
                                                      );

    ASSERT_DEBUG_SYNC(dy_shader != NULL,
                      "shaders_fragment_convolution3x3_create() failed");

    if (dy_shader == NULL)
    {
        LOG_ERROR("Could not create DY sobel object.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_sobel;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_sobel object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Sobel object instance.");

        goto end;
    }

    result_object->dx_shader = dx_shader;
    result_object->dy_shader = dy_shader;

    /* Return the object */
    return (shaders_fragment_sobel) result_object;

end:
    if (dx_shader != NULL)
    {
        shaders_fragment_convolution3x3_release(dx_shader);

        dx_shader = NULL;
    }

    if (dy_shader != NULL)
    {
        shaders_fragment_convolution3x3_release(dy_shader);

        dy_shader = NULL;
    }

    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_fragment_sobel_get_dx_shader(shaders_fragment_sobel shader)
{
    return shaders_fragment_convolution3x3_get_shader(((_shaders_fragment_sobel*)shader)->dx_shader);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_fragment_sobel_get_dy_shader(shaders_fragment_sobel shader)
{
    return shaders_fragment_convolution3x3_get_shader(((_shaders_fragment_sobel*)shader)->dy_shader);
}