#ifndef RAL_COMMAND_BUFFER_H
#define RAL_COMMAND_BUFFER_H

#include "ral/ral_types.h"


#define N_MAX_DEPENDENCIES (8)


typedef enum
{
    /* Called back when command buffer contents is about to start recording.
     *
     * This call-back may also be fired for already recorded command buffers, in
     * which case it is assumed that previous cmd buffer contents should be wiped
     * out. This will only occur for command bufers which have been marked as
     * resettable at creation time.
     *
     * arg: ral_command_buffer instance.
     **/
    RAL_COMMAND_BUFFER_CALLBACK_ID_RECORDING_STARTED,

    /* Called back when command buffer contents has finished recording.
     *
     * arg: ral_command_buffer instance.
     */
    RAL_COMMAND_BUFFER_CALLBACK_ID_RECORDING_STOPPED,

    /* Always last */
    RAL_COMMAND_BUFFER_CALLBACK_ID_COUNT
} ral_command_buffer_callback_id;

typedef struct ral_command_buffer_copy_texture_to_texture_command_info
{
    ral_texture_aspect_bits aspect;
    ral_texture             dst_texture;
    ral_texture             src_texture;

    uint32_t n_dst_texture_layer;
    uint32_t n_dst_texture_mipmap;
    uint32_t n_src_texture_layer;
    uint32_t n_src_texture_mipmap;

    uint32_t dst_start_xyz[3];
    uint32_t src_start_xyz[3];

    uint32_t           dst_size[3];
    ral_texture_filter scaling_filter;
    uint32_t           src_size[3];

} ral_command_buffer_copy_texture_to_texture_command_info;

/* Defines any load/store/texel fetch-based buffer/texture access (read or write) a program may perform.
 *
 * You do not need to define binding accesses for:
 *
 * - buffers used for vertex fetches
 * - images used as render-targets
 * - images used as source/destination for copy ops.
 *
 * These dependencies will be automatically recognized when wrapping the command buffer in
 * a RAL present task.
 */
typedef struct ral_object_access
{
    ral_access_bits         accesses;
    ral_pipeline_stage_bits stages;

    ral_buffer       buffer;
    ral_texture_view texture_view;

    struct
    {
        /* NOTE: assumed to hold meaningful info if size != 0 */
        uint32_t offset;

        /* if known, set to != 0; otherwise, set to 0 */
        uint32_t size;
    } buffer_access_info;

    struct
    {
        ral_texture_aspect_bits aspects;
        uint32_t                n_base_layer;
        uint32_t                n_base_mip;
        uint32_t                n_layers;
        uint32_t                n_mips;
    } image_access_info;

    /* TODO.
     *
     * NOTE: For buffer object accesses only
     */
    static ral_object_access get_buffer_instance(ral_buffer              in_buffer,
                                                 ral_access_bits         in_accesses,
                                                 ral_pipeline_stage_bits in_stages,
                                                 uint32_t                in_offset,
                                                 uint32_t                in_size)
    {
        ral_object_access instance;

        memset(&instance,
               sizeof(instance),
               0);

        instance.accesses                  = in_accesses;
        instance.buffer                    = in_buffer;
        instance.buffer_access_info.offset = in_offset;
        instance.buffer_access_info.size   = in_size;
        instance.stages                    = in_stages;

        return instance;
    }

    /* TODO.
     *
     * NOTE: For texture object accesses only.
     */
    static ral_object_access get_texture_view_instance(ral_texture_view        in_texture_view,
                                                       ral_access_bits         in_accesses,
                                                       ral_pipeline_stage_bits in_stages,
                                                       ral_texture_aspect_bits in_aspects,
                                                       uint32_t                in_n_base_layer,
                                                       uint32_t                in_n_base_mip,
                                                       uint32_t                in_n_layers,
                                                       uint32_t                in_n_mips)
    {
        ral_object_access instance;

        memset(&instance,
               sizeof(instance),
               0);

        instance.accesses                       = in_accesses;
        instance.image_access_info.aspects      = in_aspects;
        instance.image_access_info.n_base_layer = in_n_base_layer;
        instance.image_access_info.n_base_mip   = in_n_base_mip;
        instance.image_access_info.n_layers     = in_n_layers;
        instance.image_access_info.n_mips       = in_n_mips;
        instance.stages                         = in_stages;
        instance.texture_view                   = in_texture_view;

        return instance;
    }
} ral_object_access;

typedef struct ral_command_buffer_draw_call_indexed_command_info
{
    ral_buffer     index_buffer;
    ral_index_type index_type;

    uint32_t n_indices;
    uint32_t n_instances;

    uint32_t base_instance;

    uint32_t first_index; /* number of indices to move forward in the index buffer before consuming index data */
    uint32_t base_vertex; /* constant added to each index when choosing elements from vertex arrays */

    ral_object_access read_deps[N_MAX_DEPENDENCIES];
    ral_object_access write_deps[N_MAX_DEPENDENCIES];
    uint32_t          n_read_deps;
    uint32_t          n_write_deps;
} ral_command_buffer_draw_call_indexed_command_info;

