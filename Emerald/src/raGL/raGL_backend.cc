/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_textures.h"
#include "ral/ral_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_framebuffer.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_window.h"


typedef struct _raGL_backend
{
    system_hash64map        framebuffers_map; /* maps ral_framebuffer to raGL_framebuffer instance; owns the mapped raGL_framebuffer instances */
    system_critical_section framebuffers_map_cs;

    ogl_context      gl_context;       /* NOT owned - DO NOT release */
    ral_context      owner_context;
    ral_framebuffer  system_framebuffer;

    uint32_t max_framebuffer_color_attachments;

    _raGL_backend(ral_context in_owner_context)
    {
        framebuffers_map                  = system_hash64map_create       (sizeof(raGL_framebuffer) );
        framebuffers_map_cs               = system_critical_section_create();
        gl_context                        = NULL;
        max_framebuffer_color_attachments = 0;
        owner_context                     = in_owner_context;
        system_framebuffer                = NULL;

        ASSERT_DEBUG_SYNC(framebuffers_map != NULL,
                          "Could not create the framebuffers map");
    }

    ~_raGL_backend()
    {
        /* Delete the system framebuffer and then proceed with release of other dangling framebuffers. */
        if (system_framebuffer != NULL)
        {
            ral_context_delete_framebuffers(owner_context,
                                            1, /* n_framebuffers */
                                           &system_framebuffer);

            system_framebuffer = NULL;
        } /* if (system_framebuffer != NULL) */

        system_critical_section_enter(framebuffers_map_cs);
        {
            if (framebuffers_map != NULL)
            {
                uint32_t n_framebuffers_map_items = 0;

                system_hash64map_get_property(framebuffers_map,
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_framebuffers_map_items);

                for (uint32_t n_item = 0;
                              n_item < n_framebuffers_map_items;
                            ++n_item)
                {
                    raGL_framebuffer current_fb = NULL;

                    if (!system_hash64map_get_element_at(framebuffers_map,
                                                         n_item,
                                                        &current_fb,
                                                         NULL) ) /* result_hash_ptr */
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve raGL_framebuffer instance");

                        continue;
                    }

                    raGL_framebuffer_release(current_fb);
                    current_fb = NULL;
                } /* for (all FB map items) */

                system_hash64map_release(framebuffers_map);
                framebuffers_map = NULL;
            } /* if (framebuffers_map != NULL) */

            system_critical_section_leave(framebuffers_map_cs);
        }

        system_critical_section_release(framebuffers_map_cs);
        framebuffers_map_cs = NULL;
    }
} _raGL_backend;

typedef struct
{

    _raGL_backend*                                          backend_ptr;
    ral_context_callback_framebuffers_created_callback_arg* ral_callback_arg_ptr;

} _raGL_backend_on_framebuffers_created_rendering_callback_arg;


/* Forward declarations */
PRIVATE void _raGL_backend_cache_limits                              (_raGL_backend* backend_ptr);
PRIVATE void _raGL_backend_init_system_fbo                           (_raGL_backend* backend_ptr);
PRIVATE void _raGL_backend_on_framebuffers_created                   (const void*    callback_arg_data,
                                                                      void*          backend);
PRIVATE void _raGL_backend_on_framebuffers_created_rendering_callback(ogl_context    context,
                                                                      void*          callback_arg);
PRIVATE void _raGL_backend_on_framebuffers_deleted                   (const void*    callback_arg_data,
                                                                      void*          backend);
PRIVATE void _raGL_backend_subscribe_for_notifications               (_raGL_backend* backend_ptr,
                                                                      bool           should_subscribe);


