#ifndef RAL_COMMAND_BUFFER_H
#define RAL_COMMAND_BUFFER_H

#include "ral/ral_types.h"

typedef enum
{
    RAL_COMMAND_BUFFER_BINDING_TYPE_INDEX_BUFFER,           /* binding must always be 0 */
    RAL_COMMAND_BUFFER_BINDING_TYPE_SHADER_STORAGE_BUFFER,
    RAL_COMMAND_BUFFER_BINDING_TYPE_UNIFORM_BUFFER,
    RAL_COMMAND_BUFFER_BINDING_TYPE_VERTEX_BUFFER
} ral_command_buffer_binding_type;

typedef struct ral_command_buffer_buffer_binding
{
    uint32_t                        binding_index;
    ral_command_buffer_binding_type binding_type;

    ral_buffer                      buffer;
    uint32_t                        range;
    uint32_t                        start_offset;

    explicit ral_command_buffer_buffer_binding(uint32_t                        in_binding_index,
                                               ral_command_buffer_binding_type in_binding_type,
                                               ral_buffer                      in_buffer,
                                               uint32_t                        in_range,
                                               uint32_t                        in_start_offset);
} ral_command_buffer_buffer_binding;

typedef struct ral_command_buffer_draw_call_indexed
{
    uint32_t n_indices;
    uint32_t n_instances;

    uint32_t base_instance;

    uint32_t base_index;  /* first index to use */
    uint32_t base_vertex; /* constant added to each index when choosing elements from the vertex arrays */

    explicit ral_command_buffer_draw_call_indexed(uint32_t in_n_indices,
                                                  uint32_t in_n_instances,
                                                  uint32_t in_base_instance,
                                                  uint32_t in_base_index,
                                                  uint32_t in_base_vertex);
} ral_command_buffer_draw_call_indexed;

typedef struct ral_command_buffer_draw_call_indirect
{
    ral_buffer buffer;
    uint32_t   offset;

    uint32_t   n_draw_calls;
    uint32_t   stride;

    explicit ral_command_buffer_draw_call_indirect(ral_buffer in_buffer,
                                                   uint32_t   in_offset,
                                                   uint32_t   in_n_draw_calls,
                                                   uint32_t   in_stride);
} ral_command_buffer_draw_call_indirect;

typedef struct ral_command_buffer_draw_call_regular
{
    uint32_t n_instances;
    uint32_t n_vertices;

    uint32_t base_instance;
    uint32_t base_vertex;

    explicit ral_command_buffer_draw_call_regular(uint32_t in_n_instances,
                                                  uint32_t in_n_vertices,
                                                  uint32_t in_base_instance,
                                                  uint32_t in_base_vertex);
} ral_command_buffer_draw_call_regular;

typedef enum
{
    RAL_COMMAND_BUFFER_FB_ATTACHMENT_INIT_CLEAR,
    RAL_COMMAND_BUFFER_FB_ATTACHMENT_INIT_INVALIDATE,
    RAL_COMMAND_BUFFER_FB_ATTACHMENT_INIT_REUSE
} ral_command_buffer_fb_attachment_init_method;

typedef struct ral_command_buffer_image_binding
{
    uint32_t binding;

    ral_texture_format image_format;
    ral_texture        texture;
    bool               will_be_written_to;

    explicit ral_command_buffer_image_binding(uint32_t           in_binding,
                                              ral_texture_format in_image_format,
                                              ral_texture        in_texture,
                                              bool               in_will_be_written_to);
} ral_command_buffer_image_binding;

typedef struct ral_command_buffer_scissor_box
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];

    explicit ral_command_buffer_scissor_box(uint32_t        in_index,
                                            const uint32_t* size_uvec2,
                                            const uint32_t* xy_uvec2);
} ral_command_buffer_scissor_box;

typedef struct ral_command_buffer_texture_binding
{
    uint32_t    location;

    ral_sampler sampler;
    ral_texture texture;

    explicit ral_command_buffer_texture_binding(uint32_t    in_location,
                                                ral_sampler in_sampler,
                                                ral_texture in_texture);
} ral_command_buffer_texture_binding;

typedef struct ral_command_buffer_texture_to_texture_copy
{
    ral_texture dst_texture;
    ral_texture src_texture;

    uint32_t n_dst_texture_mipmap;
    uint32_t n_src_texture_mipmap;

    uint32_t dst_start_xyz[3];
    uint32_t src_start_xyz[3];

    uint32_t size[3];

    explicit ral_command_buffer_texture_to_texture_copy(ral_texture     in_dst_texture,
                                                        ral_texture     in_src_texture,
                                                        uint32_t        in_n_dst_texture_mipmap,
                                                        uint32_t        in_n_src_texture_mipmap,
                                                        const uint32_t* in_dst_start_xyz_uvec3,
                                                        const uint32_t* in_src_start_xyz_uvec3,
                                                        const uint32_t* in_size_uvec3);
} ral_command_buffer_texture_to_texture_copy;

typedef struct ral_command_buffer_viewport
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];

    explicit ral_command_buffer_viewport(uint32_t        in_index,
                                         const uint32_t* in_size_uvec2,
                                         const uint32_t* in_xy_uvec2);
} ral_command_buffer_viewport;

typedef enum
{

    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_BYTE_SINT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_BYTE_SNORM,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_BYTE_UINT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_BYTE_UNORM,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_FLOAT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_SHORT_SINT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_SHORT_SNORM,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_SHORT_UINT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_SHORT_UNORM,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_INT_SINT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_INT_SNORM,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_INT_UINT,
    RAL_COMMAND_BUFFER_VERTEX_ATTRIBUTE_FORMAT_INT_UNORM

} ral_command_buffer_vertex_attribute_format;