typedef struct ral_command_buffer_draw_call_indirect_command_info
{
    ral_buffer indirect_buffer;
    uint32_t   offset;
    uint32_t   stride;

    /* May be nullptr */
    ral_buffer index_buffer;
    /* Ignored if index_buffer == nullptr */
    ral_index_type index_type;

    ral_object_access read_deps [N_MAX_DEPENDENCIES];
    ral_object_access write_deps[N_MAX_DEPENDENCIES];
    uint32_t          n_read_deps;
    uint32_t          n_write_deps;
} ral_command_buffer_draw_call_indirect_command_info;

typedef struct ral_command_buffer_draw_call_regular_command_info
{
    uint32_t n_instances;
    uint32_t n_vertices;

    uint32_t base_instance;
    uint32_t base_vertex;

    ral_object_access read_deps[N_MAX_DEPENDENCIES];
    ral_object_access write_deps[N_MAX_DEPENDENCIES];
    uint32_t          n_read_deps;
    uint32_t          n_write_deps;
} ral_command_buffer_draw_call_regular_command_info;

typedef struct ral_command_buffer_execute_command_buffer_command_info
{

    ral_command_buffer command_buffer;

} ral_command_buffer_execute_command_buffer_command_info;

typedef enum
{
    /* not settable; system_callback_manager */
    RAL_COMMAND_BUFFER_PROPERTY_CALLBACK_MANAGER,

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

    /* not settable; uint32_t */
    RAL_COMMAND_BUFFER_PROPERTY_N_RECORDED_COMMANDS,

    /* not settable; ral_command_buffer_status */
    RAL_COMMAND_BUFFER_PROPERTY_STATUS,
} ral_command_buffer_property;

typedef struct
{
    ral_buffer buffer;
    uint32_t   offset;

    /* use 0 for the region to contain data from <offset, buffer size) range */
    uint32_t size;

} ral_command_buffer_buffer_binding_info;

typedef struct
{
    uint32_t rt_index;

} ral_command_buffer_rendertarget_binding_info;

typedef struct
{
    ral_sampler      sampler;
    ral_texture_view texture_view;

} ral_command_buffer_sampled_image_binding_info;

typedef struct
{
    ral_image_access_bits access_bits;
    ral_texture_view      texture_view;

} ral_command_buffer_storage_image_binding_info;

typedef struct ral_command_buffer_set_binding_command_info
{
    ral_binding_type binding_type;

    /* Binding or uniform name, to which the object is to be assigned */
    system_hashed_ansi_string name;

    union
    {
        ral_command_buffer_rendertarget_binding_info  rendertarget_binding;
        ral_command_buffer_sampled_image_binding_info sampled_image_binding;
        ral_command_buffer_buffer_binding_info        storage_buffer_binding;
        ral_command_buffer_storage_image_binding_info storage_image_binding;
        ral_command_buffer_buffer_binding_info        uniform_buffer_binding;
    };
} ral_command_buffer_set_binding_command_info;

typedef struct ral_command_buffer_set_gfx_state_command_info
{

    ral_gfx_state new_state;

} ral_command_buffer_set_gfx_state_command_info;

typedef struct ral_command_buffer_set_program_command_info
{

    ral_program new_program;

} ral_command_buffer_set_program_command_info;

typedef struct ral_command_buffer_set_color_rendertarget_command_info
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
    ral_texture_view texture_view;

    /* NOTE: OpenGL is far more flexible in terms of configuring channel writes, but
     *       Vulkan's our main API of interest, so need to take its restrictions into
     *       account here. */
    struct
    {
        bool color0  : 1;
        bool color1  : 1;
        bool color2  : 1;
        bool color3  : 1;
    } channel_writes;

    static ral_command_buffer_set_color_rendertarget_command_info get_preinitialized_instance()
    {
        ral_command_buffer_set_color_rendertarget_command_info result;

        result.blend_constant.f32[0]   = 0.0f;
        result.blend_constant.f32[1]   = 0.0f;
        result.blend_constant.f32[2]   = 0.0f;
        result.blend_constant.f32[3]   = 0.0f;
        result.blend_enabled           = false;
        result.blend_op_alpha          = RAL_BLEND_OP_ADD;
        result.blend_op_color          = RAL_BLEND_OP_ADD;
        result.channel_writes.color0   = true;
        result.channel_writes.color1   = true;
        result.channel_writes.color2   = true;
        result.channel_writes.color3   = true;
        result.dst_alpha_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.dst_color_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.rendertarget_index      = -1;
        result.src_alpha_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.src_color_blend_factor  = RAL_BLEND_FACTOR_ONE;
        result.texture_view            = nullptr;

        return result;
    }
} ral_command_buffer_set_color_rendertarget_command_info;

