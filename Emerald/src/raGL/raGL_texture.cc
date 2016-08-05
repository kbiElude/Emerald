/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_to_bindings.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_sync.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_scheduler.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"
#include "system/system_log.h"
#include <algorithm>


typedef struct
{
    bool                                                                         async;
    struct _raGL_texture*                                                        texture_ptr;
    std::vector<std::shared_ptr<ral_texture_mipmap_client_sourced_update_info> > updates;

} _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg;

typedef struct _raGL_texture
{
    ogl_context      context;      /* NOT owned      */
    ral_texture      texture;      /* DO NOT release */
    ral_texture_view texture_view; /* DO NOT release */

    GLuint      id;      /* OWNED; can either be a RBO ID (if is_renderbuffer is true), or a TO ID (otherwise) */
    bool        is_renderbuffer;

    ogl_context_gl_entrypoints* entrypoints_ptr;


    _raGL_texture(ogl_context in_context,
                  ral_texture in_texture)
    {
        context         = in_context;
        id              = 0;
        is_renderbuffer = false;
        texture         = in_texture;
        texture_view    = nullptr;

        /* NOTE: Only GL is supported at the moment. */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "TODO");
    }

    _raGL_texture(ogl_context      in_context,
                  GLuint           in_id,
                  ral_texture_view in_texture_view)
    {
        ASSERT_DEBUG_SYNC(in_id != 0,
                          "Zero texture ID specified for a texture view");

        context      = in_context;
        id           = in_id;
        texture      = nullptr;
        texture_view = in_texture_view;
    }

    ~_raGL_texture()
    {
        ASSERT_DEBUG_SYNC(id == 0,
                          "RBO/TO should have been deleted at _release*() call time.");
    }
} _raGL_texture;

