#ifndef RAL_COMMAND_BUFFER_H
#define RAL_COMMAND_BUFFER_H

#include "ral/ral_types.h"


typedef struct ral_command_buffer_copy_texture_to_texture_command_info
{
    ral_texture dst_texture;
    ral_texture src_texture;

    uint32_t n_dst_texture_mipmap;
    uint32_t n_src_texture_mipmap;

    uint32_t dst_start_xyz[3];
    uint32_t src_start_xyz[3];

    uint32_t size[3];

    explicit ral_command_buffer_copy_texture_to_texture_command_info(ral_texture     in_dst_texture,
                                                                     ral_texture     in_src_texture,
                                                                     uint32_t        in_n_dst_texture_mipmap,
                                                                     uint32_t        in_n_src_texture_mipmap,
                                                                     const uint32_t* in_dst_start_xyz_uvec3,
                                                                     const uint32_t* in_src_start_xyz_uvec3,
                                                                     const uint32_t* in_size_uvec3);
} ral_command_buffer_copy_texture_to_texture_command_info;

typedef struct ral_command_buffer_draw_call_indexed_command_info
{
    uint32_t n_indices;
    uint32_t n_instances;

    uint32_t base_instance;

    uint32_t base_index;  /* first index to use */
    uint32_t base_vertex; /* constant added to each index when choosing elements from the vertex arrays */

    explicit ral_command_buffer_draw_call_indexed_command_info(uint32_t in_n_indices,
                                                               uint32_t in_n_instances,
                                                               uint32_t in_base_instance,
                                                               uint32_t in_base_index,
                                                               uint32_t in_base_vertex);
} ral_command_buffer_draw_call_indexed_command_info;

typedef struct ral_command_buffer_draw_call_indirect_command_info
{
    ral_buffer buffer;
    uint32_t   offset;

    uint32_t   n_draw_calls;
    uint32_t   stride;

    explicit ral_command_buffer_draw_call_indirect_command_info(ral_buffer in_buffer,
                                                                uint32_t   in_offset,
                                                                uint32_t   in_n_draw_calls,
                                                                uint32_t   in_stride);
} ral_command_buffer_draw_call_indirect_command_info;

typedef struct ral_command_buffer_draw_call_regular_command_info
{
    uint32_t n_instances;
    uint32_t n_vertices;

    uint32_t base_instance;
    uint32_t base_vertex;

    explicit ral_command_buffer_draw_call_regular_command_info(uint32_t in_n_instances,
                                                               uint32_t in_n_vertices,
                                                               uint32_t in_base_instance,
                                                               uint32_t in_base_vertex);
} ral_command_buffer_draw_call_regular_command_info;


typedef struct ral_command_buffer_set_rendertarget_command_info
{
    ral_color        blend_constant;
    bool             blend_enabled;
    ral_blend_op     blend_op_alpha;
    ral_blend_op     blend_op_color;
    ral_blend_factor dst_alpha_blend_factor;
    ral_blend_factor dst_color_blend_factor;
    ral_blend_factor src_alpha_blend_factor;
    ral_blend_factor src_color_blend_factor;
    ral_texture_view texture_view;

    uint32_t         location;

    struct
    {
        bool channel0 : 1;
        bool channel1 : 1;
        bool channel2 : 1;
        bool channel3 : 1;
    } channel_writes;

    ral_command_buffer_set_rendertarget_command_info()
    {
        blend_enabled          = false;
        blend_op_alpha         = RAL_BLEND_OP_ADD;
        blend_op_color         = RAL_BLEND_OP_ADD;
        dst_alpha_blend_factor = RAL_BLEND_FACTOR_ONE;
        dst_color_blend_factor = RAL_BLEND_FACTOR_ONE;
        src_alpha_blend_factor = RAL_BLEND_FACTOR_ONE;
        src_color_blend_factor = RAL_BLEND_FACTOR_ONE;
    }
} ral_command_buffer_set_rendertarget_command_info;

