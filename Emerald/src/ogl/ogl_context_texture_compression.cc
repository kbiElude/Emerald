/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_texture_compression.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** TODO */
typedef struct _ogl_context_texture_compression_algorithm
{
    system_hashed_ansi_string file_extension;
    GLenum                    gl_value;
    system_hashed_ansi_string name;
} _ogl_context_texture_compression_algorithm;

/** TODO */
typedef struct _ogl_context_texture_compression
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    system_resizable_vector algorithms;                  /* stores _ogl_context_texture_compression_algorithm instaces */
    system_hash64map        compression_enums_map;       /* maps GLenum values to _ogl_context_texture_compression_algorithm* owned by
                                                          * algorithms. */

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;

    _ogl_context_texture_compression()
    {
        algorithms            = system_resizable_vector_create(4 /* capacity */);
        compression_enums_map = system_hash64map_create       (sizeof(GLenum) );
        context               = nullptr;
    }

    ~_ogl_context_texture_compression()
    {
        if (algorithms != nullptr)
        {
            _ogl_context_texture_compression_algorithm* entry = nullptr;

            while (system_resizable_vector_pop(algorithms,
                                              &entry) )
            {
                delete entry;

                entry = nullptr;
            }
            system_resizable_vector_release(algorithms);

            algorithms = nullptr;
        }

        if (compression_enums_map != nullptr)
        {
            system_hash64map_release(compression_enums_map);

            compression_enums_map = nullptr;
        }
    }
} _ogl_context_texture_compression;


/** Forward declarations */
PRIVATE void _ogl_context_texture_compression_init_bptc_support       (_ogl_context_texture_compression* texture_compression_ptr,
                                                                       GLenum*                           compressed_texture_formats,
                                                                       uint32_t                          n_compressed_texture_formats);
PRIVATE void _ogl_context_texture_compression_init_dxt1_support       (_ogl_context_texture_compression* texture_compression_ptr);
PRIVATE void _ogl_context_texture_compression_init_dxt3_dxt5_support  (_ogl_context_texture_compression* texture_compression_ptr);
PRIVATE void _ogl_context_texture_compression_init_latc_support       (_ogl_context_texture_compression* texture_compression_ptr);
PRIVATE void _ogl_context_texture_compression_init_rgtc_support       (_ogl_context_texture_compression* texture_compression_ptr,
                                                                       GLenum*                           compressed_texture_formats,
                                                                       uint32_t                          n_compressed_texture_formats);
PRIVATE bool _ogl_context_texture_compression_is_compression_supported(GLenum*                           compressed_texture_formats,
                                                                       uint32_t                          n_compressed_texture_formats,
                                                                       GLenum                            internalformat);

