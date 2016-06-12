/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_textures.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


typedef struct _raGL_textures_texture_heap
{
    /* Holds allocated raGL_texture instances. */
    system_resizable_vector heap;

    _raGL_textures_texture_heap()
    {
        heap = system_resizable_vector_create(4 /* capacity */);
    }

    ~_raGL_textures_texture_heap()
    {
        if (heap != nullptr)
        {
            /* Textures should have been released at _release() time. */
            uint32_t n_textures_in_heap = 0;

            system_resizable_vector_get_property(heap,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_textures_in_heap);

            ASSERT_DEBUG_SYNC(n_textures_in_heap == 0,
                              "raGL_texture instances present in heap at destruction time.");

            /* Release the vector nevertheless */
            system_resizable_vector_release(heap);
            heap = nullptr;
        }
    }
} _raGL_textures_texture_heap;

typedef struct _raGL_textures
{
    ogl_context             context;                      /* NOT owned */
    ral_context             context_ral;
    system_critical_section cs;
    system_hash64map        key_hash_to_texture_heap_map; /* owns the _raGL_textures_texture_heap instances */


    REFCOUNT_INSERT_VARIABLES;

    _raGL_textures(ogl_context in_context,
                   ral_context in_context_ral)
    {
        context                      = in_context;
        context_ral                  = in_context_ral;
        cs                           = system_critical_section_create();
        key_hash_to_texture_heap_map = system_hash64map_create       (sizeof(_raGL_textures_texture_heap*) );
    }

    ~_raGL_textures()
    {
        if (cs != nullptr)
        {
            system_critical_section_release(cs);

            cs = nullptr;
        }

        if (key_hash_to_texture_heap_map != nullptr)
        {
            /* Texture vectors should have been released at _release() time .. */
            uint32_t n_items_in_texture_item_map = 0;

            system_hash64map_get_property(key_hash_to_texture_heap_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_items_in_texture_item_map);

            ASSERT_DEBUG_SYNC(n_items_in_texture_item_map == 0,
                              "key hash->texture item map still holds raGL_textures_texture instances at destruction time.");

            /* Release the map nevertheless */
            system_hash64map_release(key_hash_to_texture_heap_map);

            key_hash_to_texture_heap_map = nullptr;
        }
    }
} _raGL_textures;

typedef struct
{
    raGL_texture    result_texture;
    ral_texture     texture_ral;
    _raGL_textures* textures_ptr;

} _raGL_textures_alloc_texture_rendering_thread_callback_arg;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(raGL_textures,
                               raGL_textures,
                              _raGL_textures);


/* Forward declarations */
PRIVATE void          _raGL_textures_alloc_texture_rendering_thread_callback(ogl_context context,
                                                                             void*       callback_arg);
PRIVATE system_hash64 _raGL_textures_get_texture_hash                       (ral_texture texture);
PRIVATE void          _raGL_textures_release                                (void*       textures);
PRIVATE void          _raGL_textures_release_rendering_thread_callback      (ogl_context context,
                                                                             void*       callback_arg);

/** TODO */
PRIVATE void _raGL_textures_alloc_texture_rendering_thread_callback(ogl_context context,
                                                                    void*       callback_arg)
{
    _raGL_textures_alloc_texture_rendering_thread_callback_arg* callback_arg_ptr = (_raGL_textures_alloc_texture_rendering_thread_callback_arg*) callback_arg;

    callback_arg_ptr->result_texture = raGL_texture_create(callback_arg_ptr->textures_ptr->context,
                                                           callback_arg_ptr->texture_ral);
}

