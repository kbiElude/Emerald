/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "gfx/gfx_image.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_resizable_vector.h"


typedef struct _ral_texture_mipmap
{
    bool         contents_set;
    unsigned int depth;
    unsigned int height;
    unsigned int width;

    explicit _ral_texture_mipmap(uint32_t in_width,
                                 uint32_t in_height,
                                 uint32_t in_depth)
    {
        contents_set = false;
        depth        = in_depth;
        height       = in_height;
        width        = in_width;
    }
} _ral_texture_mipmap;

typedef struct _ral_texture_layer
{
    /* Holds _ral_texture_mipmap* items */
    system_resizable_vector mipmaps;

    _ral_texture_layer()
    {
        mipmaps = system_resizable_vector_create(4 /* capacity */);
    }

    ~_ral_texture_layer()
    {
        if (mipmaps != nullptr)
        {
            _ral_texture_mipmap* current_mipmap_ptr = nullptr;

            while (system_resizable_vector_pop(mipmaps,
                                              &current_mipmap_ptr) )
            {
                delete current_mipmap_ptr;
                current_mipmap_ptr = nullptr;
            }

            system_resizable_vector_release(mipmaps);
            mipmaps = nullptr;
        }
    }
} _ral_texture_layer;

typedef struct _ral_texture
{
    uint32_t                  base_mipmap_size[3];
    system_callback_manager   callback_manager;
    ral_context               context;
    system_hashed_ansi_string description;
    system_hashed_ansi_string file_name;
    bool                      fixed_sample_locations;
    ral_format                format;
    uint32_t                  n_layers;
    uint32_t                  n_mipmaps_per_layer;
    uint32_t                  n_samples;
    ral_texture_type          type;
    system_hashed_ansi_string unique_name;
    ral_texture_usage_bits    usage;

    bool             being_released;

    system_hash64map        views; /* view hash -> owned ral_texture_view instance */
    system_critical_section views_cs;

    /* Holds _ral_texture_layer instances.
     *
     * 1D/2D/3D textures = 1 layer; CM = 6 layer; CM array = 6n layers;
     */
    system_resizable_vector layers;


    _ral_texture(ral_context               in_context,
                 uint32_t                  in_base_mipmap_width,
                 uint32_t                  in_base_mipmap_height,
                 uint32_t                  in_base_mipmap_depth,
                 bool                      in_fixed_sample_locations,
                 ral_format                in_format,
                 uint32_t                  in_n_layers,
                 uint32_t                  in_n_mipmaps,
                 uint32_t                  in_n_samples,
                 system_hashed_ansi_string in_unique_name,
                 system_hashed_ansi_string in_description,
                 ral_texture_type          in_type,
                 ral_texture_usage_bits    in_usage)
    {
        base_mipmap_size[0]     = in_base_mipmap_width;
        base_mipmap_size[1]     = in_base_mipmap_height;
        base_mipmap_size[2]     = in_base_mipmap_depth;
        being_released          = false;
        callback_manager        = system_callback_manager_create( (_callback_id) RAL_TEXTURE_CALLBACK_ID_COUNT);
        context                 = in_context;
        description             = in_description;
        file_name               = nullptr;
        fixed_sample_locations  = in_fixed_sample_locations;
        format                  = in_format;
        layers                  = system_resizable_vector_create(in_n_layers);
        n_layers                = in_n_layers;
        n_mipmaps_per_layer     = in_n_mipmaps;
        n_samples               = in_n_samples;
        type                    = in_type;
        unique_name             = in_unique_name;
        usage                   = in_usage;
        views                   = system_hash64map_create       (sizeof(ral_texture_view) );
        views_cs                = system_critical_section_create();
    }

    ~_ral_texture()
    {
        being_released = true;

        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }

        if (layers != nullptr)
        {
            _ral_texture_layer* current_layer_ptr = nullptr;

            while (system_resizable_vector_pop(layers,
                                              &current_layer_ptr) )
            {
                delete current_layer_ptr;

                current_layer_ptr = nullptr;
            }

            system_resizable_vector_release(layers);
            layers = nullptr;
        }

        if (views != nullptr)
        {
            uint32_t n_views = 0;

            system_hash64map_get_property(views,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_views);

            for (uint32_t n_view = 0;
                          n_view < n_views;
                        ++n_view)
            {
                ral_texture_view view;

                system_hash64map_get_element_at(views,
                                                n_view,
                                               &view,
                                                nullptr); /* result_hash_ptr */

                ral_context_delete_objects(context,
                                           RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                           1, /* n_objects */
                                           reinterpret_cast<void* const*>(&view) );
            }

            system_hash64map_release(views);
            views = nullptr;
        }

        if (views_cs != nullptr)
        {
            system_critical_section_release(views_cs);

            views_cs = nullptr;
        }
    }
} _ral_texture;


