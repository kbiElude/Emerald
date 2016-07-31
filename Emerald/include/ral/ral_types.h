#ifndef RAL_TYPES_H
#define RAL_TYPES_H

#include "system/system_types.h"

DECLARE_HANDLE(ral_buffer);
DECLARE_HANDLE(ral_command_buffer);
DECLARE_HANDLE(ral_context);
DECLARE_HANDLE(ral_gfx_state);
DECLARE_HANDLE(ral_present_job);
DECLARE_HANDLE(ral_present_task);
DECLARE_HANDLE(ral_program);
DECLARE_HANDLE(ral_rendering_handler);
DECLARE_HANDLE(ral_sampler);
DECLARE_HANDLE(ral_scheduler);
DECLARE_HANDLE(ral_shader);
DECLARE_HANDLE(ral_texture);
DECLARE_HANDLE(ral_texture_pool);
DECLARE_HANDLE(ral_texture_view);


typedef struct
{
    system_time frame_time;
    uint32_t    n_frame;
    const int*  rendering_area_px_topdown;
} ral_rendering_handler_rendering_callback_frame_data;

/** Rendering handler call-back
 *
 *  @param context           TODO
 *  @param frame_time        TODO
 *  @param n_frame           TODO
 *  @param rendering_area_px_topdown [0]: x1 of the rendering area (in pixels)
 *                           [1]: y1 of the rendering area (in pixels)
 *                           [2]: x2 of the rendering area (in pixels)
 *                           [3]: y2 of the rendering area (in pixels)
 *  @param user_arg          TODO
 *
 */
typedef ral_present_job (*PFNRALRENDERINGHANDLERRENDERINGCALLBACK)(ral_context                                                context,
                                                                   void*                                                      user_arg,
                                                                   const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr);

typedef enum
{
    RAL_ACCESS_COLOR_ATTACHMENT_READ_BIT          = 1 << 0,
    RAL_ACCESS_COLOR_ATTACHMENT_WRITE_BIT         = 1 << 1,
    RAL_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT  = 1 << 2,
    RAL_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 1 << 3,
    RAL_ACCESS_HOST_READ_BIT                      = 1 << 4,
    RAL_ACCESS_HOST_WRITE_BIT                     = 1 << 5,
    RAL_ACCESS_INDEX_READ_BIT                     = 1 << 6,
    RAL_ACCESS_INDIRECT_COMMAND_READ_BIT          = 1 << 7,
    RAL_ACCESS_INPUT_ATTACHMENT_READ_BIT          = 1 << 8,
    RAL_ACCESS_MEMORY_READ_BIT                    = 1 << 9,
    RAL_ACCESS_MEMORY_WRITE_BIT                   = 1 << 10,
    RAL_ACCESS_SHADER_READ_BIT                    = 1 << 11,
    RAL_ACCESS_SHADER_WRITE_BIT                   = 1 << 12,
    RAL_ACCESS_TRANSFER_READ_BIT                  = 1 << 13,
    RAL_ACCESS_TRANSFER_WRITE_BIT                 = 1 << 14,
    RAL_ACCESS_UNIFORM_READ_BIT                   = 1 << 15,
    RAL_ACCESS_VERTEX_ATTRIBUTE_READ_BIT          = 1 << 16,
} ral_access;
typedef uint32_t ral_access_bits;

typedef enum
{
    RAL_BACKEND_TYPE_ES,
    RAL_BACKEND_TYPE_GL,

    RAL_BACKEND_TYPE_UNKNOWN,
    RAL_BACKEND_TYPE_COUNT = RAL_BACKEND_TYPE_UNKNOWN
} ral_backend_type;

typedef enum
{
    /* NOTE: If you need to change this enum, also update raGL_utils_get_ogl_enum_for_ral_blend_factor() */

    RAL_BLEND_FACTOR_CONSTANT_ALPHA,
    RAL_BLEND_FACTOR_CONSTANT_COLOR,
    RAL_BLEND_FACTOR_DST_ALPHA,
    RAL_BLEND_FACTOR_DST_COLOR,
    RAL_BLEND_FACTOR_ONE,
    RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    RAL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    RAL_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    RAL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    RAL_BLEND_FACTOR_SRC_ALPHA,
    RAL_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    RAL_BLEND_FACTOR_SRC_COLOR,
    RAL_BLEND_FACTOR_ZERO,
} ral_blend_factor;

typedef enum
{
    /* NOTE: If you need to change this enum, also update raGL_utils_get_ogl_enum_for_ral_blend_op(). */

    RAL_BLEND_OP_ADD,
    RAL_BLEND_OP_MAX,
    RAL_BLEND_OP_MIN,
    RAL_BLEND_OP_SUBTRACT,
    RAL_BLEND_OP_SUBTRACT_REVERSE,
} ral_blend_op;

enum
{
    RAL_BUFFER_MAPPABILITY_NONE = 0,

    RAL_BUFFER_MAPPABILITY_READ_OP_BIT  = (1 << 0),
    RAL_BUFFER_MAPPABILITY_WRITE_OP_BIT = (1 << 1),

    RAL_BUFFER_MAPPABILITY_COHERENT_BIT = (1 << 2),

    /* Always last */
    RAL_BUFFER_MAPPABILITY_LAST_USED_BIT = 2
} ral_buffer_mappability;
typedef int ral_buffer_mappability_bits;

enum
{
    /* Use sparse memory backing */
    RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT = (1 << 0),

    /* Always last */
    RAL_BUFFER_PROPERTY_LAST_USED_BIT = 0
};
typedef int ral_buffer_property_bits;

/** All data required to perform a RAM->buffer memory transfer */
typedef void (*PFNRALBUFFERUPDATEINFODESTRUCTIONCALLBACKPROC)(void* user_arg);

typedef struct ral_buffer_client_sourced_update_info
{
    const void* data;
    uint32_t    data_size;
    uint32_t    start_offset;

    PFNRALBUFFERUPDATEINFODESTRUCTIONCALLBACKPROC pfn_op_finished_callback_proc;
    void*                                         op_finished_callback_user_arg;

    ral_buffer_client_sourced_update_info()
    {
        data                          = nullptr;
        data_size                     = 0;
        op_finished_callback_user_arg = nullptr;
        pfn_op_finished_callback_proc = nullptr;
        start_offset                  = 0;
    }
} ral_buffer_client_sourced_update_info;