typedef struct ral_command_buffer_set_scissor_box_command_info
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];

    explicit ral_command_buffer_set_scissor_box_command_info(uint32_t        in_index,
                                                             const uint32_t* size_uvec2,
                                                             const uint32_t* xy_uvec2);
} ral_command_buffer_set_scissor_box_command_info;


typedef struct ral_command_buffer_set_vertex_attributes_command_info
{
    ral_vertex_attribute_format format;
    ral_vertex_input_rate       input_rate;
    uint32_t                    location;
    uint32_t                    offset;
    uint32_t                    stride;

    explicit ral_command_buffer_set_vertex_attributes_command_info(uint32_t                    in_location,
                                                                   ral_vertex_attribute_format in_format,
                                                                   uint32_t                    in_offset,
                                                                   uint32_t                    in_stride,
                                                                   ral_vertex_input_rate       in_input_rate);
} ral_command_buffer_set_vertex_attributes_command_info;

typedef struct ral_command_buffer_set_viewport_command_info
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];

    explicit ral_command_buffer_set_viewport_command_info(uint32_t        in_index,
                                                          const uint32_t* in_size_uvec2,
                                                          const uint32_t* in_xy_uvec2);
} ral_command_buffer_set_viewport_command_info;


/** TODO */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context context,
                                                    int         usage_bits,
                                                    bool        invokable_from_other_command_buffers);

/** TODO */
PUBLIC void ral_command_buffer_record_copy_texture_to_texture(ral_command_buffer                                             recording_command_buffer,
                                                              uint32_t                                                       n_copy_ops,
                                                              const ral_command_buffer_copy_texture_to_texture_command_info* copy_op_ptrs);
/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_indexed(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_indexed_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_indexed_indirect(ral_command_buffer                                        recording_command_buffer,
                                                                 uint32_t                                                  n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_regular(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_regular_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_regular_indirect(ral_command_buffer                                        recording_command_buffer,
                                                                 uint32_t                                                  n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_execute_command_buffer(ral_command_buffer recording_command_buffer,
                                                             ral_command_buffer command_buffer_to_execute);

/** TODO */
PUBLIC void ral_command_buffer_record_set_scissor_boxes(ral_command_buffer                                     recording_command_buffer,
                                                        uint32_t                                               n_scissor_boxes,
                                                        const ral_command_buffer_set_scissor_box_command_info* scissor_boxes);

/** TODO */
PUBLIC void ral_command_buffer_record_set_viewports(ral_command_buffer                                  recording_command_buffer,
                                                    uint32_t                                            n_viewport,
                                                    const ral_command_buffer_set_viewport_command_info* viewport_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer);

/** TODO */
PUBLIC void ral_command_buffer_record_set_gfx_state(ral_command_buffer recording_command_buffer,
                                                    ral_gfx_state      gfx_state);

/** TODO */
PUBLIC void ral_command_buffer_record_set_program(ral_command_buffer recording_command_buffer,
                                                  ral_program        program);

/** TODO */
PUBLIC void ral_command_buffeR_record_set_rendertargets(ral_command_buffer                                      recording_command_buffer,
                                                        uint32_t                                                n_rendertarget,
                                                        const ral_command_buffer_set_rendertarget_command_info* rendertarget_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_vertex_attributes(ral_command_buffer                                           recording_command_buffer,
                                                            uint32_t                                                     n_vertex_attributes,
                                                            const ral_command_buffer_set_vertex_attributes_command_info* vertex_attribute_ptrs);

/** TODO */
PUBLIC bool ral_command_buffer_start_recording(ral_command_buffer command_buffer,
                                               ral_framebuffer    framebuffer);

/** TODO */
PUBLIC bool ral_command_buffer_stop_recording(ral_command_buffer command_buffer);


#endif /* RAL_COMMAND_BUFFER_H */