/** TODO */
PRIVATE void _ogl_context_texture_compression_init_bptc_support(_ogl_context_texture_compression* texture_compression_ptr,
                                                                GLenum*                           compressed_texture_formats,
                                                                uint32_t                          n_compressed_texture_formats)
{
    /* BC6H */
    _ogl_context_texture_compression_algorithm* item_rgb_bptc_signed_float_ptr   = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rgb_bptc_unsigned_float_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_rgb_bptc_signed_float_ptr   != nullptr &&
                       item_rgb_bptc_unsigned_float_ptr != nullptr,
                       "Out of memory");

    item_rgb_bptc_signed_float_ptr->file_extension = system_hashed_ansi_string_create("bc6h_sf");
    item_rgb_bptc_signed_float_ptr->gl_value       = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB;
    item_rgb_bptc_signed_float_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT");

    item_rgb_bptc_unsigned_float_ptr->file_extension = system_hashed_ansi_string_create("bc6h_uf");
    item_rgb_bptc_unsigned_float_ptr->gl_value       = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
    item_rgb_bptc_unsigned_float_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT");

    /* BC7 */
    _ogl_context_texture_compression_algorithm* item_rgba_bptc_unorm_ptr       = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_srgb_alpha_bptc_unorm_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_rgba_bptc_unorm_ptr       != nullptr &&
                       item_srgb_alpha_bptc_unorm_ptr != nullptr,
                       "Out of memory");

    item_rgba_bptc_unorm_ptr->file_extension = system_hashed_ansi_string_create("bc7_rgba");
    item_rgba_bptc_unorm_ptr->gl_value       = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
    item_rgba_bptc_unorm_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGBA_BPTC_UNORM");

    item_srgb_alpha_bptc_unorm_ptr->file_extension = system_hashed_ansi_string_create("bc7_srgba");
    item_srgb_alpha_bptc_unorm_ptr->gl_value       = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
    item_srgb_alpha_bptc_unorm_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM");

    /* Store the texture compression algorithms if GL_ARB_texture_compression_bptc extension
     * is reported as supported (it should be)
     */
    _ogl_context_texture_compression_algorithm* algorithms[] =
    {
        item_rgb_bptc_signed_float_ptr,
        item_rgb_bptc_unsigned_float_ptr,
        item_rgba_bptc_unorm_ptr,
        item_srgb_alpha_bptc_unorm_ptr
    };
    const uint32_t n_algorithms = sizeof(algorithms) / sizeof(algorithms[0] );

    if (ogl_context_is_extension_supported(texture_compression_ptr->context,
                                           system_hashed_ansi_string_create("GL_ARB_texture_compression_bptc") ))
    {
        for (uint32_t n_algorithm = 0;
                      n_algorithm < n_algorithms;
                    ++n_algorithm)
        {
            system_resizable_vector_push(texture_compression_ptr->algorithms,
                                         algorithms[n_algorithm]);

            system_hash64map_insert(texture_compression_ptr->compression_enums_map,
                                    algorithms[n_algorithm]->gl_value,
                                    algorithms[n_algorithm],
                                    nullptr,  /* on_remove_callback */
                                    nullptr); /* on_remove_callback_user_arg */
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "GL_ARB_texture_compression_bptc not reported for an OpenGL 4.2 context");

        for (uint32_t n_algorithm = 0;
                      n_algorithm < n_algorithms;
                    ++n_algorithm)
        {
            delete algorithms[n_algorithm];

            algorithms[n_algorithm] = nullptr;
        }
    }
}

/** TODO */
PRIVATE void _ogl_context_texture_compression_init_dxt1_support(_ogl_context_texture_compression* texture_compression_ptr)
{
    _ogl_context_texture_compression_algorithm* item_rgb_s3tc_dxt1_ptr  = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rgba_s3tc_dxt1_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_rgb_s3tc_dxt1_ptr  != nullptr && 
                       item_rgba_s3tc_dxt1_ptr != nullptr,
                       "Out of memory");

    item_rgb_s3tc_dxt1_ptr->file_extension = system_hashed_ansi_string_create("rgb_s3tc");
    item_rgb_s3tc_dxt1_ptr->gl_value       = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    item_rgb_s3tc_dxt1_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGB_S3TC_DXT1_EXT");

    item_rgba_s3tc_dxt1_ptr->file_extension = system_hashed_ansi_string_create("rgba_s3tc");
    item_rgba_s3tc_dxt1_ptr->gl_value       = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    item_rgba_s3tc_dxt1_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGBA_S3TC_DXT1_EXT");

    /* Store the texture compression algorithms if GL_ARB_texture_compression_bptc extension
     * is reported as supported (it should be)
     */
    _ogl_context_texture_compression_algorithm* algorithms[] =
    {
        item_rgb_s3tc_dxt1_ptr,
        item_rgba_s3tc_dxt1_ptr
    };
    const uint32_t n_algorithms = sizeof(algorithms) / sizeof(algorithms[0] );

    for (uint32_t n_algorithm = 0;
                  n_algorithm < n_algorithms;
                ++n_algorithm)
    {
        system_resizable_vector_push(texture_compression_ptr->algorithms,
                                     algorithms[n_algorithm]);

        system_hash64map_insert(texture_compression_ptr->compression_enums_map,
                                algorithms[n_algorithm]->gl_value,
                                algorithms[n_algorithm],
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */
    }
}

