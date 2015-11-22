/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


typedef struct _ral_framebuffer_attachment
{
    bool                                    is_enabled;
    uint32_t                                n_layer;
    uint32_t                                n_mipmap;
    ral_texture                             texture;
    ral_framebuffer_attachment_texture_type texture_type;

    _ral_framebuffer_attachment()
    {
        is_enabled   = false;
        n_mipmap     = 0;
        n_layer      = 0;
        texture      = NULL;
        texture_type = RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_UNKNOWN;
    }
} _ral_framebuffer_attachment;

typedef struct _ral_framebuffer
{
    system_callback_manager      callback_manager;
    _ral_framebuffer_attachment* color_attachments;         /* holds max_color_attachments items */
    _ral_framebuffer_attachment  depth_stencil_attachment;
    uint32_t                     max_color_attachments;
    system_hashed_ansi_string    name;

    REFCOUNT_INSERT_VARIABLES;

    _ral_framebuffer(system_hashed_ansi_string in_name)
    {
        callback_manager      = system_callback_manager_create( (_callback_id) RAL_FRAMEBUFFER_CALLBACK_ID_COUNT);
        color_attachments     = NULL;
        max_color_attachments = 0;
        name                  = in_name;
    }

    ~_ral_framebuffer()
    {
        /* Carry on with the tear-down */
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */

        if (color_attachments != NULL)
        {
            delete [] color_attachments;

            color_attachments = NULL;
        }
    }
} _ral_framebuffer;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_framebuffer,
                               ral_framebuffer,
                              _ral_framebuffer);


/** TODO */
PRIVATE bool _ral_framebuffer_get_attachment(ral_framebuffer                 framebuffer,
                                             ral_framebuffer_attachment_type attachment_type,
                                             uint32_t                        index,
                                             _ral_framebuffer_attachment**   out_attachment_ptr)
{
    _ral_framebuffer* framebuffer_ptr = (_ral_framebuffer*) framebuffer;
    bool              result          = false;

    if (attachment_type >= RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COUNT)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized attachment type");

        goto end;
    }

    switch (attachment_type)
    {
        case RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR:
        {
            if (index >= framebuffer_ptr->max_color_attachments)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Specified attachment index [%d] is >= platform-specific max color attachment constant",
                                  index,
                                  framebuffer_ptr->max_color_attachments);

                goto end;
            }

            *out_attachment_ptr = framebuffer_ptr->color_attachments + index;
            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL:
        {
            if (index != 0)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Index != 0 for a depth-stencil attachment");

                goto end;
            }

            *out_attachment_ptr = &framebuffer_ptr->depth_stencil_attachment;
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized framebuffer attachment type specified");

            goto end;
        }
    } /* switch (attachment_type) */

    result = true;

end:
    return result;
}

/** TODO */
PRIVATE void _ral_framebuffer_release(void* fb)
{
    _ral_framebuffer* fb_ptr = (_ral_framebuffer*) fb;

    ASSERT_DEBUG_SYNC(fb != NULL,
                      "Input ral_framebuffer instance is NULL");

    /* All assets will be released in the destructor */
}


/** TODO */
PUBLIC ral_framebuffer ral_framebuffer_create(ral_context               context,
                                              system_hashed_ansi_string name)
{
    _ral_framebuffer* new_fb_ptr = new (std::nothrow) _ral_framebuffer(name);

    ASSERT_ALWAYS_SYNC(new_fb_ptr != NULL,
                       "Out of memory");
    if (new_fb_ptr != NULL)
    {
        /* Create as many color attachment descriptor as the back-end permits */
        uint32_t max_color_attachments = 0;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS,
                                &max_color_attachments);

        if (max_color_attachments == 0)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Zero maximum color attachments permitted by the back-end");

            delete new_fb_ptr;
            new_fb_ptr = NULL;

            goto end;
        }

        new_fb_ptr->max_color_attachments = max_color_attachments;
        new_fb_ptr->color_attachments     = new _ral_framebuffer_attachment[new_fb_ptr->max_color_attachments];

        if (new_fb_ptr->color_attachments == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            delete new_fb_ptr;
            new_fb_ptr = NULL;

            goto end;
        }

        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_fb_ptr,
                                                       _ral_framebuffer_release,
                                                       OBJECT_TYPE_RAL_FRAMEBUFFER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Framebuffers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_fb_ptr != NULL) */

end:
    return (ral_framebuffer) new_fb_ptr;
}