/** TODO */
PRIVATE system_hash64 _ral_texture_get_view_hash(const ral_texture_view_create_info* view_create_info_ptr)
{
    system_hash64 result;

    view_create_info_ptr->aspect;
    view_create_info_ptr->component_order;
    view_create_info_ptr->format;
    view_create_info_ptr->n_base_layer;
    view_create_info_ptr->n_base_mip;
    view_create_info_ptr->n_layers;
    view_create_info_ptr->n_mips;
    view_create_info_ptr->type;

    ASSERT_DEBUG_SYNC(view_create_info_ptr->aspect                                    < (1 << 3) &&
                      static_cast<uint64_t>(view_create_info_ptr->component_order[0]) < (1 << 3) &&
                      static_cast<uint64_t>(view_create_info_ptr->component_order[1]) < (1 << 3) &&
                      static_cast<uint64_t>(view_create_info_ptr->component_order[2]) < (1 << 3) &&
                      static_cast<uint64_t>(view_create_info_ptr->component_order[3]) < (1 << 3) &&
                      view_create_info_ptr->format                                    < (1 << 7) &&
                      view_create_info_ptr->n_base_layer                              < (1 << 4) &&
                      view_create_info_ptr->n_base_mip                                < (1 << 4) &&
                      view_create_info_ptr->n_layers                                  < (1 << 4) &&
                      view_create_info_ptr->n_mips                                    < (1 << 4) &&
                      view_create_info_ptr->type                                      < (1 << 4),
                      "Texture view parameter out of range");

    result = (((static_cast<int64_t>(view_create_info_ptr->aspect)             & ((1 << 3) - 1)) << 0)  |
              ((static_cast<int64_t>(view_create_info_ptr->component_order[0]) & ((1 << 3) - 1)) << 3)  |
              ((static_cast<int64_t>(view_create_info_ptr->component_order[1]) & ((1 << 3) - 1)) << 6)  |
              ((static_cast<int64_t>(view_create_info_ptr->component_order[2]) & ((1 << 3) - 1)) << 9)  |
              ((static_cast<int64_t>(view_create_info_ptr->component_order[3]) & ((1 << 3) - 1)) << 12) |
              ((static_cast<int64_t>(view_create_info_ptr->format)             & ((1 << 7) - 1)) << 15) |
              ((static_cast<int64_t>(view_create_info_ptr->n_base_layer)       & ((1 << 4) - 1)) << 22) |
              ((static_cast<int64_t>(view_create_info_ptr->n_base_mip)         & ((1 << 4) - 1)) << 26) |
              ((static_cast<int64_t>(view_create_info_ptr->n_layers)           & ((1 << 4) - 1)) << 30) |
              ((static_cast<int64_t>(view_create_info_ptr->n_mips)             & ((1 << 4) - 1)) << 34) |
              ((static_cast<int64_t>(view_create_info_ptr->type)               & ((1 << 4) - 1)) << 38));

    return result;
}

/** TODO */
PRIVATE void _ral_texture_init_mipmap_chain(_ral_texture* texture_ptr)
{
    uint32_t n_precall_layers = -1;

    /* Sanity checks */
    system_resizable_vector_get_property(texture_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_precall_layers);

    ASSERT_DEBUG_SYNC(n_precall_layers == 0,
                      "_ral_texture_init_mipmap_chain() called for a ral_texture instance with an initialized mip-map chain");

    ASSERT_DEBUG_SYNC(texture_ptr->n_layers >= 1,
                      "Invalid number of layers requested for a ral_texture instance.");

    /* Spawn mipmap descriptors for all layers */
    for (uint32_t n_layer = 0;
                  n_layer < texture_ptr->n_layers;
                ++n_layer)
    {
        uint32_t mipmap_width  = texture_ptr->base_mipmap_size[0];
        uint32_t mipmap_height = texture_ptr->base_mipmap_size[1];
        uint32_t mipmap_depth  = texture_ptr->base_mipmap_size[2];

        _ral_texture_layer* new_texture_layer_ptr = new (std::nothrow) _ral_texture_layer();

        ASSERT_ALWAYS_SYNC(new_texture_layer_ptr != nullptr,
                           "Out of memory");

        system_resizable_vector_push(texture_ptr->layers,
                                     new_texture_layer_ptr);

        for (uint32_t n_mipmap = 0;
                      n_mipmap < texture_ptr->n_mipmaps_per_layer;
                    ++n_mipmap)
        {
            _ral_texture_mipmap* new_texture_mipmap_ptr = new (std::nothrow) _ral_texture_mipmap(mipmap_width,
                                                                                                 mipmap_height,
                                                                                                 mipmap_depth);

            ASSERT_ALWAYS_SYNC(new_texture_mipmap_ptr != nullptr,
                               "Out of memory");

            system_resizable_vector_push(new_texture_layer_ptr->mipmaps,
                                         new_texture_mipmap_ptr);

            /* Update the mipmap size */
            mipmap_width  /= 2;
            mipmap_height /= (texture_ptr->type == RAL_TEXTURE_TYPE_1D        ||
                              texture_ptr->type == RAL_TEXTURE_TYPE_1D_ARRAY) ? 1
                                                                              : 2;
            mipmap_depth  /= (texture_ptr->type == RAL_TEXTURE_TYPE_3D)       ? 2
                                                                              : 1;

            if (mipmap_width < 1)
            {
                mipmap_width = 1;
            }

            if (mipmap_height < 1)
            {
                mipmap_height = 1;
            }

            if (mipmap_depth < 1)
            {
                mipmap_depth = 1;
            }
        }
    }
}

/** TODO */
PRIVATE void _ral_texture_mipmap_update_release_gfx_image_handler(ral_texture_mipmap_client_sourced_update_info* info_ptr)
{
    gfx_image_release( (gfx_image&)info_ptr->delete_handler_proc_user_arg);
}

/** TODO */
PRIVATE void _ral_texture_release(void* texture)
{
    /* Nothing to do - tear-down handled completely by the destructor */
}


