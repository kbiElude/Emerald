/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"


typedef struct _raGL_framebuffer
{
    /* NOTE: Draw / read buffers are managed externally */

    GLint id; /* NOT owned */


    _raGL_framebuffer(ogl_context in_context,
                      GLint       in_fb_id)
    {
        id = in_fb_id;

        /* NOTE: Only GL is supported at the moment. */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "TODO");
    }

    ~_raGL_framebuffer()
    {
        /* Do not release the framebuffer. Object life-time is handled by raGL_backend. */
    }
} _raGL_framebuffer;

typedef struct _raGL_framebuffer_init_rendering_thread_callback_data
{
    const ral_texture_view* color_attachments;
    ral_texture_view        ds_attachment;
    _raGL_framebuffer*      fbo_ptr;
    uint32_t                n_color_attachments;

    explicit _raGL_framebuffer_init_rendering_thread_callback_data(const ral_texture_view* in_color_attachments,
                                                                   ral_texture_view        in_ds_attachment,
                                                                   _raGL_framebuffer*      in_fbo_ptr,
                                                                   uint32_t                in_n_color_attachments)
    {
        color_attachments   = in_color_attachments;
        ds_attachment       = in_ds_attachment;
        fbo_ptr             = in_fbo_ptr;
        n_color_attachments = in_n_color_attachments;
    }
} _raGL_framebuffer_init_rendering_thread_callback_data;


/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_framebuffer_init_rendering_thread_calback(ogl_context context,
                                                                                    void*       user_arg)
{
    _raGL_framebuffer_init_rendering_thread_callback_data*    args_ptr            = (_raGL_framebuffer_init_rendering_thread_callback_data*) user_arg;
    raGL_backend                                              backend_gl          = nullptr;
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(args_ptr          != nullptr &&
                      args_ptr->fbo_ptr != nullptr,
                      "Input callback args are NULL.");

    /* Iterate over all attachments */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_gl);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    for (uint32_t n_iteration = 0;
                  n_iteration < 2; /* color, & depth/stencil attachment. */
                ++n_iteration)
    {
        const ral_texture_view* attachments   = (n_iteration == 0) ?  args_ptr->color_attachments
                                                                   : (args_ptr->ds_attachment != nullptr) ? &args_ptr->ds_attachment : nullptr;
        const uint32_t          n_attachments = (n_iteration == 0) ?  args_ptr->n_color_attachments
                                                                   : (args_ptr->ds_attachment != nullptr) ? 1 : 0;

        for (uint32_t n_attachment = 0;
                      n_attachment < n_attachments;
                    ++n_attachment)
        {
            GLenum             target_gl;
            ral_texture_view   texture_view = attachments[n_attachment];
            ral_texture_aspect texture_view_aspect;
            ral_format         texture_view_format;
            uint32_t           texture_view_n_base_layer;
            uint32_t           texture_view_n_base_mipmap;
            uint32_t           texture_view_n_layers;
            raGL_texture       texture_view_raGL       = nullptr;
            GLuint             texture_view_raGL_id    = 0;
            bool               texture_view_raGL_is_rb = false;
            ral_texture_type   texture_view_type;

            /* Configure the attachment */
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_ASPECT,
                                         &texture_view_aspect);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                         &texture_view_format);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_BASE_LAYER,
                                         &texture_view_n_base_layer);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_BASE_MIPMAP,
                                         &texture_view_n_base_mipmap);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                         &texture_view_n_layers);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                         &texture_view_type);

            raGL_backend_get_texture_view(backend_gl,
                                          texture_view,
                                          (void**) &texture_view_raGL);
            raGL_texture_get_property    (texture_view_raGL,
                                          RAGL_TEXTURE_PROPERTY_ID,
                                          (void**) &texture_view_raGL_id);
            raGL_texture_get_property    (texture_view_raGL,
                                          RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                          (void**) &texture_view_raGL_is_rb);

            switch (n_iteration)
            {
                /* Color attachment */
                case 0:
                {
                    target_gl = GL_COLOR_ATTACHMENT0 + n_attachment;

                    break;
                }

                /* D / DS / S attachment */
                case 1:
                {
                    const bool is_d_attachment = (texture_view_aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT)   != 0;
                    const bool is_s_attachment = (texture_view_aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0;

                    target_gl = ( is_d_attachment &&  is_s_attachment) ? GL_DEPTH_STENCIL_ATTACHMENT
                              : ( is_d_attachment && !is_s_attachment) ? GL_DEPTH_ATTACHMENT
                              : (!is_d_attachment &&  is_s_attachment) ? GL_STENCIL_ATTACHMENT
                                                                       : GL_NONE;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Invalid iteration index");
                }
            }

            switch (texture_view_type)
            {
                case RAL_TEXTURE_TYPE_2D:
                {
                    if (texture_view_raGL_is_rb)
                    {
                        ASSERT_DEBUG_SYNC(texture_view_n_base_layer == 0,
                                          "Invalid base layer specified.");
                        ASSERT_DEBUG_SYNC(texture_view_n_base_mipmap == 0,
                                          "Invalid base mipmap level specified.");
                        ASSERT_DEBUG_SYNC(texture_view_n_layers == 1,
                                          "Invalid number of layers specified.");

                        entrypoints_dsa_ptr->pGLNamedFramebufferRenderbufferEXT(args_ptr->fbo_ptr->id,
                                                                                target_gl,
                                                                                GL_RENDERBUFFER,
                                                                                texture_view_raGL_id);
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(texture_view_n_base_layer == 0,
                                          "Invalid base layer specified.");
                        ASSERT_DEBUG_SYNC(texture_view_n_layers == 1,
                                          "Invalid number of layers specified.");

                        entrypoints_dsa_ptr->pGLNamedFramebufferTexture2DEXT(args_ptr->fbo_ptr->id,
                                                                             target_gl,
                                                                             GL_TEXTURE_2D,
                                                                             texture_view_raGL_id,
                                                                             texture_view_n_base_mipmap);
                    }

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "TODO");
                }
            }
        }
    }

    /* Make sure the FB is complete */
    GLenum completeness_status = entrypoints_dsa_ptr->pGLCheckNamedFramebufferStatusEXT(args_ptr->fbo_ptr->id,
                                                                                        GL_DRAW_FRAMEBUFFER);

    ASSERT_DEBUG_SYNC(completeness_status == GL_FRAMEBUFFER_COMPLETE,
                      "Incomplete framebuffer reported.");
}

