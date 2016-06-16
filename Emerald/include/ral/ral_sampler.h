#ifndef RAL_SAMPLER_H
#define RAL_SAMPLER_H

#include "ral/ral_types.h"

typedef enum
{
    /* not settable; ral_color */
    RAL_SAMPLER_PROPERTY_BORDER_COLOR,

    /* not settable, bool */
    RAL_SAMPLER_PROPERTY_COMPARE_MODE_ENABLED,

    /* not settable; ral_compare_op */
    RAL_SAMPLER_PROPERTY_COMPARE_OP,

    /* not settable, ral_context */
    RAL_SAMPLER_PROPERTY_CONTEXT,

    /* not settable, float */
    RAL_SAMPLER_PROPERTY_LOD_BIAS,

    /* not settable, float */
    RAL_SAMPLER_PROPERTY_LOD_MAX,

    /* not settable, float */
    RAL_SAMPLER_PROPERTY_LOD_MIN,

    /* not settable, ral_texture_filter */
    RAL_SAMPLER_PROPERTY_MAG_FILTER,

    /* not settable, float */
    RAL_SAMPLER_PROPERTY_MAX_ANISOTROPY,

    /* not settable, ral_texture_filter */
    RAL_SAMPLER_PROPERTY_MIN_FILTER,

    /* not settable, ral_texture_mipmap_mode */
    RAL_SAMPLER_PROPERTY_MIPMAP_MODE,

    /* not settable, ral_texture_wrap_mode */
    RAL_SAMPLER_PROPERTY_WRAP_R,

    /* not settable, ral_texture_wrap_mode */
    RAL_SAMPLER_PROPERTY_WRAP_S,

    /* not settable, ral_texture_wrap_mode */
    RAL_SAMPLER_PROPERTY_WRAP_T

} ral_sampler_property;


/** TODO
 *
 *  NOTE: This function should only be used by ral_context. Use ral_context_* entry-points() to create
 *        a sampler instance instead.
 **/
PUBLIC ral_sampler ral_sampler_create(ral_context                    context,
                                      system_hashed_ansi_string      name,
                                      const ral_sampler_create_info* sampler_create_info_ptr);

/** TODO */
PUBLIC void ral_sampler_get_property(ral_sampler          sampler,
                                     ral_sampler_property property,
                                     void*                out_result_ptr);

/** TODO */
PUBLIC bool ral_sampler_is_equal_to_create_info(ral_sampler                    sampler,
                                                const ral_sampler_create_info* sampler_create_info_ptr);

/** TODO */
PUBLIC void ral_sampler_release(ral_sampler& sampler);

#endif /* RAL_SAMPLER_H */