/** TODO */
PRIVATE void _ogl_context_texture_compression_init_dxt3_dxt5_support(_ogl_context_texture_compression* texture_compression_ptr)
{
    _ogl_context_texture_compression_algorithm* item_rgba_s3tc_dxt3_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rgba_s3tc_dxt5_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_rgba_s3tc_dxt3_ptr != nullptr &&
                       item_rgba_s3tc_dxt5_ptr != nullptr,
                       "Out of memory");

    item_rgba_s3tc_dxt3_ptr->file_extension = system_hashed_ansi_string_create("rgba_s3tc_dxt3");
    item_rgba_s3tc_dxt3_ptr->gl_value       = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    item_rgba_s3tc_dxt3_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGBA_S3TC_DXT3_EXT");

    item_rgba_s3tc_dxt5_ptr->file_extension = system_hashed_ansi_string_create("rgba_s3tc_dxt5");
    item_rgba_s3tc_dxt5_ptr->gl_value       = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    item_rgba_s3tc_dxt5_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGBA_S3TC_DXT5_EXT");

    /* Store the texture compression algorithms if GL_ARB_texture_compression_bptc extension
     * is reported as supported (it should be)
     */
    _ogl_context_texture_compression_algorithm* algorithms[] =
    {
        item_rgba_s3tc_dxt3_ptr,
        item_rgba_s3tc_dxt5_ptr
    };
    const uint32_t n_algorithms = sizeof(algorithms) / sizeof(algorithms[0] );

    for (uint32_t n_algorithm = 0;
                  n_algorithm < n_algorithms;
                ++n_algorithm)
    {
        system_resizable_vector_push(texture_compression_ptr->algorithms,
                                     algorithms[n_algorithm]);

        system_hash64map_insert(texture_compression_ptr->compression_enums_map,
                                algorithms[n_algorithm]->gl_value,
                                algorithms[n_algorithm],
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */
    }
}

