#ifndef RAL_TYPES_H
#define RAL_TYPES_H

#include "system/system_types.h"

DECLARE_HANDLE(ral_buffer);
DECLARE_HANDLE(ral_command_buffer);
DECLARE_HANDLE(ral_context);
DECLARE_HANDLE(ral_framebuffer);
DECLARE_HANDLE(ral_graphics_state);
DECLARE_HANDLE(ral_sampler);
DECLARE_HANDLE(ral_texture);

typedef enum
{
    RAL_BUFFER_MAPPABILITY_NONE = 0,

    RAL_BUFFER_MAPPABILITY_READ_OP_BIT  = (1 << 0),
    RAL_BUFFER_MAPPABILITY_WRITE_OP_BIT = (1 << 1),

    RAL_BUFFER_MAPPABILITY_COHERENT_BIT = (1 << 2),

    /* Always last */
    RAL_BUFFER_MAPPABILITY_LAST_USED_BIT = 2
};
typedef int ral_buffer_mappability_bits;

typedef enum
{
    /* Use sparse memory backing */
    RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT = (1 << 0),

    /* Always last */
    RAL_BUFFER_PROPERTY_LAST_USED_BIT = 0
};
typedef int ral_buffer_property_bits;

typedef enum
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
};
typedef int ral_buffer_usage_bits;

typedef enum
{
    /* This queue accepts compute shader invocations */
    RAL_QUEUE_COMPUTE_BIT = 1 << 0,

    /* This queue accepts rasterization operations */
    RAL_QUEUE_GRAPHICS_BIT = 1 << 1,

    /* This queue accepts blit/copy/transfer ops */
    RAL_QUEUE_TRANSFER_BIT = 1 << 2,

    /* Always last */
    RAL_QUEUE_LAST_USED_BIT = 2
};
typedef int ral_queue_bits;

/** All info required to create a single buffer instance */
typedef struct
{
    uint32_t                    size;
    ral_buffer_mappability_bits mappability_bits;
    ral_buffer_property_bits    property_bits;
    ral_buffer_usage_bits       usage_bits;
    ral_queue_bits              user_queue_bits;
} ral_buffer_create_info;

/* RAL RGBA color */
typedef struct
{
    union
    {
        float    f32[4];
        int32_t  i32[4];
        uint32_t u32[4];
    };

    enum
    {
        RAL_COLOR_DATA_TYPE_FLOAT,
        RAL_COLOR_DATA_TYPE_SINT,
        RAL_COLOR_DATA_TYPE_UINT,
    } data_type;

} ral_color;

/* RAL compare function */
typedef enum
{

    RAL_COMPARE_FUNCTION_ALWAYS,
    RAL_COMPARE_FUNCTION_EQUAL,
    RAL_COMPARE_FUNCTION_LEQUAL,
    RAL_COMPARE_FUNCTION_LESS,
    RAL_COMPARE_FUNCTION_GEQUAL,
    RAL_COMPARE_FUNCTION_GREATER,
    RAL_COMPARE_FUNCTION_NEVER,
    RAL_COMPARE_FUNCTION_NOTEQUAL,

    RAL_COMPARE_FUNCTION_COUNT,
    RAL_COMPARE_FUNCTION_UNKNOWN = RAL_COMPARE_FUNCTION_COUNT
} ral_compare_function;

typedef enum
{
    /* not settable; system_callback_manager */
    RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,

    /* not settable, uint32_t.
     *
     * Maximum number of color attachments that can be configured and used at the same time for a single framebuffer.
     *
     * NOTE: This query will be passed to the rendering back-end.
     **/
    RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS,

    /* not settable, uint32_t.
     *
     * A system framebuffer is a framebuffer which should be used to copy/render data into the
     * back buffer of the rendering surface.
     *
     * NOTE: This query will be passed to the rendering back-end.
     * NOTE: Backends can use one or more system framebuffers. Make no assumptions in this regard.
     *       It is guaranteed that all framebuffer attachments will use the same properties (n of samples,
     *       texture format).
     */
    RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS,

    /* not settable; ogl_rendering_handler */
    RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,

    /* not settable, ral_texture_format
     *
     * NOTE: This query will be passed to the rendering back-end.
     **/
    RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_TEXTURE_FORMAT,

    /* not settable, ral_framebuffer[RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS].
     *
     * Please see documentation of RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS for more details.
     *
     * NOTE: This query will be passed to the rendering back-end.
     * */
    RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS,

    /* not settable; system_window */
    RAL_CONTEXT_PROPERTY_WINDOW
} ral_context_property;

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
    RAL_TEXTURE_MIPMAP_MODE_BASE,
    RAL_TEXTURE_MIPMAP_MODE_NEAREST,
    RAL_TEXTURE_MIPMAP_MODE_LINEAR,

    RAL_TEXTURE_MIPMAP_MODE_UNKNOWN
} ral_texture_mipmap_mode;

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
    ral_compare_function    compare_function;
    bool                    compare_mode_enabled;
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
        border_color.data_type = ral_color::RAL_COLOR_DATA_TYPE_FLOAT;
        border_color.f32[0]    = 0.0f;
        border_color.f32[1]    = 0.0f;
        border_color.f32[2]    = 0.0f;
        border_color.f32[3]    = 1.0f;
        compare_function       = RAL_COMPARE_FUNCTION_ALWAYS;
        compare_mode_enabled   = false;
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

