/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ral/ral_sampler.h"
#include "system/system_log.h"


typedef struct _ral_sampler
{
    ral_color                 border_color;
    ral_compare_function      compare_function;
    bool                      compare_mode_enabled;
    float                     lod_bias;
    float                     lod_min;
    float                     lod_max;
    ral_texture_filter        mag_filter;
    float                     max_anisotropy;
    ral_texture_filter        min_filter;
    ral_texture_mipmap_mode   mipmap_mode;
    system_hashed_ansi_string name;
    ral_texture_wrap_mode     wrap_r;
    ral_texture_wrap_mode     wrap_s;
    ral_texture_wrap_mode     wrap_t;

    _ral_sampler(system_hashed_ansi_string in_name,
                 ral_color                 in_border_color,
                 ral_compare_function      in_compare_function,
                 bool                      in_compare_mode_enabled,
                 float                     in_lod_bias,
                 float                     in_lod_min,
                 float                     in_lod_max,
                 ral_texture_filter        in_mag_filter,
                 float                     in_max_anisotropy,
                 ral_texture_filter        in_min_filter,
                 ral_texture_mipmap_mode   in_mipmap_mode,
                 ral_texture_wrap_mode     in_wrap_r,
                 ral_texture_wrap_mode     in_wrap_s,
                 ral_texture_wrap_mode     in_wrap_t)
    {
        border_color         = in_border_color;
        compare_function     = in_compare_function;
        compare_mode_enabled = in_compare_mode_enabled;
        lod_bias             = in_lod_bias;
        lod_max              = in_lod_max;
        lod_min              = in_lod_min;
        mag_filter           = in_mag_filter;
        max_anisotropy       = in_max_anisotropy;
        min_filter           = in_min_filter;
        mipmap_mode          = in_mipmap_mode;
        name                 = in_name;
        wrap_r               = in_wrap_r;
        wrap_s               = in_wrap_s;
        wrap_t               = in_wrap_t;
    }

    ~_ral_sampler()
    {
        /* Nothing to do here. */
    }
} _ral_sampler;


/** Please see header for specification */
PRIVATE void _ral_sampler_release(void* sampler)
{
    delete (_ral_sampler*) sampler;
}


/** Please see header for specification */
PUBLIC ral_sampler ral_sampler_create(system_hashed_ansi_string      name,
                                      const ral_sampler_create_info* sampler_create_info_ptr)
{
    _ral_sampler* sampler_ptr = new (std::nothrow) _ral_sampler(name,
                                                                sampler_create_info_ptr->border_color,
                                                                sampler_create_info_ptr->compare_function,
                                                                sampler_create_info_ptr->compare_mode_enabled,
                                                                sampler_create_info_ptr->lod_bias,
                                                                sampler_create_info_ptr->lod_min,
                                                                sampler_create_info_ptr->lod_max,
                                                                sampler_create_info_ptr->mag_filter,
                                                                sampler_create_info_ptr->max_anisotropy,
                                                                sampler_create_info_ptr->min_filter,
                                                                sampler_create_info_ptr->mipmap_mode,
                                                                sampler_create_info_ptr->wrap_r,
                                                                sampler_create_info_ptr->wrap_s,
                                                                sampler_create_info_ptr->wrap_t);

    ASSERT_ALWAYS_SYNC(sampler_ptr != NULL,
                       "Out of memory");

    return (ral_sampler) sampler_ptr;
}

