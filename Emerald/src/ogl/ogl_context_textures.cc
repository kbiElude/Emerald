/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_textures.h"
#include "ogl/ogl_texture.h"
#include "system/system_atomics.h"
#include "system/system_log.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/* Private type definitions */
typedef struct
 {
     unsigned int       base_mipmap_depth;
     unsigned int       base_mipmap_height;
     unsigned int       base_mipmap_width;
     bool               fixed_sample_locations;
     ral_texture_format format;
     ogl_texture        result;
     unsigned int       n_mipmaps;
     unsigned int       n_samples;
     char*              spawned_texture_id_text;
     ral_texture_type   type;
 } _ogl_context_textures_create_and_init_texture_rendering_callback_arg;

typedef struct _ogl_context_textures
{
    ogl_context      context;
    system_hash64map textures_by_filename;
    system_hash64map textures_by_name;

    system_hash64map        reusable_texture_key_to_ogl_texture_vector_map;
    system_resizable_vector reusable_textures;

    _ogl_context_textures()
    {
        context                                        = NULL;
        reusable_texture_key_to_ogl_texture_vector_map = system_hash64map_create       (sizeof(system_resizable_vector) );
        reusable_textures                              = system_resizable_vector_create(4 /* capacity */);
        textures_by_filename                           = system_hash64map_create       (sizeof(ogl_texture) );
        textures_by_name                               = system_hash64map_create       (sizeof(ogl_texture) );
    }

    ~_ogl_context_textures()
    {
        LOG_INFO("Texture manager deallocating..");

        /* While releasing all allocated textures, make sure the number of entries found
         * in the map matches the total number of reusable textures that have been created.
         * A mismatch indicates texture leaks.
         */
        unsigned int n_reusable_textures_released = 0;

        if (reusable_texture_key_to_ogl_texture_vector_map != NULL)
        {
            uint32_t n_map_vectors = 0;

            system_hash64map_get_property(reusable_texture_key_to_ogl_texture_vector_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_map_vectors);

            for (uint32_t n_map_vector = 0;
                          n_map_vector < n_map_vectors;
                        ++n_map_vector)
            {
                system_resizable_vector map_vector           = NULL;
                uint32_t                n_map_vector_entries = 0;

                if (!system_hash64map_get_element_at(reusable_texture_key_to_ogl_texture_vector_map,
                                                     n_map_vector,
                                                    &map_vector,
                                                     NULL) ) /* outHash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve hash map entry at index [%d]",
                                      n_map_vector);

                    continue;
                }

                system_resizable_vector_get_property(map_vector,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_map_vector_entries);

                n_reusable_textures_released += n_map_vector_entries;

                system_resizable_vector_release(map_vector);
                map_vector = NULL;
            } /* for (all vectors stored in the map) */

            system_hash64map_release(reusable_texture_key_to_ogl_texture_vector_map);

            reusable_texture_key_to_ogl_texture_vector_map = NULL;
        } /* if (reusable_texture_key_to_ogl_texture_vector_map != NULL) */

        if (reusable_textures != NULL)
        {
            uint32_t n_textures = 0;

            system_resizable_vector_get_property(reusable_textures,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_textures);

            ASSERT_DEBUG_SYNC(n_textures == n_reusable_textures_released,
                              "Reusable texture memory leak detected");

            for (uint32_t n_texture = 0;
                          n_texture < n_textures;
                        ++n_texture)
            {
                ogl_texture current_texture = NULL;

                if (!system_resizable_vector_get_element_at(reusable_textures,
                                                            n_texture,
                                                           &current_texture) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve re-usable texture at index [%d]",
                                      n_texture);

                    continue;
                }

                ogl_texture_release(current_texture);
                current_texture = NULL;
            } /* for (all reusable textures) */

            system_resizable_vector_release(reusable_textures);
            reusable_textures = NULL;
        }

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
} _ogl_context_textures;


/** TODO */
PRIVATE system_hash64 _ogl_context_textures_get_reusable_texture_key(ral_texture_type   type,
                                                                     unsigned int       base_mipmap_depth,
                                                                     unsigned int       base_mipmap_height,
                                                                     unsigned int       base_mipmap_width,
                                                                     unsigned int       n_mipmaps,
                                                                     unsigned int       n_samples,
                                                                     ral_texture_format format,
                                                                     bool               fixed_sample_locations)
{
    /* Key structure:
     *
     * 00-04bit: n mipmaps              (0-15)
     * 05-10bit: n samples              (0-31)
     * 11-19bit: internalformat         (0-63, internal enum that maps to a specific GLenum)
     * 20-24bit: texture type           (0-15, internal enum that maps to a specific type)
     * 25-38bit: base mipmap width      (0-16383)
     * 39-52bit: base mipmap height     (0-16383)
     * 53-60bit: base mipmap depth      (0-255).
     *    61bit: fixed sample locations (0-1).
     */

    /* Some checks to make sure the crucial properties fit within the key.. */

    ASSERT_DEBUG_SYNC((n_mipmaps          < (1 << 5)   &&
                       n_samples          < (1 << 6)   &&
                       format             < (1 << 10)  &&
                       type               < (1 << 5)   &&
                       base_mipmap_depth  < (1 << 8)   &&
                       base_mipmap_height < (1 << 16)  &&
                       base_mipmap_width  < (1 << 16)),
                      "Texture properties overflow the texture pool key");

    return (( ((system_hash64) n_mipmaps)                      & ((1 << 5)  - 1)) << 0)  |
           (( ((system_hash64) n_samples)                      & ((1 << 6)  - 1)) << 5)  |
           (( ((system_hash64) format)                         & ((1 << 9)  - 1)) << 11) |
           (( ((system_hash64) type)                           & ((1 << 5)  - 1)) << 20) |
           (( ((system_hash64) base_mipmap_width)              & ((1 << 14) - 1)) << 25) |
           (( ((system_hash64) base_mipmap_height)             & ((1 << 14) - 1)) << 39) |
           (( ((system_hash64) base_mipmap_depth)              & ((1 << 8)  - 1)) << 53) |
           (   (system_hash64)(fixed_sample_locations ? 1 : 0)                    << 61);
}

PRIVATE void _ogl_context_textures_create_and_init_texture_rendering_callback(ogl_context context,
                                                                              void*       arg)
 {
     _ogl_context_textures_create_and_init_texture_rendering_callback_arg* arg_ptr = (_ogl_context_textures_create_and_init_texture_rendering_callback_arg*) arg;

     arg_ptr->result = ogl_texture_create_and_initialize(context,
                                                         system_hashed_ansi_string_create_by_merging_two_strings("Re-usable texture ",
                                                                                                                 arg_ptr->spawned_texture_id_text),
                                                         arg_ptr->type,
                                                         arg_ptr->n_mipmaps,
                                                         arg_ptr->format,
                                                         arg_ptr->base_mipmap_width,
                                                         arg_ptr->base_mipmap_height,
                                                         arg_ptr->base_mipmap_depth,
                                                         arg_ptr->n_samples,
                                                         arg_ptr->fixed_sample_locations);

     ASSERT_DEBUG_SYNC(arg_ptr->result != NULL,
                       "ogl_texture_create_and_initialize() failed.");
 }

/** Please see header for specification */
PUBLIC void ogl_context_textures_add_texture(ogl_context_textures textures,
                                             ogl_texture          texture)
{
    system_hashed_ansi_string texture_name         = NULL;
    system_hashed_ansi_string texture_src_filename = NULL;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_NAME,
                            &texture_name);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_SRC_FILENAME,
                            &texture_src_filename);

    system_hash64          texture_name_hash = system_hashed_ansi_string_get_hash(texture_name);
    _ogl_context_textures* textures_ptr      = (_ogl_context_textures*) textures;

    /* Make sure the texture has not already been added */
    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(texture_name) > 0,
                      "Texture name is NULL");

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
                                NULL,  /* on_remove_callback          */
                                NULL); /* on_remove_callback_argument */
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC ogl_context_textures ogl_context_textures_create(ogl_context context)
{
    _ogl_context_textures* textures_ptr = new (std::nothrow) _ogl_context_textures;

    ASSERT_ALWAYS_SYNC(textures_ptr != NULL,
                       "Out of memory");

    if (textures_ptr != NULL)
    {
        textures_ptr->context = context;
    }

    return (ogl_context_textures) textures_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_from_pool(ogl_context        context,
                                                                          ral_texture_type   type,
                                                                          unsigned int       n_mipmaps,
                                                                          ral_texture_format format,
                                                                          unsigned int       base_mipmap_width,
                                                                          unsigned int       base_mipmap_height,
                                                                          unsigned int       base_mipmap_depth,
                                                                          unsigned int       n_samples,
                                                                          bool               fixed_sample_locations)
{
    ogl_texture            result           = NULL;
    const system_hash64    texture_key      = _ogl_context_textures_get_reusable_texture_key(type,
                                                                                             base_mipmap_depth,
                                                                                             base_mipmap_height,
                                                                                             base_mipmap_width,
                                                                                             n_mipmaps,
                                                                                             n_samples,
                                                                                             format,
                                                                                             fixed_sample_locations);
    _ogl_context_textures* textures_ptr     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &textures_ptr);

    ASSERT_DEBUG_SYNC(n_samples >= 1,
                      "Number of samples must not be 0");

    /* Is a re-usable texture already available? */
    system_resizable_vector reusable_textures = NULL;

    if (!system_hash64map_get(textures_ptr->reusable_texture_key_to_ogl_texture_vector_map,
                              texture_key,
                             &reusable_textures) )
    {
        /* No vector available. Let's spawn one. */
        reusable_textures = system_resizable_vector_create(4 /* capacity */);

        system_hash64map_insert(textures_ptr->reusable_texture_key_to_ogl_texture_vector_map,
                                texture_key,
                                reusable_textures,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }

    if (!system_resizable_vector_pop(reusable_textures,
                                    &result) )
    {
        static uint32_t spawned_textures_cnt    = 1;
               uint32_t spawned_texture_id      = 0;
               char     spawned_texture_id_text[16];

        spawned_texture_id = system_atomics_increment(&spawned_textures_cnt);

        snprintf(spawned_texture_id_text,
                 sizeof(spawned_texture_id_text),
                 "%d",
                 spawned_texture_id);

        /* No free re-usable texture available. Spawn a new texture object. This needs to be done from
         * a rendering thread.
         */
        LOG_INFO("Creating a new re-usable texture object [index:%d]..",
                 spawned_texture_id);

        _ogl_context_textures_create_and_init_texture_rendering_callback_arg callback_arg;

        callback_arg.base_mipmap_depth       = base_mipmap_depth;
        callback_arg.base_mipmap_height      = base_mipmap_height;
        callback_arg.base_mipmap_width       = base_mipmap_width;
        callback_arg.fixed_sample_locations  = fixed_sample_locations;
        callback_arg.format                  = format;
        callback_arg.result                  = NULL;
        callback_arg.n_mipmaps               = n_mipmaps;
        callback_arg.n_samples               = n_samples;
        callback_arg.spawned_texture_id_text = spawned_texture_id_text;
        callback_arg.type                    = type;

        ogl_context_request_callback_from_context_thread(context,
                                                         _ogl_context_textures_create_and_init_texture_rendering_callback,
                                                        &callback_arg);

        ASSERT_DEBUG_SYNC(callback_arg.result != NULL,
                          "_ogl_context_textures_create_and_init_texture_rendering_callback() call failed.");

        if (callback_arg.result != NULL)
        {
            system_resizable_vector_push(textures_ptr->reusable_textures,
                                         callback_arg.result);

            result = callback_arg.result;
        } /* if (result != NULL) */
    } /* if (no reusable texture available) */

    /* OK, "result" should hold the handle at this point */
    ASSERT_DEBUG_SYNC(result != NULL,
                      "Texture pool about to return a NULL ogl_texture instance");

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_context_textures_delete_texture(ogl_context_textures      textures,
                                                system_hashed_ansi_string texture_name)
{
    _ogl_context_textures* textures_ptr = (_ogl_context_textures*) textures;

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
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_by_filename(ogl_context_textures      textures,
                                                                            system_hashed_ansi_string texture_filename)
{
    ogl_texture            result       = NULL;
    _ogl_context_textures* textures_ptr = (_ogl_context_textures*) textures;

    system_hash64map_get(textures_ptr->textures_by_filename,
                         system_hashed_ansi_string_get_hash(texture_filename),
                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_by_name(ogl_context_textures      textures,
                                                                        system_hashed_ansi_string texture_name)
{
    ogl_texture            result       = NULL;
    _ogl_context_textures* textures_ptr = (_ogl_context_textures*) textures;

    system_hash64map_get(textures_ptr->textures_by_name,
                         system_hashed_ansi_string_get_hash(texture_name),
                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_context_textures_release(ogl_context_textures textures)
{
    if (textures != NULL)
    {
        delete (_ogl_context_textures*) textures;

        textures = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_context_textures_return_reusable(ogl_context context,
                                                             ogl_texture released_texture)
{
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    system_resizable_vector           owner_vector                   = NULL;
    system_hash64                     reusable_texture_key           = 0;
    unsigned int                      texture_base_mipmap_depth      = 0;
    unsigned int                      texture_base_mipmap_height     = 0;
    unsigned int                      texture_base_mipmap_width      = 0;
    bool                              texture_fixed_sample_locations;
    GLuint                            texture_id                     = 0;
    ral_texture_format                texture_format;
    unsigned int                      texture_n_mipmaps              = 0;
    unsigned int                      texture_n_samples              = 0;
    _ogl_context_textures*            textures_ptr                   = NULL;
    ral_texture_type                  texture_type                   = RAL_TEXTURE_TYPE_UNKNOWN;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &textures_ptr);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(system_resizable_vector_find(textures_ptr->reusable_textures,
                                                   released_texture) != ITEM_NOT_FOUND,
                      "Texture returned to the pool is NOT a re-usable texture!");

    /* Identify the texture key */
    ogl_texture_get_mipmap_property(released_texture,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                   &texture_base_mipmap_depth);
    ogl_texture_get_mipmap_property(released_texture,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                   &texture_base_mipmap_height);
    ogl_texture_get_mipmap_property(released_texture,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                   &texture_base_mipmap_width);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_TYPE,
                                  &texture_type);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,
                                  &texture_fixed_sample_locations);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_FORMAT_RAL,
                                  &texture_format);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_N_MIPMAPS,
                                  &texture_n_mipmaps);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_N_SAMPLES,
                                  &texture_n_samples);

    reusable_texture_key = _ogl_context_textures_get_reusable_texture_key(texture_type,
                                                                          texture_base_mipmap_depth,
                                                                          texture_base_mipmap_height,
                                                                          texture_base_mipmap_width,
                                                                          texture_n_mipmaps,
                                                                          texture_n_samples,
                                                                          texture_format,
                                                                          texture_fixed_sample_locations);

    /* Look for the owner vector */
    if (!system_hash64map_get(textures_ptr->reusable_texture_key_to_ogl_texture_vector_map,
                              reusable_texture_key,
                             &owner_vector) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Cannot put a reusable texture back into owner pool - owner vector not found");

        ogl_texture_release(released_texture);
        released_texture = NULL;

        goto end;
    }

    ASSERT_DEBUG_SYNC(owner_vector != NULL,
                      "Reusable texture owner vector is NULL");

    system_resizable_vector_push(owner_vector,
                                 released_texture);

    /* Finally, invalidate the texture contents */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);
    ogl_texture_get_property(released_texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    for (unsigned int n_mipmap = 0;
                      n_mipmap < texture_n_mipmaps;
                    ++n_mipmap)
    {
        entrypoints->pGLInvalidateTexImage(texture_id,
                                           n_mipmap);
    } /* for (all mipmaps) */

end:
    ;
}