/* Forward declarations */
PRIVATE ral_present_job _raGL_texture_client_memory_sourced_update_renderer_callback (ral_context                                                context,
                                                                                      void*                                                      user_arg,
                                                                                      const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE void            _raGL_texture_client_memory_sourced_update_scheduler_callback(void*                                                      user_arg);
PRIVATE ral_present_job _raGL_texture_deinit_storage_rendering_callback              (ral_context                                                context,
                                                                                      void*                                                      user_arg,
                                                                                      const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE ral_present_job _raGL_texture_generate_mipmaps_renderer_callback             (ral_context                                                context,
                                                                                      void*                                                      texture);
PRIVATE void            _raGL_texture_generate_mipmaps_scheduler_callback            (void*                                                      texture);
PRIVATE ral_present_job _raGL_texture_init_storage_renderer_callback                 (ral_context                                                context,
                                                                                      void*                                                      texture,
                                                                                      const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE void            _raGL_texture_init_renderbuffer_storage                      (_raGL_texture*                                             texture_ptr);
PRIVATE void            _raGL_texture_init_texture_storage                           (_raGL_texture*                                             texture_ptr);
PRIVATE ral_present_job _raGL_texture_init_view_renderer_callback                    (ral_context                                                context,
                                                                                      void*                                                      texture,
                                                                                      const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE void            _raGL_texture_verify_conformance_to_ral_texture              (_raGL_texture*                                             texture_ptr);


/** TODO */
PRIVATE ral_present_job _raGL_texture_client_memory_sourced_update_renderer_callback(ral_context                                                context,
                                                                                     void*                                                      user_arg,
                                                                                     const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_texture_client_memory_sourced_update_scheduler_callback(user_arg);

    /* We speak GL here. No need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE void _raGL_texture_client_memory_sourced_update_scheduler_callback(void* user_arg)
{
    _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg* callback_arg_ptr          = reinterpret_cast<_raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg*>(user_arg);
    ogl_context                                                               context                   = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints_ext_direct_state_access*                 entrypoints_dsa_ptr       = nullptr;
    const ogl_context_gl_entrypoints*                                         entrypoints_ptr           = nullptr;
    ogl_texture_data_format                                                   texture_data_format_gl    = OGL_TEXTURE_DATA_FORMAT_UNDEFINED;
    ral_format                                                                texture_format            = RAL_FORMAT_UNKNOWN;
    bool                                                                      texture_format_compressed = false;
    GLenum                                                                    texture_format_gl;
    ral_texture_type                                                          texture_type              = RAL_TEXTURE_TYPE_UNKNOWN;

    ASSERT_DEBUG_SYNC(!callback_arg_ptr->texture_ptr->is_renderbuffer,
                      "Client memory-based updates are forbidden for renderbuffer-backed raGL_instances.");

    /* Sync with other contexts before continuing .. */
    raGL_backend_sync();

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    ral_texture_get_property(callback_arg_ptr->texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &texture_format);
    ral_texture_get_property(callback_arg_ptr->texture_ptr->texture,
                            RAL_TEXTURE_PROPERTY_TYPE,
                           &texture_type);

    texture_data_format_gl = raGL_utils_get_ogl_data_format_for_ral_format(texture_format);
    texture_format_gl      = raGL_utils_get_ogl_enum_for_ral_format       (texture_format);

    ral_utils_get_format_property(texture_format,
                                  RAL_FORMAT_PROPERTY_IS_COMPRESSED,
                                 &texture_format_compressed);

    for (auto update_info_ptr : callback_arg_ptr->updates)
    {
        GLenum texture_data_type_gl      = GL_NONE;
        GLuint texture_layer_gl          = -1;
        GLenum texture_target_gl         = GL_NONE;

        texture_data_type_gl = raGL_utils_get_ogl_enum_for_ral_texture_data_type(update_info_ptr->data_type);
        texture_layer_gl     = update_info_ptr->n_layer;

        /* Ensure the row alignment used by OpenGL matches the update request.
         *
         * TODO: We could handle any row alignment by rearranging input data. */
        if (update_info_ptr->data_row_alignment != 1 &&
            update_info_ptr->data_row_alignment != 2 &&
            update_info_ptr->data_row_alignment != 4 &&
            update_info_ptr->data_row_alignment != 8)
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO: The requested row alignment [%d] is not currently supported",
                              update_info_ptr->data_row_alignment);

            continue;
        }
        else
        {
            entrypoints_ptr->pGLPixelStorei(GL_UNPACK_ALIGNMENT,
                                            update_info_ptr->data_row_alignment);
        }

        /* Determine the texture target for the update operation */
        if (texture_type == RAL_TEXTURE_TYPE_CUBE_MAP       ||
            texture_type == RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY)
        {
            switch (update_info_ptr->n_layer % 6)
            {
                case 0: texture_target_gl = GL_TEXTURE_CUBE_MAP_NEGATIVE_X; break;
                case 1: texture_target_gl = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y; break;
                case 2: texture_target_gl = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; break;
                case 3: texture_target_gl = GL_TEXTURE_CUBE_MAP_POSITIVE_X; break;
                case 4: texture_target_gl = GL_TEXTURE_CUBE_MAP_POSITIVE_Y; break;
                case 5: texture_target_gl = GL_TEXTURE_CUBE_MAP_POSITIVE_Z; break;
            }

            texture_layer_gl /= 6;
        }
        else
        {
            texture_target_gl = raGL_utils_get_ogl_enum_for_ral_texture_type(texture_type);
        }

        /* Update the requested mip-map */
        if (texture_format_compressed)
        {
            switch (texture_type)
            {
                case RAL_TEXTURE_TYPE_2D:
                {
                    entrypoints_dsa_ptr->pGLCompressedTextureSubImage2DEXT(callback_arg_ptr->texture_ptr->id,
                                                                           texture_target_gl,
                                                                           update_info_ptr->n_mipmap,
                                                                           update_info_ptr->region_start_offset[0],
                                                                           update_info_ptr->region_start_offset[1],
                                                                           update_info_ptr->region_size[0],
                                                                           update_info_ptr->region_size[1],
                                                                           texture_format_gl,
                                                                           update_info_ptr->data_size,
                                                                           update_info_ptr->data);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Client memory-sourced texture update not supported for the requested texture type.");
                }
            }
        }
        else
        {
            switch (texture_type)
            {
                case RAL_TEXTURE_TYPE_1D:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage1DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info_ptr->n_mipmap,
                                                                 update_info_ptr->region_start_offset[0],
                                                                 update_info_ptr->region_start_offset[1],
                                                                 texture_data_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info_ptr->data);

                    break;
                }

                case RAL_TEXTURE_TYPE_1D_ARRAY:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage2DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info_ptr->n_mipmap,
                                                                 update_info_ptr->region_start_offset[0],
                                                                 update_info_ptr->n_layer,
                                                                 update_info_ptr->region_size[0],
                                                                 1, /* height */
                                                                 texture_data_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info_ptr->data);
                    break;
                }

                case RAL_TEXTURE_TYPE_2D:
                case RAL_TEXTURE_TYPE_CUBE_MAP:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage2DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info_ptr->n_mipmap,
                                                                 update_info_ptr->region_start_offset[0],
                                                                 update_info_ptr->region_start_offset[1],
                                                                 update_info_ptr->region_size[0],
                                                                 update_info_ptr->region_size[1],
                                                                 texture_data_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info_ptr->data);

                    break;
                }

                case RAL_TEXTURE_TYPE_2D_ARRAY:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage3DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info_ptr->n_mipmap,
                                                                 update_info_ptr->region_start_offset[0],
                                                                 update_info_ptr->region_start_offset[1],
                                                                 update_info_ptr->n_layer,
                                                                 update_info_ptr->region_size[0],
                                                                 update_info_ptr->region_size[1],
                                                                 1, /* depth */
                                                                 texture_data_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info_ptr->data);

                    break;
                }

                case RAL_TEXTURE_TYPE_3D:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage3DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info_ptr->n_mipmap,
                                                                 update_info_ptr->region_start_offset[0],
                                                                 update_info_ptr->region_start_offset[1],
                                                                 update_info_ptr->region_start_offset[2],
                                                                 update_info_ptr->region_size[0],
                                                                 update_info_ptr->region_size[1],
                                                                 update_info_ptr->region_size[2],
                                                                 texture_data_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info_ptr->data);

                    break;
                }

                case RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage3DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info_ptr->n_mipmap,
                                                                 update_info_ptr->region_start_offset[0],
                                                                 update_info_ptr->region_start_offset[1],
                                                                 update_info_ptr->n_layer,
                                                                 update_info_ptr->region_size[0],
                                                                 update_info_ptr->region_size[1],
                                                                 1, /* depth */
                                                                 texture_data_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info_ptr->data);

                    break;
                }

                case RAL_TEXTURE_TYPE_MULTISAMPLE_2D:
                case RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "OpenGL does not support multisample texture updates");

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unsupported texture type");
                }
            }
        }
    }

    if (callback_arg_ptr->async)
    {
        delete callback_arg_ptr;
    }

    /* Sync other contexts. */
    raGL_backend_enqueue_sync();
}

