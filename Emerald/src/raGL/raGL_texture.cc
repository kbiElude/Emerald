/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_texture.h"
#include "ral/ral_utils.h"
#include "system/system_log.h"
#include <algorithm>


typedef struct
{
    uint32_t                                             n_updates;
    struct _raGL_texture*                                texture_ptr;
    const ral_texture_mipmap_client_sourced_update_info* updates;

} _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg;

typedef struct _raGL_texture
{
    ogl_context context; /* NOT owned */
    ral_texture texture; /* DO NOT release */

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

        /* NOTE: Only GL is supported at the moment. */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "TODO");
    }

    ~_raGL_texture()
    {
        ASSERT_DEBUG_SYNC(id == 0,
                          "RBO/TO should have been deleted at _release*() call time.");
    }
} _raGL_texture;

/* Forward declarations */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_client_memory_sourced_update_rendering_thread_callback(ogl_context    context,
                                                                                                         void*          user_arg);
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_deinit_storage_rendering_callback                     (ogl_context    context,
                                                                                                         void*          user_arg);
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_generate_mipmaps_rendering_callback                   (ogl_context    context,
                                                                                                         void*          user_arg);
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_init_storage_rendering_callback                       (ogl_context    context,
                                                                                                         void*          user_arg);
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_init_renderbuffer_storage                             (_raGL_texture* texture_ptr);
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_init_texture_storage                                  (_raGL_texture* texture_ptr);
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_verify_conformance_to_ral_texture                     (_raGL_texture* texture_ptr);