typedef struct ral_command_buffer_set_scissor_box_command_info
{
    uint32_t index;

    uint32_t size[2];
    uint32_t xy  [2];
} ral_command_buffer_set_scissor_box_command_info;


typedef struct ral_command_buffer_set_vertex_buffer_command_info
{
    ral_buffer buffer;
    uint32_t   location;
    uint32_t   start_offset;
} ral_command_buffer_set_vertex_buffer_command_info;

typedef struct ral_command_buffer_set_viewport_command_info
{
    uint32_t index;

    float depth_range[2]; // [0] = near, [1] = far
    float size       [2];
    float xy         [2];
} ral_command_buffer_set_viewport_command_info;

typedef enum
{
    RAL_COMMAND_BUFFER_STATUS_UNDEFINED,
    RAL_COMMAND_BUFFER_STATUS_RECORDING,
    RAL_COMMAND_BUFFER_STATUS_RECORDED
} ral_command_buffer_status;

typedef enum
{
    RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE,
    RAL_COMMAND_TYPE_DRAW_CALL_INDEXED,
    RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT,
    RAL_COMMAND_TYPE_DRAW_CALL_REGULAR,
    RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER,
    RAL_COMMAND_TYPE_SET_BINDING,
    RAL_COMMAND_TYPE_SET_COLOR_RENDERTARGET,
    RAL_COMMAND_TYPE_SET_GFX_STATE,
    RAL_COMMAND_TYPE_SET_PROGRAM,
    RAL_COMMAND_TYPE_SET_SCISSOR_BOX,
    RAL_COMMAND_TYPE_SET_VERTEX_BUFFER,
    RAL_COMMAND_TYPE_SET_VIEWPORT,

    RAL_COMMAND_TYPE_UNKNOWN
} ral_command_type;


/** TODO */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context                           context,
                                                    const ral_command_buffer_create_info* create_info_ptr);

/** TODO */
PUBLIC void ral_command_buffer_deinit();

/** TODO */
PUBLIC void ral_command_buffer_get_property(ral_command_buffer          command_buffer,
                                            ral_command_buffer_property property,
                                            void*                       out_result_ptr);

/** TODO */
PUBLIC bool ral_command_buffer_get_recorded_command(ral_command_buffer command_buffer,
                                                    uint32_t           n_command,
                                                    ral_command_type*  out_command_type_ptr,
                                                    const void**       out_command_ptr_ptr);

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
PUBLIC void ral_command_buffer_record_draw_call_indirect_regular(ral_command_buffer                                        recording_command_buffer,
                                                                 uint32_t                                                  n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect_command_info* draw_call_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_draw_call_regular(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_regular_command_info* draw_call_ptrs);

/** TODO
 *
 *  NOTE: All scheduled command buffers must be created for the same context.
 **/
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

PUBLIC void ral_command_buffer_record_set_program(ral_command_buffer recording_command_buffer,
                                                  ral_program        program);

/** TODO */
PUBLIC void ral_command_buffer_record_set_color_rendertarget(ral_command_buffer                                            recording_command_buffer,
                                                             uint32_t                                                      n_rendertargets,
                                                             const ral_command_buffer_set_color_rendertarget_command_info* rendertarget_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_scissor_boxes(ral_command_buffer                                     recording_command_buffer,
                                                        uint32_t                                               n_scissor_boxes,
                                                        const ral_command_buffer_set_scissor_box_command_info* scissor_box_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_vertex_buffers(ral_command_buffer                                       recording_command_buffer,
                                                         uint32_t                                                 n_vertex_buffers,
                                                         const ral_command_buffer_set_vertex_buffer_command_info* vertex_buffer_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_set_viewports(ral_command_buffer                                  recording_command_buffer,
                                                    uint32_t                                            n_viewports,
                                                    const ral_command_buffer_set_viewport_command_info* viewport_ptrs);

/** TODO */
PUBLIC void ral_command_buffer_record_sync_point(ral_command_buffer recording_command_buffer,
                                                 uint32_t*          out_sync_point_id);

/** TODO */
PUBLIC void ral_command_buffer_record_wait_for_sync_point(ral_command_buffer       recording_command_buffer,
                                                          const ral_command_buffer sync_point_parent_command_buffer,
                                                          uint32_t                 sync_point_id);

/** TODO */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer);

/** TODO */
PUBLIC bool ral_command_buffer_start_recording(ral_command_buffer command_buffer);

/** TODO */
PUBLIC bool ral_command_buffer_stop_recording(ral_command_buffer command_buffer);


#endif /* RAL_COMMAND_BUFFER_H */