/** TODO */
PRIVATE ral_present_job _raGL_texture_deinit_storage_rendering_callback(ral_context                                                context,
                                                                        void*                                                      user_arg,
                                                                        const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    ogl_context                       context_gl      = nullptr;
    const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    _raGL_texture*                    texture_ptr     = reinterpret_cast<_raGL_texture*>(user_arg);

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (texture_ptr->is_renderbuffer)
    {
        LOG_INFO("[GL back-end]: Deleting texture storage (GL renderbuffer ID [%u])",
             texture_ptr->id);

        entrypoints_ptr->pGLDeleteRenderbuffers(1,
                                               &texture_ptr->id);
    }
    else
    {
        LOG_INFO("[GL back-end]: Deleting texture storage (GL texture ID [%u])",
                 texture_ptr->id);

        entrypoints_ptr->pGLDeleteTextures(1,
                                          &texture_ptr->id);
    }

    raGL_backend_enqueue_sync();

    texture_ptr->id = 0;

    /* We speak GL here, no need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_texture_generate_mipmaps_renderer_callback(ral_context                                                context,
                                                                         void*                                                      texture,
                                                                         const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_texture_generate_mipmaps_scheduler_callback(texture);

    /* We speak GL here, no need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE void _raGL_texture_generate_mipmaps_scheduler_callback(void* texture)
{
    const ogl_context                                         current_context     = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = nullptr;
    _raGL_texture*                                            texture_ptr         = reinterpret_cast<_raGL_texture*>(texture);
    ral_texture_type                                          texture_type        = RAL_TEXTURE_TYPE_UNKNOWN;

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    /* Make sure the context is sync'ed before continuing */
    raGL_backend_sync();

    /* Use the super-helpful ;) GL func to generate the mipmap data */
    entrypoints_dsa_ptr->pGLGenerateTextureMipmapEXT(texture_ptr->id,
                                                     raGL_utils_get_ogl_enum_for_ral_texture_type(texture_type) );

    /* Sync other contexts */
    raGL_backend_enqueue_sync();
}