enum
{
    RAL_BUFFER_USAGE_COPY_BIT                     = (1 << 0),
    RAL_BUFFER_USAGE_INDEX_BUFFER_BIT             = (1 << 1),
    RAL_BUFFER_USAGE_INDIRECT_DISPATCH_BUFFER_BIT = (1 << 2),
    RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT     = (1 << 3),
    RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT    = (1 << 4),
    RAL_BUFFER_USAGE_RO_TEXTURE_BUFFER_BIT        = (1 << 5), /* read-only texture buffer */
    RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT           = (1 << 6),
    RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT            = (1 << 7),

    /* Always last */
    RAL_BUFFER_USAGE_LAST_USED_BIT = 7
} ral_buffer_usage;
typedef int ral_buffer_usage_bits;

typedef enum
{
    /* NOTE: Update raGL_utils_get_ogl_enum_for_ral_compare_op() if this enum is ever updated. */

    RAL_COMPARE_OP_ALWAYS,
    RAL_COMPARE_OP_EQUAL,
    RAL_COMPARE_OP_GEQUAL,
    RAL_COMPARE_OP_LEQUAL,
    RAL_COMPARE_OP_LESS,
    RAL_COMPARE_OP_NEQUAL,
    RAL_COMPARE_OP_NEVER,
    RAL_COMPARE_OP_GREATER,

    RAL_COMPARE_OP_UNKNOWN
} ral_compare_op;

typedef enum
{
    RAL_IMAGE_ACCESS_READ  = 1 << 0,
    RAL_IMAGE_ACCESS_WRITE = 1 << 1
} ral_image_access_bits;

typedef enum
{
    RAL_INDEX_TYPE_8BIT,
    RAL_INDEX_TYPE_16BIT,
    RAL_INDEX_TYPE_32BIT,

    /* Always last */
    RAL_INDEX_TYPE_COUNT
} ral_index_type;

typedef enum
{
    RAL_PIPELINE_STAGE_ALL_COMMANDS_BIT                   = 1 << 0,
    RAL_PIPELINE_STAGE_ALL_GRAPHICS_BIT                   = 1 << 1,
    RAL_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                 = 1 << 2,
    RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT        = 1 << 3,
    RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT                 = 1 << 4,
    RAL_PIPELINE_STAGE_DRAW_INDIRECT_BIT                  = 1 << 5,
    RAL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT           = 1 << 6,
    RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                = 1 << 7,
    RAL_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                = 1 << 8,
    RAL_PIPELINE_STAGE_HOST_BIT                           = 1 << 9,
    RAL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT            = 1 << 10,
    RAL_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT    = 1 << 11,
    RAL_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 1 << 12,
    RAL_PIPELINE_STAGE_TOP_OF_PIPE_BIT                    = 1 << 13,
    RAL_PIPELINE_STAGE_TRANSFER_BIT                       = 1 << 14,
    RAL_PIPELINE_STAGE_VERTEX_INPUT_BIT                   = 1 << 15,
    RAL_PIPELINE_STAGE_VERTEX_SHADER_BIT                  = 1 << 16,
} ral_pipeline_stage;
typedef int ral_pipeline_stage_bits;

typedef uint32_t ral_present_job_connection_id;
typedef uint32_t ral_present_task_id;

typedef void (*PFNRALPRESENTTASKCPUCALLBACKPROC)(void* user_arg);

/** Enumerator that describes allowed types for a program attribute */
typedef enum
{
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_VEC2,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_VEC3,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_VEC4,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT2,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT3,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT4,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT2x3,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT2x4,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT3x2,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT3x4,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT4x2,
    RAL_PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT4x3,
    RAL_PROGRAM_ATTRIBUTE_TYPE_INT,
    RAL_PROGRAM_ATTRIBUTE_TYPE_INT_VEC2,
    RAL_PROGRAM_ATTRIBUTE_TYPE_INT_VEC3,
    RAL_PROGRAM_ATTRIBUTE_TYPE_INT_VEC4,
    RAL_PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT,
    RAL_PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT_VEC2,
    RAL_PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT_VEC3,
    RAL_PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT_VEC4,

    RAL_PROGRAM_ATTRIBUTE_TYPE_UNDEFINED
} ral_program_attribute_type;

/** Structure that describes properties of a program attribute */
typedef struct 
{
    system_hashed_ansi_string  name;
    int32_t                    length;
    int32_t                    size;
    ral_program_attribute_type type;

} ral_program_attribute;

DECLARE_HANDLE(ral_program_block_buffer);

/* Describes RAL-recognized block types. */
typedef enum
{
    RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER,
    RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,

    RAL_PROGRAM_BLOCK_TYPE_UNDEFINED
} ral_program_block_type;

/** Enumerator that describes allowed types for a program uniform */
typedef enum
{
    /* NOTE: If you need to modify this array, make sure to update:
     *
     *       * raGL_utils_get_ogl_enum_for_ral_program_variable_type()
     *       * raGL_utils_get_ral_program_variable_type_for_ogl_enum()
     *       * ral_utils_get_ral_program_variable_type_property()
     **/

    RAL_PROGRAM_VARIABLE_TYPE_BOOL,
    RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC2,
    RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC3,
    RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC4,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
    RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_RECT,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_3D,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_BUFFER,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE,
    RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE_MAP_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_RECT,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_3D,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_BUFFER,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE,
    RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE_MAP_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_RECT,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_3D,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_BUFFER,
    RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_CUBE,
    RAL_PROGRAM_VARIABLE_TYPE_INT_VEC2,
    RAL_PROGRAM_VARIABLE_TYPE_INT_VEC3,
    RAL_PROGRAM_VARIABLE_TYPE_INT_VEC4,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_3D,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_BUFFER,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_SHADOW,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_RECT,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_3D,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_BUFFER,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_RECT,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_3D,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_BUFFER,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE_ARRAY,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC2,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC3,
    RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC4,
    RAL_PROGRAM_VARIABLE_TYPE_VOID,

    RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED,
    RAL_PROGRAM_VARIABLE_TYPE_COUNT = RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED

} ral_program_variable_type;

typedef enum
{
    /* NOTE: Make sure to also update ral_utils_get_ral_program_variable_type_class_property() if you adjust
     *       this enum.
     */

    RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE,
    RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX,
    RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER,
    RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR,
    RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR,

    RAL_PROGRAM_VARIABLE_TYPE_CLASS_UNKNOWN
} ral_program_variable_type_class;

typedef enum
{
    /* system_hashed_ansi_string */
    RAL_PROGRAM_VARIABLE_TYPE_CLASS_PROPERTY_NAME
} ral_program_variable_type_class_property;

typedef enum
{
    /* ral_program_variable_type_class */
    RAL_PROGRAM_VARIABLE_TYPE_PROPERTY_CLASS,

    /* system_hashed_ansi_string */
    RAL_PROGRAM_VARIABLE_TYPE_PROPERTY_NAME,
} ral_program_variable_type_property;