/** TODO */
PRIVATE system_hash64 _raGL_textures_get_texture_hash(ral_texture texture)
{
    system_hash64    result                         = 0;
    bool             texture_fixed_sample_locations = false;
    uint32_t         texture_base_mipmap_depth      = 0;
    uint32_t         texture_base_mipmap_height     = 0;
    uint32_t         texture_base_mipmap_width      = 0;
    ral_format       texture_format                 = RAL_FORMAT_UNKNOWN;
    uint32_t         texture_n_layers               = 0;
    uint32_t         texture_n_mipmaps              = 0;
    uint32_t         texture_n_samples              = 0;
    ral_texture_type texture_type                   = RAL_TEXTURE_TYPE_UNKNOWN;

    /* Retrieve the input texture properties */
    ral_texture_get_property       (texture,
                                    RAL_TEXTURE_PROPERTY_FORMAT,
                                   &texture_format);
    ral_texture_get_property       (texture,
                                    RAL_TEXTURE_PROPERTY_N_LAYERS,
                                   &texture_n_layers);
    ral_texture_get_property       (texture,
                                    RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                   &texture_n_mipmaps);
    ral_texture_get_property       (texture,
                                    RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                   &texture_n_samples);
    ral_texture_get_property       (texture,
                                    RAL_TEXTURE_PROPERTY_TYPE,
                                   &texture_type);
    ral_texture_get_mipmap_property(texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                   &texture_base_mipmap_depth);
    ral_texture_get_mipmap_property(texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                   &texture_base_mipmap_height);
    ral_texture_get_mipmap_property(texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                   &texture_base_mipmap_width);

    /* Make sure these properties fit within the key constraints:
     *
     * base_mipmap_depth:  0-255  (8  bits)
     * base_mipmap_height: 0-4095 (13 bits)
     * base_mipmap_width:  0-4095 (13 bits)
     * texture_format:     0-127  (7  bits)
     * texture_type:       0-15   (4  bits)
     * n_layers:           0-15   (4  bits)
     * n_mipmaps:          0-15   (4  bits)
     * n_samples:          0-31   (5  bits)
     *                            --------
     *                      total: 56 bits
     */
    ASSERT_DEBUG_SYNC(texture_format             < (1 << 7)  &&
                      texture_n_layers           < (1 << 4)  &&
                      texture_n_mipmaps          < (1 << 4)  &&
                      texture_n_samples          < (1 << 5)  &&
                      texture_type               < (1 << 4)  &&
                      texture_base_mipmap_depth  < (1 << 8)  &&
                      texture_base_mipmap_height < (1 << 13) &&
                      texture_base_mipmap_width  < (1 << 13),
                      "RAL texture properties overflow the texture key");

    result = ( ((int64_t)texture_base_mipmap_depth)  & ((1 << 8)  - 1)) << 0  |
             ( ((int64_t)texture_base_mipmap_height) & ((1 << 13) - 1)) << 8  |
             ( ((int64_t)texture_base_mipmap_width)  & ((1 << 13) - 1)) << 21 |
             ( ((int64_t)texture_format)             & ((1 << 7)  - 1)) << 34 |
             ( ((int64_t)texture_type)               & ((1 << 4)  - 1)) << 41 |
             ( ((int64_t)texture_n_layers)           & ((1 << 4)  - 1)) << 45 |
             ( ((int64_t)texture_n_mipmaps)          & ((1 << 4)  - 1)) << 49 |
             ( ((int64_t)texture_n_samples)          & ((1 << 5)  - 1)) << 53;

    return result;
}

/** TODO */
PRIVATE void _raGL_textures_release(void* textures)
{
    _raGL_textures* textures_ptr = (_raGL_textures*) textures;

    /* Request a rendering thread call-back, so that we can release all the textures we
     * were holding in the heaps */
    ogl_context_request_callback_from_context_thread(textures_ptr->context,
                                                     _raGL_textures_release_rendering_thread_callback,
                                                     textures_ptr);
}

/** TODO */
PRIVATE void _raGL_textures_release_rendering_thread_callback(ogl_context context,
                                                              void*       callback_arg)
{
    _raGL_textures* textures_ptr = (_raGL_textures*) callback_arg;

    /* Iterate over all heaps.. */
    uint32_t n_heaps = 0;

    system_critical_section_enter(textures_ptr->cs);
    {
        system_hash64map_get_property(textures_ptr->key_hash_to_texture_heap_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_heaps);

        for (uint32_t n_heap = 0;
                      n_heap < n_heaps;
                    ++n_heap)
        {
            raGL_texture                 current_texture = nullptr;
            _raGL_textures_texture_heap* heap_ptr        = nullptr;

            if (!system_hash64map_get_element_at(textures_ptr->key_hash_to_texture_heap_map,
                                                 n_heap,
                                                &heap_ptr,
                                                 nullptr /* result_hash_ptr */) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve the heap descriptor at index [%d]",
                                  n_heap);

                continue;
            }

            while (system_resizable_vector_pop(heap_ptr->heap,
                                              &current_texture) )
            {
                raGL_texture_release(current_texture);

                current_texture = nullptr;
            }
        }

        system_hash64map_clear(textures_ptr->key_hash_to_texture_heap_map);
    }
    system_critical_section_leave(textures_ptr->cs);
}


/** Please see header for spec */
PUBLIC raGL_textures raGL_textures_create(ral_context context_ral,
                                          ogl_context context)
{
    _raGL_textures* new_textures_ptr = new (std::nothrow) _raGL_textures(context,
                                                                         context_ral);

    if (new_textures_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    /* Register in the object manager */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_textures_ptr,
                                                   _raGL_textures_release,
                                                   OBJECT_TYPE_RAGL_TEXTURES,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\raGL Textures\\",
                                                                                                           "Context-wide GL texture object manager") );