/** TODO */
PRIVATE void _ogl_context_texture_compression_init_etc_etc2_support(_ogl_context_texture_compression* texture_compression_ptr,
                                                                    GLenum*                           compressed_texture_formats,
                                                                    uint32_t                          n_compressed_texture_formats)
{
    _ogl_context_texture_compression_algorithm* item_r11_eac_ptr                        = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rg11_eac_ptr                       = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rgb8_etc2_ptr                      = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rgb8_punchthrough_alpha1_etc2_ptr  = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_rgba8_etc2_eac_ptr                 = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_signed_r11_eac_ptr                 = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_signed_rg11_eac_ptr                = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_srgb8_etc2_ptr                     = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_srgb8_punchthrough_alpha1_etc2_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_srgb8_alpha8_etc2_eac_ptr          = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_r11_eac_ptr                        != nullptr &&
                       item_rg11_eac_ptr                       != nullptr &&
                       item_rgb8_etc2_ptr                      != nullptr &&
                       item_rgb8_punchthrough_alpha1_etc2_ptr  != nullptr &&
                       item_rgba8_etc2_eac_ptr                 != nullptr &&
                       item_signed_r11_eac_ptr                 != nullptr &&
                       item_signed_rg11_eac_ptr                != nullptr &&
                       item_srgb8_etc2_ptr                     != nullptr &&
                       item_srgb8_punchthrough_alpha1_etc2_ptr != nullptr &&
                       item_srgb8_alpha8_etc2_eac_ptr          != nullptr,
                       "Out of memory");

    item_r11_eac_ptr->file_extension = system_hashed_ansi_string_create("r11_eac");
    item_r11_eac_ptr->gl_value       = GL_COMPRESSED_R11_EAC;
    item_r11_eac_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_R11_EAC");

    item_rg11_eac_ptr->file_extension = system_hashed_ansi_string_create("r1g1_eac");
    item_rg11_eac_ptr->gl_value       = GL_COMPRESSED_RG11_EAC;
    item_rg11_eac_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RG11_EAC");

    item_rgb8_etc2_ptr->file_extension = system_hashed_ansi_string_create("rgb8_etc2");
    item_rgb8_etc2_ptr->gl_value       = GL_COMPRESSED_RGB8_ETC2;
    item_rgb8_etc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGB8_ETC2");

    item_rgb8_punchthrough_alpha1_etc2_ptr->file_extension = system_hashed_ansi_string_create("r11_eac");
    item_rgb8_punchthrough_alpha1_etc2_ptr->gl_value       = GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    item_rgb8_punchthrough_alpha1_etc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2");

    item_rgba8_etc2_eac_ptr->file_extension = system_hashed_ansi_string_create("rgba8_etc2");
    item_rgba8_etc2_eac_ptr->gl_value       = GL_COMPRESSED_RGBA8_ETC2_EAC;
    item_rgba8_etc2_eac_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RGBA8_ETC2_EAC");

    item_signed_r11_eac_ptr->file_extension = system_hashed_ansi_string_create("rs11_eac");
    item_signed_r11_eac_ptr->gl_value       = GL_COMPRESSED_SIGNED_R11_EAC;
    item_signed_r11_eac_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SIGNED_R11_EAC");

    item_signed_rg11_eac_ptr->file_extension = system_hashed_ansi_string_create("rgs11_eac");
    item_signed_rg11_eac_ptr->gl_value       = GL_COMPRESSED_SIGNED_RG11_EAC;
    item_signed_rg11_eac_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SIGNED_RG11_EAC");

    item_srgb8_etc2_ptr->file_extension = system_hashed_ansi_string_create("srgb8_etc2");
    item_srgb8_etc2_ptr->gl_value       = GL_COMPRESSED_SRGB8_ETC2;
    item_srgb8_etc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SRGB8_ETC2");

    item_srgb8_punchthrough_alpha1_etc2_ptr->file_extension = system_hashed_ansi_string_create("srgb8_pa1");
    item_srgb8_punchthrough_alpha1_etc2_ptr->gl_value       = GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    item_srgb8_punchthrough_alpha1_etc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2");

    item_srgb8_alpha8_etc2_eac_ptr->file_extension = system_hashed_ansi_string_create("srgb8_a8_etc2");
    item_srgb8_alpha8_etc2_eac_ptr->gl_value       = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
    item_srgb8_alpha8_etc2_eac_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC");

    /* Store the texture compression algorithms */
    _ogl_context_texture_compression_algorithm* algorithms[] =
    {
        item_r11_eac_ptr,
        item_rg11_eac_ptr,
        item_rgb8_etc2_ptr,
        item_rgb8_punchthrough_alpha1_etc2_ptr,
        item_rgba8_etc2_eac_ptr,
        item_signed_r11_eac_ptr,
        item_signed_rg11_eac_ptr,
        item_srgb8_etc2_ptr,
        item_srgb8_punchthrough_alpha1_etc2_ptr,
        item_srgb8_alpha8_etc2_eac_ptr
    };
    const uint32_t n_algorithms = sizeof(algorithms) / sizeof(algorithms[0] );

    for (uint32_t n_algorithm = 0;
                  n_algorithm < n_algorithms;
                ++n_algorithm)
    {
        if (_ogl_context_texture_compression_is_compression_supported(compressed_texture_formats,
                                                                      n_compressed_texture_formats,
                                                                      algorithms[n_algorithm]->gl_value) )
        {
            system_resizable_vector_push(texture_compression_ptr->algorithms,
                                         algorithms[n_algorithm]);

            system_hash64map_insert(texture_compression_ptr->compression_enums_map,
                                    algorithms[n_algorithm]->gl_value,
                                    algorithms[n_algorithm],
                                    nullptr,  /* on_remove_callback */
                                    nullptr); /* on_remove_callback_user_arg */
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Even though GL_ARB_ES3_compatibility is reported, compression algorithm [%x] is not reported as supported.");
        }
    }
}