/** Structure that describes properties of a program uniform, or a shader storage block member, or the like.
 *
 *  For uniforms, this structure is used to describe both uniforms coming from the default uniform block,
 *  and regular uniform blocks.
 */
typedef struct
{
    /* Buffer variables */
    uint32_t top_level_array_size;
    uint32_t top_level_array_stride;

    /* Buffer variables, uniforms (default & regular uniform block): */
    uint32_t                      array_stride;
    uint32_t                      block_offset;
    uint32_t                      is_row_major_matrix; /* 1 = row-major, 0 = column-major OR not a matrix */
    uint32_t                      matrix_stride;
    int32_t                       length;
    int32_t                       size;             /* array size for arrayed uniforms or 1 otherwise */

    /* Output variables + uniforms (default & regular uniform block): */
    int32_t location;

    /* Common */
    system_hashed_ansi_string name;
    ral_program_variable_type type;

} ral_program_variable;

enum
{
    /* This queue accepts compute shader invocations */
    RAL_QUEUE_COMPUTE_BIT = 1 << 0,

    /* This queue accepts rasterization operations */
    RAL_QUEUE_GRAPHICS_BIT = 1 << 1,

    /* This queue accepts blit/copy/transfer ops */
    RAL_QUEUE_TRANSFER_BIT = 1 << 2,

    /* Always last */
    RAL_QUEUE_LAST_USED_BIT = 2
} ral_queue;
typedef int ral_queue_bits;

typedef enum
{
    RAL_RENDERING_HANDLER_EXECUTION_MODE_ONLY_IF_IDLE_BLOCK_TILL_FINISHED,
    RAL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_DONT_BLOCK,
    RAL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_BLOCK_TILL_FINISHED,
} ral_rendering_handler_execution_mode;

/** Enumerator that describes current rendering handler's playback status */
typedef enum
{
    RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED,
    RAL_RENDERING_HANDLER_PLAYBACK_STATUS_PAUSED,
    RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED
} ral_rendering_handler_playback_status;

/** Enumerator that describes rendering handler policy. */
typedef enum
{
    /* Rendering handler should limit the number of window swaps as defined by user-provided FPS rate. */
    RAL_RENDERING_HANDLER_POLICY_FPS,

    /* Rendering handler should not limit the number of window swaps */
    RAL_RENDERING_HANDLER_POLICY_MAX_PERFORMANCE,

    /* Rendering handler will render only ONE frame per Play() call */
    RAL_RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST
} ral_rendering_handler_policy;

typedef enum
{
    /* framebuffer RT index */
    RAL_BINDING_TYPE_RENDERTARGET,

    /* sampler + texture */
    RAL_BINDING_TYPE_SAMPLED_IMAGE,

    /* buffer (+ buffer_offset, if not 0) */
    RAL_BINDING_TYPE_STORAGE_BUFFER,

    /* texture */
    RAL_BINDING_TYPE_STORAGE_IMAGE,

    /* buffer (+ buffer offset, if not 0) */
    RAL_BINDING_TYPE_UNIFORM_BUFFER,

    RAL_BINDING_TYPE_UNKNOWN
} ral_binding_type;

/** All info required to perform a buffer region clear op */
typedef struct ral_buffer_clear_region_info
{
    uint32_t clear_value;
    uint32_t offset;
    uint32_t size;
} ral_buffer_clear_region_info;

/** All info required to perform a buffer->buffer copy */
typedef struct ral_buffer_copy_to_buffer_info
{
    uint32_t dst_buffer_region_start_offset;
    uint32_t region_size;
    uint32_t src_buffer_region_start_offset;

    ral_buffer_copy_to_buffer_info()
    {
        dst_buffer_region_start_offset = 0;
        region_size                    = 0;
        src_buffer_region_start_offset = 0;
    }
} ral_buffer_copy_to_buffer_info;

/** All info required to create a single buffer instance */
typedef struct ral_buffer_create_info
{
    ral_buffer                  parent_buffer;
    uint32_t                    size;           /* if parent_buffer != NULL && size == 0: all parent_buffer's storage should be used */
    uint32_t                    start_offset;

    ral_buffer_mappability_bits mappability_bits;
    ral_buffer_property_bits    property_bits;
    ral_buffer_usage_bits       usage_bits;
    ral_queue_bits              user_queue_bits;

    ral_buffer_create_info()
    {
        mappability_bits = 0;
        parent_buffer    = NULL;
        property_bits    = 0;
        size             = 0;
        start_offset     = 0;
        usage_bits       = 0;
        user_queue_bits  = 0;
    }
} ral_buffer_create_info;

typedef struct
{
    union
    {
        float    f32[4];
        int32_t  i32[4];
        uint32_t u32[4];
    } color;

    float    depth;
    uint32_t stencil;

} ral_clear_value;

typedef enum
{
    RAL_COLOR_DATA_TYPE_FLOAT,
    RAL_COLOR_DATA_TYPE_SINT,
    RAL_COLOR_DATA_TYPE_UINT,
} ral_color_data_type;

/* RAL RGBA color */
typedef struct ral_color
{
    union
    {
        float    f32[4];
        int32_t  i32[4];
        uint32_t u32[4];
    };

    ral_color_data_type data_type;

    bool operator==(const ral_color& in) const
    {
        if (!in.data_type != data_type)
        {
            return false;
        }

        switch (in.data_type)
        {
            case RAL_COLOR_DATA_TYPE_FLOAT:
            {
                return fabs(in.f32[0] - f32[0]) < 1e-5f &&
                       fabs(in.f32[1] - f32[1]) < 1e-5f &&
                       fabs(in.f32[2] - f32[2]) < 1e-5f &&
                       fabs(in.f32[3] - f32[3]) < 1e-5f;
            }

            case RAL_COLOR_DATA_TYPE_SINT:
            {
                return in.i32[0] == i32[0] &&
                       in.i32[1] == i32[1] &&
                       in.i32[2] == i32[2] &&
                       in.i32[3] == i32[3];
            }

            case RAL_COLOR_DATA_TYPE_UINT:
            {
                return in.u32[0] == u32[0] &&
                       in.u32[1] == u32[1] &&
                       in.u32[2] == u32[2] &&
                       in.u32[3] == u32[3];
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL color data type");
            }
        }

        return false;
    }
} ral_color;

