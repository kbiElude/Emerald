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
} ral_command_buffer_copy_texture_to_texture_command_info;

typedef struct ral_command_buffer_draw_call_indexed_command_info
{
    ral_buffer index_buffer;

    uint32_t n_indices;
    uint32_t n_instances;

    uint32_t base_instance;

    uint32_t base_index;  /* first index to use */
    uint32_t base_vertex; /* constant added to each index when choosing elements from the vertex arrays */
} ral_command_buffer_draw_call_indexed_command_info;

typedef struct ral_command_buffer_draw_call_indirect_regular_command_info
{
    ral_buffer buffer;
    uint32_t   offset;

    uint32_t   n_draw_calls;
    uint32_t   stride;
} ral_command_buffer_draw_call_indirect_regular_command_info;

typedef struct ral_command_buffer_draw_call_regular_command_info
{
    uint32_t n_instances;
    uint32_t n_vertices;

    uint32_t base_instance;
    uint32_t base_vertex;
} ral_command_buffer_draw_call_regular_command_info;

typedef struct ral_command_buffer_execute_command_buffer_command_info
{

    ral_command_buffer command_buffer;

} ral_command_buffer_execute_command_buffer_command_info;

typedef enum
{
    /* not settable; ral_queue_bits */
    RAL_COMMAND_BUFFER_PROPERTY_COMPATIBLE_QUEUES,

    /* not settable; ral_context */
    RAL_COMMAND_BUFFER_PROPERTY_CONTEXT,

    /* not settable; bool */
    RAL_COMMAND_BUFFER_PROPERTY_IS_INVOKABLE_FROM_OTHER_COMMAND_BUFFERS,

    /* not settable; bool */
    RAL_COMMAND_BUFFER_PROPERTY_IS_RESETTABLE,

    /* not settable; bool */
    RAL_COMMAND_BUFFER_PROPERTY_IS_TRANSIENT,
} ral_command_buffer_property;

typedef struct ral_command_buffer_set_binding_command_info
{
    uint32_t buffer_offset;
    uint32_t n_binding;
    uint32_t n_set;

    union
    {
        ral_buffer  buffer;
        ral_texture texture;
    };

    ral_sampler      sampler;
    ral_binding_type type;
} ral_command_buffer_set_binding_command_info;

typedef struct ral_command_buffer_set_gfx_state_command_info
{

    ral_gfx_state new_state;

} ral_command_buffer_set_gfx_state_command_info;

typedef struct ral_command_buffer_set_program_command_info
{

    ral_program new_program;

} ral_command_buffer_set_program_command_info;

typedef struct ral_command_buffer_set_rendertarget_state_command_info
{
    ral_color        blend_constant;
    bool             blend_enabled;
    ral_blend_op     blend_op_alpha;
    ral_blend_op     blend_op_color;
    ral_blend_factor dst_alpha_blend_factor;
    ral_blend_factor dst_color_blend_factor;
    uint32_t         rendertarget_index;
    ral_blend_factor src_alpha_blend_factor;
    ral_blend_factor src_color_blend_factor;

    struct
    {
        bool channel0 : 1;
        bool channel1 : 1;
        bool channel2 : 1;
        bool channel3 : 1;
    } channel_writes;

    static ral_command_buffer_set_rendertarget_state_command_info get_preinitialized_instance()
    {
        ral_command_buffer_set_rendertarget_state_command_info result;

        result.blend_constant.f32[0]   = 0.0f;
        result.blend_constant.f32[1]   = 0.0f;
        result.blend_constant.f32[2]   = 0.0f;
        result.blend_constant.f32[3]   = 0.0f;
        result.blend_enabled           = false;
        result.blend_op_alpha          = RAL_BLEND_OP_ADD;
        result.blend_op_color          = RAL_BLEND_OP_ADD;
        result.channel_writes.channel0 = true;
        result.channel_writes.channel1 = true;
        result.channel_writes.channel2 = true;
        result.channel_writes.channel3 = true;
        result.dst_alpha_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.dst_color_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.rendertarget_index      = -1;
        result.src_alpha_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.src_color_blend_factor  = RAL_BLEND_FACTOR_ONE;

        return result;
    }
} ral_command_buffer_set_rendertarget_state_command_info;

typedef struct ral_command_buffer_set_scissor_box_command_info
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];
} ral_command_buffer_set_scissor_box_command_info;


typedef struct ral_command_buffer_set_vertex_attribute_command_info
{
    ral_buffer buffer;
    uint32_t   location;
    uint32_t   start_offset;
} ral_command_buffer_set_vertex_attribute_command_info;

typedef struct ral_command_buffer_set_viewport_command_info
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];
} ral_command_buffer_set_viewport_command_info;


/** TODO */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context    context,
                                                    ral_queue_bits compatible_queues,
                                                    bool           is_invokable_from_other_command_buffers,
                                                    bool           is_resettable,
                                                    bool           is_transient);

/** TODO */
PUBLIC void ral_command_buffer_deinit();

/** TODO */
PUBLIC void ral_command_buffer_get_property(ral_command_buffer          command_buffer,
                                            ral_command_buffer_property property,
                                            void*                       out_result_ptr);

/** TODO */
PUBLIC void ral_command_buffer_init();

/** TODO */
PUBLIC void ral_command_buffer_record_copy_texture_to_texture(ral_command_buffer                                             recording_command_buffer,
                                                              uint32_t                                                       n_copy_ops,
                                                              const ral_command_buffer_copy_texture_to_texture_command_info* copy_op_ptrs);
/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_indexed(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_indexed_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_indirect_regular(ral_command_buffer                                                recording_command_buffer,
                                                                 uint32_t                                                          n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect_regular_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_regular(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_regular_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_execute_command_buffer(ral_command_buffer                                            recording_command_buffer,
                                                             uint32_t                                                      n_commands,
                                                             const ral_command_buffer_execute_command_buffer_command_info* command_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_bindings(ral_command_buffer                           recording_command_buffer,
                                                   uint32_t                                     n_bindings,
                                                   ral_command_buffer_set_binding_command_info* binding_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_gfx_state(ral_command_buffer recording_command_buffer,
                                                    ral_gfx_state      gfx_state);

/** TODO */
PUBLIC void ral_command_buffer_record_set_program(ral_command_buffer recording_command_buffer,
                                                  ral_program        program);

/** TODO */
PUBLIC void ral_command_buffer_record_set_rendertargets(ral_command_buffer                                            recording_command_buffer,
                                                        uint32_t                                                      n_rendertargets,
                                                        const ral_command_buffer_set_rendertarget_state_command_info* rendertarget_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_scissor_boxes(ral_command_buffer                                     recording_command_buffer,
                                                        uint32_t                                               n_scissor_boxes,
                                                        const ral_command_buffer_set_scissor_box_command_info* scissor_box_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_vertex_attributes(ral_command_buffer                                          recording_command_buffer,
                                                            uint32_t                                                    n_vertex_attributes,
                                                            const ral_command_buffer_set_vertex_attribute_command_info* vertex_attribute_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_viewports(ral_command_buffer                                  recording_command_buffer,
                                                    uint32_t                                            n_viewports,
                                                    const ral_command_buffer_set_viewport_command_info* viewport_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer);

/** TODO */
PUBLIC bool ral_command_buffer_start_recording(ral_command_buffer command_buffer);

/** TODO */
PUBLIC bool ral_command_buffer_stop_recording(ral_command_buffer command_buffer);


#endif /* RAL_COMMAND_BUFFER_H */