/** TODO */
PRIVATE void _ogl_context_texture_compression_init_latc_support(_ogl_context_texture_compression* texture_compression_ptr)
{
    _ogl_context_texture_compression_algorithm* item_luminance_latc1_ptr              = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_signed_luminance_latc1_ptr       = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_luminance_alpha_latc2_ptr        = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_signed_luminance_alpha_latc2_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_luminance_latc1_ptr              != nullptr &&
                       item_signed_luminance_latc1_ptr       != nullptr &&
                       item_luminance_alpha_latc2_ptr        != nullptr &&
                       item_signed_luminance_alpha_latc2_ptr != nullptr,
                       "Out of memory");

    item_luminance_latc1_ptr->file_extension = system_hashed_ansi_string_create("luminance_latc1");
    item_luminance_latc1_ptr->gl_value       = GL_COMPRESSED_LUMINANCE_LATC1_EXT;
    item_luminance_latc1_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_LUMINANCE_LATC1_EXT");

    item_signed_luminance_latc1_ptr->file_extension = system_hashed_ansi_string_create("signed_luminance_latc1");
    item_signed_luminance_latc1_ptr->gl_value       = GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT;
    item_signed_luminance_latc1_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT");

    item_luminance_alpha_latc2_ptr->file_extension = system_hashed_ansi_string_create("luminance_alpha_latc2");
    item_luminance_alpha_latc2_ptr->gl_value       = GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
    item_luminance_alpha_latc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT");

    item_signed_luminance_alpha_latc2_ptr->file_extension = system_hashed_ansi_string_create("signed_luminance_alpha_latc2");
    item_signed_luminance_alpha_latc2_ptr->gl_value       = GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT;
    item_signed_luminance_alpha_latc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT");

    /* Store the texture compression algorithms if GL_ARB_texture_compression_bptc extension
     * is reported as supported (it should be)
     */
    _ogl_context_texture_compression_algorithm* algorithms[] =
    {
        item_luminance_latc1_ptr,
        item_signed_luminance_latc1_ptr,
        item_luminance_alpha_latc2_ptr,
        item_signed_luminance_alpha_latc2_ptr
    };
    const uint32_t n_algorithms = sizeof(algorithms) / sizeof(algorithms[0] );

    for (uint32_t n_algorithm = 0;
                  n_algorithm < n_algorithms;
                ++n_algorithm)
    {
        system_resizable_vector_push(texture_compression_ptr->algorithms,
                                     algorithms[n_algorithm]);

        system_hash64map_insert(texture_compression_ptr->compression_enums_map,
                                algorithms[n_algorithm]->gl_value,
                                algorithms[n_algorithm],
                                nullptr,  /* on_remove_callback */
                                nullptr); /* on_remove_callback_user_arg */
    }
}

/** TODO */
PRIVATE void _ogl_context_texture_compression_init_rgtc_support(_ogl_context_texture_compression* texture_compression_ptr,
                                                                GLenum*                           compressed_texture_formats,
                                                                uint32_t                          n_compressed_texture_formats)
{
    /* BC4 */
    _ogl_context_texture_compression_algorithm* item_red_rgtc1_ptr        = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_signed_red_rgtc1_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_red_rgtc1_ptr        != nullptr &&
                       item_signed_red_rgtc1_ptr != nullptr,
                       "Out of memory");

    item_red_rgtc1_ptr->file_extension = system_hashed_ansi_string_create("bc4_r");
    item_red_rgtc1_ptr->gl_value       = GL_COMPRESSED_RED_RGTC1;
    item_red_rgtc1_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RED_RGTC1");

    item_signed_red_rgtc1_ptr->file_extension = system_hashed_ansi_string_create("bc4_rs");
    item_signed_red_rgtc1_ptr->gl_value       = GL_COMPRESSED_SIGNED_RED_RGTC1;
    item_signed_red_rgtc1_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SIGNED_RED_RGTC1");

    /* BC5 */
    _ogl_context_texture_compression_algorithm* item_rg_rgtc2_ptr        = new (std::nothrow) _ogl_context_texture_compression_algorithm;
    _ogl_context_texture_compression_algorithm* item_signed_rg_rgtc2_ptr = new (std::nothrow) _ogl_context_texture_compression_algorithm;

    ASSERT_ALWAYS_SYNC(item_rg_rgtc2_ptr        != nullptr &&
                       item_signed_rg_rgtc2_ptr != nullptr,
                       "Out of memory");

    item_rg_rgtc2_ptr->file_extension = system_hashed_ansi_string_create("bc5_rg");
    item_rg_rgtc2_ptr->gl_value       = GL_COMPRESSED_RG_RGTC2;
    item_rg_rgtc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_RG_RGTC2");

    item_signed_rg_rgtc2_ptr->file_extension = system_hashed_ansi_string_create("bc5_rgs");
    item_signed_rg_rgtc2_ptr->gl_value       = GL_COMPRESSED_SIGNED_RG_RGTC2;
    item_signed_rg_rgtc2_ptr->name           = system_hashed_ansi_string_create("GL_COMPRESSED_SIGNED_RG_RGTC2");

    /* Store the texture compression algorithms if GL_ARB_texture_compression_rgtc extension
     * is reported as supported (it should be)
     */
    _ogl_context_texture_compression_algorithm* algorithms[] =
    {
        item_red_rgtc1_ptr,
        item_signed_red_rgtc1_ptr,
        item_rg_rgtc2_ptr,
        item_signed_rg_rgtc2_ptr
    };
    const uint32_t n_algorithms = sizeof(algorithms) / sizeof(algorithms[0] );

    if (ogl_context_is_extension_supported(texture_compression_ptr->context,
                                           system_hashed_ansi_string_create("GL_ARB_texture_compression_rgtc") ))
    {
        for (uint32_t n_algorithm = 0;
                      n_algorithm < n_algorithms;
                    ++n_algorithm)
        {
            system_resizable_vector_push(texture_compression_ptr->algorithms,
                                         algorithms[n_algorithm]);

            system_hash64map_insert(texture_compression_ptr->compression_enums_map,
                                    algorithms[n_algorithm]->gl_value,
                                    algorithms[n_algorithm],
                                    nullptr,  /* on_remove_callback */
                                    nullptr); /* on_remove_callback_user_arg */
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "GL_ARB_texture_compression_rgtc not reported for an OpenGL 4.3 context");

        for (uint32_t n_algorithm = 0;
                      n_algorithm < n_algorithms;
                    ++n_algorithm)
        {
            delete algorithms[n_algorithm];

            algorithms[n_algorithm] = nullptr;
        }
    }
}