/** TODO */
PRIVATE ral_present_job _raGL_texture_init_storage_renderer_callback(ral_context                                                context,
                                                                     void*                                                      texture,
                                                                     const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    uint32_t               texture_n_mipmaps = 0;
    _raGL_texture*         texture_ptr       = reinterpret_cast<_raGL_texture*>(texture);
    ral_texture_type       texture_type      = RAL_TEXTURE_TYPE_UNKNOWN;
    ral_texture_usage_bits texture_usage     = 0;

    /* A raGL_texture instance can hold either a renderbuffer or a texture object, depending on what the
     * requested usage pattern is and a few other things.
     *
     * NOTE: We don't really care about fixed_sample_locations value when determining which storage to use.
     *       This might (?) be a problem in the future, although very unlikely.
     */
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &texture_n_mipmaps);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_USAGE,
                            &texture_usage);

    if ( ((texture_usage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)         != 0                   ||
          (texture_usage & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)                  &&
          (texture_usage & RAL_TEXTURE_USAGE_SAMPLED_BIT)                  == 0                   &&
          (texture_usage & RAL_TEXTURE_USAGE_CLIENT_MEMORY_UPDATE_BIT)     == 0                   &&
          texture_n_mipmaps                                                == 1                   &&
          texture_type                                                     == RAL_TEXTURE_TYPE_2D)
    {
        _raGL_texture_init_renderbuffer_storage(texture_ptr);
    }
    else
    {
        _raGL_texture_init_texture_storage(texture_ptr);
    }

    #ifdef _DEBUG
    {
        _raGL_texture_verify_conformance_to_ral_texture(texture_ptr);
    }
    #endif /* _DEBUG */

    /* We speak GL here, no need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE void _raGL_texture_init_renderbuffer_storage(_raGL_texture* texture_ptr)
{
    ogl_context                                               current_context     = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = nullptr;
    uint32_t                                                  texture_base_height = 0;
    uint32_t                                                  texture_base_width  = 0;
    ral_format                                                texture_format      = RAL_FORMAT_UNKNOWN;
    GLenum                                                    texture_format_gl;
    uint32_t                                                  texture_n_samples   = 0;

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &texture_format);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_SAMPLES,
                            &texture_n_samples);

    ral_texture_get_mipmap_property(texture_ptr->texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                   &texture_base_height);
    ral_texture_get_mipmap_property(texture_ptr->texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                   &texture_base_width);

    entrypoints_ptr->pGLGenRenderbuffers(1,
                                        &texture_ptr->id);

    LOG_INFO("[GL back-end]: Allocating new renderbuffer storage for GL renderbuffer ID [%u]",
             texture_ptr->id);

    texture_format_gl = raGL_utils_get_ogl_enum_for_ral_format(texture_format);

    if (texture_n_samples > 1)
    {
        entrypoints_dsa_ptr->pGLNamedRenderbufferStorageMultisampleEXT(texture_ptr->id,
                                                                       texture_n_samples,
                                                                       texture_format_gl,
                                                                       texture_base_width,
                                                                       texture_base_height);
    }
    else
    {
        entrypoints_dsa_ptr->pGLNamedRenderbufferStorageEXT(texture_ptr->id,
                                                            texture_format_gl,
                                                            texture_base_width,
                                                            texture_base_height);
    }

    raGL_backend_enqueue_sync();

    texture_ptr->is_renderbuffer = true;
}

/** TODO */
PRIVATE void _raGL_texture_init_texture_storage(_raGL_texture* texture_ptr)
{
    const ogl_context                                         current_context                = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr            = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr                = nullptr;
    GLuint                                                    precall_bound_to_id            = 0;
    uint32_t                                                  texture_base_depth             = 0;
    uint32_t                                                  texture_base_height            = 0;
    uint32_t                                                  texture_base_width             = 0;
    bool                                                      texture_fixed_sample_locations = false;
    ral_format                                                texture_format                 = RAL_FORMAT_UNKNOWN;
    GLenum                                                    texture_format_gl;
    uint32_t                                                  texture_n_layers               = 0;
    uint32_t                                                  texture_n_mipmaps              = 0;
    uint32_t                                                  texture_n_samples              = 0;
    GLenum                                                    texture_target                 = GL_NONE;
    ral_texture_type                                          texture_type                   = RAL_TEXTURE_TYPE_UNKNOWN;
    ogl_context_to_bindings                                   to_bindings_cache              = nullptr;

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings_cache);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,
                            &texture_fixed_sample_locations);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &texture_format);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_LAYERS,
                            &texture_n_layers);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &texture_n_mipmaps);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_SAMPLES,
                            &texture_n_samples);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    ral_texture_get_mipmap_property(texture_ptr->texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                   &texture_base_depth);
    ral_texture_get_mipmap_property(texture_ptr->texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                   &texture_base_height);
    ral_texture_get_mipmap_property(texture_ptr->texture,
                                    0, /* n_layer  */
                                    0, /* n_mipmap */
                                    RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                   &texture_base_width);

    texture_format_gl = raGL_utils_get_ogl_enum_for_ral_format(texture_format);

    entrypoints_ptr->pGLGenTextures(1,
                                   &texture_ptr->id);

    LOG_INFO("[GL back-end]: Allocating new texture storage for GL texture ID [%u]",
             texture_ptr->id);

    switch (texture_type)
    {
        case RAL_TEXTURE_TYPE_1D:
        {
            entrypoints_dsa_ptr->pGLTextureStorage1DEXT(texture_ptr->id,
                                                        GL_TEXTURE_1D,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width);

            break;
        }

        case RAL_TEXTURE_TYPE_1D_ARRAY:
        {
            entrypoints_dsa_ptr->pGLTextureStorage2DEXT(texture_ptr->id,
                                                        GL_TEXTURE_1D_ARRAY,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width,
                                                        texture_n_layers);

            break;
        }

        case RAL_TEXTURE_TYPE_2D:
        {
            entrypoints_dsa_ptr->pGLTextureStorage2DEXT(texture_ptr->id,
                                                        GL_TEXTURE_2D,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width,
                                                        texture_base_height);

            break;
        }

        case RAL_TEXTURE_TYPE_2D_ARRAY:
        {
            entrypoints_dsa_ptr->pGLTextureStorage3DEXT(texture_ptr->id,
                                                        GL_TEXTURE_2D_ARRAY,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width,
                                                        texture_base_height,
                                                        texture_n_layers);

            break;
        }

        case RAL_TEXTURE_TYPE_3D:
        {
            entrypoints_dsa_ptr->pGLTextureStorage3DEXT(texture_ptr->id,
                                                        GL_TEXTURE_3D,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width,
                                                        texture_base_height,
                                                        texture_base_depth);

            break;
        }

        case RAL_TEXTURE_TYPE_CUBE_MAP:
        {
            entrypoints_dsa_ptr->pGLTextureStorage2DEXT(texture_ptr->id,
                                                        GL_TEXTURE_CUBE_MAP,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width,
                                                        texture_base_height);

            break;
        }

        case RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY:
        {
            entrypoints_dsa_ptr->pGLTextureStorage3DEXT(texture_ptr->id,
                                                        GL_TEXTURE_CUBE_MAP_ARRAY,
                                                        texture_n_mipmaps,
                                                        texture_format_gl,
                                                        texture_base_width,
                                                        texture_base_height,
                                                        texture_n_layers);

            break;
        }

        case RAL_TEXTURE_TYPE_MULTISAMPLE_2D:
        {
            entrypoints_dsa_ptr->pGLTextureStorage2DMultisampleEXT(texture_ptr->id,
                                                                   GL_TEXTURE_2D_MULTISAMPLE,
                                                                   texture_n_samples,
                                                                   texture_format_gl,
                                                                   texture_base_width,
                                                                   texture_base_height,
                                                                   texture_fixed_sample_locations);

            break;
        }

        case RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY:
        {
            entrypoints_dsa_ptr->pGLTextureStorage3DMultisampleEXT(texture_ptr->id,
                                                                   GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                                                   texture_n_samples,
                                                                   texture_format_gl,
                                                                   texture_base_width,
                                                                   texture_base_height,
                                                                   texture_n_layers,
                                                                   texture_fixed_sample_locations);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_type value.");
        }
    }

    /* Sync other contexts */
    raGL_backend_enqueue_sync();

    texture_ptr->is_renderbuffer = false;
}