typedef struct ral_command_buffer_create_info
{
    ral_queue_bits compatible_queues;
    bool           is_invokable_from_other_command_buffers;
    bool           is_resettable;
    bool           is_transient;

    /* If false, this command buffer will be ignored by back-ends. This means you will not be permitted
     * to schedule such cmd buffer for execution.
     *
     * Useful if you need the command buffer to act as a command container. Commnds from such cmd buffer
     * can still be appended to another executable command buffer.
     */
    bool is_executable;

    ral_command_buffer_create_info()
    {
        compatible_queues                       = 0;
        is_executable                           = true;
        is_invokable_from_other_command_buffers = false;
        is_resettable                           = false;
        is_transient                            = false;
    }
} ral_command_buffer_create_info;

typedef enum
{
    /* Backend instance. Private use only.
     *
     * not settable; void* (eg. raGL_backend for ES / GL).
     **/
    RAL_CONTEXT_PROPERTY_BACKEND,

    /* Backend-specific context handle. Private use only.
     *
     * not settable; void* (eg. ogl_context for ES / GL).
     **/
    RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,

    /* not settable; ral_backend_type.
     *
     * NOTE: This query will be passed to the rendering back-end.
     */
    RAL_CONTEXT_PROPERTY_BACKEND_TYPE,

    /* not settable; system_callback_manager */
    RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,

    /* not settable; bool
     *
     * NOTE: This query will be passed to the rendering back-end.
     */
    RAL_CONTEXT_PROPERTY_IS_INTEL_DRIVER,

    /* not settable; bool
     *
     * NOTE: This query will be passed to the rendering back-end.
     */
    RAL_CONTEXT_PROPERTY_IS_NV_DRIVER,

    /* not settable, uint32_t[3].
     *
     * NOTE: This query will be passed to the rendering back-end.
     **/
    RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_COUNT,

    /* not settable, uint32_t.
     *
     * NOTE: This query will be passed to the rendeering back-end.
     */
    RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,

    /* not settable, uint32_t[3].
     *
     * NOTE: This query will be passed to the rendering back-end.
     **/
    RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,

    /* not settable, uint32_t.
     *
     * NOTE: This query will be passed to the rendeering back-end.
     */
    RAL_CONTEXT_PROPERTY_MAX_UNIFORM_BLOCK_SIZE,

    /* not settable; ral_rendering_handler */
    RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,

    /* not settable, uint32_t.
     *
     * Required storage buffer alignment.
     *
     * NOTE: This query will be passed to the rendering back-end.
     **/
    RAL_CONTEXT_PROPERTY_STORAGE_BUFFER_ALIGNMENT,

    /* not settable, ral_texture_format
     *
     * NOTE: This query will be passed to the rendering back-end.
     */
    RAL_CONTEXT_PROPERTY_SYSTEM_FB_BACK_BUFFER_COLOR_FORMAT,

    /* not settable, uint32_t[2]
     *
     * NOTE: This query will be passed to the rendering back-end.
     */
    RAL_CONTEXT_PROPERTY_SYSTEM_FB_SIZE,

    /* not settable, uint32_t.
     *
     * Required uniform buffer alignment.
     *
     * NOTE: This query will be passed to the rendering back-end.
     **/
    RAL_CONTEXT_PROPERTY_UNIFORM_BUFFER_ALIGNMENT,

    /* not settable; demo_window */
    RAL_CONTEXT_PROPERTY_WINDOW_DEMO,

    /* not settable; system_window */
    RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM
} ral_context_property;

typedef enum
{
    RAL_CULL_MODE_BACK,
    RAL_CULL_MODE_FRONT,
    RAL_CULL_MODE_FRONT_AND_BACK,
    RAL_CULL_MODE_NONE,
} ral_cull_mode;

typedef enum
{
    RAL_FRONT_FACE_CCW,
    RAL_FRONT_FACE_CW,

    RAL_FRONT_FACE_UNKNOWN
} ral_front_face;

typedef enum
{
    /* source & destination */
    RAL_LOGIC_OP_AND,

    /* ~source & destination */
    RAL_LOGIC_OP_AND_INVERTED,

    /* source & ~destination */
    RAL_LOGIC_OP_AND_REVERSE,

    /* 0 */
    RAL_LOGIC_OP_CLEAR,

    /* source value */
    RAL_LOGIC_OP_COPY,

    /* inverted source value */
    RAL_LOGIC_OP_COPY_INVERTED,

    /* ~(source ^ destination) */
    RAL_LOGIC_OP_EQUIVALENT,

    /* inverted destination value */
    RAL_LOGIC_OP_INVERT,

    /* ~(source & destination) */
    RAL_LOGIC_OP_NAND,

    /* destination */
    RAL_LOGIC_OP_NOOP,

    /* ~(source | destination) */
    RAL_LOGIC_OP_NOR,

    /* source | destination */
    RAL_LOGIC_OP_OR,

    /* ~source | destination */
    RAL_LOGIC_OP_OR_INVERTED,

    /* source | ~destination */
    RAL_LOGIC_OP_OR_REVERSE,

    /* 1 */
    RAL_LOGIC_OP_SET,

    /* source ^ destination */
    RAL_LOGIC_OP_XOR,

    RAL_LOGIC_OP_UNKNOWN
} ral_logic_op;

typedef enum
{
    RAL_POLYGON_MODE_FILL,
    RAL_POLYGON_MODE_LINES,
    RAL_POLYGON_MODE_POINTS,

    RAL_POLYGON_MODE_UNKNOWN
} ral_polygon_mode;

/* Primitive types supported by RAL */
typedef enum
{
    /* NOTE: Make sure to update *_get_*_primitive_type_for_ral_primitive_type() functions whenever modifying
     *       this enum definition!
     */
    RAL_PRIMITIVE_TYPE_LINE_LOOP,
    RAL_PRIMITIVE_TYPE_LINE_STRIP,
    RAL_PRIMITIVE_TYPE_LINE_STRIP_ADJACENCY,
    RAL_PRIMITIVE_TYPE_LINES,
    RAL_PRIMITIVE_TYPE_LINES_ADJACENCY,
    RAL_PRIMITIVE_TYPE_POINTS,
    RAL_PRIMITIVE_TYPE_TRIANGLE_FAN,
    RAL_PRIMITIVE_TYPE_TRIANGLE_STRIP,
    RAL_PRIMITIVE_TYPE_TRIANGLE_STRIP_ADJACENCY,
    RAL_PRIMITIVE_TYPE_TRIANGLES,
    RAL_PRIMITIVE_TYPE_TRIANGLES_ADJACENCY,

    RAL_PRIMITIVE_TYPE_UNKNOWN,
    RAL_PRIMITIVE_TYPE_COUNT = RAL_PRIMITIVE_TYPE_UNKNOWN
} ral_primitive_type;