/** Please see header for specification */
PUBLIC void ral_framebuffer_get_attachment_property(ral_framebuffer                     framebuffer,
                                                    ral_framebuffer_attachment_type     attachment_type,
                                                    uint32_t                            index,
                                                    ral_framebuffer_attachment_property property,
                                                    void*                               out_result_ptr)
{
    _ral_framebuffer_attachment* attachment_ptr  = NULL;
    _ral_framebuffer*            framebuffer_ptr = (_ral_framebuffer*) framebuffer;

    /* Sanity checks */
    if (framebuffer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input framebuffer instance is NULL");

        goto end;
    }

    if (attachment_type >= RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COUNT)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid attachment type requested.");

        goto end;
    }

    if (attachment_type == RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR && 
        index           >= framebuffer_ptr->max_color_attachments)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid color attachment index requested.");

        goto end;
    }
    else
    if (attachment_type == RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL &&
        index           >= 1)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid depth/stencil attachment index requested.");

        goto end;
    }

    /* Retrieve the requested attachment descriptor.. */
    switch (attachment_type)
    {
        case RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR:
        {
            attachment_ptr = framebuffer_ptr->color_attachments + index;

            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL:
        {
            attachment_ptr = &framebuffer_ptr->depth_stencil_attachment;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized attachment type requested.");

            goto end;
        }
    } /* switch (attachment_type) */

    /* Return the requested value */
    switch (property)
    {
        case RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_BOUND_TEXTURE_RAL:
        {
            *(ral_texture*) out_result_ptr = attachment_ptr->texture;

            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_IS_ENABLED:
        {
            *(bool*) out_result_ptr = attachment_ptr->is_enabled;

            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_N_MIPMAP:
        {
            *(uint32_t*) out_result_ptr = attachment_ptr->n_mipmap;

            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_N_LAYER:
        {
            *(uint32_t*) out_result_ptr = attachment_ptr->n_layer;

            break;
        }

        case RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_TEXTURE_TYPE:
        {
            *(ral_framebuffer_attachment_texture_type*) out_result_ptr = attachment_ptr->texture_type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_framebuffer_attachment_property value requested.");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_framebuffer_get_property(ral_framebuffer          framebuffer,
                                         ral_framebuffer_property property,
                                         void*                    out_result_ptr)
{
    _ral_framebuffer* framebuffer_ptr = (_ral_framebuffer*) framebuffer;

    /* Sanity checks */
    if (framebuffer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input FB instance is NULL");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAL_FRAMEBUFFER_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = framebuffer_ptr->callback_manager;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_framebuffer_property value.");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC bool ral_framebuffer_set_attachment_2D(ral_framebuffer                 framebuffer,
                                              ral_framebuffer_attachment_type attachment_type,
                                              uint32_t                        index,
                                              ral_texture                     texture_2d,
                                              uint32_t                        n_mipmap,
                                              bool                            should_enable)
{
    _ral_framebuffer*            framebuffer_ptr       = (_ral_framebuffer*) framebuffer;
    bool                         result                = false;
    _ral_framebuffer_attachment* target_attachment_ptr = NULL;
    uint32_t                     texture_n_mipmaps     = 0;
    ral_texture_type             texture_type          = RAL_TEXTURE_TYPE_UNKNOWN;

    /* Sanity checks */
    if (framebuffer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input framebuffer instance is NULL");

        goto end;
    } /* if (framebuffer == NULL) */

    if (!_ral_framebuffer_get_attachment(framebuffer,
                                         attachment_type,
                                         index,
                                        &target_attachment_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve attachment descriptor");

        goto end;
    }

    /* Verify the specified texture is valid.. */
    ral_texture_get_property(texture_2d,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &texture_n_mipmaps);
    ral_texture_get_property(texture_2d,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    if (texture_type != RAL_TEXTURE_TYPE_2D)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified texture instance is not a 2D texture");

        goto end;
    }

    if (texture_n_mipmaps <= n_mipmap)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified 2D texture does not define the specified mipmap level [%d]",
                          n_mipmap);

        goto end;
    }

    /* Looks good. Update the internal storage. */
    target_attachment_ptr = framebuffer_ptr->color_attachments + index;

    if (target_attachment_ptr->texture != NULL)
    {
        ral_texture_release(target_attachment_ptr->texture);

        target_attachment_ptr->texture = NULL;
    }

    target_attachment_ptr->is_enabled   = should_enable;
    target_attachment_ptr->n_mipmap     = n_mipmap;
    target_attachment_ptr->n_layer      = 0;
    target_attachment_ptr->texture      = texture_2d;
    target_attachment_ptr->texture_type = RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_2D;

    ral_texture_retain(texture_2d);

    /* Update the subscribers */
    system_callback_manager_call_back(framebuffer_ptr->callback_manager,
                                      (attachment_type == RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR) ? RAL_FRAMEBUFFER_CALLBACK_ID_COLOR_ATTACHMENT_CONFIGURATION_CHANGED
                                                                                                 : RAL_FRAMEBUFFER_CALLBACK_ID_DEPTH_STENCIL_ATTACHMENT_CONFIGURATION_CHANGED,
                                      (void*) index);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_framebuffer_set_attachment_property(ral_framebuffer                     framebuffer,
                                                    ral_framebuffer_attachment_type     attachment_type,
                                                    uint32_t                            index,
                                                    ral_framebuffer_attachment_property property,
                                                    const void*                         data)
{
    _ral_framebuffer*            framebuffer_ptr       = (_ral_framebuffer*) framebuffer;
    bool                         result                = false;
    _ral_framebuffer_attachment* target_attachment_ptr = NULL;

    /* Sanity checks */
    if (framebuffer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input framebuffer instance is NULL");

        goto end;
    } /* if (framebuffer == NULL) */

    if (!_ral_framebuffer_get_attachment(framebuffer,
                                         attachment_type,
                                         index,
                                        &target_attachment_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve attachment descriptor");

        goto end;
    }

    /* Update the requested property value. */
    switch (property)
    {
        case RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_IS_ENABLED:
        {
            target_attachment_ptr->is_enabled = *(bool*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported ral_framebuffer_attachment_property value.");

            goto end;
        }
    } /* switch (property) */

    /* Notify the subscribers of the change */
    system_callback_manager_call_back(framebuffer_ptr->callback_manager,
                                      (attachment_type == RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR) ? RAL_FRAMEBUFFER_CALLBACK_ID_COLOR_ATTACHMENT_CONFIGURATION_CHANGED
                                                                                                 : RAL_FRAMEBUFFER_CALLBACK_ID_DEPTH_STENCIL_ATTACHMENT_CONFIGURATION_CHANGED,
                                     &index);

    /* All done */
    result = true;

end:
    return result;
}
