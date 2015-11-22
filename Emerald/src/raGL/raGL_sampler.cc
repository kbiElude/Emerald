/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_sampler.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"


typedef struct _raGL_sampler
{
    ogl_context context; /* NOT owned */
    ral_sampler sampler;
    GLint       id;      /* NOT owned */

    float max_anisotropy;
    float max_lod_bias;

    ogl_context_gl_entrypoints* entrypoints_ptr;

    _raGL_sampler(ogl_context in_context,
                  GLint       in_sampler_id,
                  ral_sampler in_sampler)
    {
        ogl_context_gl_limits_ext_texture_filter_anisotropic* limits_ext_texture_filter_anisotropic_ptr = NULL;
        ogl_context_gl_limits*                                limits_ptr                                = NULL;

        context = in_context;
        sampler = in_sampler;
        id      = in_sampler_id;

        ral_sampler_retain(sampler);

        max_anisotropy = 0.0f;
        max_lod_bias   = 0.0f;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS_EXT_TEXTURE_FILTER_ANISOTROPIC,
                                &limits_ext_texture_filter_anisotropic_ptr);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        max_anisotropy = limits_ext_texture_filter_anisotropic_ptr->texture_max_anisotropy_ext;
        max_lod_bias   = limits_ptr->max_texture_lod_bias;

        /* NOTE: Only GL is supported at the moment. */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "TODO");
    }

    ~_raGL_sampler()
    {
        /* Do not release the GL object. Object life-time is handled by raGL_backend. */
        if (sampler != NULL)
        {
            ral_sampler_release(sampler);

            sampler = NULL;
        } /* if (sampler != NULL) */
    }
} _raGL_sampler;


/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_sampler_init_rendering_thread_callback(ogl_context context,
                                                                                 void*       sampler)
{
    ral_color               border_color;
    GLenum                  compare_function_gl     = GL_NONE;
    ral_compare_function    compare_function_ral    = RAL_COMPARE_FUNCTION_UNKNOWN;
    bool                    compare_mode_enabled    = false;
    bool                    is_anisotropy_supported = false;
    float                   lod_bias;
    GLenum                  mag_filter_gl        = GL_NONE;
    ral_texture_filter      mag_filter_ral       = RAL_TEXTURE_FILTER_UNKNOWN;
    float                   max_anisotropy;
    float                   max_lod;
    GLenum                  min_filter_gl        = GL_NONE;
    ral_texture_filter      min_filter_ral       = RAL_TEXTURE_FILTER_UNKNOWN;
    float                   min_lod;
    ral_texture_mipmap_mode mipmap_mode_ral      = RAL_TEXTURE_MIPMAP_MODE_UNKNOWN;
    _raGL_sampler*          sampler_ptr          = (_raGL_sampler*) sampler;
    GLenum                  wrap_r_gl            = GL_NONE;
    ral_texture_wrap_mode   wrap_r_ral           = RAL_TEXTURE_WRAP_MODE_UNKNOWN;
    GLenum                  wrap_s_gl            = GL_NONE;
    ral_texture_wrap_mode   wrap_s_ral           = RAL_TEXTURE_WRAP_MODE_UNKNOWN;
    GLenum                  wrap_t_gl            = GL_NONE;
    ral_texture_wrap_mode   wrap_t_ral           = RAL_TEXTURE_WRAP_MODE_UNKNOWN;

    LOG_ERROR("Performance warning: raGL_sampler sync request.");

    /* Sanity checks */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_TEXTURE_FILTER_ANISOTROPIC,
                            &is_anisotropy_supported);

    ASSERT_DEBUG_SYNC (sampler_ptr != NULL,
                       "Input raGL_sampler instance is NULL");
    ASSERT_DEBUG_SYNC (context == sampler_ptr->context,
                       "Rendering context mismatch");
    ASSERT_ALWAYS_SYNC(is_anisotropy_supported,
                       "Anisotropic textures are not supported but the GL back-end assumes its availability.")

    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_BORDER_COLOR,
                            &border_color);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_COMPARE_FUNCTION,
                            &compare_function_ral);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_COMPARE_MODE_ENABLED,
                            &compare_mode_enabled);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_LOD_BIAS,
                            &lod_bias);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_LOD_MAX,
                            &max_lod);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_LOD_MIN,
                            &min_lod);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_MAG_FILTER,
                            &mag_filter_ral);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_MAX_ANISOTROPY,
                            &max_anisotropy);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_MIN_FILTER,
                            &min_filter_ral);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_MIPMAP_MODE,
                            &mipmap_mode_ral);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_WRAP_R,
                            &wrap_r_ral);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_WRAP_S,
                            &wrap_s_ral);
    ral_sampler_get_property(sampler_ptr->sampler,
                             RAL_SAMPLER_PROPERTY_WRAP_T,
                            &wrap_t_ral);

    compare_function_gl = raGL_utils_get_ogl_compare_function_for_ral_compare_function  (compare_function_ral);
    mag_filter_gl       = raGL_utils_get_ogl_texture_filter_for_ral_mag_texture_filter  (mag_filter_ral);
    min_filter_gl       = raGL_utils_get_ogl_texture_filter_for_ral_min_texture_filter  (min_filter_ral,
                                                                                         mipmap_mode_ral);
    wrap_r_gl           = raGL_utils_get_ogl_texture_wrap_mode_for_ral_texture_wrap_mode(wrap_r_ral);
    wrap_s_gl           = raGL_utils_get_ogl_texture_wrap_mode_for_ral_texture_wrap_mode(wrap_s_ral);
    wrap_t_gl           = raGL_utils_get_ogl_texture_wrap_mode_for_ral_texture_wrap_mode(wrap_t_ral);

    if (max_anisotropy > sampler_ptr->max_anisotropy)
    {
        /* Clamp the max anisotropy setting. */
        LOG_ERROR("Clamping max anisotropy setting for a GL sampler instance: requested value [%f] exceeds the maximum permitted value [%f]",
                  max_anisotropy,
                  sampler_ptr->max_anisotropy);

        max_anisotropy = sampler_ptr->max_anisotropy;
    }

    /* Configure the sampler instance. */
    switch (border_color.data_type)
    {
        case ral_color::RAL_COLOR_DATA_TYPE_FLOAT:
        {
            sampler_ptr->entrypoints_ptr->pGLSamplerParameterfv(sampler_ptr->id,
                                                                GL_TEXTURE_BORDER_COLOR,
                                                                border_color.f32);

            break;
        }

        case ral_color::RAL_COLOR_DATA_TYPE_SINT:
        {
            sampler_ptr->entrypoints_ptr->pGLSamplerParameterIiv(sampler_ptr->id,
                                                                 GL_TEXTURE_BORDER_COLOR,
                                                                 border_color.i32);

            break;
        }

        case ral_color::RAL_COLOR_DATA_TYPE_UINT:
        {
            sampler_ptr->entrypoints_ptr->pGLSamplerParameterIuiv(sampler_ptr->id,
                                                                  GL_TEXTURE_BORDER_COLOR,
                                                                  border_color.u32);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized border color data type");
        }
    } /* switch (border_color) */

    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_COMPARE_FUNC,
                                                       compare_function_gl);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_COMPARE_MODE,
                                                       compare_mode_enabled ? GL_COMPARE_REF_TO_TEXTURE
                                                                            : GL_NONE);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameterf(sampler_ptr->id,
                                                       GL_TEXTURE_LOD_BIAS,
                                                       lod_bias);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_MAG_FILTER,
                                                       mag_filter_gl);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameterf(sampler_ptr->id,
                                                       GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                                       max_anisotropy);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameterf(sampler_ptr->id,
                                                       GL_TEXTURE_MAX_LOD,
                                                       max_lod);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_MIN_FILTER,
                                                       min_filter_gl);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameterf(sampler_ptr->id,
                                                       GL_TEXTURE_MIN_LOD,
                                                       min_lod);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_WRAP_R,
                                                       wrap_r_gl);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_WRAP_S,
                                                       wrap_s_gl);
    sampler_ptr->entrypoints_ptr->pGLSamplerParameteri(sampler_ptr->id,
                                                       GL_TEXTURE_WRAP_T,
                                                       wrap_t_gl);
}