/** Please see header for specification */
PUBLIC ral_texture ral_texture_create(ral_context                    context,
                                      const ral_texture_create_info* create_info_ptr)
{
    uint32_t      n_mipmaps  = 0;
    ral_texture   result     = nullptr;
    _ral_texture* result_ptr = nullptr;

    /* Sanity checks */
    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture_create_info is NULL");

        goto end;
    }

    /* Sanity checks */
    #ifdef _DEBUG
    {
        bool req_base_mipmap_depth_set_to_one           = false;
        bool req_base_mipmap_height_set_to_one          = false;
        bool req_base_mipmap_height_equals_mipmap_width = false;
        bool req_n_layers_mul_six                       = false;
        bool req_n_layers_set_to_one                    = false;
        bool req_n_layers_set_to_six                    = false;
        bool req_n_samples_set_to_one                   = false;

        switch (create_info_ptr->type)
        {
            case RAL_TEXTURE_TYPE_1D:
            {
                req_base_mipmap_depth_set_to_one  = true;
                req_base_mipmap_height_set_to_one = true;
                req_n_layers_set_to_one           = true;
                req_n_samples_set_to_one          = true;

                break;
            }

            case RAL_TEXTURE_TYPE_1D_ARRAY:
            {
                req_base_mipmap_depth_set_to_one  = true;
                req_base_mipmap_height_set_to_one = true;
                req_n_samples_set_to_one          = true;

                break;
            }

            case RAL_TEXTURE_TYPE_CUBE_MAP:
            {
                req_base_mipmap_depth_set_to_one           = true;
                req_base_mipmap_height_equals_mipmap_width = true;
                req_n_layers_set_to_six                    = true;

                break;
            }

            case RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY:
            {
                req_base_mipmap_depth_set_to_one           = true;
                req_base_mipmap_height_equals_mipmap_width = true;
                req_n_layers_mul_six                       = true;

                break;
            }

            case RAL_TEXTURE_TYPE_2D:
            {
                req_base_mipmap_depth_set_to_one = true;
                req_n_layers_set_to_one          = true;
                req_n_samples_set_to_one         = true;

                break;
            }

            case RAL_TEXTURE_TYPE_2D_ARRAY:
            {
                 req_base_mipmap_depth_set_to_one = true;
                 req_n_samples_set_to_one         = true;

                break;
            }

            case RAL_TEXTURE_TYPE_3D:
            {
                req_n_layers_set_to_one  = true;
                req_n_samples_set_to_one = true;

                break;
            }

            case RAL_TEXTURE_TYPE_MULTISAMPLE_2D:
            {
                req_base_mipmap_depth_set_to_one = true;
                req_n_layers_set_to_one          = true;

                break;
            }

            case RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY:
            {
                /* No requirements .. */
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL texture type");

                goto end;
            }
        }

        if (req_base_mipmap_depth_set_to_one)
        {
            if (create_info_ptr->base_mipmap_depth != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Base mipmap depth must be 1");

                goto end;
            }
        }
        else
        {
            if (create_info_ptr->base_mipmap_depth < 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Base mipmap depth must not be smaller than 1");

                goto end;
            }
        }

        if (req_base_mipmap_height_set_to_one)
        {
            if (create_info_ptr->base_mipmap_height != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Base mipmap height must be 1");

                goto end;
            }
        }
        else
        {
            if (create_info_ptr->base_mipmap_height < 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Base mipmap height must not be smaller than 1");

                goto end;
            }
        }

        if (req_base_mipmap_height_equals_mipmap_width)
        {
            if (create_info_ptr->base_mipmap_height != create_info_ptr->base_mipmap_width)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Base mipmap height must be equal to base mipmap width");

                goto end;
            }
        }

        if (req_n_layers_mul_six)
        {
            if ((create_info_ptr->n_layers % 6) != 0)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers is not a multiple of 6.");

                goto end;
            }
        }

        if (req_n_layers_set_to_one)
        {
            if (create_info_ptr->n_layers != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers is not 1.");

                goto end;
            }
        }

        if (req_n_layers_set_to_six)
        {
            if (create_info_ptr->n_layers != 6)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers is not 6.");

                goto end;
            }
        }

        if (req_n_samples_set_to_one)
        {
            if (create_info_ptr->n_samples != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Exactly one sample must be requested for the specified texture type");

                goto end;
            }
        }
        else
        {
            if (create_info_ptr->n_samples < 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "At least one sample must be requested for the specified texture type");

                goto end;
            }
        }
    }
    #endif /* _DEBUG */

    /* Determine the number of mipmaps we need. */
    if (create_info_ptr->use_full_mipmap_chain)
    {
        uint32_t max_size = create_info_ptr->base_mipmap_width;

        if (create_info_ptr->base_mipmap_height > max_size)
        {
            max_size = create_info_ptr->base_mipmap_height;
        }

        if (create_info_ptr->base_mipmap_height > max_size)
        {
            max_size = create_info_ptr->base_mipmap_height;
        }

        if (max_size != 0)
        {
            n_mipmaps = 1 + system_math_other_log2_uint32(max_size);
        }
        else
        {
            n_mipmaps = 1;
        }
    }
    else
    {
        n_mipmaps = 1;
    }

    /* Spawn the new descriptor */
    result_ptr = new (std::nothrow) _ral_texture(context,
                                                 create_info_ptr->base_mipmap_width,
                                                 create_info_ptr->base_mipmap_height,
                                                 create_info_ptr->base_mipmap_depth,
                                                 create_info_ptr->fixed_sample_locations,
                                                 create_info_ptr->format,
                                                 create_info_ptr->n_layers,
                                                 n_mipmaps,
                                                 create_info_ptr->n_samples,
                                                 create_info_ptr->unique_name,
                                                 create_info_ptr->description,
                                                 create_info_ptr->type,
                                                 create_info_ptr->usage);

    if (result_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    /* Set up the mipmap chain */
    _ral_texture_init_mipmap_chain(result_ptr);

    /* All done */
    result = (ral_texture) result_ptr;

end:
    return result;
}

