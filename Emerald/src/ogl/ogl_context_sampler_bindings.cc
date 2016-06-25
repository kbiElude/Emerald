/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_sampler_bindings.h"
#include "system/system_hash64map.h"

/** TODO */
typedef struct _ogl_context_sampler_bindings_sampler_info
{
    GLuint so_id;

    _ogl_context_sampler_bindings_sampler_info()
    {
        so_id = 0;
    }
} _ogl_context_sampler_bindings_sampler_info;

/** TODO */
typedef struct _ogl_context_sampler_bindings
{
    /* Contains exactly GL_MAX_TEXTURE_IMAGE_UNITS items */
    _ogl_context_sampler_bindings_sampler_info* bindings_context;
    _ogl_context_sampler_bindings_sampler_info* bindings_local;

    /* Used by sync() */
    GLuint* sync_data_samplers;

    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;
    bool        dirty;
    uint32_t    gl_max_texture_image_units_value;
    bool        is_arb_multi_bind_supported;

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;
} _ogl_context_sampler_bindings;


/** TODO */
PRIVATE void _ogl_context_sampler_bindings_sync_multi_bind_process(ogl_context_sampler_bindings bindings)
{
    _ogl_context_sampler_bindings* bindings_ptr      = (_ogl_context_sampler_bindings*) bindings;
    uint32_t                       dirty_start_index = 0xFFFFFFFF;
    uint32_t                       dirty_end_index   = 0;

    /* Determine how many bindings we need to update */
    if (bindings_ptr->dirty)
    {
        for (unsigned int n_texture_unit = 0;
                          n_texture_unit < bindings_ptr->gl_max_texture_image_units_value;
                        ++n_texture_unit)
        {
            if (bindings_ptr->bindings_local[n_texture_unit].so_id != bindings_ptr->bindings_context[n_texture_unit].so_id)
            {
                if (dirty_start_index == 0xFFFFFFFF)
                {
                    dirty_start_index = n_texture_unit;
                }

                dirty_end_index = n_texture_unit;
            }
        }

        /* Prepare the arguments */
        const int n_bindings_to_update = dirty_end_index - dirty_start_index + 1;
        GLuint*   sync_data_units      = bindings_ptr->sync_data_samplers;

        /* Configure the input arrays */
        if (dirty_start_index != 0xFFFFFFFF)
        {
            for (unsigned int n_texture_unit = 0;
                              n_texture_unit < bindings_ptr->gl_max_texture_image_units_value;
                            ++n_texture_unit)
            {
                const _ogl_context_sampler_bindings_sampler_info* local_binding_ptr = bindings_ptr->bindings_local + (dirty_start_index + n_texture_unit);

                sync_data_units[n_texture_unit] = local_binding_ptr->so_id;
            } /* for (all texture units that we need to update) */

            /* Issue the GL call */
            bindings_ptr->entrypoints_private_ptr->pGLBindSamplers(dirty_start_index,
                                                                   n_bindings_to_update,
                                                                   sync_data_units);

            /* Mark all bindings as updated */
            for (int n_texture_unit = 0;
                     n_texture_unit < n_bindings_to_update;
                   ++n_texture_unit)
            {
                _ogl_context_sampler_bindings_sampler_info* context_binding_ptr = bindings_ptr->bindings_context + (dirty_start_index + n_texture_unit);
                _ogl_context_sampler_bindings_sampler_info* local_binding_ptr   = bindings_ptr->bindings_local   + (dirty_start_index + n_texture_unit);

                context_binding_ptr->so_id = local_binding_ptr->so_id;
            }
        }
    }

    bindings_ptr->dirty = false;
}

/** TODO */
PRIVATE void _ogl_context_sampler_bindings_sync_non_multi_bind_process(ogl_context_sampler_bindings bindings)
{
    _ogl_context_sampler_bindings* bindings_ptr = (_ogl_context_sampler_bindings*) bindings;

    /* Determine how many bindings we need to update */
    if (bindings_ptr->dirty)
    {
        for (uint32_t n_texture_unit = 0;
                      n_texture_unit < bindings_ptr->gl_max_texture_image_units_value;
                    ++n_texture_unit)
        {
            _ogl_context_sampler_bindings_sampler_info* context_binding_ptr = bindings_ptr->bindings_context + n_texture_unit;
            _ogl_context_sampler_bindings_sampler_info* local_binding_ptr   = bindings_ptr->bindings_local   + n_texture_unit;

            if (local_binding_ptr->so_id != context_binding_ptr->so_id)
            {
                bindings_ptr->entrypoints_private_ptr->pGLBindSampler(n_texture_unit,
                                                                      local_binding_ptr->so_id);

                /* Update internal representation */
                context_binding_ptr->so_id = local_binding_ptr->so_id;
            }
        }

        bindings_ptr->dirty = false;
    }
}

/** Please see header for spec */
PUBLIC ogl_context_sampler_bindings ogl_context_sampler_bindings_create(ogl_context context)
{
    _ogl_context_sampler_bindings* new_bindings = new (std::nothrow) _ogl_context_sampler_bindings;

    ASSERT_ALWAYS_SYNC(new_bindings != nullptr,
                       "Out of memory");

    if (new_bindings != nullptr)
    {
        new_bindings->context = context;
    }

    return (ogl_context_sampler_bindings) new_bindings;
}

