/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"

typedef struct _ral_texture_view
{
    ral_context                  context;
    ral_texture_view_create_info create_info;

    explicit _ral_texture_view(const ral_texture_view_create_info* create_info_ptr)
    {
        /* Note: We do NOT retain parent texture here, as that would lead to cycles.
         *       Texture views are owned by textures, so everything should be fine,
         *       as long as the application does not attempt to manipulate with texture
         *       view ref counters.
         */
        create_info = *create_info_ptr;

        ral_texture_get_property(create_info_ptr->texture,
                                 RAL_TEXTURE_PROPERTY_CONTEXT,
                                &context);
    }

    ~_ral_texture_view()
    {
        bool parent_texture_being_released = false;

        ral_texture_get_property(create_info.texture,
                                 RAL_TEXTURE_PROPERTY_IS_BEING_RELEASED,
                                &parent_texture_being_released);

        ASSERT_DEBUG_SYNC(parent_texture_being_released,
                          "Texture view being destroyed, even though parent texture is still alive");
    }
} _ral_texture_view;


/** Please see header for specification */
PUBLIC ral_texture_view ral_texture_view_create(const ral_texture_view_create_info* create_info_ptr)
{
    ral_format         parent_texture_format         = RAL_FORMAT_UNKNOWN;
    uint32_t           parent_texture_n_layers       = 0;
    uint32_t           parent_texture_n_mips         = 0;
    ral_texture_type   parent_texture_type           = RAL_TEXTURE_TYPE_UNKNOWN;
    bool               swizzle_valid                 = true;
    bool               texture_aspect_valid          = true;
    bool               texture_format_valid          = true;
    bool               texture_type_valid            = true;
    _ral_texture_view* texture_view_ptr              = nullptr;
    bool               view_format_has_color_comps   = false;
    bool               view_format_has_depth_comps   = false;
    bool               view_format_has_stencil_comps = false;

    /* Sanity checks */
    if (create_info_ptr->texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->texture == nullptr),
                          "Parent texture is NULL");

        goto end;
    }

    ral_texture_get_property(create_info_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &parent_texture_format);
    ral_texture_get_property(create_info_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_LAYERS,
                            &parent_texture_n_layers);
    ral_texture_get_property(create_info_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &parent_texture_n_mips);
    ral_texture_get_property(create_info_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &parent_texture_type);

    switch (create_info_ptr->type)
    {
        case RAL_TEXTURE_TYPE_1D:
        case RAL_TEXTURE_TYPE_1D_ARRAY:
        {
            texture_type_valid = (parent_texture_type == RAL_TEXTURE_TYPE_1D ||
                                  parent_texture_type != RAL_TEXTURE_TYPE_1D_ARRAY);

            break;
        }

        case RAL_TEXTURE_TYPE_2D:
        case RAL_TEXTURE_TYPE_2D_ARRAY:
        {
            texture_type_valid = (parent_texture_type == RAL_TEXTURE_TYPE_2D ||
                                  parent_texture_type == RAL_TEXTURE_TYPE_2D_ARRAY);

            break;
        }

        case RAL_TEXTURE_TYPE_3D:
        {
            texture_type_valid = (parent_texture_type == RAL_TEXTURE_TYPE_3D);

            break;
        }

        case RAL_TEXTURE_TYPE_MULTISAMPLE_2D:
        case RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY:
        {
            texture_type_valid = (parent_texture_type == RAL_TEXTURE_TYPE_MULTISAMPLE_2D ||
                                  parent_texture_type == RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture type of the requested texture view");

            goto end;
        }
    }

    if (!texture_type_valid)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Requested texture view type is invalid with the specified parent texture.");

        goto end;
    }

    if (create_info_ptr->n_layers == 0)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_layers == 0),
                          "Texture view must define at least 1 layer.");

        goto end;
    }

    if (create_info_ptr->n_mips == 0)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_mips == 0),
                          "Texture view must define at least 1 mipmap.");

        goto end;
    }

    if (!(create_info_ptr->n_base_layer                              <  parent_texture_n_layers &&
         (create_info_ptr->n_base_layer + create_info_ptr->n_layers) <= parent_texture_n_layers) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Parent texture contains %d layers, but the requested texture view is to hold <%d, %d) layers.",
                          parent_texture_n_layers,
                          create_info_ptr->n_base_layer,
                          create_info_ptr->n_base_layer + create_info_ptr->n_layers);

        goto end;
    }

    if (!(create_info_ptr->n_base_mip                            <  parent_texture_n_mips &&
         (create_info_ptr->n_base_mip + create_info_ptr->n_mips) <= parent_texture_n_mips))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Parent texture contains %d mips, but the requested texture view is to hold <%d, %d) mipmaps.",
                          parent_texture_n_mips,
                          create_info_ptr->n_base_mip,
                          create_info_ptr->n_base_mip + create_info_ptr->n_mips);

        goto end;
    }

    switch (create_info_ptr->format)
    {
        /* 128-bit */
        case RAL_FORMAT_RGBA32_FLOAT:
        case RAL_FORMAT_RGBA32_SINT:
        case RAL_FORMAT_RGBA32_UINT:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_RGBA32_FLOAT ||
                                    parent_texture_format == RAL_FORMAT_RGBA32_SINT  ||
                                    parent_texture_format == RAL_FORMAT_RGBA32_UINT);

            break;
        }

        /* 96-bit */
        case RAL_FORMAT_RGB32_FLOAT:
        case RAL_FORMAT_RGB32_SINT:
        case RAL_FORMAT_RGB32_UINT:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_RGB32_FLOAT ||
                                    parent_texture_format == RAL_FORMAT_RGB32_SINT  ||
                                    parent_texture_format == RAL_FORMAT_RGB32_UINT);

            break;
        }

        /* 64-bit */
        case RAL_FORMAT_RG32_FLOAT:
        case RAL_FORMAT_RG32_SINT:
        case RAL_FORMAT_RG32_UINT:
        case RAL_FORMAT_RGBA16_FLOAT:
        case RAL_FORMAT_RGBA16_UINT:
        case RAL_FORMAT_RGBA16_UNORM:
        case RAL_FORMAT_RGBA16_SINT:
        case RAL_FORMAT_RGBA16_SNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_RG32_FLOAT   ||
                                    parent_texture_format == RAL_FORMAT_RG32_SINT    ||
                                    parent_texture_format == RAL_FORMAT_RG32_UINT    ||
                                    parent_texture_format == RAL_FORMAT_RGBA16_FLOAT ||
                                    parent_texture_format == RAL_FORMAT_RGBA16_UINT  ||
                                    parent_texture_format == RAL_FORMAT_RGBA16_UNORM ||
                                    parent_texture_format == RAL_FORMAT_RGBA16_SINT  ||
                                    parent_texture_format == RAL_FORMAT_RGBA16_SNORM);

            break;
        }

         /* 48-bit */
        case RAL_FORMAT_RGB16_UNORM:
        case RAL_FORMAT_RGB16_SNORM:
        case RAL_FORMAT_RGB16_FLOAT:
        case RAL_FORMAT_RGB16_UINT:
        case RAL_FORMAT_RGB16_SINT:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_RGB16_UNORM ||
                                    parent_texture_format == RAL_FORMAT_RGB16_SNORM ||
                                    parent_texture_format == RAL_FORMAT_RGB16_FLOAT ||
                                    parent_texture_format == RAL_FORMAT_RGB16_UINT  ||
                                    parent_texture_format == RAL_FORMAT_RGB16_SINT);

            break;
        }

        /* 40-bit */
        case RAL_FORMAT_DEPTH32F_STENCIL8:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_DEPTH32F_STENCIL8);

            break;
        }

        /* 32-bit */
        case RAL_FORMAT_DEPTH24_STENCIL8:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_DEPTH24_STENCIL8);

            break;
        }

        case RAL_FORMAT_DEPTH32_FLOAT:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_DEPTH32_FLOAT);

            break;
        }

        case RAL_FORMAT_DEPTH32_SNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_DEPTH32_SNORM);

            break;
        }

        case RAL_FORMAT_R11FG11FB10F:
        case RAL_FORMAT_R32_FLOAT:
        case RAL_FORMAT_R32_SINT:
        case RAL_FORMAT_R32_UINT:
        case RAL_FORMAT_RG16_FLOAT:
        case RAL_FORMAT_RG16_SINT:
        case RAL_FORMAT_RG16_SNORM:
        case RAL_FORMAT_RG16_UINT:
        case RAL_FORMAT_RG16_UNORM:
        case RAL_FORMAT_RGB10A2_UINT:
        case RAL_FORMAT_RGB10A2_UNORM:
        case RAL_FORMAT_RGB9E5_FLOAT:
        case RAL_FORMAT_RGBA8_SINT:
        case RAL_FORMAT_RGBA8_SNORM:
        case RAL_FORMAT_RGBA8_UINT:
        case RAL_FORMAT_RGBA8_UNORM:
        case RAL_FORMAT_SRGBA8_UNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_R11FG11FB10F  ||
                                    parent_texture_format == RAL_FORMAT_R32_FLOAT     ||
                                    parent_texture_format == RAL_FORMAT_R32_SINT      ||
                                    parent_texture_format == RAL_FORMAT_R32_UINT      ||
                                    parent_texture_format == RAL_FORMAT_RG16_FLOAT    ||
                                    parent_texture_format == RAL_FORMAT_RG16_SINT     ||
                                    parent_texture_format == RAL_FORMAT_RG16_SNORM    ||
                                    parent_texture_format == RAL_FORMAT_RG16_UINT     ||
                                    parent_texture_format == RAL_FORMAT_RG16_UNORM    ||
                                    parent_texture_format == RAL_FORMAT_RGB10A2_UINT  ||
                                    parent_texture_format == RAL_FORMAT_RGB10A2_UNORM ||
                                    parent_texture_format == RAL_FORMAT_RGB9E5_FLOAT  ||
                                    parent_texture_format == RAL_FORMAT_RGBA8_SINT    ||
                                    parent_texture_format == RAL_FORMAT_RGBA8_SNORM   ||
                                    parent_texture_format == RAL_FORMAT_RGBA8_UINT    ||
                                    parent_texture_format == RAL_FORMAT_RGBA8_UNORM   ||
                                    parent_texture_format == RAL_FORMAT_SRGBA8_UNORM);

            break;
        }

        /* 24-bit */
        case RAL_FORMAT_DEPTH24_SNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_DEPTH24_SNORM);

            break;
        }

        case RAL_FORMAT_RGB8_SINT:
        case RAL_FORMAT_RGB8_SNORM:
        case RAL_FORMAT_RGB8_UINT:
        case RAL_FORMAT_RGB8_UNORM:
        case RAL_FORMAT_SRGB8_UNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_RGB8_SINT   ||
                                    parent_texture_format == RAL_FORMAT_RGB8_SNORM  ||
                                    parent_texture_format == RAL_FORMAT_RGB8_UINT   ||
                                    parent_texture_format == RAL_FORMAT_RGB8_UNORM  ||
                                    parent_texture_format == RAL_FORMAT_SRGB8_UNORM);

            break;
        }

        /* 16-bit */
        case RAL_FORMAT_DEPTH16_SNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_DEPTH16_SNORM);

            break;
        }

        case RAL_FORMAT_R16_FLOAT:
        case RAL_FORMAT_RG8_UINT:
        case RAL_FORMAT_R16_UINT:
        case RAL_FORMAT_RG8_SINT:
        case RAL_FORMAT_R16_SINT:
        case RAL_FORMAT_RG8_UNORM:
        case RAL_FORMAT_R16_UNORM:
        case RAL_FORMAT_RG8_SNORM:
        case RAL_FORMAT_R16_SNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_R16_FLOAT ||
                                    parent_texture_format == RAL_FORMAT_RG8_UINT  ||
                                    parent_texture_format == RAL_FORMAT_R16_UINT  ||
                                    parent_texture_format == RAL_FORMAT_RG8_SINT  ||
                                    parent_texture_format == RAL_FORMAT_R16_SINT  ||
                                    parent_texture_format == RAL_FORMAT_RG8_UNORM ||
                                    parent_texture_format == RAL_FORMAT_R16_UNORM ||
                                    parent_texture_format == RAL_FORMAT_RG8_SNORM ||
                                    parent_texture_format == RAL_FORMAT_R16_SNORM);

            break;
        }

        /* 8-bit */
        case RAL_FORMAT_R8_SINT:
        case RAL_FORMAT_R8_SNORM:
        case RAL_FORMAT_R8_UINT:
        case RAL_FORMAT_R8_UNORM:
        {
            texture_format_valid = (parent_texture_format == RAL_FORMAT_R8_SINT  ||
                                    parent_texture_format == RAL_FORMAT_R8_SNORM ||
                                    parent_texture_format == RAL_FORMAT_R8_UINT  ||
                                    parent_texture_format == RAL_FORMAT_R8_UNORM);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture format requested for the texture view.");
        }
    }

    if (!texture_format_valid)
    {
        ASSERT_DEBUG_SYNC(texture_format_valid,
                          "Texture format, requested for the view, is incompatible with the parent texture's format.");

        goto end;
    }

    ral_utils_get_format_property(create_info_ptr->format,
                                  RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                 &view_format_has_color_comps);
    ral_utils_get_format_property(create_info_ptr->format,
                                  RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                 &view_format_has_depth_comps);
    ral_utils_get_format_property(create_info_ptr->format,
                                  RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                 &view_format_has_stencil_comps);

    if ((create_info_ptr->aspect & RAL_TEXTURE_ASPECT_COLOR_BIT) && !view_format_has_color_comps)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL texture does not hold color data, but the texture view is to expose it.");

        goto end;
    }

    if ((create_info_ptr->aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) && !view_format_has_depth_comps)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL texture does not hold depth data, but the texture view is to expose it.");

        goto end;
    }

    if ((create_info_ptr->aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) && !view_format_has_stencil_comps)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL texture does not hold stencil data, but the texture view is to expose it.");

        goto end;
    }

    if (view_format_has_color_comps)
    {
        uint32_t n_color_comps = 0;

        ral_utils_get_format_property(create_info_ptr->format,
                                      RAL_FORMAT_PROPERTY_N_COMPONENTS,
                                     &n_color_comps);

        for (uint32_t n_component = 0;
                      n_component < sizeof(create_info_ptr->component_order) / sizeof(create_info_ptr->component_order[0]);
                    ++n_component)
        {
            const ral_texture_component current_component = create_info_ptr->component_order[n_component];

            switch (n_color_comps)
            {
                case 1:
                {
                    swizzle_valid &= (current_component == RAL_TEXTURE_COMPONENT_IDENTITY ||
                                      current_component == RAL_TEXTURE_COMPONENT_ONE      ||
                                      current_component == RAL_TEXTURE_COMPONENT_RED      ||
                                      current_component == RAL_TEXTURE_COMPONENT_ZERO);

                    break;
                }

                case 2:
                {
                    swizzle_valid &= (current_component == RAL_TEXTURE_COMPONENT_GREEN    ||
                                      current_component == RAL_TEXTURE_COMPONENT_IDENTITY ||
                                      current_component == RAL_TEXTURE_COMPONENT_ONE      ||
                                      current_component == RAL_TEXTURE_COMPONENT_RED      ||
                                      current_component == RAL_TEXTURE_COMPONENT_ZERO);

                    break;
                }

                case 3:
                {
                    swizzle_valid &= (current_component == RAL_TEXTURE_COMPONENT_BLUE     ||
                                      current_component == RAL_TEXTURE_COMPONENT_GREEN    ||
                                      current_component == RAL_TEXTURE_COMPONENT_IDENTITY ||
                                      current_component == RAL_TEXTURE_COMPONENT_ONE      ||
                                      current_component == RAL_TEXTURE_COMPONENT_RED      ||
                                      current_component == RAL_TEXTURE_COMPONENT_ZERO);

                    break;
                }

                case 4:
                {
                    swizzle_valid &= (current_component == RAL_TEXTURE_COMPONENT_ALPHA    ||
                                      current_component == RAL_TEXTURE_COMPONENT_BLUE     ||
                                      current_component == RAL_TEXTURE_COMPONENT_GREEN    ||
                                      current_component == RAL_TEXTURE_COMPONENT_IDENTITY ||
                                      current_component == RAL_TEXTURE_COMPONENT_ONE      ||
                                      current_component == RAL_TEXTURE_COMPONENT_RED      ||
                                      current_component == RAL_TEXTURE_COMPONENT_ZERO);

                    break;
                }
            }
        }
    }

    if (view_format_has_depth_comps   ||
        view_format_has_stencil_comps)
    {
        for (uint32_t n_component = 0;
                      n_component < sizeof(create_info_ptr->component_order) / sizeof(create_info_ptr->component_order[0]);
                    ++n_component)
        {
            const ral_texture_component current_component = create_info_ptr->component_order[n_component];

            swizzle_valid &= (current_component == RAL_TEXTURE_COMPONENT_IDENTITY ||
                              current_component == RAL_TEXTURE_COMPONENT_ONE      ||
                              current_component == RAL_TEXTURE_COMPONENT_ZERO);
        }
    }

    if (!swizzle_valid)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid component order specified for the texture view.");

        goto end;
    }

    /* Safe to create the texture view */
    texture_view_ptr = new _ral_texture_view(create_info_ptr);

    ASSERT_ALWAYS_SYNC(texture_view_ptr != nullptr,
                       "Out of memory");

    /* All done */
