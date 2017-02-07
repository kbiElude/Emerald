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
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"


typedef struct _raGL_framebuffer
{
    /* NOTE: Draw / read buffers are managed externally */

    ogl_context context;
    GLuint      id;

    _raGL_framebuffer(ogl_context in_context);

    ~_raGL_framebuffer()
    {
        ASSERT_DEBUG_SYNC(id == 0,
                          "raGL_framebuffer instance about to be destroyed with FB id != 0");
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
_raGL_framebuffer::_raGL_framebuffer(ogl_context in_context)
{
    context = in_context;
    id      = -1;           /* will be set later */

    /* NOTE: Only GL is supported at the moment. */
    ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

    ogl_context_get_property(in_context,
                             OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                      "TODO");

}


/** TODO */
PRIVATE ral_present_job _raGL_framebuffer_deinit_rendering_thread_calback(ral_context                                                context_ral,
                                                                          void*                                                      user_arg,
                                                                          const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    ogl_context                       context         = nullptr;
    const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    _raGL_framebuffer*                fb_ptr          = reinterpret_cast<_raGL_framebuffer*>(user_arg);

    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    LOG_INFO("[!] Deleting GL FBO [%d]",
             fb_ptr->id);

    entrypoints_ptr->pGLDeleteFramebuffers(1, /* n */
                                           reinterpret_cast<const GLuint*>(&fb_ptr->id) );

    fb_ptr->id = 0;

    /* We issue GL calls directly from this entrypoint, so we don't need a present job */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_framebuffer_init_rendering_thread_calback(ral_context                                                context_ral,
                                                                        void*                                                      user_arg,
                                                                        const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_framebuffer_init_rendering_thread_callback_data*    args_ptr            = reinterpret_cast<_raGL_framebuffer_init_rendering_thread_callback_data*>(user_arg);
    raGL_backend                                              backend_gl          = nullptr;
    ogl_context                                               context             = nullptr;
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(args_ptr          != nullptr &&
                      args_ptr->fbo_ptr != nullptr,
                      "Input callback args are nullptr.");

    /* Iterate over all attachments */
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_gl);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    /* Generate a new FB id */
    entrypoints_ptr->pGLGenFramebuffers(1, /* n */
                                       &args_ptr->fbo_ptr->id);

    LOG_INFO("[!] Created GL FBO [%d]",
             args_ptr->fbo_ptr->id);

    /* Configure the attachments */
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
            uint32_t           texture_view_n_layers;
            uint32_t           texture_view_n_mips;
            raGL_texture       texture_view_raGL       = nullptr;
            GLuint             texture_view_raGL_id    = 0;
            bool               texture_view_raGL_is_rb = false;
            ral_texture_type   texture_view_type;

            if (texture_view == nullptr)
            {
                continue;
            }

            /* Configure the attachment */
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_ASPECT,
                                         &texture_view_aspect);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                         &texture_view_format);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                         &texture_view_n_layers);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                         &texture_view_n_mips);
            ral_texture_view_get_property(texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                         &texture_view_type);

            raGL_backend_get_texture_view(backend_gl,
                                          texture_view,
                                         &texture_view_raGL);
            raGL_texture_get_property    (texture_view_raGL,
                                          RAGL_TEXTURE_PROPERTY_ID,
                                          reinterpret_cast<void**>(&texture_view_raGL_id) );
            raGL_texture_get_property    (texture_view_raGL,
                                          RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                          reinterpret_cast<void**>(&texture_view_raGL_is_rb) );

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
                        ASSERT_DEBUG_SYNC(texture_view_n_layers == 1,
                                          "Invalid number of layers specified.");
                        ASSERT_DEBUG_SYNC(texture_view_n_mips == 1,
                                          "Invalid number of mips specified.");

                        entrypoints_dsa_ptr->pGLNamedFramebufferRenderbufferEXT(args_ptr->fbo_ptr->id,
                                                                                target_gl,
                                                                                GL_RENDERBUFFER,
                                                                                texture_view_raGL_id);
                    }
                    else
                    {
                        if (texture_view_n_layers == 1)
                        {
                            entrypoints_dsa_ptr->pGLNamedFramebufferTexture2DEXT(args_ptr->fbo_ptr->id,
                                                                                 target_gl,
                                                                                 GL_TEXTURE_2D,
                                                                                 texture_view_raGL_id,
                                                                                 0); /* level */
                        }
                        else
                        {
                            /* TODO: This is a bit tricky and needs to be implemented carefully. */
                            ASSERT_DEBUG_SYNC(false,
                                              "TODO");
                        }
                    }

                    break;
                }

                case RAL_TEXTURE_TYPE_2D_ARRAY:
                {
                    ASSERT_DEBUG_SYNC(!texture_view_raGL_is_rb,
                                      "OpenGL does not support 2D array renderbuffers");

                    entrypoints_dsa_ptr->pGLNamedFramebufferTextureEXT(args_ptr->fbo_ptr->id,
                                                                       target_gl,
                                                                       texture_view_raGL_id,
                                                                       0); /* level */

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

    /* Make sure the FB is complete.
     *
     * NOTE: Completeness should be checked in regard for each context state configuration we use. Oh well.
     */
    GLenum completeness_status;

    completeness_status = entrypoints_dsa_ptr->pGLCheckNamedFramebufferStatusEXT(args_ptr->fbo_ptr->id,
                                                                                 GL_DRAW_FRAMEBUFFER);

    ASSERT_DEBUG_SYNC(completeness_status == GL_FRAMEBUFFER_COMPLETE,
                      "Incomplete framebuffer reported.");

    /* We fire GL calls directly from this entrypoint, so it's safe to return null here */
    return nullptr;
}