/** Please see header for spec */
PUBLIC GLuint ogl_context_sampler_bindings_get_binding(const ogl_context_sampler_bindings bindings,
                                                       GLuint                             texture_unit)
{
    const _ogl_context_sampler_bindings* bindings_ptr = (const _ogl_context_sampler_bindings*) bindings;
    GLuint                               result       = 0;

    ASSERT_DEBUG_SYNC(texture_unit < bindings_ptr->gl_max_texture_image_units_value,
                      "Invalid texture unit requested");

    if (texture_unit < bindings_ptr->gl_max_texture_image_units_value)
    {
        result = bindings_ptr->bindings_local[texture_unit].so_id;
    }

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_sampler_bindings_init(ogl_context_sampler_bindings              bindings,
                                              const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_sampler_bindings* bindings_ptr = (_ogl_context_sampler_bindings*) bindings;
    const ogl_context_gl_limits*   limits_ptr   = nullptr;

    ogl_context_get_property(bindings_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* Cache info in private descriptor */
    bindings_ptr->entrypoints_private_ptr          = entrypoints_private_ptr;
    bindings_ptr->gl_max_texture_image_units_value = limits_ptr->max_texture_image_units;

    /* Determine if GL_ARB_multi_bind is supported */
    bindings_ptr->is_arb_multi_bind_supported = ogl_context_is_extension_supported(bindings_ptr->context,
                                                                                   system_hashed_ansi_string_create("GL_ARB_multi_bind") );

    /* Initialize indiced binding descriptors */
    ASSERT_ALWAYS_SYNC(limits_ptr->max_texture_image_units != 0,
                       "GL_MAX_TEXTURE_IMAGE_UNITS is 0, crash imminent.");

    bindings_ptr->bindings_context = new (std::nothrow) _ogl_context_sampler_bindings_sampler_info[limits_ptr->max_texture_image_units];
    bindings_ptr->bindings_local   = new (std::nothrow) _ogl_context_sampler_bindings_sampler_info[limits_ptr->max_texture_image_units];

    ASSERT_ALWAYS_SYNC(bindings_ptr->bindings_context != nullptr &&
                       bindings_ptr->bindings_local   != nullptr,
                       "Out of memory");

    /* Set up binding properties */
    for (unsigned int n_binding = 0;
                      n_binding < bindings_ptr->gl_max_texture_image_units_value;
                    ++n_binding)
    {
        bindings_ptr->bindings_context[n_binding].so_id = 0;
        bindings_ptr->bindings_local  [n_binding].so_id = 0;
    }

    /* Allocate arrays used by sync() */
    bindings_ptr->sync_data_samplers = new (std::nothrow) GLuint[limits_ptr->max_texture_image_units];

    ASSERT_ALWAYS_SYNC(bindings_ptr->sync_data_samplers != nullptr,
                       "Out of memory");
}

/** Please see header for spec */
PUBLIC void ogl_context_sampler_bindings_release(ogl_context_sampler_bindings bindings)
{
    _ogl_context_sampler_bindings* bindings_ptr = (_ogl_context_sampler_bindings*) bindings;

    /* Release binding storage */
    if (bindings_ptr->bindings_context != nullptr)
    {
        delete [] bindings_ptr->bindings_context;

        bindings_ptr->bindings_context = nullptr;
    }

    if (bindings_ptr->bindings_local != nullptr)
    {
        delete [] bindings_ptr->bindings_local;

        bindings_ptr->bindings_local = nullptr;
    }

    /* Release helper buffers */
    if (bindings_ptr->sync_data_samplers != nullptr)
    {
        delete [] bindings_ptr->sync_data_samplers;

        bindings_ptr->sync_data_samplers = nullptr;
    }

    /* Done */
    delete bindings_ptr;

    bindings_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC void ogl_context_sampler_bindings_set_binding(ogl_context_sampler_bindings bindings,
                                                     GLuint                       texture_unit,
                                                     GLuint                       sampler)
{
    _ogl_context_sampler_bindings* bindings_ptr = (_ogl_context_sampler_bindings*) bindings;

    ASSERT_DEBUG_SYNC(texture_unit < bindings_ptr->gl_max_texture_image_units_value,
                      "Invalid texture unit requested");

    if (texture_unit < bindings_ptr->gl_max_texture_image_units_value)
    {
        if (bindings_ptr->bindings_local[texture_unit].so_id != sampler)
        {
            bindings_ptr->bindings_local[texture_unit].so_id = sampler;
            bindings_ptr->dirty                              = true;
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_sampler_bindings_sync(ogl_context_sampler_bindings bindings)
{
    /* NOTE: bindings is NULL during rendering context initialization */
    if (bindings != nullptr)
    {
        _ogl_context_sampler_bindings* bindings_ptr = (_ogl_context_sampler_bindings*) bindings;

        if (bindings_ptr->is_arb_multi_bind_supported)
        {
            _ogl_context_sampler_bindings_sync_multi_bind_process(bindings);
        }
        else
        {
            _ogl_context_sampler_bindings_sync_non_multi_bind_process(bindings);
        }
    }
}