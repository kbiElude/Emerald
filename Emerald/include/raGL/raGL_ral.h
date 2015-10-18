#ifndef RAGL_RAL_HEADER
#define RAGL_RAL_HEADER

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"

/** TODO */
PUBLIC ogl_primitive_type raGL_get_ogl_primive_type_for_ral_primitive_type(ral_primitive_type in_primitive_type);

/** TODO */
PUBLIC ogl_texture_internalformat raGL_get_ogl_texture_internalformat_for_ral_texture_format(ral_texture_format in_texture_format);

#endif /* RAGL_RAL_HEADER */