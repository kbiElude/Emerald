/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_texture.h"
#include "system/system_log.h"
#include "system/system_hash64map.h"

/* Private type definitions */
typedef struct _ogl_textures
{
    ogl_context      context;
    system_hash64map textures_by_filename;
    system_hash64map textures_by_name;

    _ogl_textures()
    {
        context              = NULL;
        textures_by_filename = system_hash64map_create(sizeof(ogl_texture) );
        textures_by_name     = system_hash64map_create(sizeof(ogl_texture) );
    }

    ~_ogl_textures()
    {
        LOG_INFO("Texture manager deallocating..");

        if (textures_by_filename != NULL)
        {
            system_hash64map_release(textures_by_filename);

            textures_by_filename = NULL;
        }

        if (textures_by_name != NULL)
        {
            system_hash64 texture_hash = 0;
            ogl_texture   texture      = NULL;

            while (system_hash64map_get_element_at(textures_by_name,
                                                   0,
                                                  &texture,
                                                  &texture_hash) )
            {
                ogl_texture_release(texture);

                system_hash64map_remove(textures_by_name,
                                        texture_hash);
            }

            system_hash64map_release(textures_by_name);

            textures_by_name = NULL;
        }
    }
} _ogl_textures;


/** Please see header for specification */
PUBLIC void ogl_textures_add_texture(__in __notnull ogl_textures textures,
                                     __in __notnull ogl_texture  texture)
{
    system_hashed_ansi_string texture_name         = NULL;
    system_hashed_ansi_string texture_src_filename = NULL;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_NAME,
                            &texture_name);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_SRC_FILENAME,
                            &texture_src_filename);

    system_hash64  texture_name_hash = system_hashed_ansi_string_get_hash(texture_name);
    _ogl_textures* textures_ptr      = (_ogl_textures*) textures;

    /* Make sure the texture has not already been added */
    if (system_hash64map_contains(textures_ptr->textures_by_name,
                                  texture_name_hash) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Cannot add texture [%s]: texture already added",
                           system_hashed_ansi_string_get_buffer(texture_name) );

        goto end;
    }

    /* Retain the object and store it */
    ogl_texture_retain(texture);

    system_hash64map_insert(textures_ptr->textures_by_name,
                            texture_name_hash,
                            texture,
                            NULL,
                            NULL);

    if (texture_src_filename != NULL)
    {
        system_hash64 texture_src_filename_hash = system_hashed_ansi_string_get_hash(texture_src_filename);

        system_hash64map_insert(textures_ptr->textures_by_filename,
                                texture_src_filename_hash,
                                texture,
                                NULL,
                                NULL);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC ogl_textures ogl_textures_create(__in __notnull ogl_context context)
{
    _ogl_textures* textures_ptr = new (std::nothrow) _ogl_textures;

    ASSERT_ALWAYS_SYNC(textures_ptr != NULL, "Out of memory");
    if (textures_ptr != NULL)
    {
        textures_ptr->context = context;
    }

    return (ogl_textures) textures_ptr;
}

/** Please see header for specification */
PUBLIC void ogl_textures_delete_texture(__in __notnull ogl_textures              textures,
                                        __in __notnull system_hashed_ansi_string texture_name)
{
    _ogl_textures* textures_ptr = (_ogl_textures*) textures;

    if (textures != NULL)
    {
        if (!system_hash64map_remove(textures_ptr->textures_by_name,
                                     system_hashed_ansi_string_get_hash(texture_name) ))
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not remove texture [%s]: not found",
                               system_hashed_ansi_string_get_buffer(texture_name) );
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_textures_get_texture_by_filename(__in __notnull ogl_textures              textures,
                                                                    __in __notnull system_hashed_ansi_string texture_filename)
{
    ogl_texture    result       = NULL;
    _ogl_textures* textures_ptr = (_ogl_textures*) textures;

    system_hash64map_get(textures_ptr->textures_by_filename,
                         system_hashed_ansi_string_get_hash(texture_filename),
                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_textures_get_texture_by_name(__in __notnull ogl_textures              textures,
                                                                __in __notnull system_hashed_ansi_string texture_name)
{
    ogl_texture    result       = NULL;
    _ogl_textures* textures_ptr = (_ogl_textures*) textures;

    system_hash64map_get(textures_ptr->textures_by_name,
                         system_hashed_ansi_string_get_hash(texture_name),
                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_textures_release(__in __notnull ogl_textures textures)
{
    if (textures != NULL)
    {
        delete (_ogl_textures*) textures;

        textures = NULL;
    }
}
