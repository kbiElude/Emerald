/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_image.h"
#include "ral/ral_texture.h"
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
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
        if (mipmaps != NULL)
        {
            _ral_texture_mipmap* current_mipmap_ptr = NULL;

            while (system_resizable_vector_pop(mipmaps,
                                              &current_mipmap_ptr) )
            {
                delete current_mipmap_ptr;
                current_mipmap_ptr = NULL;
            }

            system_resizable_vector_release(mipmaps);
            mipmaps = NULL;
        } /* if (mipmaps != NULL) */
    }
} _ral_texture_layer;

typedef struct _ral_texture
{
    system_callback_manager   callback_manager;
    bool                      fixed_sample_locations;
    ral_texture_format        format;
    uint32_t                  n_mipmaps_per_layer;
    uint32_t                  n_samples;
    system_hashed_ansi_string name;
    ral_texture_type          type;
    ral_texture_usage_bits    usage;

    /* Holds _ral_texture_layer instances.
     *
     * 1D/2D/3D textures = 1 layer; CM = 6 layer; CM array = 6n layers;
     */
    system_resizable_vector layers;

    REFCOUNT_INSERT_VARIABLES;


    _ral_texture(bool                      in_fixed_sample_locations,
                 ral_texture_format        in_format,
                 uint32_t                  in_n_layers,
                 uint32_t                  in_n_mipmaps,
                 uint32_t                  in_n_samples,
                 system_hashed_ansi_string in_name,
                 ral_texture_type          in_type,
                 ral_texture_usage_bits    in_usage)
    {
        callback_manager       = system_callback_manager_create( (_callback_id) RAL_TEXTURE_CALLBACK_ID_COUNT);
        fixed_sample_locations = in_fixed_sample_locations;
        format                 = in_format;
        layers                 = system_resizable_vector_create(4 /* capacity */);
        n_mipmaps_per_layer    = in_n_mipmaps;
        n_samples              = n_samples;
        name                   = in_name;
        type                   = in_type;
        usage                  = in_usage;
    }

    ~_ral_texture()
    {
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */

        if (layers != NULL)
        {
            _ral_texture_layer* current_layer_ptr = NULL;

            while (system_resizable_vector_pop(layers,
                                              &current_layer_ptr) )
            {
                delete current_layer_ptr;

                current_layer_ptr = NULL;
            } /* while (there are mipmap vectors to pop) */

            system_resizable_vector_release(layers);
            layers = NULL;
        } /* if (layers != NULL) */
    }
} _ral_texture;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_texture,
                               ral_texture,
                              _ral_texture);


/** TODO */
PRIVATE void _ral_texture_release(void* texture)
{
    /* Nothing to do - tear-down handled completely by the destructor */
}


/** Please see header for specification */
PUBLIC ral_texture ral_texture_create(system_hashed_ansi_string      name,
                                      const ral_texture_create_info* create_info_ptr)
{
    uint32_t      n_mipmaps  = 0;
    ral_texture   result     = NULL;
    _ral_texture* result_ptr = NULL;

    /* Sanity checks */
    if (name            == NULL ||
        create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "One or more of the input variables are NULL");

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
        } /* switch (create_info_ptr->type) */

        if (req_base_mipmap_depth_set_to_one)
        {
            if (create_info_ptr->base_mipmap_depth != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Base mipmap depth must be 1");

                goto end;
            }
        } /* if (req_base_mipmap_depth_set_to_one) */
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
        } /* if (req_base_mipmap_height_set_to_one) */
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
        } /* if (req_base_mipmap_height_equals_mipmap_width) */

        if (req_n_layers_mul_six)
        {
            if ((create_info_ptr->n_layers % 6) != 0)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers is not a multiple of 6.");

                goto end;
            }
        } /* if (req_n_layers_mul_six) */

        if (req_n_layers_set_to_one)
        {
            if (create_info_ptr->n_layers != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers is not 1.");

                goto end;
            }
        } /* if (req_n_layers_set_to_one) */

        if (req_n_layers_set_to_six)
        {
            if (create_info_ptr->n_layers != 6)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers is not 6.");

                goto end;
            }
        } /* if (req_n_layers_set_to_six) */

        if (req_n_samples_set_to_one)
        {
            if (create_info_ptr->n_samples != 1)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Exactly one sample must be requested for the specified texture type");

                goto end;
            }
        } /* if (req_n_samples_set_to_one) */
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
    } /* if (create_info_ptr->use_full_mipmap_chain) */

    /* Spawn the new descriptor */
    result_ptr = new (std::nothrow) _ral_texture(create_info_ptr->fixed_sample_locations,
                                                 create_info_ptr->format,
                                                 create_info_ptr->n_layers,
                                                 n_mipmaps,
                                                 create_info_ptr->n_samples,
                                                 name,
                                                 create_info_ptr->type,
                                                 create_info_ptr->usage);

    if (result_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    /* Register in the object manager */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                   _ral_texture_release,
                                                   OBJECT_TYPE_RAL_TEXTURE,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Textures\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* All done */
    result = (ral_texture) result_ptr;

end:
    return result;
}