typedef struct ral_command_buffer_vertex_attribute
{
    uint32_t attribute_index;
    uint32_t binding_index;

    ral_command_buffer_vertex_attribute_format format;
    uint32_t                                   n_components;

    explicit ral_command_buffer_vertex_attribute(uint32_t                                   in_attribute_index,
                                                 uint32_t                                   in_binding_index,
                                                 ral_command_buffer_vertex_attribute_format in_format,
                                                 uint32_t                                   in_n_components);
} ral_command_buffer_vertex_attribute;

typedef struct ral_command_buffer_vertex_binding
{
    uint32_t binding_index;

    uint32_t divisor;
    uint32_t stride;

    explicit ral_command_buffer_vertex_binding(uint32_t in_binding_index,
                                               uint32_t in_divisor,
                                               uint32_t in_stride);
} ral_command_buffer_vertex_binding;

typedef uint32_t ral_command_buffer_sync_point_id;


/** TODO */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context context,
                                                    int         usage_bits,
                                                    bool        invokable_from_other_command_buffers);

/** TODO */
PUBLIC void ral_command_buffer_record_copy_texture_to_texture(ral_command_buffer                                recording_command_buffer,
                                                              uint32_t                                          n_copy_ops,
                                                              const ral_command_buffer_texture_to_texture_copy* copy_ops);
/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_indexed(ral_command_buffer                          recording_command_buffer,
                                                        uint32_t                                    n_draw_calls,
                                                        const ral_command_buffer_draw_call_indexed* draw_calls);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_indexed_indirect(ral_command_buffer                           recording_command_buffer,
                                                                 uint32_t                                     n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect* draw_calls);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_regular(ral_command_buffer                          recording_command_buffer,
                                                        uint32_t                                    n_draw_calls,
                                                        const ral_command_buffer_draw_call_regular* draw_calls);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_regular_indirect(ral_command_buffer                           recording_command_buffer,
                                                                 uint32_t                                     n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect* draw_calls);

/** TODO */
PUBLIC void ral_command_buffer_record_execute_command_buffer(ral_command_buffer recording_command_buffer,
                                                             ral_command_buffer command_buffer_to_execute);

/** TODO */
PUBLIC ral_command_buffer_sync_point_id ral_command_buffer_record_sync_point(ral_command_buffer recording_command_buffer);

/** TODO */
PUBLIC void ral_command_buffer_record_set_buffer_bindings(ral_command_buffer                       recording_command_buffer,
                                                          uint32_t                                 n_buffer_bindings,
                                                          const ral_command_buffer_buffer_binding* buffer_bindings);

/** TODO */
PUBLIC void ral_command_buffer_record_set_framebuffer_binding(ral_command_buffer                                  recording_command_buffer,
                                                              ral_framebuffer                                     draw_framebuffer,
                                                              uint32_t                                            n_color_attachments,
                                                              const ral_command_buffer_fb_attachment_init_method* color_attachment_init_methods,
                                                              ral_command_buffer_fb_attachment_init_method        depth_init_method,
                                                              ral_command_buffer_fb_attachment_init_method        stencil_init_method);

/** TODO */
PUBLIC void ral_command_buffer_record_set_graphics_state(ral_command_buffer recording_command_buffer,
                                                         ral_graphics_state state);

/** TODO */
PUBLIC void ral_command_buffer_record_set_image_bindings(ral_command_buffer                      recording_command_buffer,
                                                         uint32_t                                n_image_bindings,
                                                         const ral_command_buffer_image_binding* image_bindings);
/** TODO */
PUBLIC void ral_command_buffer_record_set_input_vertex_attributes(ral_command_buffer                         recording_command_buffer,
                                                                  uint32_t                                   n_vertex_attributes,
                                                                  const ral_command_buffer_vertex_attribute* vertex_attributes);

/** TODO */
PUBLIC void ral_command_buffer_record_set_input_vertex_bindings(ral_command_buffer                       recording_command_buffer,
                                                                uint32_t                                 n_vertex_bindings,
                                                                const ral_command_buffer_vertex_binding* vertex_bindings);

/** TODO */
PUBLIC void ral_command_buffer_record_set_scissor_boxes(ral_command_buffer                    recording_command_buffer,
                                                        uint32_t                              n_scissor_boxes,
                                                        const ral_command_buffer_scissor_box* scissor_boxes);
/** TODO */
PUBLIC void ral_command_buffer_record_set_texture_bindings(ral_command_buffer                        recording_command_buffer,
                                                           uint32_t                                  n_texture_bindings,
                                                           const ral_command_buffer_texture_binding* texture_bindings);

/** TODO */
PUBLIC void ral_command_buffer_record_set_viewports(ral_command_buffer                 recording_command_buffer,
                                                    uint32_t                           n_viewport,
                                                    const ral_command_buffer_viewport* viewports);

/** TODO */
PUBLIC void ral_command_buffer_record_wait_on_sync_point(ral_command_buffer               recording_command_buffer,
                                                         ral_command_buffer               sync_point_command_buffer,
                                                         ral_command_buffer_sync_point_id sync_point);

/** TODO */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer);

/** TODO */
PUBLIC bool ral_command_buffer_start_recording(ral_command_buffer command_buffer,
                                               ral_framebuffer    framebuffer);

/** TODO */
PUBLIC bool ral_command_buffer_stop_recording(ral_command_buffer command_buffer);


#endif /* RAL_COMMAND_BUFFER_H */