typedef enum
{
    RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE         = 1 << 0,
    RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT        = 1 << 1,
    RAL_PROGRAM_SHADER_STAGE_BIT_GEOMETRY        = 1 << 2,
    RAL_PROGRAM_SHADER_STAGE_BIT_TESS_CONTROL    = 1 << 3,
    RAL_PROGRAM_SHADER_STAGE_BIT_TESS_EVALUATION = 1 << 4,
    RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX          = 1 << 5,
} ral_program_shader_stage_bit;

typedef uint32_t ral_program_shader_stage_bits;

/* Information required to create a new RAL program instance */
typedef struct
{
    ral_program_shader_stage_bits active_shader_stages;
    system_hashed_ansi_string     name;
} ral_program_create_info;

typedef enum
{
    RAL_RENDERTARGET_LOAD_TYPE_CLEAR,
    RAL_RENDERTARGET_LOAD_TYPE_INVALIDATE,
    RAL_RENDERTARGET_LOAD_TYPE_READ
} ral_rendertarget_load_type;

/* RAL texture filter modes */
typedef enum
{
    RAL_TEXTURE_FILTER_NEAREST,
    RAL_TEXTURE_FILTER_LINEAR,

    RAL_TEXTURE_FILTER_COUNT,
    RAL_TEXTURE_FILTER_UNKNOWN = RAL_TEXTURE_FILTER_COUNT
} ral_texture_filter;

/* RAL texture mipmap modes */
typedef enum
{
    RAL_TEXTURE_MIPMAP_MODE_NEAREST,
    RAL_TEXTURE_MIPMAP_MODE_LINEAR,

    RAL_TEXTURE_MIPMAP_MODE_UNKNOWN
} ral_texture_mipmap_mode;

typedef enum
{
    /* not settable, bool */
    RAL_TEXTURE_MIPMAP_PROPERTY_CONTENTS_SET,

    /* not settable, uint32_t int */
    RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,

    /* not settable, uint32_t */
    RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,

    /* not settable, uint32_t */
    RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
} ral_texture_mipmap_property;

/* RAL texture wrap modes */
typedef enum
{

    RAL_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER,
    RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE,
    RAL_TEXTURE_WRAP_MODE_MIRRORED_CLAMP_TO_EDGE,
    RAL_TEXTURE_WRAP_MODE_MIRRORED_REPEAT,
    RAL_TEXTURE_WRAP_MODE_REPEAT,

    RAL_TEXTURE_WRAP_MODE_COUNT,
    RAL_TEXTURE_WRAP_MODE_UNKNOWN = RAL_TEXTURE_WRAP_MODE_COUNT,
} ral_texture_wrap_mode;

/** All info required to create a single sampler instance */
typedef struct ral_sampler_create_info
{
    ral_color               border_color;
    bool                    compare_mode_enabled;
    ral_compare_op          compare_op;
    float                   lod_bias;
    float                   lod_min;
    float                   lod_max;
    ral_texture_filter      mag_filter;
    float                   max_anisotropy;
    ral_texture_filter      min_filter;
    ral_texture_mipmap_mode mipmap_mode;
    ral_texture_wrap_mode   wrap_r;
    ral_texture_wrap_mode   wrap_s;
    ral_texture_wrap_mode   wrap_t;

    ral_sampler_create_info()
    {
        border_color.data_type = RAL_COLOR_DATA_TYPE_FLOAT;
        border_color.f32[0]    = 0.0f;
        border_color.f32[1]    = 0.0f;
        border_color.f32[2]    = 0.0f;
        border_color.f32[3]    = 1.0f;
        compare_mode_enabled   = false;
        compare_op             = RAL_COMPARE_OP_ALWAYS;
        lod_bias               = 0.0f;
        lod_min                = -1000.0f;
        lod_max                =  1000.0f;
        mag_filter             = RAL_TEXTURE_FILTER_LINEAR;
        max_anisotropy         = 1.0f;
        min_filter             = RAL_TEXTURE_FILTER_LINEAR;
        mipmap_mode            = RAL_TEXTURE_MIPMAP_MODE_LINEAR;
        wrap_r                 = RAL_TEXTURE_WRAP_MODE_REPEAT;
        wrap_s                 = RAL_TEXTURE_WRAP_MODE_REPEAT;
        wrap_t                 = RAL_TEXTURE_WRAP_MODE_REPEAT;
    }
} ral_sampler_create_info;

/* RAL shader source */
typedef enum
{
    RAL_SHADER_SOURCE_GLSL,

    RAL_SHADER_SOURCE_UNKNOWN
} ral_shader_source;

/* RAL shader stages */
typedef enum
{
    RAL_SHADER_TYPE_FIRST,

    RAL_SHADER_TYPE_COMPUTE = RAL_SHADER_TYPE_FIRST,
    RAL_SHADER_TYPE_FRAGMENT,
    RAL_SHADER_TYPE_GEOMETRY,
    RAL_SHADER_TYPE_TESSELLATION_CONTROL,
    RAL_SHADER_TYPE_TESSELLATION_EVALUATION,
    RAL_SHADER_TYPE_VERTEX,

    /* Always last */
    RAL_SHADER_TYPE_UNKNOWN,
    RAL_SHADER_TYPE_COUNT = RAL_SHADER_TYPE_UNKNOWN
} ral_shader_type;

typedef struct ral_shader_create_info
{
    /* NOTE: This structure does not take GLSL body, as RAL context needs to sign up for RAL shader notifications
     *       and that happens right after the object is constructed. */
    system_hashed_ansi_string name;
    ral_shader_source         source;
    ral_shader_type           type;

    /** Dummy constructor */
    ral_shader_create_info()
    {
        name      = NULL;
        source    = RAL_SHADER_SOURCE_UNKNOWN;
        type      = RAL_SHADER_TYPE_UNKNOWN;
    }

    /** TODO. Use to initialize the structure for GLSL-backed shader creation */
    ral_shader_create_info(system_hashed_ansi_string in_name,
                           ral_shader_type           in_type)
    {
        name      = in_name;
        source    = RAL_SHADER_SOURCE_GLSL;
        type      = in_type;
    }
} ral_shader_create_info;

typedef enum
{
    /* NOTE: If this enum is ever chasnged, make sure to update raGL_utils_get_ogl_enum_for_ral_stencil_op() */

    RAL_STENCIL_OP_DECREMENT_AND_CLAMP,
    RAL_STENCIL_OP_DECREMENT_AND_WRAP,
    RAL_STENCIL_OP_INCREMENT_AND_CLAMP,
    RAL_STENCIL_OP_INCREMENT_AND_WRAP,
    RAL_STENCIL_OP_KEEP,
    RAL_STENCIL_OP_REPLACE,
    RAL_STENCIL_OP_ZERO,
} ral_stencil_op;