/** Please see header for specification */
PUBLIC ral_texture ral_texture_create_from_file_name(system_hashed_ansi_string name,
                                                     system_hashed_ansi_string file_name,
                                                     ral_texture_usage_bits    usage)
{
    gfx_image   new_gfx_image = NULL;
    ral_texture result        = NULL;

    /* Sanity checks */
    if (name      == NULL ||
        file_name == NULL)
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

    if (new_gfx_image == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a gfx_image instance for file [%s]",
                          system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Now try to set up a ogl_texture instance using the gfx_image we created. */
    result = ral_texture_create_from_gfx_image(name,
                                               new_gfx_image,
                                               usage);

    if (result == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a ral_texture from a gfx_image instance created for file [%s]",
                          system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Release the gfx_image instance - we don't need it anymore. */
    gfx_image_release(new_gfx_image);
    new_gfx_image = NULL;

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC ral_texture ral_texture_create_from_gfx_image(system_hashed_ansi_string name,
                                                     gfx_image                 image,
                                                     ral_texture_usage_bits    usage)
{
    system_hashed_ansi_string                      base_image_file_name    = NULL;
    unsigned int                                   base_image_height       = 0;
    unsigned int                                   base_image_width        = 0;
    ral_texture_format                             image_format            = RAL_TEXTURE_FORMAT_UNKNOWN;
    bool                                           image_is_compressed     = false;
    unsigned int                                   image_n_mipmaps         = 0;
    ral_texture_mipmap_client_sourced_update_info* mipmap_update_info_ptrs = NULL;
    ral_texture                                    result                  = NULL;
    ral_texture_create_info                        result_create_info;
    _ral_texture*                                  result_ptr              = NULL;

    /* Sanity checks */
    if (image == NULL ||
        name  == NULL)
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
                                  GFX_IMAGE_MIPMAP_PROPERTY_IS_COMPRESSED,
                                 &image_is_compressed);
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

    mipmap_update_info_ptrs = new (std::nothrow) ral_texture_mipmap_client_sourced_update_info[image_n_mipmaps];

    if (mipmap_update_info_ptrs == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    } /* if (mipmap_update_info_ptrs == NULL) */

    /* Set up the ral_texture instance */
    result_create_info.base_mipmap_depth      = 1;
    result_create_info.base_mipmap_height     = base_image_height;
    result_create_info.base_mipmap_width      = base_image_width;
    result_create_info.fixed_sample_locations = false;
    result_create_info.format                 = image_format;
    result_create_info.n_layers               = 1;
    result_create_info.n_samples              = 1;
    result_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    result_create_info.usage                  = usage;
    result_create_info.use_full_mipmap_chain  = (image_n_mipmaps > 1) ? true : false;

    result = ral_texture_create(name,
                               &result_create_info);

    if (result == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "ral_texture_create() failed.");

        goto end;
    }

    /* Upload the mipmaps */
    for (uint32_t n_mipmap = 0;
                  n_mipmap < image_n_mipmaps;
                ++n_mipmap)
    {
        void*                 mipmap_data_ptr           = NULL;
        uint32_t              mipmap_data_row_alignment = 0;
        uint32_t              mipmap_data_size          = 0;
        ral_texture_data_type mipmap_data_type          = RAL_TEXTURE_DATA_TYPE_UNKNOWN;

        gfx_image_get_mipmap_property(image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
                                     &mipmap_data_ptr);
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

        mipmap_update_info_ptrs[n_mipmap].data               = mipmap_data_ptr;
        mipmap_update_info_ptrs[n_mipmap].data_row_alignment = mipmap_data_row_alignment;
        mipmap_update_info_ptrs[n_mipmap].data_size          = mipmap_data_size;
        mipmap_update_info_ptrs[n_mipmap].data_type          = mipmap_data_type;
        mipmap_update_info_ptrs[n_mipmap].n_layer            = 0;
        mipmap_update_info_ptrs[n_mipmap].n_mipmap           = n_mipmap;
    } /* for (all mipmap descriptors) */

    if (!ral_texture_set_mipmap_data_from_client_memory(result,
                                                        image_n_mipmaps,
                                                        mipmap_update_info_ptrs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to set mipmap data for a ral_texture instance deriving from a gfx_image instance.");

        ral_texture_release(result);
        result = NULL;
    } /* if (!result) */

    /* All done */
end:
    if (mipmap_update_info_ptrs != NULL)
    {
        delete [] mipmap_update_info_ptrs;

        mipmap_update_info_ptrs = NULL;
    }
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_generate_mipmaps(ral_texture texture)
{
    bool          result      = false;
    _ral_texture* texture_ptr = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    /* Fire a notification, so that the backend can handle the request */
    system_callback_manager_call_back(texture_ptr->callback_manager,
                                      RAL_TEXTURE_CALLBACK_ID_MIPMAP_GENERATION_REQUESTED,
                                      texture);

    /* All done */
    result = true;

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
    _ral_texture_layer*  texture_layer_ptr  = NULL;
    _ral_texture_mipmap* texture_mipmap_ptr = NULL;
    _ral_texture*        texture_ptr        = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == NULL)
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
    } /* switch (mipmap_property) */

    /* All done. */

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_get_property(ral_texture          texture,
                                                 ral_texture_property property,
                                                 void*                out_result_ptr)
{
    bool          result      = false;
    _ral_texture* texture_ptr = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == NULL)
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

        case RAL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS:
        {
            *(bool*) out_result_ptr = texture_ptr->fixed_sample_locations;

            break;
        }

        case RAL_TEXTURE_PROPERTY_FORMAT:
        {
            *(ral_texture_format*) out_result_ptr = texture_ptr->format;

            break;
        }

        case RAL_TEXTURE_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = texture_ptr->name;

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
    } /* switch (property) */

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_texture_set_mipmap_data_from_client_memory(ral_texture                                          texture,
                                                                       uint32_t                                             n_updates,
                                                                       const ral_texture_mipmap_client_sourced_update_info* updates)
{
    uint32_t      n_texture_layers = 0;
    bool          result          = false;
    _ral_texture* texture_ptr     = (_ral_texture*) texture;

    /* Sanity checks */
    if (texture == NULL)
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

    if (updates == NULL)
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
            _ral_texture_layer* texture_layer_ptr       = NULL;

            /* Is valid data provided for texture dimensions required by the specified texture's type? */
            for (uint32_t n_texture_dimension = 0;
                          n_texture_dimension < n_texture_dimensions;
                        ++n_texture_dimension)
            {
                if (updates[n_update].region_size[n_texture_dimension] == 0)
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
                if (updates[n_update].region_size[n_texture_dimension] != 0)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "%d%s component of the region size is not 0 which is invalid, given the target texture type.",
                                      n_texture_dimension,
                                      (n_texture_dimension == 1) ? "st" : "th");

                    goto end;
                }
            } /* for (all unused texture dimensions) */

            /* Is the layer index correct? */
            if (updates[n_update].n_layer >= n_texture_layers)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Update descriptor at index [%d] refers to a non-existing layer.",
                                  n_update);

                goto end;
            }

            /* Is the requested mipmap index valid? */
            if (!system_resizable_vector_get_element_at(texture_ptr->layers,
                                                        updates[n_update].n_layer,
                                                       &texture_layer_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve texture layer descriptor.");

                goto end;
            }

            system_resizable_vector_get_property(texture_layer_ptr->mipmaps,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_texture_layer_mipmaps);

            if (updates[n_update].n_mipmap >= n_texture_layer_mipmaps)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Update descriptor at index [%d] refers to a non-existing mipmap.",
                                  n_update);

                goto end;
            }
        } /* for (all update descriptors) */
    }
    #endif /* _DEBUG */

    /* Fire a notification, so that the listening backend can execute the request. */
    _ral_texture_client_memory_source_update_requested_callback_arg callback_arg;

    callback_arg.n_updates = n_updates;
    callback_arg.texture   = texture;
    callback_arg.updates   = updates;

    system_callback_manager_call_back(texture_ptr->callback_manager,
                                      RAL_TEXTURE_CALLBACK_ID_CLIENT_MEMORY_SOURCE_UPDATE_REQUESTED,
                                     &callback_arg);

    /* Mark the mipmaps as set. This is a simplified approach, since the updates might have only touched
     * mipmap storage partially, but it's enough for scene loaders */
    for (uint32_t n_update = 0;
                  n_update < n_updates;
                ++n_update)
    {
        _ral_texture_layer*  texture_layer_ptr  = NULL;
        _ral_texture_mipmap* texture_mipmap_ptr = NULL;

        if (!system_resizable_vector_get_element_at(texture_ptr->layers,
                                                   updates[n_update].n_layer,
                                                  &texture_layer_ptr) )
       {
           ASSERT_DEBUG_SYNC(false,
                             "Could not retrieve texture layer descriptor.");

           continue;
       }

       if (!system_resizable_vector_get_element_at(texture_layer_ptr->mipmaps,
                                                   updates[n_update].n_mipmap,
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
