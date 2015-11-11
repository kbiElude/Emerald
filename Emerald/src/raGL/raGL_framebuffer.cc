/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_framebuffer.h"
#include "ral/ral_framebuffer.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"


typedef struct _raGL_framebuffer_attachment
{
    bool dirty;

    /* Are multiple layers bound to the attachment? */
    bool bound_layered_texture_gl;
    bool bound_layered_texture_local;

    /* What's the texture bound to the attachment? */
    ogl_texture bound_texture_gl;
    ogl_texture bound_texture_local;

    /* Is the draw buffer enabled for this attachment? */
    bool is_enabled_gl;
    bool is_enabled_local;

    /* Which layer is bound to the attachment? */
    GLint n_texture_layer_gl;
    GLint n_texture_layer_local;

    /* What mipmap level is bound to the attachment? */
    GLint n_texture_mipmap_level_gl;
    GLint n_texture_mipmap_level_local;

    /* What texture type is bound to the attachment ? */
    ral_framebuffer_attachment_texture_type texture_type_gl;
    ral_framebuffer_attachment_texture_type texture_type_local;

    _raGL_framebuffer_attachment()
    {
        bound_layered_texture_gl     = false;
        bound_layered_texture_local  = false;
        bound_texture_gl             = NULL;
        bound_texture_local          = NULL;
        dirty                        = false;
        is_enabled_gl                = false;
        is_enabled_local             = false;
        n_texture_layer_gl           = 0;
        n_texture_layer_local        = 0;
        n_texture_mipmap_level_gl    = 0;
        n_texture_mipmap_level_local = 0;
    }
} _raGL_framebuffer_attachment;

typedef struct _raGL_framebuffer
{
    _raGL_framebuffer_attachment* color_attachments;
    ogl_context                   context; /* NOT owned */
    _raGL_framebuffer_attachment  depth_stencil_attachment;
    GLenum*                       draw_buffers;
    ral_framebuffer               fb;
    GLint                         id;      /* NOT owned */
    uint32_t                      max_color_attachments;
    uint32_t                      max_draw_buffers;

    ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr;
    ogl_context_gl_entrypoints*                         entrypoints_ptr;
    ogl_context_gl_limits*                              limits_ptr;


    _raGL_framebuffer(ogl_context     in_context,
                      GLint           in_fb_id,
                      ral_framebuffer in_fb)
    {
        color_attachments = NULL;
        context           = in_context;
        draw_buffers      = NULL;
        fb                = in_fb;
        id                = in_fb_id;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &entrypoints_dsa_ptr);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        max_color_attachments = limits_ptr->max_color_attachments;
        max_draw_buffers      = limits_ptr->max_draw_buffers;

        /* NOTE: Only GL is supported at the moment. */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "TODO");
    }

    ~_raGL_framebuffer()
    {
        if (color_attachments != NULL)
        {
            delete [] color_attachments;

            color_attachments = NULL;
        } /* if (color_attachments != NULL) */

        if (draw_buffers != NULL)
        {
            delete [] draw_buffers;

            draw_buffers = NULL;
        }

        /* Do not release the framebuffer. Object life-time is handled by raGL_backend. */
    }
} _raGL_framebuffer;


/* Forward declarations */
PRIVATE void _raGL_framebuffer_on_color_attachment_configuration_changed        (const void*                     callback_data,
                                                                                 void*                           user_arg);
PRIVATE void _raGL_framebuffer_on_depth_stencil_attachment_configuration_changed(const void*                     callback_data,
                                                                                 void*                           user_arg);
PRIVATE void _raGL_framebuffer_on_sync_required                                 (const void*                     callback_data,
                                                                                 void*                           user_arg);
PRIVATE void _raGL_framebuffer_subscribe_for_notifications                      (_raGL_framebuffer*              fb_ptr,
                                                                                 bool                            should_subscribe);
PRIVATE void _raGL_framebuffer_update_attachment_configuration                  (_raGL_framebuffer*              fb_ptr,
                                                                                 ral_framebuffer_attachment_type attachment_type,
                                                                                 uint32_t                        attachment_index);


/** TODO */
PRIVATE bool _raGL_framebuffer_get_attachment(_raGL_framebuffer*              fb_ptr,
                                              ral_framebuffer_attachment_type attachment_type,
                                              uint32_t                        attachment_index,
                                              _raGL_framebuffer_attachment**  out_attachment_ptr)
{
    bool result = false;

    switch (attachment_type)
    {
        case RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR:
        {
            if (attachment_index >= fb_ptr->max_color_attachments)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid color attachment index requested.");

                goto end;
            } /* if (attachment_index >= fb_ptr->max_color_attachments) */

            *out_attachment_ptr = fb_ptr->color_attachments + attachment_index;

            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL:
        {
            if (attachment_index > 0)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid depth/stencil attachment index requested.");

                goto end;
            }

            *out_attachment_ptr = &fb_ptr->depth_stencil_attachment;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_framebuffer_attachment_type value.");

            goto end;
        }
    } /* switch (attachment_type) */

    result = true;