/** Please see header for specification */
PUBLIC void ral_sampler_get_property(ral_sampler          sampler,
                                     ral_sampler_property property,
                                     void*                out_result_ptr)
{
    const _ral_sampler* sampler_ptr = (_ral_sampler*) sampler;

    /* Sanity checks */
    if (sampler == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input sampler instance is NULL");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAL_SAMPLER_PROPERTY_BORDER_COLOR:
        {
            *(ral_color*) out_result_ptr = sampler_ptr->border_color;

            break;
        }

        case RAL_SAMPLER_PROPERTY_COMPARE_FUNCTION:
        {
            *(ral_compare_function*) out_result_ptr = sampler_ptr->compare_function;

            break;
        }

        case RAL_SAMPLER_PROPERTY_COMPARE_MODE_ENABLED:
        {
            *(bool*) out_result_ptr = sampler_ptr->compare_mode_enabled;

            break;
        }

        case RAL_SAMPLER_PROPERTY_LOD_BIAS:
        {
            *(float*) out_result_ptr = sampler_ptr->lod_bias;

            break;
        }

        case RAL_SAMPLER_PROPERTY_LOD_MAX:
        {
            *(float*) out_result_ptr = sampler_ptr->lod_max;

            break;
        }

        case RAL_SAMPLER_PROPERTY_LOD_MIN:
        {
            *(float*) out_result_ptr = sampler_ptr->lod_min;

            break;
        }

        case RAL_SAMPLER_PROPERTY_MAG_FILTER:
        {
            *(ral_texture_filter*) out_result_ptr = sampler_ptr->mag_filter;

            break;
        }

        case RAL_SAMPLER_PROPERTY_MAX_ANISOTROPY:
        {
            *(float*) out_result_ptr = sampler_ptr->max_anisotropy;

            break;
        }

        case RAL_SAMPLER_PROPERTY_MIN_FILTER:
        {
            *(ral_texture_filter*) out_result_ptr = sampler_ptr->min_filter;

            break;
        }

        case RAL_SAMPLER_PROPERTY_MIPMAP_MODE:
        {
            *(ral_texture_mipmap_mode*) out_result_ptr = sampler_ptr->mipmap_mode;

            break;
        }

        case RAL_SAMPLER_PROPERTY_WRAP_R:
        {
            *(ral_texture_wrap_mode*) out_result_ptr = sampler_ptr->wrap_r;

            break;
        }

        case RAL_SAMPLER_PROPERTY_WRAP_S:
        {
            *(ral_texture_wrap_mode*) out_result_ptr = sampler_ptr->wrap_s;

            break;
        }

        case RAL_SAMPLER_PROPERTY_WRAP_T:
        {
            *(ral_texture_wrap_mode*) out_result_ptr = sampler_ptr->wrap_t;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_sampler_property value.");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC bool ral_sampler_is_equal_to_create_info(ral_sampler                    sampler,
                                                const ral_sampler_create_info* sampler_create_info_ptr)
{
    bool          result      = false;
    _ral_sampler* sampler_ptr = (_ral_sampler*) sampler;

    /* Sanity checks */
    if (sampler == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input sampler is NULL");

        goto end;
    }

    if (sampler_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input sampler create info is NULL");

        goto end;
    }

    /* Compare the provided sampler instance with the create info descriptor */
    if (sampler_ptr->border_color.data_type != sampler_create_info_ptr->border_color.data_type)
    {
        goto end;
    }

    switch (sampler_ptr->border_color.data_type)
    {
        case ral_color::RAL_COLOR_DATA_TYPE_FLOAT:
        {
            if (fabs(sampler_ptr->border_color.f32[0] - sampler_create_info_ptr->border_color.f32[0]) > 1e-5f ||
                fabs(sampler_ptr->border_color.f32[1] - sampler_create_info_ptr->border_color.f32[1]) > 1e-5f ||
                fabs(sampler_ptr->border_color.f32[2] - sampler_create_info_ptr->border_color.f32[2]) > 1e-5f ||
                fabs(sampler_ptr->border_color.f32[3] - sampler_create_info_ptr->border_color.f32[3]) > 1e-5f)
            {
                goto end;
            }

            break;
        }

        case ral_color::RAL_COLOR_DATA_TYPE_SINT:
        case ral_color::RAL_COLOR_DATA_TYPE_UINT:
        {
            if (sampler_ptr->border_color.i32[0] != sampler_create_info_ptr->border_color.i32[0] ||
                sampler_ptr->border_color.i32[1] != sampler_create_info_ptr->border_color.i32[1] ||
                sampler_ptr->border_color.i32[2] != sampler_create_info_ptr->border_color.i32[2] ||
                sampler_ptr->border_color.i32[3] != sampler_create_info_ptr->border_color.i32[3])
            {
                goto end;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized border color type");

            break;
        }
    } /* switch (sampler_a_ptr->border_color.data_type) */

    if (sampler_ptr->compare_function     != sampler_create_info_ptr->compare_function     ||
        sampler_ptr->compare_mode_enabled != sampler_create_info_ptr->compare_mode_enabled ||
        sampler_ptr->mag_filter           != sampler_create_info_ptr->mag_filter           ||
        sampler_ptr->min_filter           != sampler_create_info_ptr->min_filter           ||
        sampler_ptr->mipmap_mode          != sampler_create_info_ptr->mipmap_mode          ||
        sampler_ptr->wrap_r               != sampler_create_info_ptr->wrap_r               ||
        sampler_ptr->wrap_s               != sampler_create_info_ptr->wrap_s               ||
        sampler_ptr->wrap_t               != sampler_create_info_ptr->wrap_t)
    {
        goto end;
    }

    if (fabs(sampler_ptr->lod_bias       - sampler_create_info_ptr->lod_bias)       > 1e-5f ||
        fabs(sampler_ptr->lod_max        - sampler_create_info_ptr->lod_max)        > 1e-5f ||
        fabs(sampler_ptr->lod_min        - sampler_create_info_ptr->lod_min)        > 1e-5f ||
        fabs(sampler_ptr->max_anisotropy - sampler_create_info_ptr->max_anisotropy) > 1e-5f)
    {
        goto end;
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC void ral_sampler_release(ral_sampler& sampler)
{
    delete (_ral_sampler*) sampler;
}