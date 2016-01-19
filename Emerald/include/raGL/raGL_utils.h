#ifndef RAGL_UTILS_HEADER
#define RAGL_UTILS_HEADER

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"


/** TODO */
PUBLIC EMERALD_API ogl_compare_function raGL_utils_get_ogl_compare_function_for_ral_compare_function(ral_compare_function in_compare_function);

/** TODO */
PUBLIC EMERALD_API ogl_texture_data_format raGL_utils_get_ogl_data_format_for_ral_texture_format(ral_texture_format in_format);

/** TODO */
PUBLIC EMERALD_API ogl_texture_data_type raGL_utils_get_ogl_data_type_for_ral_texture_data_type(ral_texture_data_type in_data_type);

/** TODO */
PUBLIC EMERALD_API GLenum raGL_utils_get_ogl_enum_for_ral_program_uniform_type(ral_program_uniform_type in_uniform_type);

/** TODO */
PUBLIC EMERALD_API ogl_primitive_type raGL_utils_get_ogl_primive_type_for_ral_primitive_type(ral_primitive_type in_primitive_type);

/** TODO */
PUBLIC EMERALD_API ogl_shader_type raGL_utils_get_ogl_shader_type_for_ral_shader_type(ral_shader_type in_shader_type);

/** TODO */
PUBLIC EMERALD_API ogl_texture_filter raGL_utils_get_ogl_texture_filter_for_ral_mag_texture_filter(ral_texture_filter in_texture_filter);

/** TODO */
PUBLIC EMERALD_API ogl_texture_filter raGL_utils_get_ogl_texture_filter_for_ral_min_texture_filter(ral_texture_filter      in_texture_filter,
                                                                                                   ral_texture_mipmap_mode in_mipmap_mode);

/** TODO */
PUBLIC EMERALD_API ogl_texture_internalformat raGL_utils_get_ogl_texture_internalformat_for_ral_texture_format(ral_texture_format in_texture_format);

/** TODO */
PUBLIC EMERALD_API ogl_texture_target raGL_utils_get_ogl_texture_target_for_ral_texture_type(ral_texture_type in_texture_type);

/** TODO */
PUBLIC EMERALD_API ogl_texture_wrap_mode raGL_utils_get_ogl_texture_wrap_mode_for_ral_texture_wrap_mode(ral_texture_wrap_mode in_texture_wrap_mode);

/** TODO */
PUBLIC EMERALD_API ral_texture_format raGL_utils_get_ral_texture_format_for_ogl_enum(GLenum internalformat);

/** TODO */
PUBLIC EMERALD_API ral_texture_format raGL_utils_get_ral_texture_format_for_ogl_texture_internalformat(ogl_texture_internalformat internalformat);

/** TODO */
PUBLIC EMERALD_API ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_enum(GLenum in_texture_target_glenum);

/** TODO */
PUBLIC EMERALD_API ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_texture_target(ogl_texture_target in_texture_target);

#endif /* RAGL_UTILS_HEADER */