end:
    return (ral_texture_view) texture_view_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_texture_view_get_property(ral_texture_view          texture_view,
                                                      ral_texture_view_property property,
                                                      void*                     out_result_ptr)
{
    _ral_texture_view* texture_view_ptr = reinterpret_cast<_ral_texture_view*>(texture_view);

    switch (property)
    {
        case RAL_TEXTURE_VIEW_PROPERTY_ASPECT:
        {
            *reinterpret_cast<ral_texture_aspect*>(out_result_ptr) = texture_view_ptr->create_info.aspect;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_COMPONENT_ORDER:
        {
            *reinterpret_cast<ral_texture_component**>(out_result_ptr) = texture_view_ptr->create_info.component_order;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = texture_view_ptr->context;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_FORMAT:
        {
            *reinterpret_cast<ral_format*>(out_result_ptr) = texture_view_ptr->create_info.format;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_N_BASE_LAYER:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = texture_view_ptr->create_info.n_base_layer;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_N_BASE_MIPMAP:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = texture_view_ptr->create_info.n_base_mip;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = texture_view_ptr->create_info.n_layers;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = texture_view_ptr->create_info.n_mips;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE:
        {
            *reinterpret_cast<ral_texture*>(out_result_ptr) = texture_view_ptr->create_info.texture;

            break;
        }

        case RAL_TEXTURE_VIEW_PROPERTY_TYPE:
        {
            *reinterpret_cast<ral_texture_type*>(out_result_ptr) = texture_view_ptr->create_info.type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_view_property value.");
        }
    }
}

/** Please see header for speification */
PUBLIC EMERALD_API bool ral_texture_view_get_mipmap_property(ral_texture_view            texture_view,
                                                             uint32_t                    n_layer,
                                                             uint32_t                    n_mipmap,
                                                             ral_texture_mipmap_property mipmap_property,
                                                             void*                       out_result_ptr)
{
    _ral_texture_view* texture_view_ptr = reinterpret_cast<_ral_texture_view*>(texture_view);

    return ral_texture_get_mipmap_property(texture_view_ptr->create_info.texture,
                                           n_layer,
                                           n_mipmap,
                                           mipmap_property,
                                           out_result_ptr);
}

/** Please see header for specification */
PUBLIC void ral_texture_view_release(ral_texture_view texture_view)
{
    delete reinterpret_cast<_ral_texture_view*>(texture_view);
}