/** TODO */
PRIVATE ral_present_job _raGL_texture_init_view_renderer_callback(ral_context                                                context,
                                                                  void*                                                      texture,
                                                                  const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    raGL_backend                                              backend                   = nullptr;
    ogl_context                                               context_gl                = nullptr;
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr       = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr           = nullptr;
    uint32_t                                                  n_texture_view_base_layer = -1;
    uint32_t                                                  n_texture_view_base_level = -1;
    uint32_t                                                  n_texture_view_layers     = -1;
    uint32_t                                                  n_texture_view_levels     = -1;
    ral_texture                                               parent_texture            = nullptr;
    raGL_texture                                              parent_texture_raGL       = nullptr;
    GLuint                                                    parent_texture_raGL_id    = -1;
    _raGL_texture*                                            texture_ptr               = reinterpret_cast<_raGL_texture*>(texture);
    ral_texture_aspect                                        texture_view_aspect;
    ral_format                                                texture_view_format       = RAL_FORMAT_UNKNOWN;
    ral_texture_type                                          texture_view_type         = RAL_TEXTURE_TYPE_UNKNOWN;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);

    /* Extract texture view properties */
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_ASPECT,
                                 &texture_view_aspect);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                 &texture_view_format);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_N_BASE_LAYER,
                                 &n_texture_view_base_layer);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_N_BASE_MIPMAP,
                                 &n_texture_view_base_level);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                 &n_texture_view_layers);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                 &n_texture_view_levels);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                 &parent_texture);
    ral_texture_view_get_property(texture_ptr->texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                 &texture_view_type);

    raGL_backend_get_texture (backend,
                              parent_texture,
                             &parent_texture_raGL);
    raGL_texture_get_property(parent_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                              reinterpret_cast<void**>(&parent_texture_raGL_id) );

    /* Assign a texture view to the ID */
    const GLenum texture_target = raGL_utils_get_ogl_enum_for_ral_texture_type(texture_view_type);

    entrypoints_ptr->pGLTextureView(texture_ptr->id,
                                    texture_target,
                                    parent_texture_raGL_id,
                                    texture_view_format,
                                    n_texture_view_base_level,
                                    n_texture_view_levels,
                                    n_texture_view_base_layer,
                                    n_texture_view_layers);

    /* Configure aspect for the view */
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(texture_ptr->id,
                                                 texture_target,
                                                 GL_DEPTH_STENCIL_TEXTURE_MODE,
                                                 raGL_utils_get_ogl_enum_for_ral_texture_aspect(texture_view_aspect) );

    /* We speak GL here, no need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE void _raGL_texture_verify_conformance_to_ral_texture(_raGL_texture* texture_ptr)
{
    const ogl_context                                         current_context     = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = nullptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = nullptr;

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    /* Do general properties match? */
    GLint gl_general_n_layers  = 0;
    GLint gl_general_n_mipmaps = 0;
    GLint gl_general_n_samples = 0;

    ral_format       ral_general_format    = RAL_FORMAT_UNKNOWN;
    uint32_t         ral_general_n_layers  = 0;
    uint32_t         ral_general_n_mipmaps = 0;
    uint32_t         ral_general_n_samples = 0;
    ral_texture_type ral_general_type      = RAL_TEXTURE_TYPE_UNKNOWN;
    GLenum           ral_general_type_gl   = GL_NONE;

    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &ral_general_format);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_LAYERS,
                            &ral_general_n_layers);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &ral_general_n_mipmaps);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_N_SAMPLES,
                            &ral_general_n_samples);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &ral_general_type);

    ral_general_type_gl = raGL_utils_get_ogl_enum_for_ral_texture_type(ral_general_type);

    /* TODO: Cube-map textures will need some more love. */
    ASSERT_DEBUG_SYNC(ral_general_type != RAL_TEXTURE_TYPE_CUBE_MAP &&
                      ral_general_type != RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY,
                      "TODO");

    if (texture_ptr->is_renderbuffer)
    {
        entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                   GL_RENDERBUFFER_SAMPLES,
                                                                  &gl_general_n_samples);
    }
    else
    {
        entrypoints_dsa_ptr->pGLGetTextureLevelParameterivEXT(texture_ptr->id,
                                                              ral_general_type_gl,
                                                              0, /* level */
                                                              GL_TEXTURE_DEPTH,
                                                             &gl_general_n_layers);
        entrypoints_dsa_ptr->pGLGetTextureLevelParameterivEXT(texture_ptr->id,
                                                              ral_general_type_gl,
                                                              0, /* level */
                                                              GL_TEXTURE_SAMPLES,
                                                              &gl_general_n_samples);
        entrypoints_dsa_ptr->pGLGetTextureParameterivEXT     (texture_ptr->id,
                                                              ral_general_type_gl,
                                                              GL_TEXTURE_IMMUTABLE_LEVELS,
                                                              reinterpret_cast<GLint*>(&gl_general_n_mipmaps) );
    }

    if (gl_general_n_layers == 0)
    {
        gl_general_n_layers = 1;
    }

    if (gl_general_n_samples == 0)
    {
        gl_general_n_samples = 1;
    }

    if (!texture_ptr->is_renderbuffer               &&
         ral_general_type    == RAL_TEXTURE_TYPE_3D &&
         gl_general_n_layers != ral_general_n_layers)
    {
        ASSERT_DEBUG_SYNC(false,
                          "GL texture object's number of layers does not match RAL texture's (%d vs %d)",
                          gl_general_n_layers,
                          ral_general_n_layers);
    }

    if (!texture_ptr->is_renderbuffer && gl_general_n_mipmaps != ral_general_n_mipmaps)
    {
        ASSERT_DEBUG_SYNC(false,
                          "GL object's number of mipmaps does not match RAL texture's (%d vs %d)",
                          gl_general_n_mipmaps,
                          ral_general_n_mipmaps);
    }

    if ( (uint32_t) gl_general_n_samples < ral_general_n_samples)
    {
        ASSERT_DEBUG_SYNC(false,
                          "GL object's number of samples does not match RAL texture's (%d vs %d)",
                          gl_general_n_samples,
                          ral_general_n_samples);
    }

    /* Iterate over all mipmaps and verify the dimensions are in sync */
    for (uint32_t n_mipmap = 0;
                  n_mipmap < min( (uint32_t) gl_general_n_mipmaps, ral_general_n_mipmaps);
                ++n_mipmap)
    {
        GLint      gl_mipmap_depth           = 0;
        GLint      gl_mipmap_height          = 0;
        GLenum     gl_mipmap_internal_format;
        GLint      gl_mipmap_width           = 0;
        uint32_t   ral_mipmap_depth          = 0;
        uint32_t   ral_mipmap_height         = 0;
        ral_format ral_mipmap_format         = RAL_FORMAT_UNKNOWN;
        uint32_t   ral_mipmap_width          = 0;

        if (texture_ptr->is_renderbuffer)
        {
            gl_mipmap_depth = 1;

            entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                       GL_RENDERBUFFER_INTERNAL_FORMAT,
                                                                       reinterpret_cast<GLint*>(&gl_mipmap_internal_format) );
            entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                       GL_RENDERBUFFER_HEIGHT,
                                                                      &gl_mipmap_height);
            entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                       GL_RENDERBUFFER_WIDTH,
                                                                      &gl_mipmap_width);
        }
        else
        {
            entrypoints_dsa_ptr->pGLGetTextureLevelParameterivEXT(texture_ptr->id,
                                                                  ral_general_type_gl,
                                                                  n_mipmap,
                                                                  GL_TEXTURE_DEPTH,
                                                                 &gl_mipmap_depth);
            entrypoints_dsa_ptr->pGLGetTextureLevelParameterivEXT(texture_ptr->id,
                                                                  ral_general_type_gl,
                                                                  n_mipmap,
                                                                  GL_TEXTURE_HEIGHT,
                                                                 &gl_mipmap_height);
            entrypoints_dsa_ptr->pGLGetTextureLevelParameterivEXT(texture_ptr->id,
                                                                  ral_general_type_gl,
                                                                  n_mipmap,
                                                                  GL_TEXTURE_INTERNAL_FORMAT,
                                                                  reinterpret_cast<GLint*>(&gl_mipmap_internal_format) );
            entrypoints_dsa_ptr->pGLGetTextureLevelParameterivEXT(texture_ptr->id,
                                                                  ral_general_type_gl,
                                                                  n_mipmap,
                                                                  GL_TEXTURE_WIDTH,
                                                                 &gl_mipmap_width);
        }

        ral_texture_get_mipmap_property(texture_ptr->texture,
                                        0, /* n_layer */
                                        n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                       &ral_mipmap_depth);
        ral_texture_get_mipmap_property(texture_ptr->texture,
                                        0, /* n_layer */
                                        n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                       &ral_mipmap_height);
        ral_texture_get_mipmap_property(texture_ptr->texture,
                                        0, /* n_layer */
                                        n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                       &ral_mipmap_width);

        ral_mipmap_format = raGL_utils_get_ral_format_for_ogl_enum(gl_mipmap_internal_format);

        if (ral_mipmap_format != ral_general_format)
        {
            ASSERT_DEBUG_SYNC(false,
                              "GL/RAL mip-map format mismatch detected");
        }

        if (ral_general_type == RAL_TEXTURE_TYPE_3D && gl_mipmap_depth  != ral_mipmap_depth  ||
                                                       gl_mipmap_height != ral_mipmap_height ||
                                                       gl_mipmap_width  != ral_mipmap_width)
        {
            ASSERT_DEBUG_SYNC(false,
                              "GL/RAL mip-map size detected at mip-map level [%d]",
                              n_mipmap);
        }
    }
}