/** TODO */
PRIVATE void _raGL_backend_cache_limits(_raGL_backend* backend_ptr)
{
    ogl_context_gl_limits* limits_ptr = NULL;

    ogl_context_get_property(backend_ptr->gl_context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    backend_ptr->max_framebuffer_color_attachments = limits_ptr->max_color_attachments;
}

/** TODO */
PRIVATE void _raGL_backend_init_system_fbo(_raGL_backend* backend_ptr)
{
    ogl_context_textures context_textures = NULL;
    GLuint               system_fbo_id    = 0;

    /* Create the ral_framebuffer instance from an existing FBO the ogl_context has already set up. */
    ogl_context_get_property(backend_ptr->gl_context,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                            &system_fbo_id);
    ogl_context_get_property(backend_ptr->gl_context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &context_textures);

    // TODO .. note: system FB's attachments are RBOs! will need raGL_texture to support both.
}

/** TODO */
PRIVATE void _raGL_backend_on_framebuffers_created(const void* callback_arg_data,
                                                   void*       backend)
{
    _raGL_backend*                                          backend_ptr      = (_raGL_backend*)                                          backend;
    ral_context_callback_framebuffers_created_callback_arg* callback_arg_ptr = (ral_context_callback_framebuffers_created_callback_arg*) callback_arg_data;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_ptr != NULL,
                      "Callback argument is NULL");

    /* Request a rendering call-back to create the framebuffer instances */
    _raGL_backend_on_framebuffers_created_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_ptr;

    ogl_context_request_callback_from_context_thread(backend_ptr->gl_context,
                                                     _raGL_backend_on_framebuffers_created_rendering_callback,
                                                    &rendering_callback_arg);

    /* All done - the rendering callback does all the dirty job needed. */
}

