/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shaders.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"

typedef struct _ogl_shaders
{
    system_hash64map shader_name_to_shader_map;

    _ogl_shaders()
    {
        shader_name_to_shader_map = NULL;
    }

    ~_ogl_shaders()
    {
        if (shader_name_to_shader_map != NULL)
        {
            /* No need to release the hosted ogl_shader instances */
            system_hash64map_release(shader_name_to_shader_map);

            shader_name_to_shader_map = NULL;
        } /* if (shader_name_to_shader_map != NULL) */
    }
} _ogl_shaders;

/** Please see header for spec */
PUBLIC ogl_shaders ogl_shaders_create()
{
    _ogl_shaders* new_shaders = new (std::nothrow) _ogl_shaders;

    ASSERT_DEBUG_SYNC(new_shaders != NULL,
                      "Out of memory");

    if (new_shaders != NULL)
    {
        new_shaders->shader_name_to_shader_map = system_hash64map_create(sizeof(ogl_shader),
                                                                         true); /* should_be_thread_safe */

        ASSERT_DEBUG_SYNC(new_shaders->shader_name_to_shader_map != NULL,
                          "Could not initialize a hash-map");
    }

    return (ogl_shaders) new_shaders;
}

/** Please see header for spec */
PUBLIC ogl_shader ogl_shaders_get_shader_by_name(ogl_shaders               shaders,
                                                 system_hashed_ansi_string shader_has)
{
    ogl_shader    result      = NULL;
    _ogl_shaders* shaders_ptr = (_ogl_shaders*) shaders;

    system_hash64map_get(shaders_ptr->shader_name_to_shader_map,
                         system_hashed_ansi_string_get_hash(shader_has),
                        &result);

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_shaders_release(ogl_shaders shaders)
{
    delete (_ogl_shaders*) shaders;

    shaders = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_shaders_register_shader(ogl_shaders shaders,
                                        ogl_shader  shader)
{
    system_hashed_ansi_string shader_name = ogl_shader_get_name               (shader);
    system_hash64             shader_hash = system_hashed_ansi_string_get_hash(shader_name);
    _ogl_shaders*             shaders_ptr = (_ogl_shaders*) shaders;

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(shaders_ptr->shader_name_to_shader_map,
                                                 shader_hash),
                      "About to register an already registered shader object!");

    system_hash64map_insert(shaders_ptr->shader_name_to_shader_map,
                            shader_hash,
                            shader,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */
}

/** Please see header for spec */
PUBLIC void ogl_shaders_unregister_shader(ogl_shaders shaders,
                                          ogl_shader  shader)
{
    system_hashed_ansi_string shader_name = ogl_shader_get_name               (shader);
    system_hash64             shader_hash = system_hashed_ansi_string_get_hash(shader_name);
    _ogl_shaders*             shaders_ptr = (_ogl_shaders*) shaders;

    ASSERT_DEBUG_SYNC(system_hash64map_contains(shaders_ptr->shader_name_to_shader_map,
                                                shader_hash),
                      "Cannot unregister a shader which has not been registered.");

    system_hash64map_remove(shaders_ptr->shader_name_to_shader_map,
                            shader_hash);
}