end:
    return result;
}

/** TODO */
PRIVATE void _raGL_framebuffer_on_color_attachment_configuration_changed(const void* callback_data,
                                                                         void*       user_arg)
{
    int                attachment_index = (int) callback_data;
    _raGL_framebuffer* fb_ptr           = (_raGL_framebuffer*) user_arg;

    ASSERT_DEBUG_SYNC(fb_ptr != NULL,
                      "Input _raGL_framebuffer instance is NULL");

    /* Update local attachment configuration. DO NOT issue GL calls. We'd need to request a rendering
     * thread call-back which is costly, and it's also better to wait till the framebuffer is going
     * to be actually used before updating GL state. */
    _raGL_framebuffer_update_attachment_configuration(fb_ptr,
                                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                                      attachment_index);
}

/** TODO */
PRIVATE void _raGL_framebuffer_on_depth_stencil_attachment_configuration_changed(const void* callback_data,
                                                                                 void*       user_arg)
{
    int                attachment_index = (int) callback_data;
    _raGL_framebuffer* fb_ptr           = (_raGL_framebuffer*) user_arg;

    ASSERT_DEBUG_SYNC(fb_ptr != NULL,
                      "Input _raGL_framebuffer instance is NULL");

    /* See the comment in _on_color_attachment_configuration_changed() for rationale. */
    _raGL_framebuffer_update_attachment_configuration(fb_ptr,
                                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL,
                                                      0); /* index */
}

/** TODO */
PRIVATE void _raGL_framebuffer_on_sync_required(const void* callback_data,
                                                void*       user_arg)
{
    _raGL_framebuffer* framebuffer_raGL_ptr = (_raGL_framebuffer*) user_arg;
    ral_framebuffer    framebuffer_ral      = (ral_framebuffer) callback_data;

    LOG_ERROR("Performance warning: raGL_framebuffer sync request.");


    /* Sanity checks */
    ASSERT_DEBUG_SYNC(framebuffer_raGL_ptr != NULL,
                      "Input raGL_framebuffer instance is NULL");
    ASSERT_DEBUG_SYNC(framebuffer_ral != NULL,
                      "Input RAL framebuffer is NULL");

    /* For performance reasons, sync requests should always come from a rendering thread. */
    ASSERT_DEBUG_SYNC(ogl_context_get_current_context() == framebuffer_raGL_ptr->context,
                      "Rendering context mismatch");

    /* Iterate over all attachments and sync the dirty ones. */
    for (uint32_t n_iteration = 0;
                  n_iteration < 2; /* color, & depth/stencil attachment. */
                ++n_iteration)
    {
        bool                          any_attachment_synced  = false;
        _raGL_framebuffer_attachment* attachments            = (n_iteration == 0) ?  framebuffer_raGL_ptr->color_attachments
                                                                                  : &framebuffer_raGL_ptr->depth_stencil_attachment;
        const uint32_t                n_attachments          = (n_iteration == 0) ?  framebuffer_raGL_ptr->max_color_attachments
                                                                                  :  1;
        uint32_t                      n_max_draw_buffer_used = 0;

        for (uint32_t n_attachment = 0;
                      n_attachment < n_attachments;
                    ++n_attachment)
        {
            GLenum                        attachment_gl          = (n_iteration == 0) ? (GL_COLOR_ATTACHMENT0 + n_attachment)
                                                                                      : GL_DEPTH_STENCIL_ATTACHMENT;
            _raGL_framebuffer_attachment* current_attachment_ptr = attachments + n_attachment;

            if (!current_attachment_ptr->dirty)
            {
                framebuffer_raGL_ptr->draw_buffers[n_attachment] = GL_NONE;

                continue;
            } /* if (current_attachment_ptr->dirty) */

            any_attachment_synced = true;

            /* Update the attachment.. */
            switch (current_attachment_ptr->texture_type_local)
            {
                case  RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_2D:
                {
                    framebuffer_raGL_ptr->entrypoints_dsa_ptr->pGLNamedFramebufferTexture2DEXT(framebuffer_raGL_ptr->id,
                                                                                               attachment_gl,
                                                                                               GL_TEXTURE_2D,
                                                                                               current_attachment_ptr->bound_texture_local,
                                                                                               current_attachment_ptr->n_texture_mipmap_level_local);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "TODO");
                }
            } /* switch (current_attachment_ptr->texture_type_local) */

            /* ..and cache as a draw buffer if needed */
            if (current_attachment_ptr->is_enabled_local)
            {
                framebuffer_raGL_ptr->draw_buffers[n_attachment] = GL_DRAW_BUFFER0 + n_attachment;
                current_attachment_ptr->is_enabled_gl            = true;

                n_max_draw_buffer_used = n_attachment;
            }
            else
            {
                framebuffer_raGL_ptr->draw_buffers[n_attachment] = GL_NONE;
                current_attachment_ptr->is_enabled_gl            = false;
            }

            current_attachment_ptr->dirty = false;
        } /* for (all attachments) */

        /* Also update the draw buffer array */
        if (n_iteration == 0)
        {
            framebuffer_raGL_ptr->entrypoints_dsa_ptr->pGLFramebufferDrawBuffersEXT(framebuffer_raGL_ptr->id,
                                                                                    n_max_draw_buffer_used + 1,
                                                                                    framebuffer_raGL_ptr->draw_buffers);
        }
    } /* for (both iterations) */
}