/* RAL shader stages */
typedef enum
{
    RAL_SHADER_TYPE_COMPUTE,
    RAL_SHADER_TYPE_FRAGMENT,
    RAL_SHADER_TYPE_GEOMETRY,
    RAL_SHADER_TYPE_TESSELLATION_CONTROL,
    RAL_SHADER_TYPE_TESSELLATION_EVALUATION,
    RAL_SHADER_TYPE_VERTEX,

    /* Always last */
    RAL_SHADER_TYPE_UNKNOWN,
    RAL_SHADER_TYPE_COUNT = RAL_SHADER_TYPE_UNKNOWN
} ral_shader_type;

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
    /* The value accessible under the comp-onent should be set to zero. */
    RAL_TEXTURE_COMPONENT_ZERO,

    /* Always last */
    RAL_TEXTURE_COMPONENT_COUNT,
    RAL_TEXTURE_COMPONENT_UNKNOWN = RAL_TEXTURE_COMPONENT_COUNT,
} ral_texture_component;

/* RAL texture formats */
typedef enum
{
    /* NOTE: Make sure to update *_get_*_texture_internalformat_for_ral_texture_format() AND ral_utils_get_texture_format_property()
    *        functions whenever modifying this enum definition!
     */
    RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_UNORM, // GL_COMPRESSED_R11_EAC,
    RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_UNORM,   // GL_COMPRESSED_RED_RGTC1,
    RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_UNORM, // GL_COMPRESSED_RG11_EAC,
    RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_UNORM, // GL_COMPRESSED_RG_RGTC2,
    RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_SFLOAT, // GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB,
    RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_UFLOAT, // GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB,
    RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_ETC2_UNORM, // GL_COMPRESSED_RGB8_ETC2,
    RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM, // GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    RAL_TEXTURE_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM, //GL_COMPRESSED_RGBA8_ETC2_EAC,
    RAL_TEXTURE_FORMAT_COMPRESSED_RGBA_BPTC_UNORM, // GL_COMPRESSED_RGBA_BPTC_UNORM_ARB,
    RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_SNORM, // GL_COMPRESSED_SIGNED_R11_EAC,
    RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_SNORM, // GL_COMPRESSED_SIGNED_RED_RGTC1,
    RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_SNORM, // GL_COMPRESSED_SIGNED_RG11_EAC,
    RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_SNORM, // GL_COMPRESSED_SIGNED_RG_RGTC2,
    RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM, // GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
    RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM, // GL_COMPRESSED_SRGB8_ETC2,
    RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM, // GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    RAL_TEXTURE_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, // GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB,

    RAL_TEXTURE_FORMAT_DEPTH16_SNORM,
    RAL_TEXTURE_FORMAT_DEPTH24_SNORM,
    RAL_TEXTURE_FORMAT_DEPTH32_FLOAT,
    RAL_TEXTURE_FORMAT_DEPTH32_SNORM,

    RAL_TEXTURE_FORMAT_DEPTH24_STENCIL8,
    RAL_TEXTURE_FORMAT_DEPTH32F_STENCIL8,

    RAL_TEXTURE_FORMAT_R11FG11FB10F,

    RAL_TEXTURE_FORMAT_R16_FLOAT,
    RAL_TEXTURE_FORMAT_R16_SINT,
    RAL_TEXTURE_FORMAT_R16_SNORM,
    RAL_TEXTURE_FORMAT_R16_UINT,
    RAL_TEXTURE_FORMAT_R16_UNORM,

    RAL_TEXTURE_FORMAT_R3G3B2_UNORM,

    RAL_TEXTURE_FORMAT_R32_FLOAT,
    RAL_TEXTURE_FORMAT_R32_SINT,
    RAL_TEXTURE_FORMAT_R32_UINT,

    RAL_TEXTURE_FORMAT_R8_SINT,
    RAL_TEXTURE_FORMAT_R8_SNORM,
    RAL_TEXTURE_FORMAT_R8_UINT,
    RAL_TEXTURE_FORMAT_R8_UNORM,

    RAL_TEXTURE_FORMAT_RG16_FLOAT,
    RAL_TEXTURE_FORMAT_RG16_SINT,
    RAL_TEXTURE_FORMAT_RG16_SNORM,
    RAL_TEXTURE_FORMAT_RG16_UINT,
    RAL_TEXTURE_FORMAT_RG16_UNORM,

    RAL_TEXTURE_FORMAT_RG32_FLOAT,
    RAL_TEXTURE_FORMAT_RG32_SINT,
    RAL_TEXTURE_FORMAT_RG32_UINT,

    RAL_TEXTURE_FORMAT_RG8_UNORM,
    RAL_TEXTURE_FORMAT_RG8_SINT,
    RAL_TEXTURE_FORMAT_RG8_SNORM,
    RAL_TEXTURE_FORMAT_RG8_UINT,

    RAL_TEXTURE_FORMAT_RGB10_UNORM,

    RAL_TEXTURE_FORMAT_RGB10A2_UINT,
    RAL_TEXTURE_FORMAT_RGB10A2_UNORM,

    RAL_TEXTURE_FORMAT_RGB12_UNORM,

    RAL_TEXTURE_FORMAT_RGB16_FLOAT,
    RAL_TEXTURE_FORMAT_RGB16_SINT,
    RAL_TEXTURE_FORMAT_RGB16_SNORM,
    RAL_TEXTURE_FORMAT_RGB16_UINT,
    RAL_TEXTURE_FORMAT_RGB16_UNORM,

    RAL_TEXTURE_FORMAT_RGB32_FLOAT,
    RAL_TEXTURE_FORMAT_RGB32_SINT,
    RAL_TEXTURE_FORMAT_RGB32_UINT,

    RAL_TEXTURE_FORMAT_RGB4_UNORM,

    RAL_TEXTURE_FORMAT_RGB5_UNORM,

    RAL_TEXTURE_FORMAT_RGB5A1_UNORM,

    RAL_TEXTURE_FORMAT_RGB8_SINT,
    RAL_TEXTURE_FORMAT_RGB8_SNORM,
    RAL_TEXTURE_FORMAT_RGB8_UINT,
    RAL_TEXTURE_FORMAT_RGB8_UNORM,

    RAL_TEXTURE_FORMAT_RGB9E5_FLOAT,

    RAL_TEXTURE_FORMAT_RGBA12_UNORM,

    RAL_TEXTURE_FORMAT_RGBA16_FLOAT,
    RAL_TEXTURE_FORMAT_RGBA16_SINT,
    RAL_TEXTURE_FORMAT_RGBA16_SNORM,
    RAL_TEXTURE_FORMAT_RGBA16_UINT,
    RAL_TEXTURE_FORMAT_RGBA16_UNORM,

    RAL_TEXTURE_FORMAT_RGBA2_UNORM,

    RAL_TEXTURE_FORMAT_RGBA32_FLOAT,
    RAL_TEXTURE_FORMAT_RGBA32_SINT,
    RAL_TEXTURE_FORMAT_RGBA32_UINT,

    RAL_TEXTURE_FORMAT_RGBA4_UNORM,

    RAL_TEXTURE_FORMAT_RGBA8_UNORM,
    RAL_TEXTURE_FORMAT_RGBA8_SINT,
    RAL_TEXTURE_FORMAT_RGBA8_SNORM,
    RAL_TEXTURE_FORMAT_RGBA8_UINT,

    RAL_TEXTURE_FORMAT_SRGB8_UNORM,

    RAL_TEXTURE_FORMAT_SRGBA8_UNORM,

    RAL_TEXTURE_FORMAT_UNKNOWN,
    RAL_TEXTURE_FORMAT_COUNT = RAL_TEXTURE_FORMAT_UNKNOWN
} ral_texture_format;

typedef enum
{
    /* NOTE: Make sure to update *_for_ral_texture_type() functions whenever modifying
     *       this enum definition!
     */
    RAL_TEXTURE_TYPE_1D,
    RAL_TEXTURE_TYPE_1D_ARRAY,
    RAL_TEXTURE_TYPE_2D,
    RAL_TEXTURE_TYPE_2D_ARRAY,
    RAL_TEXTURE_TYPE_3D,
    RAL_TEXTURE_TYPE_BUFFER, /* OpenGL-only */
    RAL_TEXTURE_TYPE_CUBE_MAP,
    RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY,
    RAL_TEXTURE_TYPE_RECTANGLE, /* OpenGL-only */

    RAL_TEXTURE_TYPE_MULTISAMPLE_2D,
    RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY,

    RAL_TEXTURE_TYPE_UNKNOWN,
    RAL_TEXTURE_TYPE_COUNT = RAL_TEXTURE_TYPE_UNKNOWN
} ral_texture_type;

#endif /* RAL_TYPES_H */