end:
    return (raGL_textures) new_textures_ptr;
}

/** Please see header for specification */
PUBLIC raGL_texture raGL_textures_get_texture_from_pool(raGL_textures textures,
                                                        ral_texture   texture_ral)
{
    bool                         cs_entered       = false;
    raGL_texture                 result           = nullptr;
    _raGL_textures_texture_heap* texture_heap_ptr = nullptr;
    system_hash64                texture_key      = 0;
    _raGL_textures*              textures_ptr     = (_raGL_textures*) textures;

    /* Sanity checks */
    if (textures == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_textures instance is NULL");

        goto end;
    }

    if (texture_ral == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    /* Is there a container already available which matches requirements of the specified RAL texture? */
    texture_key = _raGL_textures_get_texture_hash(texture_ral);

    system_critical_section_enter(textures_ptr->cs);
    {
        cs_entered = true;

        if (!system_hash64map_get(textures_ptr->key_hash_to_texture_heap_map,
                                  texture_key,
                                 &texture_heap_ptr))
        {
            /* Need to instantiate the heap first.. */
            texture_heap_ptr = new (std::nothrow) _raGL_textures_texture_heap();

            if (texture_heap_ptr == nullptr)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Out of memory");

                goto end;
            }

            system_hash64map_insert(textures_ptr->key_hash_to_texture_heap_map,
                                    texture_key,
                                    texture_heap_ptr,
                                    nullptr,  /* on_removal_callback          */
                                    nullptr); /* on_removal_callback_user_arg */
        }

        /* Is there a preallocated texture available in the heap? */
        if (!system_resizable_vector_pop(texture_heap_ptr->heap,
                                        &result) )
        {
            /* Nope. Need to alloc it now. */
            _raGL_textures_alloc_texture_rendering_thread_callback_arg callback_arg;

            callback_arg.result_texture = nullptr;
            callback_arg.texture_ral    = texture_ral;
            callback_arg.textures_ptr   = textures_ptr;

            ogl_context_request_callback_from_context_thread(textures_ptr->context,
                                                             _raGL_textures_alloc_texture_rendering_thread_callback,
                                                            &callback_arg);

            if (callback_arg.result_texture == nullptr)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Failed to spawn a new raGL_texture instance.");

                goto end;
            }

            result = callback_arg.result_texture;
        }
    }

    /* All done */
end:
    if (cs_entered)
    {
        system_critical_section_leave(textures_ptr->cs);
    }

    return result;
}

/** Please see header for specification */
PUBLIC raGL_texture raGL_textures_get_texture_from_pool_with_create_info(raGL_textures                  textures,
                                                                         const ral_texture_create_info* info_ptr)
{
    _raGL_textures* textures_ptr   = (_raGL_textures*) textures;
    raGL_texture    result_texture = nullptr;
    ral_texture     temp_texture   = nullptr;

    temp_texture = ral_texture_create(textures_ptr->context_ral,
                                      system_hashed_ansi_string_create("Temporary RAL texture"),
                                      info_ptr);

    result_texture = raGL_textures_get_texture_from_pool(textures,
                                                         temp_texture);

    ral_texture_release(temp_texture);

    return result_texture;
}

/** Please see header for specification */
PUBLIC void raGL_textures_return_to_pool(raGL_textures textures,
                                         raGL_texture  texture)
{
    bool                         cs_entered       = false;
    system_hash64                texture_hash     = 0;
    _raGL_textures_texture_heap* texture_heap_ptr = nullptr;
    ral_texture                  texture_ral      = nullptr;
    _raGL_textures*              textures_ptr     = (_raGL_textures*) textures;

    /* Sanity checks */
    if (textures == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_textures instance is NULL");

        goto end;
    }

    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_texture instance is NULL");

        goto end;
    }

    /* Identify the heap we want to store the returned texture in. */
    raGL_texture_get_property(texture,
                              RAGL_TEXTURE_PROPERTY_RAL_TEXTURE,
                              (void**) &texture_ral);

    texture_hash = _raGL_textures_get_texture_hash(texture_ral);

    system_critical_section_enter(textures_ptr->cs);
    {
        cs_entered = true;

        if (!system_hash64map_get(textures_ptr->key_hash_to_texture_heap_map,
                                  texture_hash,
                                 &texture_heap_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Texture heap not spawned for the specified texture. Does this texture really come from raGL_textures?");

            goto end;
        }

        /* Push the texture back onto heap */
        system_resizable_vector_push(texture_heap_ptr->heap,
                                     texture);
    }

    /* All done */
end:
    if (cs_entered)
    {
        system_critical_section_leave(textures_ptr->cs);
    }
}