/** TODO */
PRIVATE void _raGL_backend_on_framebuffers_created_rendering_callback(ogl_context context,
                                                                      void*       callback_arg)
{
    _raGL_backend_on_framebuffers_created_rendering_callback_arg* callback_arg_ptr  = (_raGL_backend_on_framebuffers_created_rendering_callback_arg*) callback_arg;
    const ogl_context_gl_entrypoints*                             entrypoints_ptr   = NULL;
    bool                                                          result            = false;
    GLuint*                                                       result_fb_ids_ptr = NULL;

    ASSERT_DEBUG_SYNC(callback_arg_ptr != NULL,
                      "Callback argument is NULL");

    ogl_context_get_property(callback_arg_ptr->backend_ptr->gl_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Generate the IDs */
    result_fb_ids_ptr = new (std::nothrow) GLuint[callback_arg_ptr->ral_callback_arg_ptr->n_framebuffers];

    if (result_fb_ids_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    } /* if (result_fb_ids_ptr == NULL) */

    entrypoints_ptr->pGLGenFramebuffers(callback_arg_ptr->ral_callback_arg_ptr->n_framebuffers,
                                        result_fb_ids_ptr);

    /* Create raGL_framebuffer instances for each ID */
    for (uint32_t n_fb_id = 0;
                  n_fb_id < callback_arg_ptr->ral_callback_arg_ptr->n_framebuffers;
                ++n_fb_id)
    {
        GLuint           current_fb_id = result_fb_ids_ptr[n_fb_id];
        raGL_framebuffer new_fb        = NULL;

        new_fb = raGL_framebuffer_create(context,
                                         current_fb_id,
                                         callback_arg_ptr->ral_callback_arg_ptr->created_framebuffers[n_fb_id]);

        if (new_fb == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "raGL_framebuffer_create() failed.");

            goto end;
        }

        system_critical_section_enter(callback_arg_ptr->backend_ptr->framebuffers_map_cs);
        {
            ASSERT_DEBUG_SYNC(!system_hash64map_contains(callback_arg_ptr->backend_ptr->framebuffers_map,
                                                         (system_hash64) new_fb),
                              "Created FB instance is already recognized?");

            system_hash64map_insert(callback_arg_ptr->backend_ptr->framebuffers_map,
                                    (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->created_framebuffers[n_fb_id],
                                    new_fb,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }
        system_critical_section_leave(callback_arg_ptr->backend_ptr->framebuffers_map_cs);
    } /* for (all generated FB ids) */

    result = true;
end:
    if (result_fb_ids_ptr != NULL)
    {
        if (!result)
        {
            /* Remove those FB instances which have been successfully spawned */
            for (uint32_t n_fb = 0;
                          n_fb < callback_arg_ptr->ral_callback_arg_ptr->n_framebuffers;
                        ++n_fb)
            {
                raGL_framebuffer fb_instance = NULL;

                if (!system_hash64map_get(callback_arg_ptr->backend_ptr->framebuffers_map,
                                          (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->created_framebuffers[n_fb],
                                         &fb_instance) )
                {
                    continue;
                }

                raGL_framebuffer_release(fb_instance);
                fb_instance = NULL;
            } /* for (all requested framebuffers) */
        } /* if (!result) */

        delete [] result_fb_ids_ptr;

        result_fb_ids_ptr = NULL;
    } /* if (result_fb_ids_ptr != NULL) */

}

/** TODO */
PRIVATE void _raGL_backend_on_framebuffers_deleted(const void* callback_arg_data,
                                                   void*       backend)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** TODO */
PRIVATE void _raGL_backend_subscribe_for_notifications(_raGL_backend* backend_ptr,
                                                       bool           should_subscribe)
{
    system_callback_manager context_callback_manager = NULL;

    ral_context_get_property(backend_ptr->owner_context,
                             RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,
                            &context_callback_manager);

    if (should_subscribe)
    {
        /* Framebuffer notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_framebuffers_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_framebuffers_deleted,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        /* Framebuffer notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                                           _raGL_backend_on_framebuffers_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                                           _raGL_backend_on_framebuffers_deleted,
                                                           backend_ptr);
    }
}


/** Please see header for specification */
PUBLIC raGL_backend raGL_backend_create(ral_context owner_context)
{
    _raGL_backend* new_backend_ptr = new (std::nothrow) _raGL_backend(owner_context);

    ASSERT_ALWAYS_SYNC(new_backend_ptr != NULL,
                       "Out of memory");
    if (new_backend_ptr != NULL)
    {
        system_window window = NULL;

        /* Retrieve the GL context handle */
        ral_context_get_property  (owner_context,
                                   RAL_CONTEXT_PROPERTY_WINDOW,
                                  &window);
        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                                  &new_backend_ptr->gl_context);

        ASSERT_DEBUG_SYNC(new_backend_ptr->gl_context != NULL,
                          "Could not retrieve the GL rendering context instance");

        /* Sign up for notifications */
        _raGL_backend_subscribe_for_notifications(new_backend_ptr,
                                                  true); /* should_subscribe */

        /* Configure the system framebuffer */
        _raGL_backend_init_system_fbo(new_backend_ptr);

        /* Cache rendering context limits */
        _raGL_backend_cache_limits(new_backend_ptr);
    }

    return (raGL_backend) new_backend_ptr;
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_framebuffer(void*           backend,
                                         ral_framebuffer framebuffer_ral,
                                         void**          out_framebuffer_backend_ptr)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;
    bool           result      = false;

    /* Sanity checks */
    if (backend == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input backend is NULL");

        goto end;
    }

    /* Try to find the raGL framebuffer instance */
    system_critical_section_enter(backend_ptr->framebuffers_map_cs);

    if (!system_hash64map_get(backend_ptr->framebuffers_map,
                              (system_hash64) framebuffer_ral,
                              out_framebuffer_backend_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Provided RAL framebuffer handle was not recognized");

        goto end;
    }

    /* All done */
    result = true;

end:
    if (backend_ptr != NULL)
    {
        system_critical_section_leave(backend_ptr->framebuffers_map_cs);
    }

    return result;
}

/** Please see header for specification */
PUBLIC void raGL_backend_get_property(void*                backend,
                                      ral_context_property property,
                                      void*                out_result_ptr)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    /* Sanity checks */
    if (backend == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_backend instance is NULL");

        goto end;
    } /* if (backend == NULL) */

    /* Retrieve the requested property value. */
    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
        {
            *(uint32_t*) out_result_ptr = backend_ptr->max_framebuffer_color_attachments;

            break;
        }

        case RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS:
        {
            /* WGL manages the back buffers */
            *(uint32_t*) out_result_ptr = 1;

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_TEXTURE_FORMAT:
        {
            ogl_context_get_property(backend_ptr->gl_context,
                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,
                                     out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS:
        {
            ASSERT_DEBUG_SYNC(false,
                              "system_framebuffer is NULL at the moment, TODO");

            *(ral_framebuffer*) out_result_ptr = backend_ptr->system_framebuffer;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_property value.");
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void raGL_backend_release(void* backend)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    ASSERT_DEBUG_SYNC(backend != NULL,
                      "Input backend is NULL");

    if (backend != NULL)
    {
        /* Sign out from notifications */
        _raGL_backend_subscribe_for_notifications(backend_ptr,
                                                  false); /* should_subscribe */

        delete (_raGL_backend*) backend;
    } /* if (backend != NULL) */
}