/** TODO */
PRIVATE void _raGL_framebuffer_subscribe_for_notifications(_raGL_framebuffer* fb_ptr,
                                                           bool               should_subscribe)
{
    system_callback_manager fb_callback_manager = NULL;

    ral_framebuffer_get_property(fb_ptr->fb,
                                 RAL_FRAMEBUFFER_PROPERTY_CALLBACK_MANAGER,
                                &fb_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(fb_callback_manager,
                                                        RAL_FRAMEBUFFER_CALLBACK_ID_COLOR_ATTACHMENT_CONFIGURATION_CHANGED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_framebuffer_on_color_attachment_configuration_changed,
                                                        fb_ptr);
        system_callback_manager_subscribe_for_callbacks(fb_callback_manager,
                                                        RAL_FRAMEBUFFER_CALLBACK_ID_DEPTH_STENCIL_ATTACHMENT_CONFIGURATION_CHANGED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_framebuffer_on_depth_stencil_attachment_configuration_changed,
                                                        fb_ptr);
        system_callback_manager_subscribe_for_callbacks(fb_callback_manager,
                                                        RAL_FRAMEBUFFER_CALLBACK_ID_SYNC_REQUIRED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_framebuffer_on_sync_required,
                                                        fb_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(fb_callback_manager,
                                                           RAL_FRAMEBUFFER_CALLBACK_ID_COLOR_ATTACHMENT_CONFIGURATION_CHANGED,
                                                           _raGL_framebuffer_on_color_attachment_configuration_changed,
                                                           fb_ptr);
        system_callback_manager_unsubscribe_from_callbacks(fb_callback_manager,
                                                           RAL_FRAMEBUFFER_CALLBACK_ID_DEPTH_STENCIL_ATTACHMENT_CONFIGURATION_CHANGED,
                                                           _raGL_framebuffer_on_depth_stencil_attachment_configuration_changed,
                                                           fb_ptr);
        system_callback_manager_unsubscribe_from_callbacks(fb_callback_manager,
                                                           RAL_FRAMEBUFFER_CALLBACK_ID_SYNC_REQUIRED,
                                                           _raGL_framebuffer_on_sync_required,
                                                           fb_ptr);
    }
}

/** TODO */
PRIVATE void _raGL_framebuffer_update_attachment_configuration(_raGL_framebuffer*              fb_ptr,
                                                               ral_framebuffer_attachment_type attachment_type,
                                                               uint32_t                        attachment_index)
{
    ogl_texture                             attachment_bound_texture = NULL;
    bool                                    attachment_enabled       = false;
    uint32_t                                attachment_n_layer       = -1;
    uint32_t                                attachment_n_mipmap      = -1;
    _raGL_framebuffer_attachment*           attachment_ptr           = NULL;
    ral_framebuffer_attachment_texture_type attachment_texture_type  = RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_UNKNOWN;
    bool                                    dirty                    = false;
    bool                                    is_n_layer_important     = false;
    bool                                    is_n_mipmap_important    = false;

    if (!_raGL_framebuffer_get_attachment(fb_ptr,
                                          attachment_type,
                                          attachment_index,
                                         &attachment_ptr) )
    {
        goto end;
    }

    /* Retrieve RAL attachment properties */
    ral_framebuffer_get_attachment_property(fb_ptr->fb,
                                            attachment_type,
                                            attachment_index,
                                            RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_BOUND_TEXTURE,
                                          &attachment_bound_texture);
    ral_framebuffer_get_attachment_property(fb_ptr->fb,
                                            attachment_type,
                                            attachment_index,
                                            RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_IS_ENABLED,
                                          &attachment_enabled);
    ral_framebuffer_get_attachment_property(fb_ptr->fb,
                                            attachment_type,
                                            attachment_index,
                                            RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_N_LAYER,
                                           &attachment_n_layer);
    ral_framebuffer_get_attachment_property(fb_ptr->fb,
                                            attachment_type,
                                            attachment_index,
                                            RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_N_MIPMAP,
                                           &attachment_n_mipmap);
    ral_framebuffer_get_attachment_property(fb_ptr->fb,
                                            attachment_type,
                                            attachment_index,
                                            RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_TEXTURE_TYPE,
                                           &attachment_texture_type);

    /* Update the attachment descriptor. Raise the 'dirty' flag if any of the local property values
     * do not match the cached GL state */
    switch (attachment_texture_type)
    {
        case RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_2D:
        {
            is_n_layer_important  = false;
            is_n_mipmap_important = true;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO");
        }
    } /* switch (attachment_texture_type) */

    if (attachment_ptr->bound_texture_gl != attachment_bound_texture)
    {
        attachment_ptr->bound_texture_local = attachment_bound_texture;
        dirty                               = true;
    }

    if (attachment_ptr->is_enabled_gl != attachment_enabled)
    {
        attachment_ptr->is_enabled_local = attachment_enabled;
        dirty                            = true;
    }

    if (is_n_layer_important                                             &&
        attachment_ptr->n_texture_layer_gl != attachment_n_layer)
    {
        attachment_ptr->n_texture_layer_local = attachment_n_layer;
        dirty                                 = true;
    }

    if (is_n_mipmap_important                                            &&
        attachment_ptr->n_texture_mipmap_level_gl != attachment_n_mipmap)
    {
        attachment_ptr->n_texture_mipmap_level_local = attachment_n_mipmap;
        dirty                                        = true;
    }

    if (attachment_ptr->texture_type_gl != attachment_texture_type)
    {
        attachment_ptr->texture_type_local = attachment_texture_type;
        dirty                              = true;
    }

    attachment_ptr->dirty = dirty;

end:
    ;
}


/** Please see header for specification */
PUBLIC raGL_framebuffer raGL_framebuffer_create(ogl_context     context,
                                                GLint           fb_id,
                                                ral_framebuffer fb)
{
    _raGL_framebuffer* new_fb_ptr = new (std::nothrow) _raGL_framebuffer(context,
                                                                         fb_id,
                                                                         fb);

    ASSERT_ALWAYS_SYNC(new_fb_ptr != NULL,
                       "Out of memory");

    if (new_fb_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(new_fb_ptr->max_color_attachments > 0 &&
                          new_fb_ptr->max_draw_buffers      > 0,
                          "Invalid GL_MAX_COLOR_ATTACHMENTS and/or GL_MAX_DRAW_BUFFERS constants values reported by the driver");

        /* Allocate color attachment descriptor array */
        new_fb_ptr->color_attachments = new (std::nothrow) _raGL_framebuffer_attachment[new_fb_ptr->max_color_attachments];

        ASSERT_ALWAYS_SYNC(new_fb_ptr->color_attachments != NULL,
                           "Out of memory");

        /* Allocate draw buffer descriptor array */
        new_fb_ptr->draw_buffers = new GLenum[new_fb_ptr->max_draw_buffers];
        ASSERT_ALWAYS_SYNC(new_fb_ptr->draw_buffers != NULL,
                           "Out of memory");

        /* In GL, first draw buffer is always enabled. */
        new_fb_ptr->color_attachments[0].is_enabled_gl    = true;
        new_fb_ptr->color_attachments[0].is_enabled_local = true;

        /* Sign up for notifications */
        _raGL_framebuffer_subscribe_for_notifications(new_fb_ptr,
                                                      true); /* should_subscribe */
    } /* if (new_fb_ptr != NULL) */

    return (raGL_framebuffer) new_fb_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_framebuffer_release(raGL_framebuffer fb)
{
    ASSERT_DEBUG_SYNC(fb != NULL,
                      "Input framebuffer is NULL");

    /* Sign out of notifications */
    _raGL_framebuffer_subscribe_for_notifications( (_raGL_framebuffer*) fb,
                                                  false); /* should_subscribe */

    /* Safe to release the instance at this point */
    delete (_raGL_framebuffer*) fb;
}