typedef struct ral_stencil_op_state
{
    uint32_t       compare_mask;
    ral_compare_op compare_op;
    ral_stencil_op depth_fail;
    ral_stencil_op fail;
    uint32_t       reference_value;
    ral_stencil_op pass;
    uint32_t       write_mask;

    ral_stencil_op_state()
    {
        compare_mask    = ~0;
        compare_op      = RAL_COMPARE_OP_ALWAYS;
        depth_fail      = RAL_STENCIL_OP_KEEP;
        fail            = RAL_STENCIL_OP_KEEP;
        reference_value = 0;
        pass            = RAL_STENCIL_OP_KEEP;
        write_mask      = ~0;
    }
} ral_stencil_op_state;

typedef enum
{
    RAL_TEXTURE_ASPECT_COLOR_BIT   = 1 << 0,
    RAL_TEXTURE_ASPECT_DEPTH_BIT   = 1 << 1,
    RAL_TEXTURE_ASPECT_STENCIL_BIT = 1 << 2
} ral_texture_aspect;
typedef uint32_t ral_texture_aspect_bits;

/* RAL texture component. This is used eg. for texture swizzling. */
typedef enum
{
    /* The value accessible under the component should be set to the value of the texture's alpha component */
    RAL_TEXTURE_COMPONENT_ALPHA,
    /* The value accessible under the component should be set to the value of the texture's blue component */
    RAL_TEXTURE_COMPONENT_BLUE,
    /* The value accessible under the component should be set to the value of the texture's green component */
    RAL_TEXTURE_COMPONENT_GREEN,
    /* The value accessible under the component should be set to one. */
    RAL_TEXTURE_COMPONENT_ONE,
    /* The value accessible under the component should be set to the value of the texture's red component */
    RAL_TEXTURE_COMPONENT_RED,
    /* The value accessible under the component should be set to zero. */
    RAL_TEXTURE_COMPONENT_ZERO,

    /* The value accessible under the component should correspond to the original texture layout */
    RAL_TEXTURE_COMPONENT_IDENTITY,

    /* Always last */
    RAL_TEXTURE_COMPONENT_COUNT,
    RAL_TEXTURE_COMPONENT_UNKNOWN = RAL_TEXTURE_COMPONENT_COUNT,
} ral_texture_component;

typedef enum
{
    RAL_TEXTURE_DATA_TYPE_FLOAT,
    RAL_TEXTURE_DATA_TYPE_SBYTE,
    RAL_TEXTURE_DATA_TYPE_SINT,
    RAL_TEXTURE_DATA_TYPE_SSHORT,
    RAL_TEXTURE_DATA_TYPE_UBYTE,
    RAL_TEXTURE_DATA_TYPE_UINT,
    RAL_TEXTURE_DATA_TYPE_USHORT,

    RAL_TEXTURE_DATA_TYPE_UNKNOWN,
} ral_texture_data_type;

/* RAL data formats */
typedef enum
{
    /* NOTE: Make sure to update *_get_*_texture_internalformat_for_ral_format() AND ral_utils_get_format_property()
    *        functions whenever modifying this enum definition!
     */
    RAL_FORMAT_COMPRESSED_R11_EAC_UNORM,                        // GL_COMPRESSED_R11_EAC,
    RAL_FORMAT_COMPRESSED_BC4_UNORM,                            // GL_COMPRESSED_RED_RGTC1,
    RAL_FORMAT_COMPRESSED_RG11_EAC_UNORM,                       // GL_COMPRESSED_RG11_EAC,
    RAL_FORMAT_COMPRESSED_BC5_UNORM,                            // GL_COMPRESSED_RG_RGTC2,
    RAL_FORMAT_COMPRESSED_BC6_SFLOAT,                           // GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB,
    RAL_FORMAT_COMPRESSED_BC6_UFLOAT,                           // GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB,
    RAL_FORMAT_COMPRESSED_RGB8_ETC2_UNORM,                      // GL_COMPRESSED_RGB8_ETC2,
    RAL_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM,  // GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    RAL_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM,                 // GL_COMPRESSED_RGBA8_ETC2_EAC,
    RAL_FORMAT_COMPRESSED_BC7_UNORM,                            // GL_COMPRESSED_RGBA_BPTC_UNORM_ARB,
    RAL_FORMAT_COMPRESSED_R11_EAC_SNORM,                        // GL_COMPRESSED_SIGNED_R11_EAC,
    RAL_FORMAT_COMPRESSED_BC4_SNORM,                            // GL_COMPRESSED_SIGNED_RED_RGTC1,
    RAL_FORMAT_COMPRESSED_RG11_EAC_SNORM,                       // GL_COMPRESSED_SIGNED_RG11_EAC,
    RAL_FORMAT_COMPRESSED_BC5_SNORM,                            // GL_COMPRESSED_SIGNED_RG_RGTC2,
    RAL_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM,          // GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
    RAL_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM,                     // GL_COMPRESSED_SRGB8_ETC2,
    RAL_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM, // GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    RAL_FORMAT_COMPRESSED_BC7_SRGB_UNORM,                       // GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB,

    RAL_FORMAT_DEPTH16_SNORM,
    RAL_FORMAT_DEPTH24_SNORM,
    RAL_FORMAT_DEPTH32_FLOAT,
    RAL_FORMAT_DEPTH32_SNORM,

    RAL_FORMAT_DEPTH24_STENCIL8,
    RAL_FORMAT_DEPTH32F_STENCIL8,

    RAL_FORMAT_R11FG11FB10F,

    RAL_FORMAT_R16_FLOAT,
    RAL_FORMAT_R16_SINT,
    RAL_FORMAT_R16_SNORM,
    RAL_FORMAT_R16_UINT,
    RAL_FORMAT_R16_UNORM,

    RAL_FORMAT_R3G3B2_UNORM,

    RAL_FORMAT_R32_FLOAT,
    RAL_FORMAT_R32_SINT,
    RAL_FORMAT_R32_UINT,

    RAL_FORMAT_R8_SINT,
    RAL_FORMAT_R8_SNORM,
    RAL_FORMAT_R8_UINT,
    RAL_FORMAT_R8_UNORM,

    RAL_FORMAT_RG16_FLOAT,
    RAL_FORMAT_RG16_SINT,
    RAL_FORMAT_RG16_SNORM,
    RAL_FORMAT_RG16_UINT,
    RAL_FORMAT_RG16_UNORM,

    RAL_FORMAT_RG32_FLOAT,
    RAL_FORMAT_RG32_SINT,
    RAL_FORMAT_RG32_UINT,

    RAL_FORMAT_RG8_UNORM,
    RAL_FORMAT_RG8_SINT,
    RAL_FORMAT_RG8_SNORM,
    RAL_FORMAT_RG8_UINT,

    RAL_FORMAT_RGB10_UNORM,

    RAL_FORMAT_RGB10A2_UINT,
    RAL_FORMAT_RGB10A2_UNORM,

    RAL_FORMAT_RGB12_UNORM,

    RAL_FORMAT_RGB16_FLOAT,
    RAL_FORMAT_RGB16_SINT,
    RAL_FORMAT_RGB16_SNORM,
    RAL_FORMAT_RGB16_UINT,
    RAL_FORMAT_RGB16_UNORM,

    RAL_FORMAT_RGB32_FLOAT,
    RAL_FORMAT_RGB32_SINT,
    RAL_FORMAT_RGB32_UINT,

    RAL_FORMAT_RGB4_UNORM,

    RAL_FORMAT_RGB5_UNORM,

    RAL_FORMAT_RGB5A1_UNORM,

    RAL_FORMAT_RGB8_SINT,
    RAL_FORMAT_RGB8_SNORM,
    RAL_FORMAT_RGB8_UINT,
    RAL_FORMAT_RGB8_UNORM,

    RAL_FORMAT_RGB9E5_FLOAT,

    RAL_FORMAT_RGBA12_UNORM,

    RAL_FORMAT_RGBA16_FLOAT,
    RAL_FORMAT_RGBA16_SINT,
    RAL_FORMAT_RGBA16_SNORM,
    RAL_FORMAT_RGBA16_UINT,
    RAL_FORMAT_RGBA16_UNORM,

    RAL_FORMAT_RGBA2_UNORM,

    RAL_FORMAT_RGBA32_FLOAT,
    RAL_FORMAT_RGBA32_SINT,
    RAL_FORMAT_RGBA32_UINT,

    RAL_FORMAT_RGBA4_UNORM,

    RAL_FORMAT_RGBA8_UNORM,
    RAL_FORMAT_RGBA8_SINT,
    RAL_FORMAT_RGBA8_SNORM,
    RAL_FORMAT_RGBA8_UINT,

    RAL_FORMAT_SRGB8_UNORM,

    RAL_FORMAT_SRGBA8_UNORM,

    RAL_FORMAT_UNKNOWN,
    RAL_FORMAT_COUNT = RAL_FORMAT_UNKNOWN
} ral_format;

