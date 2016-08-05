#include "shared.h"
#include "ral/ral_types.h"
#include "ral/ral_command_buffer.h"

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