
/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "gfx/gfx_image.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_textures.h"
#include "scene/scene_texture.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"


/* Private declarations */
typedef struct
{
    system_hashed_ansi_string filename;
    system_hashed_ansi_string name;
    ogl_texture               texture;

    REFCOUNT_INSERT_VARIABLES
} _scene_texture;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_texture, scene_texture, _scene_texture);

/** TODO */
PRIVATE void _scene_texture_init(__in __notnull _scene_texture*           data_ptr,
                                 __in __notnull system_hashed_ansi_string name,
                                 __in __notnull system_hashed_ansi_string filename)
{
    data_ptr->filename = filename;
    data_ptr->name     = name;
    data_ptr->texture  = NULL;
}

/** TODO */
PRIVATE void _scene_texture_release(void* data_ptr)
{
    _scene_texture* texture_ptr = (_scene_texture*) data_ptr;

    if (texture_ptr->texture != NULL)
    {
        ogl_texture_release(texture_ptr->texture);

        texture_ptr->texture = NULL;
    }
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_texture scene_texture_create(__in __notnull system_hashed_ansi_string name,
                                                      __in __notnull system_hashed_ansi_string filename)
{
    _scene_texture* new_scene_texture = new (std::nothrow) _scene_texture;

    ASSERT_DEBUG_SYNC(new_scene_texture != NULL, "Out of memory");
    if (new_scene_texture != NULL)
    {
        _scene_texture_init(new_scene_texture, name, filename);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_texture,
                                                       _scene_texture_release,
                                                       OBJECT_TYPE_SCENE_TEXTURE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Textures\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_texture) new_scene_texture;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_texture_get(__in  __notnull scene_texture          instance,
                                          __in            scene_texture_property property,
                                          __out __notnull void*                  result)
{
    _scene_texture* texture_ptr = (_scene_texture*) instance;

    switch (property)
    {
        case SCENE_TEXTURE_PROPERTY_FILENAME:
        {
            *((system_hashed_ansi_string*)result) = texture_ptr->filename;

            break;
        }

        case SCENE_TEXTURE_PROPERTY_OGL_TEXTURE:
        {
            *((ogl_texture*) result) = texture_ptr->texture;

            break;
        }

        case SCENE_TEXTURE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*)result) = texture_ptr->name;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized texture property requested");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_texture scene_texture_load_with_serializer(__in __notnull system_file_serializer serializer,
                                                                    __in __notnull ogl_context            context)
{
    ogl_textures              context_textures = NULL;
    system_hashed_ansi_string filename         = NULL;
    system_hashed_ansi_string name             = NULL;
    scene_texture             result           = 0;
    bool                      uses_mipmaps     = false;

    if (system_file_serializer_read_hashed_ansi_string(serializer, &name)                             &&
        system_file_serializer_read_hashed_ansi_string(serializer, &filename)                         &&
        system_file_serializer_read                   (serializer, sizeof(uses_mipmaps), &uses_mipmaps) )
    {
        result = scene_texture_create(name, filename);

        ASSERT_ALWAYS_SYNC(result != NULL, "Could not create scene texture instance");
        if (result != NULL)
        {
            /* Check with ogl_textures if the filename is recognized. If so, we need not create
             * a new ogl_texture instance, but can simply reuse one that's already been instantiated
             * for the file.
             */
            ogl_texture gl_texture = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_TEXTURES,
                                    &context_textures);

            if ( (gl_texture = ogl_textures_get_texture_by_filename(context_textures,
                                                                    filename) ) == NULL)
            {
                /* Try to initialize the ogl_texture instance */
                gfx_image image = NULL;

                LOG_INFO("Creating a gfx_image instance for file [%s]",
                         system_hashed_ansi_string_get_buffer(filename) );

                image = gfx_image_create_from_file(name,
                                                   filename,
                                                   true); /* use_alternative_filename_getter */

                if (image == NULL)
                {
                    LOG_FATAL("Could not load texture data from file [%s]",
                              system_hashed_ansi_string_get_buffer(filename) );

                    goto end_error;
                }

                gl_texture = ogl_texture_create_from_gfx_image(context,
                                                               image,
                                                               name);

                if (gl_texture == NULL)
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Could not create ogl_texture [%s] from file [%s]",
                                       system_hashed_ansi_string_get_buffer(name),
                                       system_hashed_ansi_string_get_buffer(filename) );

                    goto end_error;
                } /* if (result_ogl_texture == NULL) */

                /* Generate mipmaps if necessary */
                if (uses_mipmaps)
                {
                    ogl_texture_generate_mipmaps(gl_texture);
                }

                /* Release gfx_image instance */
                gfx_image_release(image);

                image = NULL;
            }

            /* Associate the ogl_texture instance with the scene texture */
            scene_texture_set(result,
                              SCENE_TEXTURE_PROPERTY_OGL_TEXTURE,
                             &gl_texture);
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false, "Could not load texture data");

        goto end_error;
    }

    goto end;

end_error:
    if (result != NULL)
    {
        scene_texture_release(result);

        result = NULL;
    }

end:
    return result;
}

/* Please see header for specification */
PUBLIC bool scene_texture_save(__in __notnull system_file_serializer serializer,
                               __in __notnull scene_texture          texture)
{
    bool            result      = false;
    _scene_texture* texture_ptr = (_scene_texture*) texture;
    
    if (texture_ptr != NULL)
    {
        unsigned int n_texture_mipmaps = 0;
        bool         uses_mipmaps      = false;

        ogl_texture_get_property(texture_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_N_MIPMAPS,
                                &n_texture_mipmaps);

        uses_mipmaps = (n_texture_mipmaps > 1);

        if (system_file_serializer_write_hashed_ansi_string(serializer, texture_ptr->name)                         &&
            system_file_serializer_write_hashed_ansi_string(serializer, texture_ptr->filename)                     &&
            system_file_serializer_write                   (serializer, sizeof(uses_mipmaps),    &uses_mipmaps) )
        {
            result = true;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not save texture [%s]", system_hashed_ansi_string_get_buffer(texture_ptr->name) );
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_texture_set(__in __notnull scene_texture          instance,
                                          __in           scene_texture_property property,
                                          __in __notnull void*                  value)
{
    _scene_texture* texture_ptr = (_scene_texture*) instance;

    switch(property)
    {
        case SCENE_TEXTURE_PROPERTY_FILENAME:
        {
            texture_ptr->filename = *((system_hashed_ansi_string*) value);

            break;
        }

        case SCENE_TEXTURE_PROPERTY_NAME:
        {
            texture_ptr->name = *((system_hashed_ansi_string*) value);

            break;
        }

        case SCENE_TEXTURE_PROPERTY_OGL_TEXTURE:
        {
            if (texture_ptr->texture != NULL)
            {
                ogl_texture_release(texture_ptr->texture);

                texture_ptr->texture = NULL;
            }

            texture_ptr->texture = *((ogl_texture*) value);

            if (texture_ptr->texture != NULL)
            {
                ogl_texture_retain(texture_ptr->texture);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized texture property [%d]", property);
        }
    }
}