typedef enum
{
    RAL_FORMAT_TYPE_DS,

    RAL_FORMAT_TYPE_SFLOAT,
    RAL_FORMAT_TYPE_SINT,
    RAL_FORMAT_TYPE_SNORM,

    RAL_FORMAT_TYPE_UFLOAT,
    RAL_FORMAT_TYPE_UINT,
    RAL_FORMAT_TYPE_UNORM,

} ral_format_type;

typedef enum
{
    /* NOTE: Make sure to update *_for_ral_texture_type() and ral_utils_get_texture_type_property() functions whenever modifying
     *       this enum definition!
     */
    RAL_TEXTURE_TYPE_1D,
    RAL_TEXTURE_TYPE_1D_ARRAY,
    RAL_TEXTURE_TYPE_2D,
    RAL_TEXTURE_TYPE_2D_ARRAY,
    RAL_TEXTURE_TYPE_3D,
    RAL_TEXTURE_TYPE_CUBE_MAP,
    RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY,

    RAL_TEXTURE_TYPE_MULTISAMPLE_2D,       /* TODO: remove - merge with _2D and _2D_ARRAY types */
    RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY, /* TODO: remove - merge with _2D and _2D_ARRAY types */

    RAL_TEXTURE_TYPE_UNKNOWN,
    RAL_TEXTURE_TYPE_COUNT = RAL_TEXTURE_TYPE_UNKNOWN
} ral_texture_type;

/* Defines usage patterns for texture instances.
 *
 * For OpenGL back-end, these bits help determine whether a renderbuffer or a texture
 * should be spawned when creating the raGL_texture instance for a RAL texture.
 */
enum
{
    /* The described texture will be used as a destination for blit operations */
    RAL_TEXTURE_USAGE_BLIT_DST_BIT                 = 1 << 0,

    /* The described texture will be used as a source for blit operations */
    RAL_TEXTURE_USAGE_BLIT_SRC_BIT                 = 1 << 1,

    /* The described texture will have contents uploaded from client memory */
    RAL_TEXTURE_USAGE_CLIENT_MEMORY_UPDATE_BIT     = 1 << 2,

    /* The described texture will be used as a color attachment */
    RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT         = 1 << 3,

    /* The described texture will be used as a depth-stencil attachment */
    RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 1 << 4,

    /* The described texture will be read from by means of image load ops */
    RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT           = 1 << 5,

    /* The described texture will be modified by means of image store ops */
    RAL_TEXTURE_USAGE_IMAGE_STORE_OPS_BIT          = 1 << 6,

    /* The described texture will be sampled from in shader stages */
    RAL_TEXTURE_USAGE_SAMPLED_BIT                  = 1 << 7,

    RAL_TEXTURE_MAX_BIT_USED = 7
} ral_texture_usage;
typedef int ral_texture_usage_bits;

/** All info required to create a single texture instance */
typedef struct
{
    unsigned int              base_mipmap_depth;
    unsigned int              base_mipmap_height;
    unsigned int              base_mipmap_width;
    bool                      fixed_sample_locations;
    ral_format                format;
    system_hashed_ansi_string name;
    unsigned int              n_layers;
    unsigned int              n_samples;
    ral_texture_type          type;
    ral_texture_usage_bits    usage;
    bool                      use_full_mipmap_chain;
} ral_texture_create_info;

/** All info required to update a single texture mip-map */
typedef struct ral_texture_mipmap_client_sourced_update_info
{
    unsigned int n_layer;
    unsigned int n_mipmap;

    const void*           data;
    uint32_t              data_row_alignment;
    unsigned int          data_size;
    ral_texture_data_type data_type;

    void (*pfn_delete_handler_proc)(ral_texture_mipmap_client_sourced_update_info* data_ptr);
    void*  delete_handler_proc_user_arg;

    unsigned int region_size        [3];
    unsigned int region_start_offset[3];

    /** TODO */
    ral_texture_mipmap_client_sourced_update_info()
    {
        memset(this,
               0,
               sizeof(*this) );
    }

    /** TODO */
    ~ral_texture_mipmap_client_sourced_update_info()
    {
        if (pfn_delete_handler_proc != nullptr)
        {
            pfn_delete_handler_proc(this);
        }
    }
} ral_texture_mipmap_client_sourced_update_info;

