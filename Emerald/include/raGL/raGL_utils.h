#ifndef RAGL_UTILS_HEADER
#define RAGL_UTILS_HEADER

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"


/** TODO */
PUBLIC ogl_texture_data_format raGL_utils_get_ogl_data_format_for_ral_format(ral_format in_format);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_blend_factor(ral_blend_factor blend_factor);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_blend_op(ral_blend_op blend_op);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_compare_op(ral_compare_op compare_op);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_index_type(ral_index_type in_index_type);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_polygon_mode(ral_polygon_mode polygon_mode);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_primitive_type(ral_primitive_type in_primitive_type);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_stencil_op(ral_stencil_op stencil_op);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_data_type(ral_texture_data_type in_data_type);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_filter_mag(ral_texture_filter in_texture_filter);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_filter_min(ral_texture_filter      in_texture_filter,
                                                                 ral_texture_mipmap_mode in_mipmap_mode);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_program_variable_type(ral_program_variable_type in_type);

/** TODO */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_shader_type(ral_shader_type in_shader_type);

/** TODO */
PUBLIC ogl_texture_internalformat raGL_utils_get_ogl_texture_internalformat_for_ral_format(ral_format in_texture_format);

/** TODO */
PUBLIC ogl_texture_target raGL_utils_get_ogl_texture_target_for_ral_texture_type(ral_texture_type in_texture_type);

/** TODO */
PUBLIC ogl_texture_wrap_mode raGL_utils_get_ogl_texture_wrap_mode_for_ral_texture_wrap_mode(ral_texture_wrap_mode in_texture_wrap_mode);

/** TODO */
PUBLIC ral_program_variable_type raGL_utils_get_ral_program_variable_type_for_ogl_enum(GLenum type);

/** TODO */
PUBLIC ral_format raGL_utils_get_ral_format_for_ogl_enum(GLenum internalformat);

/** TODO */
PUBLIC ral_format raGL_utils_get_ral_format_for_ogl_texture_internalformat(ogl_texture_internalformat internalformat);

/** TODO */
PUBLIC ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_enum(GLenum in_texture_target_glenum);

/** TODO */
PUBLIC EMERALD_API ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_texture_target(ogl_texture_target in_texture_target);

#endif /* RAGL_UTILS_HEADER */