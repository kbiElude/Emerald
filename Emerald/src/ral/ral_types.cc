#include "shared.h"
#include "ral/ral_types.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_texture.h"
#include "ral/ral_utils.h"

/** Please see header for spec */
PUBLIC ral_gfx_state_create_info* ral_gfx_state_create_info::clone(const ral_gfx_state_create_info* in_gfx_state_ptr,
                                                                   bool                             should_include_static_scissor_box_data,
                                                                   bool                             should_include_static_viewport_data,
                                                                   bool                             should_include_va_data)
{
    ral_gfx_state_create_info* new_create_info_ptr = new ral_gfx_state_create_info;

    memcpy(new_create_info_ptr,
           in_gfx_state_ptr,
           sizeof(*new_create_info_ptr) );

    if (should_include_static_scissor_box_data                            &&
        in_gfx_state_ptr->static_n_scissor_boxes_and_viewports > 0        &&
        in_gfx_state_ptr->static_scissor_boxes                 != nullptr &&
        in_gfx_state_ptr->static_scissor_boxes_enabled)
    {
        new_create_info_ptr->static_scissor_boxes = new (std::nothrow) ral_command_buffer_set_scissor_box_command_info[in_gfx_state_ptr->static_n_scissor_boxes_and_viewports];

        memcpy(new_create_info_ptr->static_scissor_boxes,
               in_gfx_state_ptr->static_scissor_boxes,
               sizeof(ral_command_buffer_set_scissor_box_command_info) * in_gfx_state_ptr->static_n_scissor_boxes_and_viewports);
    }
    else
    {
        new_create_info_ptr->static_scissor_boxes         = nullptr;
        new_create_info_ptr->static_scissor_boxes_enabled = false;
    }

    if (should_include_static_viewport_data                               &&
        in_gfx_state_ptr->static_n_scissor_boxes_and_viewports >  0       &&
        in_gfx_state_ptr->static_viewports                     != nullptr &&
        in_gfx_state_ptr->static_viewports_enabled)
    {
        new_create_info_ptr->static_viewports = new (std::nothrow) ral_command_buffer_set_viewport_command_info[in_gfx_state_ptr->static_n_scissor_boxes_and_viewports];

        memcpy(new_create_info_ptr->static_viewports,
               in_gfx_state_ptr->static_viewports,
               sizeof(ral_command_buffer_set_viewport_command_info) * in_gfx_state_ptr->static_n_scissor_boxes_and_viewports);
    }
    else
    {
        new_create_info_ptr->static_viewports         = nullptr;
        new_create_info_ptr->static_viewports_enabled = false;
    }

    if (should_include_va_data                             &&
        in_gfx_state_ptr->vertex_attribute_ptrs != nullptr &&
        in_gfx_state_ptr->n_vertex_attributes   >  0)
    {
        new_create_info_ptr->vertex_attribute_ptrs = new (std::nothrow) ral_gfx_state_vertex_attribute[in_gfx_state_ptr->n_vertex_attributes];

        memcpy(new_create_info_ptr->vertex_attribute_ptrs,
               in_gfx_state_ptr->vertex_attribute_ptrs,
               sizeof(ral_gfx_state_vertex_attribute) * in_gfx_state_ptr->n_vertex_attributes);
    }
    else
    {
        new_create_info_ptr->n_vertex_attributes   = 0;
        new_create_info_ptr->vertex_attribute_ptrs = nullptr;
    }

    return new_create_info_ptr;
}

/** Please see header for spec */
PUBLIC void ral_gfx_state_create_info::release(ral_gfx_state_create_info* in_gfx_state_ptr)
{
    if (in_gfx_state_ptr->static_scissor_boxes != nullptr)
    {
        delete [] in_gfx_state_ptr->static_scissor_boxes;

        in_gfx_state_ptr->static_scissor_boxes = nullptr;
    }

    if (in_gfx_state_ptr->static_viewports != nullptr)
    {
        delete [] in_gfx_state_ptr->static_viewports;

        in_gfx_state_ptr->static_viewports = nullptr;
    }

    if (in_gfx_state_ptr->vertex_attribute_ptrs != nullptr)
    {
        delete [] in_gfx_state_ptr->vertex_attribute_ptrs;

        in_gfx_state_ptr->vertex_attribute_ptrs = nullptr;
    }

    delete in_gfx_state_ptr;
}

EMERALD_API ral_texture_view_create_info::ral_texture_view_create_info(ral_texture in_texture)
{
    ral_format       texture_format;
    bool             texture_format_has_color_data;
    bool             texture_format_has_depth_data;
    bool             texture_format_has_stencil_data;
    uint32_t         texture_n_layers = 0;
    uint32_t         texture_n_mips   = 0;
    ral_texture_type texture_type;

    ral_texture_get_property(in_texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &texture_format);
    ral_texture_get_property(in_texture,
                             RAL_TEXTURE_PROPERTY_N_LAYERS,
                            &texture_n_layers);
    ral_texture_get_property(in_texture,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &texture_n_mips);
    ral_texture_get_property(in_texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    ral_utils_get_format_property(texture_format,
                                  RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                 &texture_format_has_color_data);
    ral_utils_get_format_property(texture_format,
                                  RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                 &texture_format_has_depth_data);
    ral_utils_get_format_property(texture_format,
                                  RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                 &texture_format_has_stencil_data);

    /* Fill relevant fields */
    this->aspect             = static_cast<ral_texture_aspect>(((texture_format_has_color_data)   ? RAL_TEXTURE_ASPECT_COLOR_BIT   : 0) |
                                                               ((texture_format_has_depth_data)   ? RAL_TEXTURE_ASPECT_DEPTH_BIT   : 0) |
                                                               ((texture_format_has_stencil_data) ? RAL_TEXTURE_ASPECT_STENCIL_BIT : 0));
    this->component_order[0] = RAL_TEXTURE_COMPONENT_IDENTITY;
    this->component_order[1] = RAL_TEXTURE_COMPONENT_IDENTITY;
    this->component_order[2] = RAL_TEXTURE_COMPONENT_IDENTITY;
    this->component_order[3] = RAL_TEXTURE_COMPONENT_IDENTITY;
    this->format             = texture_format;
    this->n_base_layer       = 0;
    this->n_base_mip         = 0;
    this->n_layers           = texture_n_layers;
    this->n_mips             = texture_n_mips;
    this->texture            = in_texture;
    this->type               = texture_type;
}