/** Please see header for specification */
PUBLIC raGL_sampler raGL_sampler_create(ogl_context context,
                                        GLint       sampler_id,
                                        ral_sampler sampler)
{
    _raGL_sampler* new_sampler_ptr = new (std::nothrow) _raGL_sampler(context,
                                                                      sampler_id,
                                                                      sampler);

    ASSERT_ALWAYS_SYNC(new_sampler_ptr != NULL,
                       "Out of memory");

    if (new_sampler_ptr != NULL)
    {
        /* Request rendering thread call-back to set up the sampler object */
        ogl_context_request_callback_from_context_thread(context,
                                                         _raGL_sampler_init_rendering_thread_callback,
                                                         new_sampler_ptr);
    } /* if (new_sampler_ptr != NULL) */

    return (raGL_sampler) new_sampler_ptr;
}


/** Please see header for specification */
PUBLIC void raGL_sampler_get_property(raGL_sampler          sampler,
                                      raGL_sampler_property property,
                                      void*                 out_result_ptr)
{
    _raGL_sampler* sampler_ptr = (_raGL_sampler*) sampler;

    /* Sanity checks */
    if (sampler == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input sampler is NULL");

        goto end;
    }

    /* Retrieve the requested property value */
    switch (property)
    {
        case RAGL_SAMPLER_PROPERTY_ID:
        {
            *(GLuint*) out_result_ptr = sampler_ptr->id;

            break;
        }

        case RAGL_SAMPLER_PROPERTY_SAMPLER:
        {
            *(ral_sampler*) out_result_ptr = sampler_ptr->sampler;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_sampler_property value.");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC void raGL_sampler_release(raGL_sampler sampler)
{
    ASSERT_DEBUG_SYNC(sampler != NULL,
                      "Input sampler is NULL");

    delete (_raGL_sampler*) sampler;
}
