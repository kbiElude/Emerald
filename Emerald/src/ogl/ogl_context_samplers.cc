/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_samplers.h"
#include "ogl/ogl_sampler.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>

/* Private type definitions */

/** TODO */
typedef struct _ogl_context_samplers
{
    ogl_context             context;
    system_resizable_vector samplers;

    _ogl_context_samplers()
    {
        context  = NULL;
        samplers = system_resizable_vector_create(4); /* capacity */
    }

    ~_ogl_context_samplers()
    {
        LOG_INFO("Sampler manager deallocating..");

        if (samplers != NULL)
        {
            ogl_sampler sampler = NULL;

            while (system_resizable_vector_pop(samplers,
                                              &sampler) )
            {
                ogl_sampler_release(sampler);

                sampler = NULL;
            }

            system_resizable_vector_release(samplers);

            samplers = NULL;
        }
    }
} _ogl_context_samplers;


/** TODO */
typedef struct _ogl_context_samplers_create_renderer_callback_user_arg
{
    const GLfloat* border_color;
    const GLenum*  mag_filter_ptr;
    const GLfloat* max_lod_ptr;
    const GLenum*  min_filter_ptr;
    const GLfloat* min_lod_ptr;
    const GLenum*  texture_compare_func_ptr;
    const GLenum*  texture_compare_mode_ptr;
    const GLenum*  wrap_r_ptr;
    const GLenum*  wrap_s_ptr;
    const GLenum*  wrap_t_ptr;

    ogl_sampler          result;
    ogl_context_samplers samplers;
} _ogl_context_samplers_create_renderer_callback_user_arg;


/** TODO */
PRIVATE system_hashed_ansi_string _ogl_sampler_get_new_sampler_name(ogl_context_samplers samplers)
{
    std::stringstream         name_sstream;
    unsigned int              n_samplers  = 0;
    system_hashed_ansi_string result       = NULL;
    _ogl_context_samplers*    samplers_ptr = (_ogl_context_samplers*) samplers;

    system_resizable_vector_get_property(samplers_ptr->samplers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_samplers);

    name_sstream << "Sampler "
                 << n_samplers;

    result = system_hashed_ansi_string_create(name_sstream.str().c_str() );

    return result;
}

/** TODO */
PRIVATE void _ogl_context_samplers_create_renderer_callback(ogl_context context,
                                                            void*       user_arg)
{
    _ogl_context_samplers_create_renderer_callback_user_arg* user_arg_ptr = (_ogl_context_samplers_create_renderer_callback_user_arg*) user_arg;

    user_arg_ptr->result = ogl_sampler_create(context,
                                              _ogl_sampler_get_new_sampler_name(user_arg_ptr->samplers) );

     ASSERT_DEBUG_SYNC(user_arg_ptr->result != NULL,
                       "Could not create a new ogl_sampler instance");

     if (user_arg_ptr->result != NULL)
     {
         bool should_lock = true;

         if (user_arg_ptr->border_color != NULL)
         {
             ogl_sampler_set_property(user_arg_ptr->result,
                                      OGL_SAMPLER_PROPERTY_BORDER_COLOR,
                                      user_arg_ptr->border_color);
         }

         if (user_arg_ptr->mag_filter_ptr != NULL)
         {
             ogl_sampler_set_property(user_arg_ptr->result,
                                      OGL_SAMPLER_PROPERTY_MAG_FILTER,
                                      user_arg_ptr->mag_filter_ptr);
         }

         if (user_arg_ptr->max_lod_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_MAX_LOD,
                                     user_arg_ptr->max_lod_ptr);
         }

         if (user_arg_ptr->min_filter_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_MIN_FILTER,
                                     user_arg_ptr->min_filter_ptr);
         }

         if (user_arg_ptr->min_lod_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_MIN_LOD,
                                     user_arg_ptr->min_lod_ptr);
         }

         if (user_arg_ptr->texture_compare_func_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_FUNC,
                                     user_arg_ptr->texture_compare_func_ptr);
         }

         if (user_arg_ptr->texture_compare_mode_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_MODE,
                                     user_arg_ptr->texture_compare_mode_ptr);
         }

         if (user_arg_ptr->wrap_r_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_WRAP_R,
                                     user_arg_ptr->wrap_r_ptr);
         }

         if (user_arg_ptr->wrap_s_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_WRAP_S,
                                     user_arg_ptr->wrap_s_ptr);
         }

         if (user_arg_ptr->wrap_t_ptr != NULL)
         {
            ogl_sampler_set_property(user_arg_ptr->result,
                                     OGL_SAMPLER_PROPERTY_WRAP_T,
                                     user_arg_ptr->wrap_t_ptr);
         }

         ogl_sampler_set_property(user_arg_ptr->result,
                                  OGL_SAMPLER_PROPERTY_LOCKED,
                                 &should_lock);
     } /* if (result != NULL) */
}