#if 0

TODO: Move to ral_context, only use ral_texture_create_from_gfx_image().

/** Please see header for specification */
PUBLIC ral_texture ral_texture_create_from_file_name(ral_context               context,
                                                     system_hashed_ansi_string name,
                                                     system_hashed_ansi_string file_name,
                                                     ral_texture_usage_bits    usage,
                                                     bool                      async)
{
    gfx_image   new_gfx_image = nullptr;
    ral_texture result        = nullptr;

    /* Sanity checks */
    if (name      == nullptr ||
        file_name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "One or more input arguments are NULL");

        goto end;
    }

    if ((usage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)         != 0  ||
        (usage & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Trying to create a renderable ral_texture instance from a file-based source.");

        goto end;
    }

    /* Create a new gfx_image instance for the input file name */
    new_gfx_image = gfx_image_create_from_file(name,
                                               file_name,
                                               true); /* use_alternative_filename_getter */

    if (new_gfx_image == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a gfx_image instance for file [%s]",
                          system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Now try to set up a ogl_texture instance using the gfx_image we created. */
    result = ral_texture_create_from_gfx_image(context,
                                               name,
                                               new_gfx_image,
                                               usage,
                                               async);

    if (result == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a ral_texture from a gfx_image instance created for file [%s]",
                          system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Cache the filename */
    ((_ral_texture*) result)->file_name = file_name;

    /* Release the gfx_image instance - we don't need it anymore. */
    gfx_image_release(new_gfx_image);
    new_gfx_image = nullptr;

    /* All done */
end:
    return result;
}

#endif

/** Please see header for specification */
PUBLIC ral_texture ral_texture_create_from_gfx_image(ral_context               context,
                                                     system_hashed_ansi_string name,
                                                     gfx_image                 image,
                                                     ral_texture_usage_bits    usage)
{
    system_hashed_ansi_string base_image_file_name    = nullptr;
    unsigned int              base_image_height       = 0;
    unsigned int              base_image_width        = 0;
    ral_format                image_format            = RAL_FORMAT_UNKNOWN;
    unsigned int              image_n_mipmaps         = 0;
    ral_texture               result                  = nullptr;
    ral_texture_create_info   result_create_info;

    /* Sanity checks */
    if (image == nullptr ||
        name  == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "One or more input arguments are NULL");

        goto end;
    }

    if ((usage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)         != 0  ||
        (usage & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Trying to create a renderable ral_texture instance from a gfx_image.");

        goto end;
    }

    /* Preallocate mipmap update info array. We're going to need it to upload mipmap data
     * to the ral_texture instance. */
    gfx_image_get_property       (image,
                                  GFX_IMAGE_PROPERTY_N_MIPMAPS,
                                 &image_n_mipmaps);
    gfx_image_get_mipmap_property(image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                 &base_image_height);
    gfx_image_get_mipmap_property(image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_FORMAT_RAL,
                                 &image_format);
    gfx_image_get_mipmap_property(image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                 &base_image_width);

    if (image_n_mipmaps == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "gfx_image container specifies zero mipmaps?");

        goto end;
    }

    /* Set up the ral_texture instance */
    result_create_info.base_mipmap_depth      = 1;
    result_create_info.base_mipmap_height     = base_image_height;
    result_create_info.base_mipmap_width      = base_image_width;
    result_create_info.description            = nullptr;
    result_create_info.fixed_sample_locations = false;
    result_create_info.format                 = image_format;
    result_create_info.n_layers               = 1;
    result_create_info.n_samples              = 1;
    result_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    result_create_info.unique_name            = name;
    result_create_info.usage                  = usage;
    result_create_info.use_full_mipmap_chain  = (image_n_mipmaps > 1) ? true : false;

    result = ral_texture_create(context,
                               &result_create_info);

    if (result == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "ral_texture_create() failed.");
    }

    /* All done */
end:

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_generate_mipmaps(ral_texture texture,
                                                     bool        async)
{
    bool          result      = false;
    _ral_texture* texture_ptr = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    /* Fire a notification, so that the backend can handle the request */
    _ral_texture_mipmap_generation_requested_callback_arg callback_arg;

    callback_arg.async   = async;
    callback_arg.texture = texture;

    system_callback_manager_call_back(texture_ptr->callback_manager,
                                      RAL_TEXTURE_CALLBACK_ID_MIPMAP_GENERATION_REQUESTED,
                                     &callback_arg);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task ral_texture_get_generate_mips_present_task(ral_texture texture)
{
    ral_command_buffer               cmd_buffer;
    ral_command_buffer_create_info   cmd_buffer_create_info;
    ral_present_task                 result               = nullptr;
    ral_present_task_gpu_create_info task_create_info;
    ral_present_task_io              task_io;
    _ral_texture*                    texture_ptr          = reinterpret_cast<_ral_texture*>(texture);
    ral_texture_view                 texture_view         = nullptr;

    ASSERT_DEBUG_SYNC(texture_ptr != nullptr,
                      "Input ral_texture instance is null");
    ASSERT_DEBUG_SYNC(texture_ptr->n_layers == 1,
                      "TODO: Parallelize mip generation, so that each mipchain is generated in parallel.");
    ASSERT_DEBUG_SYNC(texture_ptr->n_samples == 1,
                      "Cannot generate mipchain for a MS texture");

    /* Record a command buffer for the task */
    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT | RAL_QUEUE_GRAPHICS_BIT;
    cmd_buffer_create_info.is_executable                           = true;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(texture_ptr->context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &cmd_buffer);

    ASSERT_DEBUG_SYNC(cmd_buffer != nullptr,
                      "Failed to create a ral_command_buffer instance.");

    ral_command_buffer_start_recording(cmd_buffer);
    {
        ral_texture_aspect           format_aspects           = static_cast<ral_texture_aspect>(0);
        bool                         format_has_color_comps   = false;
        bool                         format_has_depth_comps   = false;
        bool                         format_has_stencil_comps = false;
        ral_texture_view_create_info texture_view_create_info;

        ral_utils_get_format_property(texture_ptr->format,
                                      RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                     &format_has_color_comps);
        ral_utils_get_format_property(texture_ptr->format,
                                      RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                     &format_has_depth_comps);
        ral_utils_get_format_property(texture_ptr->format,
                                      RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                     &format_has_depth_comps);

        format_aspects = static_cast<ral_texture_aspect>(((format_has_color_comps)   ? RAL_TEXTURE_ASPECT_COLOR_BIT   : 0) |
                                                         ((format_has_depth_comps)   ? RAL_TEXTURE_ASPECT_DEPTH_BIT   : 0) |
                                                         ((format_has_stencil_comps) ? RAL_TEXTURE_ASPECT_STENCIL_BIT : 0));

        texture_view_create_info = ral_texture_view_create_info(texture);
        texture_view             = ral_texture_get_view(&texture_view_create_info);

        for (uint32_t n_layer = 0;
                      n_layer < texture_ptr->n_layers;
                    ++n_layer)
        {
            uint32_t next_mip_size[3];
            uint32_t this_mip_size[3];

            ral_texture_get_mipmap_property(texture,
                                            n_layer,
                                            0, /* n_mipmap */
                                            RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                            next_mip_size);
            ral_texture_get_mipmap_property(texture,
                                            n_layer,
                                            0, /* n_mipmap */
                                            RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                            next_mip_size + 1);
            ral_texture_get_mipmap_property(texture,
                                            n_layer,
                                            0, /* n_mipmap */
                                            RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                            next_mip_size + 2);

            for (uint32_t n_mip = 0;
                          n_mip < texture_ptr->n_mipmaps_per_layer - 1;
                        ++n_mip)
            {
                ral_command_buffer_copy_texture_to_texture_command_info command;

                memcpy(this_mip_size,
                       next_mip_size,
                       sizeof(this_mip_size) );

                ral_texture_get_mipmap_property(texture,
                                                n_layer,
                                                n_mip + 1,
                                                RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                                next_mip_size);
                ral_texture_get_mipmap_property(texture,
                                                n_layer,
                                                n_mip + 1,
                                                RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                                next_mip_size + 1);
                ral_texture_get_mipmap_property(texture,
                                                n_layer,
                                                n_mip + 1,
                                                RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                                next_mip_size + 2);

                command.aspect           = format_aspects;
                command.dst_size[0]      = next_mip_size[0];
                command.dst_size[1]      = next_mip_size[1];
                command.dst_size[2]      = next_mip_size[2];
                command.dst_start_xyz[0] = 0;
                command.dst_start_xyz[1] = 0;
                command.dst_start_xyz[2] = 0;
                command.dst_texture_view = texture_view;
                command.n_dst_layer      = n_layer;
                command.n_dst_mipmap     = n_mip + 1;
                command.n_src_layer      = n_layer;
                command.n_src_mipmap     = n_mip;
                command.scaling_filter   = RAL_TEXTURE_FILTER_LINEAR;
                command.src_size[0]      = this_mip_size[0];
                command.src_size[1]      = this_mip_size[1];
                command.src_size[2]      = this_mip_size[2];
                command.src_start_xyz[0] = 0;
                command.src_start_xyz[1] = 0;
                command.src_start_xyz[2] = 0;
                command.src_texture_view = texture_view;

                ral_command_buffer_record_copy_texture_to_texture(cmd_buffer,
                                                                  1, /* n_copy_ops */
                                                                 &command);
            }
        }
    }
    ral_command_buffer_stop_recording(cmd_buffer);

    /* Bake the task */
    task_io.object_type = RAL_CONTEXT_OBJECT_TYPE_TEXTURE;
    task_io.texture     = texture;

    task_create_info.command_buffer   = cmd_buffer;
    task_create_info.n_unique_inputs  = 1;
    task_create_info.n_unique_outputs = 1;
    task_create_info.unique_inputs    = &task_io;
    task_create_info.unique_outputs   = &task_io;

    result = ral_present_task_create_gpu(system_hashed_ansi_string_create("Texture mip gen task"),
                                        &task_create_info);

    /* Clean up */
    ral_context_delete_objects(texture_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&cmd_buffer) );

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_get_mipmap_property(ral_texture                 texture,
                                                        uint32_t                    n_layer,
                                                        uint32_t                    n_mipmap,
                                                        ral_texture_mipmap_property mipmap_property,
                                                        void*                       out_result_ptr)
{
    bool                 result             = false;
    _ral_texture_layer*  texture_layer_ptr  = nullptr;
    _ral_texture_mipmap* texture_mipmap_ptr = nullptr;
    _ral_texture*        texture_ptr        = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture is NULL");

        goto end;
    }

    if (n_mipmap >= texture_ptr->n_mipmaps_per_layer)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid mipmap requested ([%d] > [%d] = preallocated number of mipmaps for the texture) )",
                          n_mipmap,
                          texture_ptr->n_mipmaps_per_layer);

        goto end;
    }

    if (!system_resizable_vector_get_element_at(texture_ptr->layers,
                                                n_layer,
                                               &texture_layer_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve descriptor of the layer at index [%d]",
                          n_layer);

        goto end;
    }

    if (!system_resizable_vector_get_element_at(texture_layer_ptr->mipmaps,
                                                n_mipmap,
                                               &texture_mipmap_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve mip-map descriptor for a mip-map at index [%d]",
                          n_mipmap);

        goto end;
    }

    /* Retrieve the requested value */
    result = true;

    switch (mipmap_property)
    {
        case RAL_TEXTURE_MIPMAP_PROPERTY_CONTENTS_SET:
        {
            *(bool*) out_result_ptr = texture_mipmap_ptr->contents_set;

            break;
        }

        case RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH:
        {
            *(uint32_t*) out_result_ptr = texture_mipmap_ptr->depth;

            break;
        }

        case RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT:
        {
            *(uint32_t*) out_result_ptr = texture_mipmap_ptr->height;

            break;
        }

        case RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH:
        {
            *(uint32_t*) out_result_ptr = texture_mipmap_ptr->width;
            
            break;
        }

        default:
        {
            result = false;

            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_mipmap_property value.");

            goto end;
        }
    }

    /* All done. */

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_get_property(const ral_texture    texture,
                                                 ral_texture_property property,
                                                 void*                out_result_ptr)
{
    bool          result      = false;
    _ral_texture* texture_ptr = reinterpret_cast<_ral_texture*>(texture);

    /* Sanity checks */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture is NULL");

        goto end;
    }

    /* Retrieve the requested property value. */
    result = true;

    switch (property)
    {
        case RAL_TEXTURE_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = texture_ptr->callback_manager;

            break;
        }

        case RAL_TEXTURE_PROPERTY_CONTEXT:
        {
            *(ral_context*) out_result_ptr = texture_ptr->context;

            break;
        }

        case RAL_TEXTURE_PROPERTY_CREATE_INFO:
        {
            ral_texture_create_info* create_info_ptr = (ral_texture_create_info*) out_result_ptr;

            create_info_ptr->base_mipmap_depth      = texture_ptr->base_mipmap_size[2];
            create_info_ptr->base_mipmap_height     = texture_ptr->base_mipmap_size[1];
            create_info_ptr->base_mipmap_width      = texture_ptr->base_mipmap_size[0];
            create_info_ptr->description            = texture_ptr->description;
            create_info_ptr->fixed_sample_locations = texture_ptr->fixed_sample_locations;
            create_info_ptr->format                 = texture_ptr->format;
            create_info_ptr->n_layers               = texture_ptr->n_layers;
            create_info_ptr->n_samples              = texture_ptr->n_samples;
            create_info_ptr->type                   = texture_ptr->type;
            create_info_ptr->unique_name            = texture_ptr->unique_name;
            create_info_ptr->usage                  = texture_ptr->usage;
            create_info_ptr->use_full_mipmap_chain  = (texture_ptr->n_mipmaps_per_layer > 1);

            break;
        }

        case RAL_TEXTURE_PROPERTY_DESCRIPTION:
        {
            *(system_hashed_ansi_string*) out_result_ptr = texture_ptr->description;

            break;
        }

        case RAL_TEXTURE_PROPERTY_FILENAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = texture_ptr->file_name;

            break;
        }

        case RAL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS:
        {
            *(bool*) out_result_ptr = texture_ptr->fixed_sample_locations;

            break;
        }

        case RAL_TEXTURE_PROPERTY_FORMAT:
        {
            *(ral_format*) out_result_ptr = texture_ptr->format;

            break;
        }

        case RAL_TEXTURE_PROPERTY_IS_BEING_RELEASED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = texture_ptr->being_released;

            break;
        }

        case RAL_TEXTURE_PROPERTY_N_LAYERS:
        {
            system_resizable_vector_get_property(texture_ptr->layers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_TEXTURE_PROPERTY_N_MIPMAPS:
        {
            *(uint32_t*) out_result_ptr = texture_ptr->n_mipmaps_per_layer;

            break;
        }

        case RAL_TEXTURE_PROPERTY_N_SAMPLES:
        {
            *(uint32_t*) out_result_ptr = texture_ptr->n_samples;

            break;
        }

        case RAL_TEXTURE_PROPERTY_TYPE:
        {
            *(ral_texture_type*) out_result_ptr = texture_ptr->type;

            break;
        }

        case RAL_TEXTURE_PROPERTY_UNIQUE_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = texture_ptr->unique_name;

            break;
        }

        case RAL_TEXTURE_PROPERTY_USAGE:
        {
            *(ral_texture_usage_bits*) out_result_ptr = texture_ptr->usage;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_property value.");

            result = false;
        }
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_texture_view ral_texture_get_view(const ral_texture_view_create_info* create_info_ptr)
{
    PFNRALCONTEXTCREATETEXTUREVIEWPROC pfn_create_texture_view_ptr = nullptr;
    ral_texture_view                   result                      = nullptr;
    _ral_texture*                      texture_ptr                 = nullptr;
    system_hash64                      view_hash;

    /* Sanity checks */
    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr == nullptr),
                          "Input ral_texture_view_create_info instance is null");

        goto end;
    }

    texture_ptr = reinterpret_cast<_ral_texture*>(create_info_ptr->texture);

    system_critical_section_enter(texture_ptr->views_cs);

    /* If the specified view has already been created, return it immediately. */
    view_hash = _ral_texture_get_view_hash(create_info_ptr);

    if (system_hash64map_get(texture_ptr->views,
                             view_hash,
                            &result) )
    {
        ASSERT_DEBUG_SYNC(result != nullptr,
                          "Null ral_texture_view instance cached for a ral_texture instance");

        goto end;
    }

    /* If not, we need to create a new view. We also retain the object, since ultimately this texture
     * should have the last word in terms of whether or not a given view can be safely released */
    ral_context_get_private_property(texture_ptr->context,
                                     RAL_CONTEXT_PRIVATE_PROPERTY_CREATE_TEXTURE_VIEW_FUNC_PTR,
                                    &pfn_create_texture_view_ptr);

    pfn_create_texture_view_ptr(texture_ptr->context,
                                1, /* n_texture_views */
                                create_info_ptr,
                               &result);

    ASSERT_DEBUG_SYNC(result != nullptr,
                      "Texture view failed to create");

    system_hash64map_insert(texture_ptr->views,
                            view_hash,
                            result,
                            nullptr,  /* on_removal_callback          */
                            nullptr); /* on_removal_callback_user_arg */

