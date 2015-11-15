/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * TODO: Use some sort of a hash-map to improve this implementation ..
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_samplers.h"
#include "ral/ral_sampler.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>

/* Private type definitions */
typedef struct
{
    raGL_sampler           result_sampler;
    ral_sampler            sampler_ral;
    struct _raGL_samplers* samplers_ptr;
} _raGL_samplers_create_new_raGL_sampler_callback_arg;


/** TODO */
typedef struct _raGL_samplers
{
    ogl_context             context;
    system_resizable_vector samplers; /* <- ugh
                                       * Holds raGL_sampler instances */

    _raGL_samplers()
    {
        context  = NULL;
        samplers = system_resizable_vector_create(4); /* capacity */
    }

    ~_raGL_samplers()
    {
        LOG_INFO("Sampler manager deallocating..");

        if (samplers != NULL)
        {
            raGL_sampler sampler = NULL;

            while (system_resizable_vector_pop(samplers,
                                              &sampler) )
            {
                raGL_sampler_release(sampler);

                sampler = NULL;
            }

            system_resizable_vector_release(samplers);

            samplers = NULL;
        }
    }
} _raGL_samplers;


/** TODO */
PRIVATE void _raGL_samplers_create_new_raGL_sampler_rendering_callback(ogl_context context,
                                                                       void*       user_arg)
{
    _raGL_samplers_create_new_raGL_sampler_callback_arg* arg_ptr         = (_raGL_samplers_create_new_raGL_sampler_callback_arg*) user_arg;
    const ogl_context_gl_entrypoints*                    entrypoints_ptr = NULL;
    GLuint                                               new_sampler_id  = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLGenSamplers(1,
                                   &new_sampler_id);

    arg_ptr->result_sampler = raGL_sampler_create(context,
                                                  new_sampler_id,
                                                  arg_ptr->sampler_ral);
    ASSERT_DEBUG_SYNC(arg_ptr->result_sampler != NULL,
                      "raGL_sampler_create() returned NULL");
}


/** Please see header for specification */
PUBLIC raGL_samplers raGL_samplers_create(ogl_context context)
{
    _raGL_samplers* samplers_ptr = new (std::nothrow) _raGL_samplers;

    ASSERT_ALWAYS_SYNC(samplers_ptr != NULL,
                       "Out of memory");

    if (samplers_ptr != NULL)
    {
        samplers_ptr->context  = context;
        samplers_ptr->samplers = system_resizable_vector_create(4 /* capacity */);
    }

    return (raGL_samplers) samplers_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API raGL_sampler raGL_samplers_get_sampler(raGL_samplers samplers,
                                                          ral_sampler   sampler)

{
    uint32_t        n_samplers   = 0;
    raGL_sampler    result       = NULL;
    _raGL_samplers* samplers_ptr = (_raGL_samplers*) samplers;

    LOG_INFO("Performance warning: slow code-path call: raGL_samplers_get_sampler()");

    system_resizable_vector_get_property(samplers_ptr->samplers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_samplers);

    for (uint32_t n_sampler = 0;
                  n_sampler < n_samplers;
                ++n_sampler)
    {
        ral_sampler  current_sampler    = NULL;
        raGL_sampler current_sampler_gl = NULL;

        if (!system_resizable_vector_get_element_at(samplers_ptr->samplers,
                                                    n_sampler,
                                                   &current_sampler_gl) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve sampler descriptor at index [%d]",
                              n_sampler);

            continue;
        }

        raGL_sampler_get_property(current_sampler_gl,
                                  RAGL_SAMPLER_PROPERTY_SAMPLER,
                                 &current_sampler);

        if (ral_sampler_is_equal(current_sampler,
                                 sampler) )
        {
            /* Found a match. */
            result = current_sampler_gl;

            goto end;
        }
    } /* for (all stored samplers) */

    /* If the sampler was not found, we need to create it. */
    if (result == NULL)
    {
        LOG_INFO("Requested sampler object has not been cached yet. Creating a new raGL_sampler instance..");

        _raGL_samplers_create_new_raGL_sampler_callback_arg arg;

        arg.result_sampler = NULL;
        arg.sampler_ral    = sampler;
        arg.samplers_ptr   = samplers_ptr;

        ogl_context_request_callback_from_context_thread(samplers_ptr->context,
                                                         _raGL_samplers_create_new_raGL_sampler_rendering_callback,
                                                        &arg);

        ASSERT_DEBUG_SYNC(arg.result_sampler != NULL,
                          "raGL_sampler instance could not have been created.");

        if (arg.result_sampler != NULL)
        {
            system_resizable_vector_push(samplers_ptr->samplers,
                                         arg.result_sampler);

            result = arg.result_sampler;
        } /* if (arg.result_sampler != NULL) */
    } /* if (result == NULL) */

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API raGL_sampler raGL_samplers_get_sampler_from_ral_sampler_create_info(raGL_samplers                  samplers,
                                                                                       const ral_sampler_create_info* sampler_create_info_ptr)
{
    raGL_sampler result       = NULL;
    ral_sampler  temp_sampler = ral_sampler_create(system_hashed_ansi_string_create("Temporary sampler"),
                                                   sampler_create_info_ptr);

    result = raGL_samplers_get_sampler(samplers,
                                       temp_sampler);

    /* temp_sampler reference counter will be released by raGL_sampler, so we need to leave the ref counter at 1. */

    return result;
}

/** Please see header for specification */
PUBLIC void raGL_samplers_release(raGL_samplers samplers)
{
    if (samplers == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_samplers instance is NULL");

        goto end;
    }

    delete (_raGL_samplers*) samplers;
    samplers = NULL;

end:
    ;
}