/** Please see header for specification */
PUBLIC raGL_framebuffer raGL_framebuffer_create(ogl_context             context,
                                                uint32_t                n_color_attachments,
                                                const ral_texture_view* color_attachments,
                                                ral_texture_view        opt_ds_attachment)
{
    _raGL_framebuffer* new_fb_ptr = new (std::nothrow) _raGL_framebuffer(context);

    ASSERT_ALWAYS_SYNC(new_fb_ptr != nullptr,
                       "Out of memory");

    if (new_fb_ptr != nullptr)
    {
        _raGL_framebuffer_init_rendering_thread_callback_data callback_data(color_attachments,
                                                                            opt_ds_attachment,
                                                                            new_fb_ptr,
                                                                            n_color_attachments);
        ral_context                                           context_ral;
        ral_rendering_handler                                 rendering_handler;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                                &context_ral);
        ral_context_get_property(context_ral,
                                 RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                &rendering_handler);

        ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                         _raGL_framebuffer_init_rendering_thread_calback,
                                                        &callback_data,
                                                         false); /* present_after_executed */
    }

    return (raGL_framebuffer) new_fb_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_framebuffer_get_property(const raGL_framebuffer    fb,
                                          raGL_framebuffer_property property,
                                          void*                     out_result_ptr)
{
    const _raGL_framebuffer* fb_ptr = reinterpret_cast<const _raGL_framebuffer*>(fb);

    /* Sanity checks */
    if (fb_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(fb_ptr != nullptr,
                          "Input raGL_framebuffer instance is nullptr");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAGL_FRAMEBUFFER_PROPERTY_ID:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = fb_ptr->id;

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
    ral_context           context_ral       = nullptr;
    _raGL_framebuffer*    fb_ptr            = reinterpret_cast<_raGL_framebuffer*>(fb);
    ral_rendering_handler rendering_handler = nullptr;

    ASSERT_DEBUG_SYNC(fb != nullptr,
                      "Input framebuffer is nullptr");

    ogl_context_get_property(fb_ptr->context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                     _raGL_framebuffer_deinit_rendering_thread_calback,
                                                     fb_ptr,
                                                     false); /* present_after_executed */

    /* Safe to release the instance at this point */
    delete reinterpret_cast<_raGL_framebuffer*>(fb);
}
