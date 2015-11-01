#ifndef RAL_TYPES_H
#define RAL_TYPES_H

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