/** Please see header for specification */
PUBLIC raGL_framebuffer raGL_framebuffer_create(ogl_context             context,
                                                GLint                   fb_id,
                                                uint32_t                n_color_attachments,
                                                const ral_texture_view* color_attachments,
                                                ral_texture_view        opt_ds_attachment)
{
    _raGL_framebuffer* new_fb_ptr = new (std::nothrow) _raGL_framebuffer(context,
                                                                         fb_id);

    ASSERT_ALWAYS_SYNC(new_fb_ptr != nullptr,
                       "Out of memory");

    if (new_fb_ptr != nullptr)
    {
        _raGL_framebuffer_init_rendering_thread_callback_data callback_data(color_attachments,
                                                                            opt_ds_attachment,
                                                                            new_fb_ptr,
                                                                            n_color_attachments);

        ogl_context_request_callback_from_context_thread(context,
                                                         _raGL_framebuffer_init_rendering_thread_calback,
                                                        &callback_data);
    }

    return (raGL_framebuffer) new_fb_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void raGL_framebuffer_get_property(raGL_framebuffer          fb,
                                                      raGL_framebuffer_property property,
                                                      void*                     out_result_ptr)
{
    _raGL_framebuffer* fb_ptr = (_raGL_framebuffer*) fb;

    /* Sanity checks */
    if (fb_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(fb_ptr != NULL,
                          "Input raGL_framebuffer instance is NULL");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAGL_FRAMEBUFFER_PROPERTY_ID:
        {
            *(GLuint*) out_result_ptr = fb_ptr->id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_framebuffer_property value.");

            goto end;
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void raGL_framebuffer_release(raGL_framebuffer fb)
{
    ASSERT_DEBUG_SYNC(fb != nullptr,
                      "Input framebuffer is NULL");

    /* Safe to release the instance at this point */
    delete (_raGL_framebuffer*) fb;
}
