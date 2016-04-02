
/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_image.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "scene/scene_texture.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"


/* Private declarations */
typedef struct
{
    ral_context               context;
    system_hashed_ansi_string filename;
    system_hashed_ansi_string name;
    ral_texture               texture;

    REFCOUNT_INSERT_VARIABLES
} _scene_texture;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_texture,
                               scene_texture,
                              _scene_texture);

/** TODO */
PRIVATE void _scene_texture_init(_scene_texture*           data_ptr,
                                 system_hashed_ansi_string name,
                                 system_hashed_ansi_string filename,
                                 ral_context               context)
{
    data_ptr->context  = context;
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
        ral_context_delete_objects(texture_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_objects */
                                   (const void**) &texture_ptr->texture);

        texture_ptr->texture = NULL;
    }
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_texture scene_texture_create(system_hashed_ansi_string name,
                                                      system_hashed_ansi_string object_manager_path,
                                                      system_hashed_ansi_string filename,
                                                      ral_context               context)
{
    _scene_texture* new_scene_texture = new (std::nothrow) _scene_texture;

    ASSERT_DEBUG_SYNC(new_scene_texture != NULL,
                      "Out of memory");

    if (new_scene_texture != NULL)
    {
        _scene_texture_init(new_scene_texture,
                            name,
                            filename,
                            context);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_texture,
                                                       _scene_texture_release,
                                                       OBJECT_TYPE_SCENE_TEXTURE,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_SCENE_TEXTURE,
                                                                       object_manager_path) );
    }

    return (scene_texture) new_scene_texture;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_texture_get(scene_texture          instance,
                                          scene_texture_property property,
                                          void*                  result)
{
    _scene_texture* texture_ptr = (_scene_texture*) instance;

    switch (property)
    {
        case SCENE_TEXTURE_PROPERTY_FILENAME:
        {
            *((system_hashed_ansi_string*)result) = texture_ptr->filename;

            break;
        }

        case SCENE_TEXTURE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*)result) = texture_ptr->name;

            break;
        }

        case SCENE_TEXTURE_PROPERTY_TEXTURE_RAL:
        {
            *((ral_texture*) result) = texture_ptr->texture;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized texture property requested");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_texture scene_texture_load_with_serializer(system_file_serializer      serializer,
                                                                    system_hashed_ansi_string   object_manager_path,
                                                                    ral_context                 context,
                                                                    PFNSETOGLTEXTUREBACKINGPROC pGLSetOGLTextureBacking_callback,
                                                                    void*                       callback_user_data)
{
    system_hashed_ansi_string filename         = NULL;
    system_hashed_ansi_string name             = NULL;
    scene_texture             result           = 0;
    bool                      uses_mipmaps     = false;

    if (system_file_serializer_read_hashed_ansi_string(serializer,
                                                      &name)                  &&
        system_file_serializer_read_hashed_ansi_string(serializer,
                                                      &filename)              &&
        system_file_serializer_read                   (serializer,
                                                       sizeof(uses_mipmaps),
                                                      &uses_mipmaps) )
    {
        result = scene_texture_create(name,
                                      object_manager_path,
                                      filename,
                                      context);

        ASSERT_ALWAYS_SYNC(result != NULL,
                          "Could not create scene texture instance");

        if (result != NULL)
        {
            /* Check with ral_context if the filename is recognized. If so, we need not create
             * a new ral_texture instance, but can simply reuse one that's already been instantiated
             * for the file.
             */
            ral_texture texture = NULL;

            if ( (texture = ral_context_get_texture_by_file_name(context,
                                                                 filename) ) == NULL)
            {
                if (pGLSetOGLTextureBacking_callback != NULL)
                {
                    /* Let the caller take care of setting up the ogl_texture instance
                     * behind this scene_texture.
                     */
                    pGLSetOGLTextureBacking_callback(result,
                                                     filename,
                                                     name,
                                                     uses_mipmaps,
                                                     callback_user_data);
                } /* if (pGLSpawnOGLTextureBacking_callback != NULL) */
                else
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

                    ral_context_create_textures_from_gfx_images(context,
                                                                1, /* n_images */
                                                               &image,
                                                               &texture);

                    if (texture == NULL)
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Could not create ral_texture [%s] from file [%s]",
                                           system_hashed_ansi_string_get_buffer(name),
                                           system_hashed_ansi_string_get_buffer(filename) );

                        goto end_error;
                    } /* if (result_ogl_texture == NULL) */

                    /* Generate mipmaps if necessary */
                    if (uses_mipmaps)
                    {
                        ral_texture_generate_mipmaps(texture,
                                                     true /* async */);
                    }

                    /* Release gfx_image instance */
                    gfx_image_release(image);

                    image = NULL;

                    /* Associate the ogl_texture instance with the scene texture */
                    scene_texture_set(result,
                                      SCENE_TEXTURE_PROPERTY_TEXTURE_RAL,
                                     &texture);
                }
            } /* if (no corresponding RAL texture is already available) */
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
PUBLIC bool scene_texture_save(system_file_serializer serializer,
                               scene_texture          texture)
{
    bool            result      = false;
    _scene_texture* texture_ptr = (_scene_texture*) texture;
    
    if (texture_ptr != NULL)
    {
        unsigned int n_texture_mipmaps = 0;
        bool         uses_mipmaps      = false;

        ral_texture_get_property(texture_ptr->texture,
                                 RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                &n_texture_mipmaps);

        uses_mipmaps = (n_texture_mipmaps > 1);

        if (system_file_serializer_write_hashed_ansi_string(serializer,
                                                            texture_ptr->name)     &&
            system_file_serializer_write_hashed_ansi_string(serializer,
                                                            texture_ptr->filename) &&
            system_file_serializer_write                   (serializer,
                                                            sizeof(uses_mipmaps),
                                                           &uses_mipmaps) )
        {
            result = true;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not save texture [%s]",
                              system_hashed_ansi_string_get_buffer(texture_ptr->name) );
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_texture_set(scene_texture          instance,
                                          scene_texture_property property,
                                          void*                  value)
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

        case SCENE_TEXTURE_PROPERTY_TEXTURE_RAL:
        {
            if (texture_ptr->texture != NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "? Verify ?");
                // ral_texture_release(texture_ptr->texture);

                texture_ptr->texture = NULL;
            }

            texture_ptr->texture = *((ral_texture*) value);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture property [%d]",
                              property);
        }
    }
}