/** TODO */
PRIVATE bool _ogl_context_texture_compression_is_compression_supported(GLenum*  compressed_texture_formats,
                                                                       uint32_t n_compressed_texture_formats,
                                                                       GLenum   internalformat)
{
    bool result = false;

    for (uint32_t n = 0;
                  n < n_compressed_texture_formats;
                ++n)
    {
        if (compressed_texture_formats[n] == internalformat)
        {
            result = true;

            break;
        }
    }

    return result;
}


/** Please see header for spec */
PUBLIC ogl_context_texture_compression ogl_context_texture_compression_create(ogl_context context)
{
    _ogl_context_texture_compression* new_instance = new (std::nothrow) _ogl_context_texture_compression;

    ASSERT_ALWAYS_SYNC(new_instance != nullptr,
                       "Out of memory");

    if (new_instance != nullptr)
    {
        new_instance->context = context;
    }

    return (ogl_context_texture_compression) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_context_texture_compression_get_algorithm_property(const ogl_context_texture_compression              texture_compression,
                                                                   uint32_t                                           index,
                                                                   ogl_context_texture_compression_algorithm_property property,
                                                                   void*                                              out_result_ptr)
{
    _ogl_context_texture_compression_algorithm* algorithm_ptr           = nullptr;
    _ogl_context_texture_compression*           texture_compression_ptr = reinterpret_cast<_ogl_context_texture_compression*>(texture_compression);

    if (system_resizable_vector_get_element_at(texture_compression_ptr->algorithms,
                                               index,
                                              &algorithm_ptr) )
    {
        switch (property)
        {
            case OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_FILE_EXTENSION:
            {
                *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = algorithm_ptr->file_extension;

                break;
            }

            case OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_GL_VALUE:
            {
                *reinterpret_cast<GLenum*>(out_result_ptr) = algorithm_ptr->gl_value;

                break;
            }

            case OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_NAME:
            {
                *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = algorithm_ptr->name;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ogl_context_texture_compression_algorithm_property value");
            }
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture compression algorithm descriptor at index [%d]",
                          index);
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_texture_compression_get_property(const ogl_context_texture_compression    texture_compression,
                                                         ogl_context_texture_compression_property property,
                                                         void*                                    out_result_ptr)
{
    _ogl_context_texture_compression* texture_compression_ptr = reinterpret_cast<_ogl_context_texture_compression*>(texture_compression);

    switch (property)
    {
        case OGL_CONTEXT_TEXTURE_COMPRESSION_PROPERTY_N_COMPRESSED_INTERNALFORMAT:
        {
            system_resizable_vector_get_property(texture_compression_ptr->algorithms,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_texture_compression_property value");
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_texture_compression_init(ogl_context_texture_compression           texture_compression,
                                                 const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_texture_compression* texture_compression_ptr = reinterpret_cast<_ogl_context_texture_compression*>(texture_compression);

    /* First, retrieve all compressed texture formats, supported by the
     * running GL implementation.
     */
    GLenum* compressed_texture_formats   = nullptr;
    GLint   n_compressed_texture_formats = 0;

    entrypoints_private_ptr->pGLGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                                           &n_compressed_texture_formats);

    ASSERT_ALWAYS_SYNC(n_compressed_texture_formats != 0,
                       "Zero compressed texture formats reported as supported");

    if (n_compressed_texture_formats != 0)
    {
        compressed_texture_formats = new (std::nothrow) GLenum[n_compressed_texture_formats];

        ASSERT_ALWAYS_SYNC(compressed_texture_formats != nullptr,
                           "Out of memory");

        if (compressed_texture_formats != nullptr)
        {
            entrypoints_private_ptr->pGLGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS,
                                                    reinterpret_cast<GLint*>(compressed_texture_formats) );

            /* OpenGL 4.3 context implies support for GL_ARB_texture_compression_bptc */
            _ogl_context_texture_compression_init_bptc_support(texture_compression_ptr,
                                                               compressed_texture_formats,
                                                               n_compressed_texture_formats);

            /* OpenGL 4.3 context implies support for GL_ARB_texture_compression_rgtc */
            _ogl_context_texture_compression_init_rgtc_support(texture_compression_ptr,
                                                               compressed_texture_formats,
                                                               n_compressed_texture_formats);

            /* GL_ARB_ES3_compatibility introduces ETC & ETC2 compression algorithms support. */
            if (ogl_context_is_extension_supported(texture_compression_ptr->context,
                                                   system_hashed_ansi_string_create("GL_ARB_ES3_compatibility") ))
            {
                _ogl_context_texture_compression_init_etc_etc2_support(texture_compression_ptr,
                                                                       compressed_texture_formats,
                                                                       n_compressed_texture_formats);
            }

            /* Add compression formats specific to GL_EXT_texture_compression_latc */
            if (ogl_context_is_extension_supported(texture_compression_ptr->context,
                                                   system_hashed_ansi_string_create("GL_EXT_texture_compression_latc") ))
            {
                _ogl_context_texture_compression_init_latc_support(texture_compression_ptr);
            }
            else
            {
                LOG_ERROR("GL_EXT_texture_compression_latc is not supported. The demo will crash if it uses LATC1/2 textures");
            }

            /* Add compression formats specific to GL_EXT_texture_compression_s3tc */
            if (ogl_context_is_extension_supported(texture_compression_ptr->context,
                                                   system_hashed_ansi_string_create("GL_EXT_texture_compression_s3tc") ))
            {
                _ogl_context_texture_compression_init_dxt1_support     (texture_compression_ptr);
                _ogl_context_texture_compression_init_dxt3_dxt5_support(texture_compression_ptr);
            }
            else
            {
                LOG_ERROR("GL_EXT_texture_compression_s3tc is not supported. The demo will crash if it uses DXT1/DXT3/DXT5 textures");
            }

            /* All done */
            delete [] compressed_texture_formats;

            compressed_texture_formats = nullptr;
        }
    }

    /* Cache info in private descriptor */
    texture_compression_ptr->entrypoints_private_ptr = entrypoints_private_ptr;
}

/** Please see header for spec */
PUBLIC void ogl_context_texture_compression_release(ogl_context_texture_compression texture_compression)
{
    _ogl_context_texture_compression* texture_compression_ptr = reinterpret_cast<_ogl_context_texture_compression*>(texture_compression);

    /* Done */
    delete texture_compression_ptr;

    texture_compression_ptr = nullptr;
}