/** Please see header for specification */
PUBLIC ogl_context_samplers ogl_context_samplers_create(ogl_context context)
{
    _ogl_context_samplers* samplers_ptr = new (std::nothrow) _ogl_context_samplers;

    ASSERT_ALWAYS_SYNC(samplers_ptr != NULL,
                       "Out of memory");

    if (samplers_ptr != NULL)
    {
        samplers_ptr->context  = context;
        samplers_ptr->samplers = system_resizable_vector_create(4 /* capacity */);
    }

    return (ogl_context_samplers) samplers_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_sampler ogl_context_samplers_get_sampler(ogl_context_samplers samplers,
                                                                const GLfloat*       border_color,
                                                                const GLenum*        mag_filter_ptr,
                                                                const GLfloat*       max_lod_ptr,
                                                                const GLenum*        min_filter_ptr,
                                                                const GLfloat*       min_lod_ptr,
                                                                const GLenum*        texture_compare_func_ptr,
                                                                const GLenum*        texture_compare_mode_ptr,
                                                                const GLenum*        wrap_r_ptr,
                                                                const GLenum*        wrap_s_ptr,
                                                                const GLenum*        wrap_t_ptr)
{
    const _ogl_context_samplers* samplers_ptr = (const _ogl_context_samplers*) samplers;
    uint32_t                     n_samplers   = 0;
    ogl_sampler                  result       = NULL;

    LOG_INFO("Performance warning: slow code-path call: ogl_samplers_get_sampler()");

    system_resizable_vector_get_property(samplers_ptr->samplers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_samplers);

    for (uint32_t n_sampler = 0;
                  n_sampler < n_samplers;
                ++n_sampler)
    {
        ogl_sampler current_sampler = NULL;

        if (system_resizable_vector_get_element_at(samplers_ptr->samplers,
                                                   n_sampler,
                                                  &current_sampler) )
        {
            GLfloat current_sampler_border_color[4]      = {0};
            GLenum  current_sampler_mag_filter           = GL_NONE;
            GLfloat current_sampler_max_lod              = 0;
            GLenum  current_sampler_min_filter           = GL_NONE;
            GLfloat current_sampler_min_lod              = 0;
            GLenum  current_sampler_texture_compare_func = GL_NONE;
            GLenum  current_sampler_texture_compare_mode = GL_NONE;
            GLenum  current_sampler_wrap_r               = GL_NONE;
            GLenum  current_sampler_wrap_s               = GL_NONE;
            GLenum  current_sampler_wrap_t               = GL_NONE;

            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_BORDER_COLOR,
                                    &current_sampler_border_color);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_MAG_FILTER,
                                    &current_sampler_mag_filter);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_MAX_LOD,
                                    &current_sampler_max_lod);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_MIN_FILTER,
                                    &current_sampler_min_filter);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_MIN_LOD,
                                    &current_sampler_min_lod);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_FUNC,
                                    &current_sampler_texture_compare_func);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_MODE,
                                    &current_sampler_texture_compare_mode);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_WRAP_R,
                                    &current_sampler_wrap_r);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_WRAP_S,
                                    &current_sampler_wrap_s);
            ogl_sampler_get_property(current_sampler,
                                     OGL_SAMPLER_PROPERTY_WRAP_T,
                                    &current_sampler_wrap_t);

            if ((border_color             == NULL ||
                (border_color             != NULL && fabs(current_sampler_border_color[0] - border_color[0]) < 1e-5f    &&
                                                     fabs(current_sampler_border_color[1] - border_color[1]) < 1e-5f    &&
                                                     fabs(current_sampler_border_color[2] - border_color[2]) < 1e-5f    &&
                                                     fabs(current_sampler_border_color[3] - border_color[3]) < 1e-5f))  &&
                (mag_filter_ptr           == NULL ||
                 mag_filter_ptr           != NULL && current_sampler_mag_filter           == *mag_filter_ptr)           &&
                (max_lod_ptr              == NULL ||
                 max_lod_ptr              != NULL && current_sampler_max_lod              == *max_lod_ptr)              &&
                (min_filter_ptr           == NULL ||
                 min_filter_ptr           != NULL && current_sampler_min_filter           == *min_filter_ptr)           &&
                (min_lod_ptr              == NULL ||
                 min_lod_ptr              != NULL && current_sampler_min_lod              == *min_lod_ptr)              &&
                (texture_compare_func_ptr == NULL ||
                 texture_compare_func_ptr != NULL && current_sampler_texture_compare_func == *texture_compare_func_ptr) &&
                (texture_compare_mode_ptr == NULL ||
                 texture_compare_mode_ptr != NULL && current_sampler_texture_compare_mode == *texture_compare_mode_ptr) &&
                (wrap_r_ptr               == NULL ||
                 wrap_r_ptr               != NULL && current_sampler_wrap_r               == *wrap_r_ptr)               &&
                (wrap_s_ptr               == NULL ||
                 wrap_s_ptr               != NULL && current_sampler_wrap_s               == *wrap_s_ptr)               &&
                (wrap_t_ptr               == NULL ||
                 wrap_t_ptr               != NULL && current_sampler_wrap_t               == *wrap_t_ptr))
            {
                result = current_sampler;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve sampler descriptor at index [%d]",
                              n_sampler);
        }
    } /* for (all stored samplers) */

    /* If the sampler was not found, we need to create it.
     *
     * Note that we lock the created sampler. This is to ensure the caller does not attempt
     * to modify the sampler configuration.
     *
     * If you run into the assertion, you should not be using ogl_samplers - it is written
     * specifically with scene rendering in mind. Instead, spawn your own ogl_sampler
     * instance and configure it as you need.
     */
    if (result == NULL)
    {
        LOG_INFO("Requested sampler object has not been cached yet. Creating a new ogl_sampler instance..");

        _ogl_context_samplers_create_renderer_callback_user_arg callback_user_arg;

        callback_user_arg.border_color             = border_color;
        callback_user_arg.mag_filter_ptr           = mag_filter_ptr;
        callback_user_arg.max_lod_ptr              = max_lod_ptr;
        callback_user_arg.min_filter_ptr           = min_filter_ptr;
        callback_user_arg.min_lod_ptr              = min_lod_ptr;
        callback_user_arg.samplers                 = samplers;
        callback_user_arg.texture_compare_func_ptr = texture_compare_func_ptr;
        callback_user_arg.texture_compare_mode_ptr = texture_compare_mode_ptr;
        callback_user_arg.wrap_r_ptr               = wrap_r_ptr;
        callback_user_arg.wrap_s_ptr               = wrap_s_ptr;
        callback_user_arg.wrap_t_ptr               = wrap_t_ptr;

        ogl_context_request_callback_from_context_thread(samplers_ptr->context,
                                                         _ogl_context_samplers_create_renderer_callback,
                                                        &callback_user_arg);

        result = callback_user_arg.result;

        if (result != NULL)
        {
            system_resizable_vector_push(samplers_ptr->samplers,
                                         result);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_context_samplers_release(ogl_context_samplers samplers)
{
    if (samplers != NULL)
    {
        delete (_ogl_context_samplers*) samplers;

        samplers = NULL;
    }
}