/** Please see header for specification */
PUBLIC raGL_texture raGL_texture_create(ogl_context context,
                                        ral_texture texture)
{
    ral_context           context_ral = nullptr;
    ral_rendering_handler context_rh  = nullptr;
    _raGL_texture*        result_ptr  = nullptr;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is nullptr");

        goto end;
    }

    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture instance is nullptr");

        goto end;
    }

    /* Spawn a new descriptor. */
    result_ptr = new (std::nothrow) _raGL_texture(context,
                                                  texture);

    if (result_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &context_rh);

    ral_rendering_handler_request_rendering_callback(context_rh,
                                                     _raGL_texture_init_storage_renderer_callback,
                                                     result_ptr,
                                                     false); /* present_after_executed */

    /* All done */
end:
    return (raGL_texture) result_ptr;
}

/** Please see header for specification */
PUBLIC raGL_texture raGL_texture_create_view(ogl_context      context,
                                             GLuint           texture_id,
                                             ral_texture_view texture_view)
{
    ral_context           context_ral = nullptr;
    ral_rendering_handler context_rh  = nullptr;
    _raGL_texture*        result_ptr  = nullptr;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(context != nullptr,
                          "Input ogl_context instance is nullptr");

        goto end;
    }

    if (texture_view == nullptr)
    {
        ASSERT_DEBUG_SYNC(texture_view != nullptr,
                          "Input RAL texture view is nullptr");

        goto end;
    }

    /* Spawn a new descriptor. */
    result_ptr = new (std::nothrow) _raGL_texture(context,
                                                  texture_id,
                                                  texture_view);

    if (result_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(result_ptr != nullptr,
                           "Out of memory");

        goto end;
    }

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &context_rh);

    ral_rendering_handler_request_rendering_callback(context_rh,
                                                     _raGL_texture_init_view_renderer_callback,
                                                     result_ptr,
                                                     false); /* present_after_executed */

    /* All done */