/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_client_memory_sourced_update_rendering_thread_callback(ogl_context context,
                                                                                                         void*       user_arg)
{
    _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg* callback_arg_ptr          = (_raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg*) user_arg;
    const ogl_context_gl_entrypoints_ext_direct_state_access*                 entrypoints_dsa_ptr       = NULL;
    const ogl_context_gl_entrypoints*                                         entrypoints_ptr           = NULL;
    ral_texture_format                                                        texture_format            = RAL_TEXTURE_FORMAT_UNKNOWN;
    bool                                                                      texture_format_compressed = false;
    GLenum                                                                    texture_format_gl         = GL_NONE;
    ral_texture_type                                                          texture_type              = RAL_TEXTURE_TYPE_UNKNOWN;

    ASSERT_DEBUG_SYNC(!callback_arg_ptr->texture_ptr->is_renderbuffer,
                      "Client memory-based updates are forbidden for renderbuffer-backed raGL_instances.");

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


    texture_format_gl = raGL_utils_get_ogl_texture_internalformat_for_ral_texture_format(texture_format);

    ral_utils_get_texture_format_property(texture_format,
                                          RAL_TEXTURE_FORMAT_PROPERTY_IS_COMPRESSED,
                                         &texture_format_compressed);

    for (uint32_t n_update = 0;
                  n_update < callback_arg_ptr->n_updates;
                ++n_update)
    {
        GLenum                                               texture_data_type_gl      = GL_NONE;
        GLuint                                               texture_layer_gl          = -1;
        GLenum                                               texture_target_gl         = GL_NONE;
        const ral_texture_mipmap_client_sourced_update_info& update_info               = callback_arg_ptr->updates[n_update];

        texture_data_type_gl = raGL_utils_get_ogl_data_type_for_ral_texture_data_type(update_info.data_type);
        texture_layer_gl     = update_info.n_layer;

        /* Ensure the row alignment used by OpenGL matches the update request.
         *
         * TODO: We could handle any row alignment by rearranging input data. */
        if (update_info.data_row_alignment != 1 &&
            update_info.data_row_alignment != 2 &&
            update_info.data_row_alignment != 4 &&
            update_info.data_row_alignment != 8)
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO: The requested row alignment [%d] is not currently supported",
                              update_info.data_row_alignment);

            continue;
        }

        /* Determine the texture target for the update operation */
        if (texture_type == RAL_TEXTURE_TYPE_CUBE_MAP       ||
            texture_type == RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY)
        {
            switch (update_info.n_layer % 6)
            {
                case 0: texture_target_gl = GL_TEXTURE_CUBE_MAP_NEGATIVE_X; break;
                case 1: texture_target_gl = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y; break;
                case 2: texture_target_gl = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; break;
                case 3: texture_target_gl = GL_TEXTURE_CUBE_MAP_POSITIVE_X; break;
                case 4: texture_target_gl = GL_TEXTURE_CUBE_MAP_POSITIVE_Y; break;
                case 5: texture_target_gl = GL_TEXTURE_CUBE_MAP_POSITIVE_Z; break;
            } /* switch (update_info.n_layer % 6) */

            texture_layer_gl /= 6;
        } /* if (cube-map or CM array texture) */
        else
        {
            texture_target_gl = raGL_utils_get_ogl_texture_target_for_ral_texture_type(texture_type);
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
                                                                           update_info.n_mipmap,
                                                                           update_info.region_start_offset[0],
                                                                           update_info.region_start_offset[1],
                                                                           update_info.region_size[0],
                                                                           update_info.region_size[1],
                                                                           texture_format_gl,
                                                                           update_info.data_size,
                                                                           update_info.data);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Client memory-sourced texture update not supported for the requested texture type.");
                }
            } /* switch (texture_type) */
        } /* if (texture_format_compressed) */
        else
        {
            switch (texture_type)
            {
                case RAL_TEXTURE_TYPE_1D:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage1DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info.n_mipmap,
                                                                 update_info.region_start_offset[0],
                                                                 update_info.region_start_offset[1],
                                                                 texture_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info.data);

                    break;
                }

                case RAL_TEXTURE_TYPE_1D_ARRAY:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage2DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info.n_mipmap,
                                                                 update_info.region_start_offset[0],
                                                                 update_info.n_layer,
                                                                 update_info.region_size[0],
                                                                 1, /* height */
                                                                 texture_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info.data);

                    break;
                }

                case RAL_TEXTURE_TYPE_2D:
                case RAL_TEXTURE_TYPE_CUBE_MAP:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage2DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info.n_mipmap,
                                                                 update_info.region_start_offset[0],
                                                                 update_info.region_start_offset[1],
                                                                 update_info.region_size[0],
                                                                 update_info.region_size[1],
                                                                 texture_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info.data);

                    break;
                }

                case RAL_TEXTURE_TYPE_2D_ARRAY:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage3DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info.n_mipmap,
                                                                 update_info.region_start_offset[0],
                                                                 update_info.region_start_offset[1],
                                                                 update_info.n_layer,
                                                                 update_info.region_size[0],
                                                                 update_info.region_size[1],
                                                                 1, /* depth */
                                                                 texture_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info.data);

                    break;
                }

                case RAL_TEXTURE_TYPE_3D:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage3DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info.n_mipmap,
                                                                 update_info.region_start_offset[0],
                                                                 update_info.region_start_offset[1],
                                                                 update_info.region_start_offset[2],
                                                                 update_info.region_size[0],
                                                                 update_info.region_size[1],
                                                                 update_info.region_size[2],
                                                                 texture_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info.data);

                    break;
                }

                case RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY:
                {
                    entrypoints_dsa_ptr->pGLTextureSubImage3DEXT(callback_arg_ptr->texture_ptr->id,
                                                                 texture_target_gl,
                                                                 update_info.n_mipmap,
                                                                 update_info.region_start_offset[0],
                                                                 update_info.region_start_offset[1],
                                                                 update_info.n_layer,
                                                                 update_info.region_size[0],
                                                                 update_info.region_size[1],
                                                                 1, /* depth */
                                                                 texture_format_gl,
                                                                 texture_data_type_gl,
                                                                 update_info.data);

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
            } /* switch (texture_type) */
        }
    } /* for (all requested updates) */
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_deinit_storage_rendering_callback(ogl_context    context,
                                                                                    void*          user_arg)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _raGL_texture*                    texture_ptr     = (_raGL_texture*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (texture_ptr->is_renderbuffer)
    {
        LOG_INFO("[GL back-end]: Deleting texture storage (GL renderbuffer ID [%d])",
             texture_ptr->id);

        entrypoints_ptr->pGLDeleteRenderbuffers(1,
                                               &texture_ptr->id);
    } /* if (texture_ptr->is_renderbuffer) */
    else
    {
        LOG_INFO("[GL back-end]: Deleting texture storage (GL texture ID [%d])",
                 texture_ptr->id);

        entrypoints_ptr->pGLDeleteTextures(1,
                                          &texture_ptr->id);
    }

    texture_ptr->id = 0;
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_generate_mipmaps_rendering_callback(ogl_context    context,
                                                                                      void*          user_arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = NULL;
    _raGL_texture*                                            texture_ptr         = (_raGL_texture*) user_arg;
    ral_texture_type                                          texture_type        = RAL_TEXTURE_TYPE_UNKNOWN;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);
    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    entrypoints_dsa_ptr->pGLGenerateTextureMipmapEXT(texture_ptr->id,
                                                     raGL_utils_get_ogl_texture_target_for_ral_texture_type(texture_type) );
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_init_storage_rendering_callback(ogl_context context,
                                                                                  void*       user_arg)
{
    uint32_t               texture_n_mipmaps = 0;
    _raGL_texture*         texture_ptr       = (_raGL_texture*) user_arg;
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
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_init_renderbuffer_storage(_raGL_texture* texture_ptr)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    uint32_t                                                  texture_base_height = 0;
    uint32_t                                                  texture_base_width  = 0;
    ral_texture_format                                        texture_format      = RAL_TEXTURE_FORMAT_UNKNOWN;
    ogl_texture_internalformat                                texture_format_gl   = OGL_TEXTURE_INTERNALFORMAT_UNKNOWN;
    uint32_t                                                  texture_n_samples   = 0;

    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);
    ogl_context_get_property(texture_ptr->context,
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

    LOG_INFO("[GL back-end]: Allocating new renderbuffer storage for GL renderbuffer ID [%d]",
             texture_ptr->id);

    texture_format_gl = raGL_utils_get_ogl_texture_internalformat_for_ral_texture_format(texture_format);

    if (texture_n_samples > 1)
    {
        entrypoints_dsa_ptr->pGLNamedRenderbufferStorageMultisampleEXT(texture_ptr->id,
                                                                       texture_n_samples,
                                                                       texture_format_gl,
                                                                       texture_base_width,
                                                                       texture_base_height);
    } /* if (texture_n_samples > 1) */
    else
    {
        entrypoints_dsa_ptr->pGLNamedRenderbufferStorageEXT(texture_ptr->id,
                                                            texture_format_gl,
                                                            texture_base_width,
                                                            texture_base_height);
    }

    texture_ptr->is_renderbuffer = true;
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_init_texture_storage(_raGL_texture* texture_ptr)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr            = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr                = NULL;
    uint32_t                                                  texture_base_depth             = 0;
    uint32_t                                                  texture_base_height            = 0;
    uint32_t                                                  texture_base_width             = 0;
    bool                                                      texture_fixed_sample_locations = false;
    ral_texture_format                                        texture_format                 = RAL_TEXTURE_FORMAT_UNKNOWN;
    ogl_texture_internalformat                                texture_format_gl              = OGL_TEXTURE_INTERNALFORMAT_UNKNOWN;
    uint32_t                                                  texture_n_layers               = 0;
    uint32_t                                                  texture_n_mipmaps              = 0;
    uint32_t                                                  texture_n_samples              = 0;
    ral_texture_type                                          texture_type                   = RAL_TEXTURE_TYPE_UNKNOWN;

    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);
    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

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

    texture_format_gl = raGL_utils_get_ogl_texture_internalformat_for_ral_texture_format(texture_format);

    entrypoints_ptr->pGLGenTextures(1,
                                   &texture_ptr->id);

    LOG_INFO("[GL back-end]: Allocating new texture storage for GL texture ID [%d]",
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
    } /* switch (texture_type) */

    texture_ptr->is_renderbuffer = false;
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_texture_verify_conformance_to_ral_texture(_raGL_texture* texture_ptr)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;

    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    /* Do general properties match? */
    GLint gl_general_fixed_sample_locations = 0;
    GLint gl_general_n_layers               = 0;
    GLint gl_general_n_mipmaps              = 0;
    GLint gl_general_n_samples              = 0;

    bool                       ral_general_fixed_sample_locations = false;
    ral_texture_format         ral_general_format                 = RAL_TEXTURE_FORMAT_UNKNOWN;
    uint32_t                   ral_general_n_layers               = 0;
    uint32_t                   ral_general_n_mipmaps              = 0;
    uint32_t                   ral_general_n_samples              = 0;
    ral_texture_type           ral_general_type                   = RAL_TEXTURE_TYPE_UNKNOWN;
    GLenum                     ral_general_type_gl                = GL_NONE;

    ral_texture_get_property(texture_ptr->texture,
                             RAL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,
                            &ral_general_fixed_sample_locations);
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

    ral_general_type_gl = raGL_utils_get_ogl_texture_target_for_ral_texture_type(ral_general_type);

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
        entrypoints_dsa_ptr->pGLGetTextureParameterivEXT(texture_ptr->id,
                                                         ral_general_type_gl,
                                                         GL_TEXTURE_FIXED_SAMPLE_LOCATIONS,
                                                        &gl_general_fixed_sample_locations);
        entrypoints_dsa_ptr->pGLGetTextureParameterivEXT(texture_ptr->id,
                                                         ral_general_type_gl,
                                                         GL_TEXTURE_DEPTH,
                                                        &gl_general_n_layers);
        entrypoints_dsa_ptr->pGLGetTextureParameterivEXT(texture_ptr->id,
                                                         ral_general_type_gl,
                                                         GL_TEXTURE_SAMPLES,
                                                         &gl_general_n_samples);
        entrypoints_dsa_ptr->pGLGetTextureParameterivEXT(texture_ptr->id,
                                                         ral_general_type_gl,
                                                         GL_TEXTURE_IMMUTABLE_LEVELS,
                                                        (GLint*) &gl_general_n_mipmaps);
    }

    if (gl_general_n_layers == 0)
    {
        gl_general_n_layers = 1;
    }

    if (gl_general_n_samples == 0)
    {
        gl_general_n_samples = 1;
    }

    if (!texture_ptr->is_renderbuffer && (gl_general_fixed_sample_locations == 0) == ral_general_fixed_sample_locations)
    {
        ASSERT_DEBUG_SYNC(false,
                          "GL texture object's fixedsamplelocations property value does not match RAL texture's (%d vs %s)",
                          gl_general_fixed_sample_locations,
                          ral_general_fixed_sample_locations ? 1 : 0);
    }

    if (!texture_ptr->is_renderbuffer && gl_general_n_layers != ral_general_n_layers)
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
        GLint                      gl_mipmap_depth           = 0;
        GLint                      gl_mipmap_height          = 0;
        ogl_texture_internalformat gl_mipmap_internal_format = OGL_TEXTURE_INTERNALFORMAT_UNKNOWN;
        GLint                      gl_mipmap_width           = 0;
        uint32_t                   ral_mipmap_depth          = 0;
        uint32_t                   ral_mipmap_height         = 0;
        ral_texture_format         ral_mipmap_format          = RAL_TEXTURE_FORMAT_UNKNOWN;
        uint32_t                   ral_mipmap_width          = 0;

        if (texture_ptr->is_renderbuffer)
        {
            gl_mipmap_depth = 1;

            entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                       GL_RENDERBUFFER_INTERNAL_FORMAT,
                                                             (GLint*) &gl_mipmap_internal_format);
            entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                       GL_RENDERBUFFER_HEIGHT,
                                                                      &gl_mipmap_height);
            entrypoints_dsa_ptr->pGLGetNamedRenderbufferParameterivEXT(texture_ptr->id,
                                                                       GL_RENDERBUFFER_WIDTH,
                                                                      &gl_mipmap_width);
        } /* if (texture_ptr->is_renderbuffer) */
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
                                                        (GLint*) &gl_mipmap_internal_format);
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

        ral_mipmap_format = raGL_utils_get_ral_texture_format_for_ogl_enum(gl_mipmap_internal_format);

        if (ral_mipmap_format != ral_general_format)
        {
            ASSERT_DEBUG_SYNC(false,
                              "GL/RAL mip-map format mismatch detected");
        }

        if (gl_mipmap_depth  != ral_mipmap_depth  ||
            gl_mipmap_height != ral_mipmap_height ||
            gl_mipmap_width  != ral_mipmap_width)
        {
            ASSERT_DEBUG_SYNC(false,
                              "GL/RAL mip-map size detected at mip-map level [%d]",
                              n_mipmap);
        }
    } /* for (all mipmaps recognized by GL and RAL) */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL raGL_texture raGL_texture_create(ogl_context context,
                                                               ral_texture texture)
{
    _raGL_texture* result_ptr = NULL;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (texture == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    /* Spawn a new descriptor. */
    result_ptr = new (std::nothrow) _raGL_texture(context,
                                                  texture);

    if (result_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    /* Init storage for the texture instance */
    ogl_context_request_callback_from_context_thread(context,
                                                     _raGL_texture_init_storage_rendering_callback,
                                                     result_ptr);

    /* All done */

end:
    return (raGL_texture) result_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_texture_generate_mipmaps(raGL_texture texture)
{
    _raGL_texture* texture_ptr = (_raGL_texture*) texture;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(texture != NULL,
                      "Input raGL_texture instance is NULL");

    /* Request a rendering thread call-back */
    ogl_context_request_callback_from_context_thread(texture_ptr->context,
                                                     _raGL_texture_generate_mipmaps_rendering_callback,
                                                     texture);
}

/** Please see header for specification */
PUBLIC bool raGL_texture_get_property(raGL_texture          texture,
                                      raGL_texture_property property,
                                      void*                 out_result_ptr)
{
    bool           result      = false;
    _raGL_texture* texture_ptr = (_raGL_texture*) texture;

    /* Sanity checks */
    if (texture == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture instance is NULL");

        goto end;
    }

    /* Retrieve the requested property value */
    result = true;

    switch (property)
    {
        case RAGL_TEXTURE_PROPERTY_ID:
        {
            *(GLuint*) out_result_ptr = texture_ptr->id;

            break;
        }

        case RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER:
        {
            *(bool*) out_result_ptr = texture_ptr->is_renderbuffer;

            break;
        }

        case RAGL_TEXTURE_PROPERTY_RAL_TEXTURE:
        {
            *(ral_texture*) out_result_ptr = texture_ptr->texture;

            break;
        }

        default:
        {
            result = false;

            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_texture_property value.");
        }
    } /* switch (property) */

end:
    return result;
}

/** Please see header for specification */
PUBLIC void raGL_texture_release(raGL_texture texture)
{
    _raGL_texture* texture_ptr = (_raGL_texture*) texture;

    ASSERT_DEBUG_SYNC(texture != NULL,
                      "Input raGL_texture instance is NULL");

    ogl_context_request_callback_from_context_thread(texture_ptr->context,
                                                     _raGL_texture_deinit_storage_rendering_callback,
                                                     texture_ptr);

    delete (_raGL_texture*) texture;
    texture = NULL;
}

/** Please see header for specification */
PUBLIC bool raGL_texture_update_with_client_sourced_data(raGL_texture                                         texture,
                                                         uint32_t                                             n_updates,
                                                         const ral_texture_mipmap_client_sourced_update_info* updates)
{
    bool           result      = false;
    _raGL_texture* texture_ptr = (_raGL_texture*) texture;

    /* Sanity checks.. */
    if (texture == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_texture instance is NULL");

        goto end;
    }

    if (n_updates == 0)
    {
        result = true;

        goto end;
    }

    if (updates == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input update info array is NULL");

        goto end;
    }

    /* Request a rendering thread call-back */
    _raGL_texture_client_memory_sourced_update_rendering_thread_callback_arg callback_arg;

    callback_arg.n_updates   = n_updates;
    callback_arg.texture_ptr = texture_ptr;
    callback_arg.updates     = updates;

    ogl_context_request_callback_from_context_thread(texture_ptr->context,
                                                     _raGL_texture_client_memory_sourced_update_rendering_thread_callback,
                                                    &callback_arg);

end:
    return result;
}