typedef enum
{
    RAL_VERTEX_INPUT_RATE_PER_INSTANCE,
    RAL_VERTEX_INPUT_RATE_PER_VERTEX,

    RAL_VERTEX_INPUT_RATE_UNKNOWN
} ral_vertex_input_rate;


typedef struct ral_gfx_state_vertex_attribute
{
    ral_format                format;
    ral_vertex_input_rate     input_rate;
    system_hashed_ansi_string name;
    uint32_t                  offset;
    uint32_t                  stride;

    bool operator==(const ral_gfx_state_vertex_attribute& in) const
    {
        return (format     == in.format     &&
                input_rate == in.input_rate &&
                offset     == in.offset     &&
                stride     == in.stride)    &&
               system_hashed_ansi_string_is_equal_to_hash_string(name,
                                                                 in.name);
    }
} ral_gfx_state_vertex_attribute;

/**
 *
 *  NOTE: This structure does not use a default destructor.
 *
 **/
typedef struct ral_gfx_state_create_info
{
    bool alpha_to_coverage;
    bool alpha_to_one;
    bool culling;
    bool depth_bias;
    bool depth_bounds_test;
    bool depth_clamp;
    bool depth_test;
    bool depth_writes;
    bool logic_op_test;
    bool primitive_restart;
    bool rasterizer_discard;
    bool sample_shading;
    bool scissor_test;
    bool stencil_test;

    /*
     * If enabled, scissor boxes cannot be changed without switching to a different gfx state.
     *
     * If disabled, they must be configured with a ral_command_buffer_record_set_scissor_boxes()
     * before the first draw call is recorded.
     *
     * Disabled by default.
     **/
    bool static_scissor_boxes_enabled; 

    /*
     * If enabled, scissor boxes cannot be changed without switching to a different gfx state.
     *
     * If disabled, they must be configured with a ral_command_buffer_record_set_scissor_boxes()
     * before the first draw call is recorded.
     *
     * Disabled by default.
     **/
    bool static_viewports_enabled; 

    uint32_t                                                static_n_scissor_boxes_and_viewports;
    struct ral_command_buffer_set_scissor_box_command_info* static_scissor_boxes;
    struct ral_command_buffer_set_viewport_command_info*    static_viewports;

    /* Depth bias */
    float depth_bias_constant_factor;
    float depth_bias_slope_factor;

    /* Depth clamp */
    float depth_clamp_value;

    /* Depth bounds test */
    float max_depth_bounds;
    float min_depth_bounds;

    /* Depth test */
    ral_compare_op depth_test_compare_op;

    /* Logic op */
    ral_logic_op logic_op;

    /* Sample shading */
    float sample_shading_min_sample_shading;

    /* Stencil test */
    ral_stencil_op_state stencil_test_back_face;
    ral_stencil_op_state stencil_test_front_face;

    /* Other */
    ral_cull_mode      cull_mode;
    ral_front_face     front_face;
    float              line_width;
    uint32_t           n_patch_control_points;
    ral_polygon_mode   polygon_mode_back;
    ral_polygon_mode   polygon_mode_front;
    ral_primitive_type primitive_type;

    uint32_t                        n_vertex_attributes;
    ral_gfx_state_vertex_attribute* vertex_attribute_ptrs;

    ral_gfx_state_create_info()
    {
        alpha_to_coverage  = false;
        alpha_to_one       = false;
        culling            = false;
        depth_bias         = false;
        depth_bounds_test  = false;
        depth_clamp        = false;
        depth_test         = false;
        depth_writes       = false;
        logic_op_test      = false;
        primitive_restart  = false;
        rasterizer_discard = false;
        sample_shading     = false;
        scissor_test       = false;
        stencil_test       = false;

        static_scissor_boxes = false;
        static_viewports     = false;

        cull_mode                         = RAL_CULL_MODE_BACK;
        depth_bias_constant_factor        = 0.0f;
        depth_bias_slope_factor           = 1.0f;
        depth_clamp_value                 = 0.0f;
        depth_test_compare_op             = RAL_COMPARE_OP_ALWAYS;
        front_face                        = RAL_FRONT_FACE_CCW;
        line_width                        = 1.0f;
        logic_op                          = RAL_LOGIC_OP_NOOP;
        max_depth_bounds                  = 1.0f;
        min_depth_bounds                  = 0.0f;
        n_vertex_attributes               = 0;
        n_patch_control_points            = 0;
        polygon_mode_back                 = RAL_POLYGON_MODE_FILL;
        polygon_mode_front                = RAL_POLYGON_MODE_FILL;
        primitive_type                    = RAL_PRIMITIVE_TYPE_TRIANGLES;
        sample_shading_min_sample_shading = 1.0f;
        vertex_attribute_ptrs             = nullptr;
    }

    /** Creates a clone of the input gfx_state instance.
     *
     *  When no longer needed, the cloned instance must be released with .release() function.
     */
    static ral_gfx_state_create_info* clone(const ral_gfx_state_create_info* in_gfx_state_ptr,
                                            bool                             should_include_static_scissor_box_data,
                                            bool                             should_include_static_viewport_data,
                                            bool                             should_include_va_data);

    /** TODO */
    static void release(ral_gfx_state_create_info* in_gfx_state_ptr);

} ral_gfx_state_create_info;

typedef struct ral_texture_view_create_info
{
    ral_texture_aspect aspect;
    ral_format         format;
    ral_texture        texture;
    ral_texture_type   type;

    uint32_t n_base_layer;
    uint32_t n_base_mip;

    uint32_t n_layers;
    uint32_t n_mips;

    ral_texture_component component_order[4];

    ral_texture_view_create_info()
    {
        aspect             = static_cast<ral_texture_aspect>(0);
        component_order[0] = RAL_TEXTURE_COMPONENT_IDENTITY;
        component_order[1] = RAL_TEXTURE_COMPONENT_IDENTITY;
        component_order[2] = RAL_TEXTURE_COMPONENT_IDENTITY;
        component_order[3] = RAL_TEXTURE_COMPONENT_IDENTITY;
        format             = RAL_FORMAT_UNKNOWN;
        n_base_layer       = -1;
        n_base_mip         = -1;
        n_layers           = 0;
        n_mips             = 0;
        texture            = nullptr;
        type               = RAL_TEXTURE_TYPE_UNKNOWN;
    }
} ral_texture_view_create_info;

#endif /* RAL_TYPES_H */