end:
    return (raGL_texture) result_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_texture_generate_mipmaps(raGL_texture texture,
                                          bool         async)
{
    _raGL_texture* texture_ptr = reinterpret_cast<_raGL_texture*>(texture);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(texture != nullptr,
                      "Input raGL_texture instance is nullptr");

    if (async)
    {
        ral_backend_type       backend_type;
        ral_scheduler_job_info new_job;
        ral_scheduler          scheduler       = nullptr;

        /* Schedule the action */
        demo_app_get_property   (DEMO_APP_PROPERTY_GPU_SCHEDULER,
                                &scheduler);
        ogl_context_get_property(texture_ptr->context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        new_job.job_type                            = RAL_SCHEDULER_JOB_TYPE_CALLBACK;
        new_job.callback_job_args.callback_user_arg = texture;
        new_job.callback_job_args.pfn_callback_proc = _raGL_texture_generate_mipmaps_scheduler_callback;
        new_job.n_write_locks                       = 1;
        new_job.write_locks[0]                      = texture;

        ral_scheduler_schedule_job(scheduler,
                                   backend_type,
                                   new_job);
    }
    else
    {
        ogl_context           current_context     = ogl_context_get_current_context();
        ral_context           current_context_ral = nullptr;
        ral_rendering_handler current_context_rh  = nullptr;

        if (texture_ptr->context != current_context)
        {
            current_context = texture_ptr->context;
        }

        ogl_context_get_property(current_context,
                                 OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                                &current_context_ral);
        ral_context_get_property(current_context_ral,
                                 RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                &current_context_rh);

        ral_rendering_handler_request_rendering_callback(current_context_rh, 
                                                         _raGL_texture_generate_mipmaps_renderer_callback,
                                                         texture,
                                                         false); /* present_after_executed */
    }
}

/** Please see header for specification */
PUBLIC bool raGL_texture_get_property(const raGL_texture    texture,
                                      raGL_texture_property property,
                                      void**                out_result_ptr)
{
    bool                 result      = false;
    const _raGL_texture* texture_ptr = reinterpret_cast<const _raGL_texture*>(texture);

    /* Sanity checks */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture instance is nullptr");

        goto end;
    }

    /* Retrieve the requested property value */
    result = true;

    switch (property)
    {
        case RAGL_TEXTURE_PROPERTY_ID:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = texture_ptr->id;

            break;
        }

        case RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = texture_ptr->is_renderbuffer;

            break;
        }

        case RAGL_TEXTURE_PROPERTY_RAL_TEXTURE:
        {
            *reinterpret_cast<ral_texture*>(out_result_ptr) = texture_ptr->texture;

            break;
        }

        default:
        {
            result = false;

            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_texture_property value.");
        }
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC void raGL_texture_release(raGL_texture texture)
{
    ral_context           context_ral = nullptr;
    ral_rendering_handler context_rh  = nullptr;
    _raGL_texture*        texture_ptr = reinterpret_cast<_raGL_texture*>(texture);

    ASSERT_DEBUG_SYNC(texture != nullptr,
                      "Input raGL_texture instance is nullptr");

    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &context_rh);

    ral_rendering_handler_request_rendering_callback(context_rh,
                                                     _raGL_texture_deinit_storage_rendering_callback,
                                                     texture_ptr,
                                                     false); /* present_after_executed */

    delete texture_ptr;
}