end:
    if (create_info_ptr != nullptr)
    {
        system_critical_section_leave(texture_ptr->views_cs);
    }

    return result;
}

/** Please see header for specification */
PUBLIC void ral_texture_release(ral_texture& texture)
{
    delete (_ral_texture*) texture;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_set_mipmap_data_from_client_memory(ral_texture                                                     texture,
                                                                       uint32_t                                                        n_updates,
                                                                       std::shared_ptr<ral_texture_mipmap_client_sourced_update_info>* updates,
                                                                       bool                                                            async)
{
    _ral_texture_client_memory_source_update_requested_callback_arg callback_arg;
    uint32_t                                                        n_texture_layers = 0;
    bool                                                            result          = false;
    _ral_texture*                                                   texture_ptr     = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    if (n_updates == 0)
    {
        result = true;

        goto end;
    }

    if (updates == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input update descriptor array is NULL");

        goto end;
    }

    #ifdef _DEBUG
    {
        uint32_t n_texture_dimensions = 0;

        ral_utils_get_texture_type_property(texture_ptr->type,
                                            RAL_TEXTURE_TYPE_PROPERTY_N_DIMENSIONS,
                                           &n_texture_dimensions);
        system_resizable_vector_get_property(texture_ptr->layers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_texture_layers);

        for (uint32_t n_update = 0;
                      n_update < n_updates;
                    ++n_update)
        {
            uint32_t            n_texture_layer_mipmaps = 0;
            _ral_texture_layer* texture_layer_ptr       = nullptr;

            /* Is valid data provided for texture dimensions required by the specified texture's type? */
            for (uint32_t n_texture_dimension = 0;
                          n_texture_dimension < n_texture_dimensions;
                        ++n_texture_dimension)
            {
                if (updates[n_update]->region_size[n_texture_dimension] == 0)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "%d%s component of the region size is 0 which is invalid, given the target texture type.",
                                      n_texture_dimension,
                                      (n_texture_dimension == 1) ? "st" : "th");

                    goto end;
                }
            } /* for (all used texture dimensions) */

            /* Are zeros set for unused texture dimensions? */
            for (uint32_t n_texture_dimension = n_texture_dimensions;
                          n_texture_dimension < 3; /* x, y, z */
                        ++n_texture_dimension)
            {
                if (updates[n_update]->region_size[n_texture_dimension] != 0)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "%d%s component of the region size is not 0 which is invalid, given the target texture type.",
                                      n_texture_dimension,
                                      (n_texture_dimension == 1) ? "st" : "th");

                    goto end;
                }
            } /* for (all unused texture dimensions) */

            /* Is the layer index correct? */
            if (updates[n_update]->n_layer >= n_texture_layers)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Update descriptor at index [%d] refers to a non-existing layer.",
                                  n_update);

                goto end;
            }

            /* Is the requested mipmap index valid? */
            if (!system_resizable_vector_get_element_at(texture_ptr->layers,
                                                        updates[n_update]->n_layer,
                                                       &texture_layer_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve texture layer descriptor.");

                goto end;
            }

            system_resizable_vector_get_property(texture_layer_ptr->mipmaps,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_texture_layer_mipmaps);

            if (updates[n_update]->n_mipmap >= n_texture_layer_mipmaps)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Update descriptor at index [%d] refers to a non-existing mipmap.",
                                  n_update);

                goto end;
            }
        }
    }
    #endif /* _DEBUG */

    /* Fire a notification, so that the listening backend can execute the request. */
    callback_arg.async   = async;
    callback_arg.texture = texture;

    for (uint32_t n_update = 0;
                  n_update < n_updates;
                ++n_update)
    {
        callback_arg.updates.push_back(updates[n_update]);
    }

    system_callback_manager_call_back(texture_ptr->callback_manager,
                                      RAL_TEXTURE_CALLBACK_ID_CLIENT_MEMORY_SOURCE_UPDATE_REQUESTED,
                                     &callback_arg);

    /* Mark the mipmaps as set. This is a simplified approach, since the updates might have only touched
     * mipmap storage partially, but it's enough for scene loaders */
    for (uint32_t n_update = 0;
                  n_update < n_updates;
                ++n_update)
    {
        _ral_texture_layer*  texture_layer_ptr  = nullptr;
        _ral_texture_mipmap* texture_mipmap_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(texture_ptr->layers,
                                                    updates[n_update]->n_layer,
                                                   &texture_layer_ptr) )
       {
           ASSERT_DEBUG_SYNC(false,
                             "Could not retrieve texture layer descriptor.");

           continue;
       }

       if (!system_resizable_vector_get_element_at(texture_layer_ptr->mipmaps,
                                                   updates[n_update]->n_mipmap,
                                                  &texture_mipmap_ptr) )
       {
           ASSERT_DEBUG_SYNC(false,
                             "Could not retrieve texture layer mipmap descriptor.");

           continue;
       }

       texture_mipmap_ptr->contents_set = true;
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC void ral_texture_set_property(ral_texture          texture,
                                     ral_texture_property property,
                                     const void*          data)
{
    _ral_texture* texture_ptr = (_ral_texture*) texture;

    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(texture != nullptr,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAL_TEXTURE_PROPERTY_DESCRIPTION:
        {
            texture_ptr->description = *(system_hashed_ansi_string*) data;

            break;
        }

        case RAL_TEXTURE_PROPERTY_FILENAME:
        {
            texture_ptr->file_name = *(system_hashed_ansi_string*) data;

            break;
        }

        case RAL_TEXTURE_PROPERTY_UNIQUE_NAME:
        {
            texture_ptr->unique_name = *(system_hashed_ansi_string*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported ral_texture_property value requested.");
        }
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_texture_upload_data_from_gfx_image(ral_texture texture,
                                                   gfx_image   image,
                                                   bool        async)
{
    /* Upload the mipmaps */
    unsigned int                                                    image_n_mipmaps         = 0;
    std::shared_ptr<ral_texture_mipmap_client_sourced_update_info>* mipmap_update_info_ptrs = nullptr;
    _ral_texture*                                                   texture_ptr             = reinterpret_cast<_ral_texture*>(texture);

    gfx_image_get_property(image,
                           GFX_IMAGE_PROPERTY_N_MIPMAPS,
                          &image_n_mipmaps);

    mipmap_update_info_ptrs = new (std::nothrow) std::shared_ptr<ral_texture_mipmap_client_sourced_update_info>[image_n_mipmaps];

    if (mipmap_update_info_ptrs == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    for (uint32_t n_mipmap = 0;
                  n_mipmap < image_n_mipmaps;
                ++n_mipmap)
    {
        uint32_t              mipmap_data_height        = 0;
        void*                 mipmap_data_ptr           = nullptr;
        uint32_t              mipmap_data_row_alignment = 0;
        uint32_t              mipmap_data_size          = 0;
        ral_texture_data_type mipmap_data_type          = RAL_TEXTURE_DATA_TYPE_UNKNOWN;
        uint32_t              mipmap_data_width         = 0;

        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
                                     &mipmap_data_ptr);
        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                     &mipmap_data_height);
        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_ROW_ALIGNMENT,
                                     &mipmap_data_row_alignment);
        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_SIZE,
                                     &mipmap_data_size);
        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_TYPE,
                                     &mipmap_data_type);
        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                     &mipmap_data_width);

        mipmap_update_info_ptrs[n_mipmap].reset(new ral_texture_mipmap_client_sourced_update_info() );

        mipmap_update_info_ptrs[n_mipmap]->data                   = mipmap_data_ptr;
        mipmap_update_info_ptrs[n_mipmap]->data_row_alignment     = mipmap_data_row_alignment;
        mipmap_update_info_ptrs[n_mipmap]->data_size              = mipmap_data_size;
        mipmap_update_info_ptrs[n_mipmap]->data_type              = mipmap_data_type;
        mipmap_update_info_ptrs[n_mipmap]->n_layer                = 0;
        mipmap_update_info_ptrs[n_mipmap]->n_mipmap               = n_mipmap;
        mipmap_update_info_ptrs[n_mipmap]->region_size[0]         = mipmap_data_width;
        mipmap_update_info_ptrs[n_mipmap]->region_size[1]         = mipmap_data_height;
        mipmap_update_info_ptrs[n_mipmap]->region_size[2]         = 0;
        mipmap_update_info_ptrs[n_mipmap]->region_start_offset[0] = 0;
        mipmap_update_info_ptrs[n_mipmap]->region_start_offset[1] = 0;
        mipmap_update_info_ptrs[n_mipmap]->region_start_offset[2] = 0;

        mipmap_update_info_ptrs[n_mipmap]->pfn_delete_handler_proc      = _ral_texture_mipmap_update_release_gfx_image_handler;
        mipmap_update_info_ptrs[n_mipmap]->delete_handler_proc_user_arg = image;

        gfx_image_retain(image);
    }

    if (!ral_texture_set_mipmap_data_from_client_memory(texture,
                                                        image_n_mipmaps,
                                                        mipmap_update_info_ptrs,
                                                        async) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to set mipmap data for a ral_texture instance deriving from a gfx_image instance.");
    }

end:
    if (mipmap_update_info_ptrs != nullptr)
    {
        delete [] mipmap_update_info_ptrs;

        mipmap_update_info_ptrs = nullptr;
    }

}