/** Please see header for specification */
PUBLIC bool raGL_texture_update_with_client_sourced_data(raGL_texture                                                                        texture,
                                                         const std::vector<std::shared_ptr<ral_texture_mipmap_client_sourced_update_info> >& updates,
                                                         bool                                                                                async)
{
    bool           result      = false;
    _raGL_texture* texture_ptr = reinterpret_cast<_raGL_texture*>(texture);

    /* Sanity checks.. */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_texture instance is nullptr");

        goto end;
    }

    if (updates.size() == 0)
    {
        result = true;

        goto end;
    }

    if (async)
    {
        /* It is OK to have multiple ongoing write ops, but none must be in flight at mipmap generation time.
         * For a texture update request, we use a "read" lock, whereas for a mipmap gen process, we will open
         * an exclusive "write" lock.
         */
        ral_backend_type                                                          backend_type;
        _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg* callback_arg_ptr = new _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg;
        ral_scheduler_job_info                                                    job_info;
        ral_scheduler                                                             scheduler   = nullptr;

        /* Request an async call-back from the GPU scheduler */
        ogl_context_get_property(texture_ptr->context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                 &backend_type);
        demo_app_get_property   (DEMO_APP_PROPERTY_GPU_SCHEDULER,
                                &scheduler);

        callback_arg_ptr->async       = true;
        callback_arg_ptr->texture_ptr = texture_ptr;
        callback_arg_ptr->updates     = updates;

        job_info.job_type                            = RAL_SCHEDULER_JOB_TYPE_CALLBACK;
        job_info.callback_job_args.callback_user_arg = callback_arg_ptr;
        job_info.callback_job_args.pfn_callback_proc = _raGL_texture_client_memory_sourced_update_scheduler_callback;
        job_info.n_read_locks                        = 1;
        job_info.read_locks[0]                       = texture_ptr;

        ral_scheduler_schedule_job(scheduler,
                                   backend_type,
                                   job_info);
    }
    else
    {
        _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg callback_arg; 
        ogl_context                                                              current_context     = ogl_context_get_current_context();
        ral_context                                                              current_context_ral = nullptr;
        ral_rendering_handler                                                    current_context_rh  = nullptr;

        callback_arg.async       = false;
        callback_arg.texture_ptr = texture_ptr;
        callback_arg.updates     = updates;

        if (texture_ptr->context != current_context)
        {
            current_context = texture_ptr->context;
        }

        ogl_context_get_property(current_context,
                                 OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                                &current_context_ral);
        ral_context_get_property(current_context_ral,
                                 RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                &current_context_rh);

        ral_rendering_handler_request_rendering_callback(current_context_rh,
                                                         _raGL_texture_client_memory_sourced_update_renderer_callback,
                                                        &callback_arg,
                                                         false); /* present after executed */

        result = true;
    }
end:
    return result;
}