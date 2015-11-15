/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_image.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_bo_bindings.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_context_texture_compression.h"
#include "ogl/ogl_context_textures.h"
#include "ogl/ogl_context_to_bindings.h"
#include "ogl/ogl_context_vaos.h"
#include "ogl/ogl_context_wrappers.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_primitive_renderer.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shaders.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_text.h"
#include "raGL/raGL_buffers.h"
#include "raGL/raGL_utils.h"
#include "raGL/raGL_samplers.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_unpacker.h"
#include "system/system_global.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include <string.h>

#ifdef _WIN32
    #include "ogl/ogl_context_win32.h"

    #define ogl_context_platform ogl_context_win32
#else
    #include "ogl/ogl_context_linux.h"

    #define ogl_context_platform ogl_context_linux
#endif


typedef void  (*PFNCONTEXTBINDTOCURRENTTHREADPROC)(ogl_context_platform            context_platform);
typedef void  (*PFNCONTEXTDEINITPROC)             (ogl_context_platform            context_platform);
typedef void* (*PFNCONTEXTGETFUNCPTRPROC)         (ogl_context_platform            context_platform,
                                                   const char*                     name);
typedef bool  (*PFNCONTEXTGETPROPERTYPROC)        (ogl_context_platform            context_platform,
                                                   ogl_context_property            property,
                                                   void*                           out_result);
typedef void  (*PFNCONTEXTINITPROC)               (ogl_context                     context,
                                                   PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation);
typedef bool  (*PFNCONTEXTSETPROPERTYPROC)        (ogl_context_platform            context_win32,
                                                   ogl_context_property            property,
                                                   const void*                     data);
typedef void  (*PFNCONTEXTSWAPBUFFERSPROC)        (ogl_context_platform            context_platform);
typedef void  (*PFNUNBINDFROMCURRENTTHREADPROC)   (ogl_context_platform            context_platform);

/** Internal variables */
typedef struct
{
#ifdef _WIN32
    ogl_context_win32           context_platform;
#else
    ogl_context_linux           context_platform;
#endif

    ogl_context_type     context_type;
    bool                 is_intel_driver;
    bool                 is_nv_driver;
    uint32_t             major_version;
    uint32_t             minor_version;
    ogl_context          parent_context;
    system_pixel_format  pfd;
    bool                 vsync_enabled;
    system_window        window;
    system_window_handle window_handle;

    GLuint               vao_no_vaas_id;

    /* Used for off-screen rendering. */
    ral_texture_format fbo_color_texture_format;
    GLuint             fbo_color_rbo_id;
    ral_texture_format fbo_depth_stencil_texture_format;
    GLuint             fbo_depth_stencil_rbo_id;
    GLuint             fbo_id;
    unsigned int       fbo_n_samples_effective;
    unsigned int       fbo_size[2];

    /* Used by the root window for MSAA enumeration only */
    GLenum msaa_enumeration_color_internalformat;
    GLint  msaa_enumeration_color_n_samples;
    GLint* msaa_enumeration_color_samples;
    GLenum msaa_enumeration_depth_stencil_internalformat;
    GLint  msaa_enumeration_depth_stencil_n_samples;
    GLint* msaa_enumeration_depth_stencil_samples;

    /* Current multisampling samples setting */
    uint32_t multisampling_samples;

    ogl_context_gl_entrypoints                           entry_points_gl;
    ogl_context_gl_entrypoints_arb_buffer_storage        entry_points_gl_arb_buffer_storage;
    ogl_context_gl_entrypoints_arb_multi_bind            entry_points_gl_arb_multi_bind;
    ogl_context_gl_entrypoints_arb_sparse_buffer         entry_points_gl_arb_sparse_buffer;
    ogl_context_gl_entrypoints_ext_direct_state_access   entry_points_gl_ext_direct_state_access;
    ogl_context_gl_entrypoints_private                   entry_points_private;
    ogl_context_gl_info                                  info;
    ogl_context_gl_limits                                limits;
    ogl_context_gl_limits_arb_sparse_buffer              limits_arb_sparse_buffer;
    ogl_context_gl_limits_ext_texture_filter_anisotropic limits_ext_texture_filter_anisotropic;

    /* OpenGL ES context support: */
    ogl_context_es_entrypoints                    entry_points_es;
    ogl_context_es_entrypoints_ext_texture_buffer entry_points_es_ext_texture_buffer;

    bool es_ext_texture_buffer_support;

    bool gl_arb_buffer_storage_support;
    bool gl_arb_multi_bind_support;
    bool gl_arb_sparse_buffer_support;
    bool gl_arb_texture_buffer_object_rgb32_support;
    bool gl_ext_direct_state_access_support;
    bool gl_ext_texture_filter_anisotropic_support;

    raGL_buffers                    buffers;

    ogl_context_bo_bindings         bo_bindings;
    ogl_flyby                       flyby;
    ogl_materials                   materials;
    ogl_programs                    programs;
    ogl_primitive_renderer          primitive_renderer;
    ogl_context_sampler_bindings    sampler_bindings;
    raGL_samplers                   samplers;
    ogl_shaders                     shaders;
    ogl_shadow_mapping              shadow_mapping;
    ogl_context_state_cache         state_cache;
    ogl_text                        text_renderer;
    ogl_context_texture_compression texture_compression;
    ogl_context_textures            textures;
    ogl_context_to_bindings         to_bindings;
    ogl_context_vaos                vaos;

    PFNCONTEXTBINDTOCURRENTTHREADPROC pfn_bind_to_current_thread;
    PFNCONTEXTDEINITPROC              pfn_deinit;
    PFNCONTEXTGETFUNCPTRPROC          pfn_get_func_ptr;
    PFNCONTEXTGETPROPERTYPROC         pfn_get_property;
    PFNCONTEXTINITPROC                pfn_init;
    PFNCONTEXTSETPROPERTYPROC         pfn_set_property;
    PFNCONTEXTSWAPBUFFERSPROC         pfn_swap_buffers;
    PFNUNBINDFROMCURRENTTHREADPROC    pfn_unbind_from_current_thread;

    REFCOUNT_INSERT_VARIABLES
} _ogl_context;

struct func_ptr_table_entry
{
    void*       func_ptr;
    const char* func_name;
};


#ifdef _WIN32
    __declspec(thread) ogl_context _current_context = NULL;
#else
    __thread ogl_context _current_context = NULL;
#endif


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_context,
                               ogl_context,
                              _ogl_context);

/* Forward declarations */
PRIVATE void APIENTRY             _ogl_context_debug_message_gl_callback                             (GLenum                       source,
                                                                                                      GLenum                       type,
                                                                                                      GLuint                       id,
                                                                                                      GLenum                       severity,
                                                                                                      GLsizei                      length,
                                                                                                      const GLchar*                message,
                                                                                                      const void*                  userParam);
PRIVATE void                      _ogl_context_enumerate_msaa_samples_rendering_thread_callback      (ogl_context                  context,
                                                                                                      void*                        user_arg);
PRIVATE bool                      _ogl_context_get_attachment_internalformats_for_system_pixel_format(const system_pixel_format    pf,
                                                                                                      GLenum*                      out_color_attachment_internalformat_ptr,
                                                                                                      GLenum*                      out_depth_stencil_attachment_internalformat_ptr);
PRIVATE bool                      _ogl_context_get_color_attachment_internalformat_for_rgba_bits     (unsigned char*               n_rgba_bits,
                                                                                                      bool                         use_srgb_color_space,
                                                                                                      GLenum*                      out_gl_internalformat);
PRIVATE system_hashed_ansi_string _ogl_context_get_compressed_filename                               (void*                        user_arg,
                                                                                                      system_hashed_ansi_string    decompressed_filename,
                                                                                                      GLenum*                      out_compressed_gl_enum,
                                                                                                      system_file_unpacker*        out_file_unpacker);
PRIVATE bool                      _ogl_context_get_depth_attachment_internalformat_for_bits          (unsigned char                n_depth_bits,
                                                                                                      GLenum*                      out_gl_internalformat);
PRIVATE bool                      _ogl_context_get_depth_stencil_attachment_internalformat_for_bits  (unsigned char                n_depth_bits,
                                                                                                      unsigned char                n_stencil_bits,
                                                                                                      GLenum*                      out_gl_internalformat);
PRIVATE bool                      _ogl_context_get_function_pointers                                 (_ogl_context*                context_ptr,
                                                                                                      func_ptr_table_entry*        entries,
                                                                                                      uint32_t                     n_entries);
PRIVATE const char*               _ogl_context_get_internalformat_string                             (GLenum                       internalformat);
PRIVATE void                      _ogl_context_gl_info_deinit                                        (ogl_context_gl_info*         info_ptr);
PRIVATE void                      _ogl_context_gl_info_init                                          (ogl_context_gl_info*         info_ptr,
                                                                                                      const ogl_context_gl_limits* limits_ptr);
PRIVATE void                      _ogl_context_init_context_after_creation                           (ogl_context                  context);
PRIVATE void                      _ogl_context_initialize_es_ext_texture_buffer_extension            (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_initialize_fbo                                        (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_initialize_gl_arb_buffer_storage_extension            (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_initialize_gl_arb_multi_bind_extension                (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_initialize_gl_arb_sparse_buffer_extension             (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_initialize_gl_ext_direct_state_access_extension       (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_initialize_gl_ext_texture_filter_anisotropic_extension(_ogl_context*                context_ptr);
PRIVATE bool                      _ogl_context_is_stencil_data_included_in_internalformat            (GLenum                       internalformat);
PRIVATE void                      _ogl_context_release                                               (void*                        ptr);
PRIVATE void                      _ogl_context_retrieve_ES_function_pointers                         (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_ARB_buffer_storage_function_pointers      (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_ARB_multi_bind_function_pointers          (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_ARB_sparse_buffer_function_pointers       (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_ARB_sparse_buffer_limits                  (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_EXT_direct_state_access_function_pointers (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_EXT_texture_filter_anisotropic_limits     (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_function_pointers                         (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_info                                      (_ogl_context*                context_ptr);
PRIVATE void                      _ogl_context_retrieve_GL_limits                                    (_ogl_context*                context_ptr);
PRIVATE bool                      _ogl_context_sort_descending                                       (void*                        in_int_1,
                                                                                                      void*                        in_int_2);


/** TODO */
PRIVATE ogl_context _ogl_context_create_from_system_window_shared(system_hashed_ansi_string name,
                                                                  ogl_context_type          type,
                                                                  system_window             window,
                                                                  system_pixel_format       in_pfd,
                                                                  bool                      vsync_enabled,
                                                                  ogl_context               parent_context)
{
    /* Create the ogl_context instance. */
    _ogl_context* new_context_ptr = new (std::nothrow) _ogl_context;

    ASSERT_ALWAYS_SYNC(new_context_ptr != NULL,
                       "Out of memory when creating ogl_context instance.");

    memset(new_context_ptr,
           0,
           sizeof(*new_context_ptr) );

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_context_ptr,
                                                   _ogl_context_release,
                                                   OBJECT_TYPE_OGL_CONTEXT,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Contexts\\",
                                                   system_hashed_ansi_string_get_buffer(name))
                                                   );

    new_context_ptr->context_platform                       = NULL;
    new_context_ptr->context_type                           = type;
    new_context_ptr->fbo_color_texture_format               = RAL_TEXTURE_FORMAT_UNKNOWN;
    new_context_ptr->fbo_depth_stencil_texture_format       = RAL_TEXTURE_FORMAT_UNKNOWN;
    new_context_ptr->msaa_enumeration_color_samples         = NULL;
    new_context_ptr->msaa_enumeration_depth_stencil_samples = NULL;
    new_context_ptr->parent_context                         = parent_context;
    new_context_ptr->pfd                                    = in_pfd;
    new_context_ptr->vsync_enabled                          = vsync_enabled;
    new_context_ptr->window                                 = window;

    #ifdef _WIN32
    {
        new_context_ptr->pfn_bind_to_current_thread     = ogl_context_win32_bind_to_current_thread;
        new_context_ptr->pfn_deinit                     = ogl_context_win32_deinit;
        new_context_ptr->pfn_get_func_ptr               = ogl_context_win32_get_func_ptr;
        new_context_ptr->pfn_get_property               = ogl_context_win32_get_property;
        new_context_ptr->pfn_init                       = ogl_context_win32_init;
        new_context_ptr->pfn_set_property               = ogl_context_win32_set_property;
        new_context_ptr->pfn_swap_buffers               = ogl_context_win32_swap_buffers;
        new_context_ptr->pfn_unbind_from_current_thread = ogl_context_win32_unbind_from_current_thread;
    }
    #else
    {
        new_context_ptr->pfn_bind_to_current_thread     = ogl_context_linux_bind_to_current_thread;
        new_context_ptr->pfn_deinit                     = ogl_context_linux_deinit;
        new_context_ptr->pfn_get_func_ptr               = ogl_context_linux_get_func_ptr;
        new_context_ptr->pfn_get_property               = ogl_context_linux_get_property;
        new_context_ptr->pfn_init                       = ogl_context_linux_init;
        new_context_ptr->pfn_set_property               = ogl_context_linux_set_property;
        new_context_ptr->pfn_swap_buffers               = ogl_context_linux_swap_buffers;
        new_context_ptr->pfn_unbind_from_current_thread = ogl_context_linux_unbind_from_current_thread;
    }
    #endif

    if (new_context_ptr->parent_context != NULL)
    {
        ogl_context_retain(new_context_ptr->parent_context);
    }

    if (type == OGL_CONTEXT_TYPE_GL)
    {
        new_context_ptr->major_version = 4;
        new_context_ptr->minor_version = 3;
    }
    else
    {
        new_context_ptr->major_version = 3;
        new_context_ptr->minor_version = 1;
    }

    /* The first stage of context creation process is platform-specific. */
    new_context_ptr->pfn_init( (ogl_context) new_context_ptr,
                               _ogl_context_init_context_after_creation);

    return (ogl_context) new_context_ptr;
}

/** TODO */
PRIVATE void APIENTRY _ogl_context_debug_message_gl_callback(GLenum        source,
                                                             GLenum        type,
                                                             GLuint        id,
                                                             GLenum        severity,
                                                             GLsizei       length,
                                                             const GLchar* message,
                                                             const void*   userParam)
{
    _ogl_context* context_ptr = (_ogl_context*) userParam;
    static char   local_message[4096];

    /* 1280: shader compilation failure. (Intel & NVIDIA)
     *
     * Most importantly, thrown when switching between driver versions or vendors, when the blobs
     * become incompatible. Since shader compilation is handled by ogl_shader which by default is
     * pretty verbose, this call-back is useless for our purposes.
     */
    /* 131184: (NVIDIA)
     *
     * [Id:131184] [Source:OpenGL] Type:[Other event] Severity:[!]: Buffer info: 
     *  Total VBO memory usage in the system:
     *   memType: SYSHEAP, 0 bytes Allocated, numAllocations: 0.
     *   memType: VID, 3.91 Mb Allocated, numAllocations: 1.
     *   memType: DMA_CACHED, 0 bytes Allocated, numAllocations: 0.
     *   memType: MALLOC, 106.04 Kb Allocated, numAllocations: 16.
     *   memType: PAGED_AND_MAPPED, 3.91 Mb Allocated, numAllocations: 1.
     *   memType: PAGED, 41.16 Mb Allocated, numAllocations: 9.
     *
     */
    /* 131185: (NVIDIA)
     *
     * [Id:131185] [Source:OpenGL] Type:[Other event] Severity:[!]: Buffer detailed info: Buffer object 14 (bound to GL_TRANSFORM_FEEDBACK_BUFFER_NV (0), and GL_TRANSFORM_FEEDBACK_BUFFER_NV, usage hint is GL_STATIC_DRAW) will use VIDEO memory as the source for buffer object operations.
     *
     */
    if ( context_ptr->is_intel_driver && (id != 1280)   ||
         context_ptr->is_nv_driver    && (id != 1280    &&
                                          id != 131184  &&
                                          id != 131185) ||
        !context_ptr->is_intel_driver && !context_ptr->is_nv_driver)
    {
        /* This function is called back from within driver layer! Do not issue GL commands from here! */
        const char*   source_name   = NULL;
        const char*   type_name     = NULL;
        const char*   severity_name = NULL;

        switch(source)
        {
            case GL_DEBUG_SOURCE_API_ARB:             source_name = "OpenGL";               break;
            case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: source_name = "GLSL Shader Compiler"; break;
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:   source_name = "WGL";                  break;
            case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:     source_name = "Third party";          break;
            case GL_DEBUG_SOURCE_APPLICATION_ARB:     source_name = "Application";          break;
            case GL_DEBUG_SOURCE_OTHER_ARB:           source_name = "Other source";         break;
            default:                                  source_name = "?Unknown source?";     break;
        }

        switch (type)
        {
            case GL_DEBUG_TYPE_ERROR_ARB:               type_name = "Erroneous event";     break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: type_name = "Deprecated behavior"; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  type_name = "Undefined behavior";  break;
            case GL_DEBUG_TYPE_PERFORMANCE_ARB:         type_name = "Performance warning"; break;
            case GL_DEBUG_TYPE_PORTABILITY_ARB:         type_name = "Portability warning"; break;
            case GL_DEBUG_TYPE_OTHER_ARB:               type_name = "Other event";         break;
            default:                                    type_name = "?Unknown event type?";break;
        }

        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH_ARB:   severity_name = "!!!"; break;
            case GL_DEBUG_SEVERITY_MEDIUM_ARB: severity_name = "!!";  break;
            case GL_DEBUG_SEVERITY_LOW_ARB:    severity_name = "!";   break;
            default:                           severity_name = "?!";  break;
        }

        sprintf(local_message,
                "[Id:%u] [Source:%s] Type:[%s] Severity:[%s]: %s",
                id,
                source_name,
                type_name,
                severity_name,
                message);

        LOG_INFO("%s",
                 local_message);

#if 0
        ASSERT_DEBUG_SYNC(type != GL_DEBUG_TYPE_ERROR_ARB,
                          "GL error detected");
#endif
    }
}

/** TODO */
PRIVATE void _ogl_context_enumerate_msaa_samples_rendering_thread_callback(ogl_context context,
                                                                           void*       user_arg)
{
    _ogl_context*                context_ptr            = (_ogl_context*) context;
    PFNGLGETINTERNALFORMATIVPROC pGLGetInternalformativ = NULL;

    /* Determine all supported n_samples for the internalformat we intend to use for the color attachment */
    struct _internalformat
    {
        GLenum  internalformat;
        GLint*  n_samples_ptr;
        GLint** samples_ptr;
    } internalformats[] =
    {
        {context_ptr->msaa_enumeration_color_internalformat,         &context_ptr->msaa_enumeration_color_n_samples,         &context_ptr->msaa_enumeration_color_samples},
        {context_ptr->msaa_enumeration_depth_stencil_internalformat, &context_ptr->msaa_enumeration_depth_stencil_n_samples, &context_ptr->msaa_enumeration_depth_stencil_samples}
    };
    const unsigned int n_internalformats = sizeof(internalformats) / sizeof(internalformats[0]);

    if (context_ptr->context_type == OGL_CONTEXT_TYPE_GL)
    {
        pGLGetInternalformativ = context_ptr->entry_points_gl.pGLGetInternalformativ;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_ES,
                          "Unrecognized context type");

        pGLGetInternalformativ = context_ptr->entry_points_es.pGLGetInternalformativ;
    }

    for (unsigned int n_internalformat = 0;
                      n_internalformat < n_internalformats;
                    ++n_internalformat)
    {
        _internalformat* internalformat_ptr = internalformats + n_internalformat;

         pGLGetInternalformativ(GL_RENDERBUFFER,
                                internalformat_ptr->internalformat,
                                GL_NUM_SAMPLE_COUNTS,
                                sizeof(*internalformat_ptr->n_samples_ptr),
                                internalformat_ptr->n_samples_ptr);

         if (*internalformat_ptr->n_samples_ptr > 0)
         {
             *internalformat_ptr->samples_ptr = new GLint[*internalformat_ptr->n_samples_ptr];

             ASSERT_DEBUG_SYNC(*internalformat_ptr->samples_ptr != NULL,
                               "Out of memory");

             pGLGetInternalformativ(GL_RENDERBUFFER,
                                    internalformat_ptr->internalformat,
                                    GL_SAMPLES,
                                    sizeof(GLint) * (*internalformat_ptr->n_samples_ptr),
                                    *internalformat_ptr->samples_ptr);
         }
         else
         {
             *internalformat_ptr->samples_ptr = NULL;
         }
    } /* for (all internalformats) */
}

/** TODO */
PRIVATE bool _ogl_context_get_attachment_internalformats_for_system_pixel_format(const system_pixel_format pf,
                                                                                 GLenum*                   out_color_attachment_internalformat_ptr,
                                                                                 GLenum*                   out_depth_stencil_attachment_internalformat_ptr)
{
    GLenum        internalformat_color          = GL_NONE;
    GLenum        internalformat_depth_stencil  = GL_NONE;
    unsigned char n_color_rgba_bits[4]          = {0};
    unsigned char n_depth_bits                  = 0;
    unsigned char n_stencil_bits                = 0;
    bool          result                        = true;

    /* Retrieve the number of bits for all three attachments */
    system_pixel_format_get_property(pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS,
                                     n_color_rgba_bits + 0);
    system_pixel_format_get_property(pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS,
                                     n_color_rgba_bits + 1);
    system_pixel_format_get_property(pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS,
                                     n_color_rgba_bits + 2);
    system_pixel_format_get_property(pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS,
                                     n_color_rgba_bits + 3);
    system_pixel_format_get_property(pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                    &n_depth_bits);
    system_pixel_format_get_property(pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_STENCIL_BITS,
                                    &n_stencil_bits);

    /* Convert the pixel format into a set of GL internalformats */
    if (!_ogl_context_get_color_attachment_internalformat_for_rgba_bits(n_color_rgba_bits,
                                                                        true, /* use_srgb_color_space */
                                                                       &internalformat_color) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot convert user-specified pixel format to color internalformat");

        result = false;
        goto end;
    }

    if (n_depth_bits != 0)
    {
        if (n_stencil_bits == 0)
        {
            if (!_ogl_context_get_depth_attachment_internalformat_for_bits(n_depth_bits,
                                                                          &internalformat_depth_stencil) )
            {
                ASSERT_DEBUG_SYNC(false,
                                "Cannot convert user-specified pixel format to depth internalformat");

                result = false;
                goto end;
            }
        }
        else
        {
            if (!_ogl_context_get_depth_stencil_attachment_internalformat_for_bits(n_depth_bits,
                                                                                n_stencil_bits,
                                                                                &internalformat_depth_stencil) )
            {
                ASSERT_DEBUG_SYNC(false,
                                "Cannot convert user-specified pixel format to depth+stencil internalformat");

                result = false;
                goto end;
            }
        }
    }

end:
    if (result)
    {
        *out_color_attachment_internalformat_ptr         = internalformat_color;
        *out_depth_stencil_attachment_internalformat_ptr = internalformat_depth_stencil;
    }
    else
    {
        *out_color_attachment_internalformat_ptr         = GL_NONE;
        *out_depth_stencil_attachment_internalformat_ptr = GL_NONE;
    }

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _ogl_context_get_compressed_filename(void*                     user_arg,
                                                                       system_hashed_ansi_string decompressed_filename,
                                                                       GLenum*                   out_compressed_gl_enum,
                                                                       system_file_unpacker*     out_file_unpacker)
{
    _ogl_context*             context_ptr                  = (_ogl_context*) user_arg;
    int                       n_compressed_internalformats = 0;
    system_hashed_ansi_string result                       = NULL;

    /* Identify location where the extension starts */
    unsigned int decompressed_filename_length         = system_hashed_ansi_string_get_length(decompressed_filename);
    const char*  decompressed_filename_ptr            = system_hashed_ansi_string_get_buffer(decompressed_filename);
    const char*  decompressed_filename_last_dot_ptr   = strrchr                             (decompressed_filename_ptr,
                                                                                             '.');
    const char*  decompressed_filename_last_slash_ptr = strrchr                             (decompressed_filename_ptr,
                                                                                            '\\');
    unsigned int n_asset_paths                        = 0;
    unsigned int n_file_unpackers                     = 0;

    ASSERT_DEBUG_SYNC(decompressed_filename_last_dot_ptr != NULL,
                      "Input file name does not use any extension");

    if (decompressed_filename_last_dot_ptr == NULL)
    {
        goto end;
    }

    /* If this function was called, we are in a dire situation where the image
     * file was not found in the original directory. This usually happens when
     * a demo application is launched on a machine different from the one used
     * to create a scene blob.
     *
     * To handle these cases, Emerald:
     *
     * 1) keeps track of the directories, which hold data/scene blobs.
     * 2) keeps track of instantiated file unpackers which can hold scene files.
     *
     * We use each such directory and each such unpacker to first try to find
     * the originally named gfx_image file.
     * For each file unpacker (including the default "non-existing" one) and for
     * each such directory we check if the originally named file is embedded there.
     * If not, we iterate over all supported compressed internalformats* and check
     * if the same file BUT with a different extension is perhaps present.
     */
    ogl_context_texture_compression_get_property(context_ptr->texture_compression,
                                                 OGL_CONTEXT_TEXTURE_COMPRESSION_PROPERTY_N_COMPRESSED_INTERNALFORMAT,
                                                &n_compressed_internalformats);
    system_global_get_general_property          (SYSTEM_GLOBAL_PROPERTY_N_ASSET_PATHS,
                                                &n_asset_paths);
    system_global_get_general_property          (SYSTEM_GLOBAL_PROPERTY_N_FILE_UNPACKERS,
                                                &n_file_unpackers);

    for (unsigned int n_file_unpacker = 0;
                      n_file_unpacker < (n_file_unpackers + 1 /* non-existing file unpacker */);
                    ++n_file_unpacker)
    {
        system_file_unpacker current_file_unpacker = NULL;

        if (n_file_unpacker < n_file_unpackers)
        {
            system_global_get_indexed_property(SYSTEM_GLOBAL_PROPERTY_FILE_UNPACKER,
                                               n_file_unpacker,
                                              &current_file_unpacker);
        }

        if (current_file_unpacker != NULL)
        {
            /* For file unpackers, also try the original file name which was not found
             * in the working directory.
             */
            if (system_file_enumerator_is_file_present_in_system_file_unpacker(current_file_unpacker,
                                                                               decompressed_filename,
                                                                               true, /* use_exact_match */
                                                                               NULL) ) /* out_file_index */
            {
                *out_compressed_gl_enum = GL_NONE;
                *out_file_unpacker      = current_file_unpacker;
                 result                 = decompressed_filename;

                 break;
            }
        } /* if (current_file_unpacker != NULL) */

        for (unsigned int n_asset_path = 0;
                          n_asset_path < n_asset_paths;
                        ++n_asset_path)
        {
            /* Retrieve current asset path */
            system_hashed_ansi_string current_asset_path     = NULL;
            const char*               current_asset_path_raw = NULL;

            system_global_get_indexed_property(SYSTEM_GLOBAL_PROPERTY_ASSET_PATH,
                                               n_asset_path,
                                              &current_asset_path);

            current_asset_path_raw = system_hashed_ansi_string_get_buffer(current_asset_path);

            /* Iterate over all internalformats.
             *
             * NOTE: We're using -1 as a start value for the index as we also
             *       need to check if the originally named file is available
             *       under any of the asset paths considered.
             */
            for (int n_compressed_internalformat = -1;
                     n_compressed_internalformat < n_compressed_internalformats;
                   ++n_compressed_internalformat)
            {
                GLenum                    enum_value = GL_NONE;
                system_hashed_ansi_string extension  = NULL;

                if (n_compressed_internalformat < 0)
                {
                    enum_value = GL_NONE;
                    extension  = system_hashed_ansi_string_create(decompressed_filename_last_dot_ptr + 1);
                }
                else
                {
                    ogl_context_texture_compression_get_algorithm_property(context_ptr->texture_compression,
                                                                           n_compressed_internalformat,
                                                                           OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_GL_VALUE,
                                                                          &enum_value);
                    ogl_context_texture_compression_get_algorithm_property(context_ptr->texture_compression,
                                                                           n_compressed_internalformat,
                                                                           OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_FILE_EXTENSION,
                                                                          &extension);
                }

                /* Form a new filename */
                std::string               result_filename;
                system_hashed_ansi_string result_filename_has;

                if (n_compressed_internalformat < 0)
                {
                    system_hashed_ansi_string decompressed_filename_wo_path = NULL;

                    decompressed_filename_wo_path = system_hashed_ansi_string_create_substring(decompressed_filename_last_slash_ptr + 1,
                                                                                               0,                             /* start_offset */
                                                                                               decompressed_filename_length); /* length */

                    result_filename  = std::string(system_hashed_ansi_string_get_buffer(current_asset_path) );
                    result_filename += std::string("/");
                    result_filename += std::string(system_hashed_ansi_string_get_buffer(decompressed_filename_wo_path) );
                }
                else
                {
                    result_filename += std::string                         (decompressed_filename_ptr,
                                                                            decompressed_filename_last_dot_ptr - decompressed_filename_ptr + 1);
                    result_filename += system_hashed_ansi_string_get_buffer(extension);
                }

                result_filename_has  = system_hashed_ansi_string_create(result_filename.c_str() );

                /* Is the file available? */
                if (current_file_unpacker != NULL && system_file_enumerator_is_file_present_in_system_file_unpacker(current_file_unpacker,
                                                                                                                    result_filename_has,
                                                                                                                    true,                    /* use_exact_match */
                                                                                                                    NULL)                 || /* out_file_index */
                    current_file_unpacker == NULL && system_file_enumerator_is_file_present                        (result_filename_has) )
                {
                    result                  = result_filename_has;
                    *out_compressed_gl_enum = enum_value;
                    *out_file_unpacker      = current_file_unpacker;

                    break;
                }
            } /* for (all compressed internalformats) */
        } /* for (all asset paths) */
    } /* for (all active file unpackers) */

end:
    return result;
}

/** TODO */
PRIVATE bool _ogl_context_get_color_attachment_internalformat_for_rgba_bits(unsigned char* n_rgba_bits,
                                                                            bool           use_srgb_color_space,
                                                                            GLenum*        out_gl_internalformat)
{
    bool result = true;

    if (n_rgba_bits[0] == 0 && n_rgba_bits[1] == 0 && n_rgba_bits[2] == 0 && n_rgba_bits[3] == 0)
    {
        *out_gl_internalformat = GL_NONE;
    }
    else
    if (use_srgb_color_space)
    {
        if (n_rgba_bits[0] == 8 && n_rgba_bits[1] == 8 && n_rgba_bits[2] == 8 && n_rgba_bits[3] == 0) *out_gl_internalformat = GL_SRGB8;       else
        if (n_rgba_bits[0] == 8 && n_rgba_bits[1] == 8 && n_rgba_bits[2] == 8 && n_rgba_bits[3] == 8) *out_gl_internalformat = GL_SRGB8_ALPHA8;else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported number of RGBA bits requested");

            result = false;
        }
    } /* if (use_srgb_color_space) */
    else
    {
        if (n_rgba_bits[0] == 8 && n_rgba_bits[1] == 8 && n_rgba_bits[2] == 8 && n_rgba_bits[3] == 0) *out_gl_internalformat = GL_RGB8; else
        if (n_rgba_bits[0] == 8 && n_rgba_bits[1] == 8 && n_rgba_bits[2] == 8 && n_rgba_bits[3] == 8) *out_gl_internalformat = GL_RGBA8;else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported number of RGBA bits requested");

            result = false;
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _ogl_context_get_depth_attachment_internalformat_for_bits(unsigned char n_depth_bits,
                                                                       GLenum*       out_gl_internalformat)
{
    bool result = true;

    switch (n_depth_bits)
    {
        case 0:
        {
            *out_gl_internalformat = GL_NONE;

            break;
        }

        case 16:
        {
            *out_gl_internalformat = GL_DEPTH_COMPONENT16;

            break;
        }

        case 24:
        {
            *out_gl_internalformat = GL_DEPTH_COMPONENT24;

            break;
        }

        case 32:
        {
            *out_gl_internalformat = GL_DEPTH_COMPONENT32;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported number of bits requested for the depth attachment");

            result = false;
        }
    } /* switch (n_depth_bits) */

    return result;
}

/** TODO */
PRIVATE bool _ogl_context_get_depth_stencil_attachment_internalformat_for_bits(unsigned char n_depth_bits,
                                                                               unsigned char n_stencil_bits,
                                                                               GLenum*       out_gl_internalformat)
{
    bool result = true;

    if (n_depth_bits == 24 && n_stencil_bits == 8)
    {
        *out_gl_internalformat = GL_DEPTH24_STENCIL8;
    }
    else
    if (n_depth_bits == 32 && n_stencil_bits == 8)
    {
        *out_gl_internalformat = GL_DEPTH32F_STENCIL8;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unsupported combination of number of depth & stencil attachment bits requetsed");

        result = false;
    }

    return result;
}

/** TODO */
PRIVATE bool _ogl_context_get_function_pointers(_ogl_context*         context_ptr,
                                                func_ptr_table_entry* entries,
                                                uint32_t              n_entries)
{
    bool result = true;

    for (uint32_t n_entry = 0;
                  n_entry < n_entries;
                ++n_entry)
    {
        GLvoid** ptr_to_update = (GLvoid**) entries[n_entry].func_ptr;
        GLchar*  func_name     = (GLchar*)  entries[n_entry].func_name;

        *ptr_to_update = context_ptr->pfn_get_func_ptr(context_ptr->context_platform,
                                                       func_name);

        if (*ptr_to_update == NULL)
        {
            LOG_ERROR("Could not retrieve function pointer to %s - update your drivers, OpenGL 4.2 is required to run.",
                      func_name);

            ASSERT_ALWAYS_SYNC(false,
                               "Function import error");

            result = false;
        }
    }

    return result;
}

/** TODO */
PRIVATE const char* _ogl_context_get_internalformat_string(GLenum internalformat)
{
    const char* result = "?!";

    switch (internalformat)
    {
        case GL_DEPTH_COMPONENT16: result = "GL_DEPTH_COMPONENT16"; break;
        case GL_DEPTH_COMPONENT24: result = "GL_DEPTH_COMPONENT24"; break;
        case GL_DEPTH_COMPONENT32: result = "GL_DEPTH_COMPONENT32"; break;
        case GL_DEPTH24_STENCIL8:  result = "GL_DEPTH24_STENCIL8";  break;
        case GL_DEPTH32F_STENCIL8: result = "GL_DEPTH32F_STENCIL8"; break;
        case GL_NONE:              result = "GL_NONE";              break;
        case GL_RGB8:              result = "GL_RGB8";              break;
        case GL_RGBA8:             result = "GL_RGBA8";             break;
        case GL_SRGB8:             result = "GL_SRGB8";             break;
        case GL_SRGB8_ALPHA8:      result = "GL_SRGB8_ALPHA8";      break;
    } /* switch (internalformat) */

    return result;
}

/** TODO */
PRIVATE void _ogl_context_gl_info_deinit(ogl_context_gl_info* info_ptr)
{
    ASSERT_DEBUG_SYNC(info_ptr != NULL,
                      "Input argument is NULL");

    if (info_ptr != NULL)
    {
        delete [] info_ptr->extensions;
        info_ptr->extensions = NULL;

        info_ptr->renderer                 = NULL;
        info_ptr->shading_language_version = NULL;
        info_ptr->vendor                   = NULL;
        info_ptr->version                  = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_context_gl_info_init(      ogl_context_gl_info*   info_ptr,
                                       const ogl_context_gl_limits* limits_ptr)
{
    ASSERT_DEBUG_SYNC(info_ptr   != NULL &&
                      limits_ptr != NULL,
                      "Input arguments are NULL.");

    if (info_ptr   != NULL &&
        limits_ptr != NULL)
    {
        info_ptr->extensions = new (std::nothrow) const GLubyte*[limits_ptr->num_extensions];

        ASSERT_ALWAYS_SYNC(info_ptr->extensions != NULL, 
                           "Out of memory while allocating extension name array (%d entries)", 
                           limits_ptr->num_extensions);
    }
}

/** TODO */
PRIVATE void _ogl_context_init_context_after_creation(ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    ASSERT_DEBUG_SYNC(context_ptr != NULL,
                      "Input argument is NULL");

    memset(&context_ptr->limits,
           0,
           sizeof(context_ptr->limits) );

    context_ptr->bo_bindings                                = NULL;
    context_ptr->buffers                                    = NULL;
    context_ptr->es_ext_texture_buffer_support              = false;
    context_ptr->fbo_color_rbo_id                           = 0;
    context_ptr->fbo_depth_stencil_rbo_id                   = 0;
    context_ptr->fbo_id                                     = 0;
    context_ptr->fbo_n_samples_effective                    = 0;
    context_ptr->flyby                                      = NULL;
    context_ptr->gl_arb_buffer_storage_support              = false;
    context_ptr->gl_arb_multi_bind_support                  = false;
    context_ptr->gl_arb_sparse_buffer_support               = false;
    context_ptr->gl_arb_texture_buffer_object_rgb32_support = false;
    context_ptr->gl_ext_direct_state_access_support         = false;
    context_ptr->gl_ext_texture_filter_anisotropic_support  = false;
    context_ptr->is_intel_driver                            = false; /* determined later */
    context_ptr->is_nv_driver                               = false; /* determined later */
    context_ptr->primitive_renderer                         = NULL;
    context_ptr->materials                                  = NULL; /* deferred till first query time */
    context_ptr->multisampling_samples                      = 0;
    context_ptr->programs                                   = NULL;
    context_ptr->sampler_bindings                           = NULL;
    context_ptr->state_cache                                = NULL;
    context_ptr->samplers                                   = raGL_samplers_create( (ogl_context) context_ptr);
    context_ptr->shaders                                    = ogl_shaders_create  ();
    context_ptr->shadow_mapping                             = NULL;
    context_ptr->text_renderer                              = NULL;
    context_ptr->texture_compression                        = NULL;
    context_ptr->textures                                   = ogl_context_textures_create( (ogl_context) context_ptr);
    context_ptr->to_bindings                                = NULL;
    context_ptr->vao_no_vaas_id                             = 0;

    /* Set up program manager. If there is a parent context, use its manager instance.
     * Otherwise, spawn a new one */
    if (context_ptr->parent_context != NULL)
    {
        ogl_context_get_property(context_ptr->parent_context,
                                 OGL_CONTEXT_PROPERTY_PROGRAMS,
                                &context_ptr->programs);

        ogl_programs_retain(context_ptr->programs);
    }
    else
    {
        context_ptr->programs = ogl_programs_create();
    }

    #ifdef _WIN32
    {
        /* Set up vertical sync. */
        context_ptr->pfn_set_property(context_ptr->context_platform,
                                    OGL_CONTEXT_PROPERTY_VSYNC_ENABLED,
                                    &context_ptr->vsync_enabled);
    }
    #endif

    /* Bind the context to the running thread for a few seconds.. */
    ogl_context_unbind_from_current_thread( (ogl_context) context_ptr);
    ogl_context_bind_to_current_thread    ( (ogl_context) context_ptr);

    /* OpenGL ES support is supposed to be low-level. For OpenGL, we use additional tools like
     * state caching to improve rendering efficiency.
     */
    if (context_ptr->context_type == OGL_CONTEXT_TYPE_ES)
    {
        _ogl_context_retrieve_ES_function_pointers(context_ptr);

        /* Retrieve GL context info */
        _ogl_context_retrieve_GL_info(context_ptr);

        /* If GL_EXT_texture_buffer is supported, initialize func pointers */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                               system_hashed_ansi_string_create("GL_EXT_texture_buffer") ))
        {
            _ogl_context_initialize_es_ext_texture_buffer_extension(context_ptr);
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        /* Retrieve function pointers for the context */
        _ogl_context_retrieve_GL_function_pointers(context_ptr);

        /* Retrieve GL context limitations */
        _ogl_context_retrieve_GL_limits(context_ptr);

        /* Retrieve GL context info */
        _ogl_context_retrieve_GL_info(context_ptr);

        /* Initialize state caching mechanisms */
        context_ptr->bo_bindings         = ogl_context_bo_bindings_create        ( (ogl_context) context_ptr);
        context_ptr->sampler_bindings    = ogl_context_sampler_bindings_create   ( (ogl_context) context_ptr);
        context_ptr->state_cache         = ogl_context_state_cache_create        ( (ogl_context) context_ptr);
        context_ptr->texture_compression = ogl_context_texture_compression_create( (ogl_context) context_ptr);
        context_ptr->to_bindings         = ogl_context_to_bindings_create        ( (ogl_context) context_ptr);
        context_ptr->vaos                = ogl_context_vaos_create               ( (ogl_context) context_ptr);

        /* If GL_ARB_bufffer_storage is supported, initialize func pointers */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                               system_hashed_ansi_string_create("GL_ARB_buffer_storage") ))
        {
            _ogl_context_initialize_gl_arb_buffer_storage_extension(context_ptr);
        }

        /* If GL_ARB_multi_bind is supported, initialize func pointers */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                               system_hashed_ansi_string_create("GL_ARB_multi_bind") ))
        {
            _ogl_context_initialize_gl_arb_multi_bind_extension(context_ptr);
        }

        /* If GL_ARB_sparse_buffer is supported, initialize func pointers and info variables */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                               system_hashed_ansi_string_create("GL_ARB_sparse_buffer") ))
        {
            _ogl_context_initialize_gl_arb_sparse_buffer_extension(context_ptr);
        }

        /* Try to initialize texture buffer object RGB32 extension */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                               system_hashed_ansi_string_create("GL_ARB_texture_buffer_object_rgb32") ))
        {
            context_ptr->gl_arb_texture_buffer_object_rgb32_support = true;
        }

        /* Try to initialize DSA func ptrs */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                               system_hashed_ansi_string_create("GL_EXT_direct_state_access") ))
        {
            _ogl_context_initialize_gl_ext_direct_state_access_extension(context_ptr);
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Direct State Access OpenGL extension is unavailable - the demo is very likely to crash");
        }

        /* If GL_EXT_texture_filter_anisotropic is supported, initialize info variables */
        if (ogl_context_is_extension_supported( (ogl_context) context_ptr,
                                                system_hashed_ansi_string_create("GL_EXT_texture_filter_anisotropic") ))
        {
            context_ptr->gl_ext_texture_filter_anisotropic_support = true;

            _ogl_context_initialize_gl_ext_texture_filter_anisotropic_extension(context_ptr);
        }

        /* Initialize debug output func ptrs */
        #ifdef _DEBUG
        {
            /* Debug build! Go ahead and regitser for callbacks */
            context_ptr->entry_points_gl.pGLDebugMessageCallback(_ogl_context_debug_message_gl_callback,
                                                                 context_ptr);
            context_ptr->entry_points_gl.pGLDebugMessageControl (GL_DONT_CARE,
                                                                 GL_DONT_CARE,
                                                                 GL_DONT_CARE,
                                                                 0,
                                                                 NULL,
                                                                 true);

            context_ptr->entry_points_private.pGLEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }
        #endif

        /* Set up cache storage */
        ogl_context_bo_bindings_init        (context_ptr->bo_bindings,
                                            &context_ptr->entry_points_private);
        ogl_context_sampler_bindings_init   (context_ptr->sampler_bindings,
                                            &context_ptr->entry_points_private);
        ogl_context_state_cache_init        (context_ptr->state_cache,
                                            &context_ptr->entry_points_private);
        ogl_context_texture_compression_init(context_ptr->texture_compression,
                                            &context_ptr->entry_points_private);
        ogl_context_to_bindings_init        (context_ptr->to_bindings,
                                            &context_ptr->entry_points_private);

        /* Set up the zero-VAA VAO */
        context_ptr->entry_points_gl.pGLGenVertexArrays(1,
                                                       &context_ptr->vao_no_vaas_id);

        /* Set up the context-wide flyby */
        context_ptr->flyby = ogl_flyby_create( (ogl_context) context_ptr);
    } /* if (type == OGL_CONTEXT_TYPE_GL) */

    /* Set up the buffer object manager */
    if (context_ptr->parent_context == NULL)
    {
        context_ptr->buffers = raGL_buffers_create((ogl_context) context_ptr);
    }
    else
    {
        context_ptr->buffers = ((_ogl_context*) context_ptr->parent_context)->buffers;

        ASSERT_DEBUG_SYNC(context_ptr->buffers != NULL,
                          "Parent context has a NULL buffer manager");

        raGL_buffers_retain(context_ptr->buffers);
    }

    /* Update gfx_image alternative file getter provider so that it can
     * search for compressed textures supported by this rendering context. */
    gfx_image_set_global_property(GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_FUNC_PTR,
                                  (void*) _ogl_context_get_compressed_filename);
    gfx_image_set_global_property(GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_USER_ARG,
                                  context_ptr);

    /* Log GPU info */
    LOG_INFO("Renderer:                 [%s]\n"
             "Shading language version: [%s]\n"
             "Vendor:                   [%s]\n"
             "Version:                  [%s]\n",
             context_ptr->info.renderer,
             context_ptr->info.shading_language_version,
             context_ptr->info.vendor,
             context_ptr->info.version);

    /* Is this an Intel driver? */
    if (strstr((const char*) context_ptr->info.vendor,
               "Intel") != NULL)
    {
        context_ptr->is_intel_driver = true;
    }

    /* Is this a NV driver? */
    if (strstr((const char*) context_ptr->info.vendor,
               "NV") != NULL)
    {
        context_ptr->is_nv_driver = true;
    }

    /* Initialize the FBO we will use to render contents, to be later blitted into the back buffer */
    _ogl_context_initialize_fbo(context_ptr);

    #ifdef __linux
    {
        /* Set up vertical sync. */
        context_ptr->pfn_set_property(context_ptr->context_platform,
                                    OGL_CONTEXT_PROPERTY_VSYNC_ENABLED,
                                    &context_ptr->vsync_enabled);
    }
    #endif

    /* Unbind the thread from the context. It is to be picked up by rendering thread */
    ogl_context_unbind_from_current_thread( (ogl_context) context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_es_ext_texture_buffer_extension(_ogl_context* context_ptr)
{
    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_es_ext_texture_buffer.pGLTexBufferEXT,      "glTexBufferEXT"},
        {&context_ptr->entry_points_es_ext_texture_buffer.pGLTexBufferRangeEXT, "glTexBufferRangeEXT"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->es_ext_texture_buffer_support = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_initialize_fbo(_ogl_context* context_ptr)
{
    GLenum                                  fbo_completeness_status                       = GL_NONE;
    GLenum                                  internalformat_color                          = GL_NONE;
    GLenum                                  internalformat_depth_stencil                  = GL_NONE;
    bool                                    internalformat_depth_stencil_includes_stencil = false;
    unsigned int                            n_samples                                     = 0;
    unsigned int                            n_samples_effective                           = 0;
    system_pixel_format                     pixel_format                                  = NULL;
    PFNGLBINDFRAMEBUFFERPROC                pGLBindFramebuffer                            = NULL;
    PFNGLBINDRENDERBUFFERPROC               pGLBindRenderbuffer                           = NULL;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC         pGLCheckFramebufferStatus                     = NULL;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC        pGLFramebufferRenderbuffer                    = NULL;
    PFNGLGENFRAMEBUFFERSPROC                pGLGenFramebuffers                            = NULL;
    PFNGLGENRENDERBUFFERSPROC               pGLGenRenderbuffers                           = NULL;
    PFNGLGETRENDERBUFFERPARAMETERIVPROC     pGLGetRenderbufferParameteriv                 = NULL;
    PFNGLRENDERBUFFERSTORAGEPROC            pGLRenderbufferStorage                        = NULL;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC pGLRenderbufferStorageMultisample             = NULL;
    int                                     window_dimensions[2]                          = {0};

    struct _rbo
    {
        GLenum internalformat;
        bool   is_color_attachment;
        bool   is_depth_attachment;
        bool   is_stencil_attachment;
        GLuint rbo_id;
    } rbos[2];

    if (context_ptr->context_type == OGL_CONTEXT_TYPE_ES)
    {
        pGLBindFramebuffer                = context_ptr->entry_points_es.pGLBindFramebuffer;
        pGLBindRenderbuffer               = context_ptr->entry_points_es.pGLBindRenderbuffer;
        pGLCheckFramebufferStatus         = context_ptr->entry_points_es.pGLCheckFramebufferStatus;
        pGLFramebufferRenderbuffer        = context_ptr->entry_points_es.pGLFramebufferRenderbuffer;
        pGLGenFramebuffers                = context_ptr->entry_points_es.pGLGenFramebuffers;
        pGLGenRenderbuffers               = context_ptr->entry_points_es.pGLGenRenderbuffers;
        pGLGetRenderbufferParameteriv     = context_ptr->entry_points_es.pGLGetRenderbufferParameteriv;
        pGLRenderbufferStorage            = context_ptr->entry_points_es.pGLRenderbufferStorage;
        pGLRenderbufferStorageMultisample = context_ptr->entry_points_es.pGLRenderbufferStorageMultisample;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        pGLBindFramebuffer                = context_ptr->entry_points_gl.pGLBindFramebuffer;
        pGLBindRenderbuffer               = context_ptr->entry_points_gl.pGLBindRenderbuffer;
        pGLCheckFramebufferStatus         = context_ptr->entry_points_gl.pGLCheckFramebufferStatus;
        pGLFramebufferRenderbuffer        = context_ptr->entry_points_gl.pGLFramebufferRenderbuffer;
        pGLGenFramebuffers                = context_ptr->entry_points_gl.pGLGenFramebuffers;
        pGLGenRenderbuffers               = context_ptr->entry_points_gl.pGLGenRenderbuffers;
        pGLGetRenderbufferParameteriv     = context_ptr->entry_points_gl.pGLGetRenderbufferParameteriv;
        pGLRenderbufferStorage            = context_ptr->entry_points_gl.pGLRenderbufferStorage;
        pGLRenderbufferStorageMultisample = context_ptr->entry_points_gl.pGLRenderbufferStorageMultisample;
    }

    /* Determine what internalformats we need to use for the color, depth and stencil attachments.
     * While on it, also extract the number of samples the caller wants us to use.
     *
     * Root window may request a pixel format with 0 bits for any of the attachments. Pay extra attention
     * to detect this case. If we encounter it, do not initialize the FBO at all.
     *
     * system_pixel_format may also report SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES for the number
     * of samples to be used for attachments. If that is the case, we need to check with the implementation,
     * what value we should be using.
     */
    system_window_get_property(context_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &pixel_format);
    system_window_get_property(context_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                              &window_dimensions);

    ASSERT_DEBUG_SYNC(window_dimensions[0] > 0 &&
                      window_dimensions[1] > 0,
                      "Window's width and/or height is reported as 0");
    ASSERT_DEBUG_SYNC(pixel_format != NULL,
                      "No pixel format associated with the rendering window");

    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_N_SAMPLES,
                                    &n_samples);

    if (n_samples == SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES)
    {
        unsigned int  reported_n_samples = 0;
        unsigned int* reported_samples   = NULL;

        ogl_context_enumerate_msaa_samples(pixel_format,
                                          &reported_n_samples,
                                          &reported_samples);

        ASSERT_ALWAYS_SYNC(reported_n_samples >= 1,
                           "No multisampling support for the requested rendering context FBO configuration!");

        /* The values are sorted in a descending order so we need to take the first value and drop the others */
        if (reported_n_samples < 1)
        {
            /* Disable multisampling once and for all. */
            n_samples = 1;
        }
        else
        {
            n_samples = reported_samples[0];
        }

        if (reported_samples != NULL)
        {
            delete [] reported_samples;

            reported_samples = NULL;
        }
    } /* if (n_samples == SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES) */

    if (!_ogl_context_get_attachment_internalformats_for_system_pixel_format(pixel_format,
                                                                            &internalformat_color,
                                                                            &internalformat_depth_stencil) )
    {
        ASSERT_DEBUG_SYNC(false,
                         "Cannot determine color/depth/stencil attachment internalformats for the window's pixel format");

        goto end;
    }

    if (internalformat_color         != GL_NONE ||
        internalformat_depth_stencil != GL_NONE)
    {
        ASSERT_DEBUG_SYNC(n_samples >= 1,
                        "Invalid number of samples requested");

        internalformat_depth_stencil_includes_stencil = _ogl_context_is_stencil_data_included_in_internalformat(internalformat_depth_stencil);

        /* Generate the GL objects */
        pGLGenFramebuffers(1,
                          &context_ptr->fbo_id);
        pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                          context_ptr->fbo_id);

        if (internalformat_color != GL_NONE)
        {
            pGLGenRenderbuffers(1,
                               &context_ptr->fbo_color_rbo_id);
        }

        if (internalformat_depth_stencil != GL_NONE)
        {
            pGLGenRenderbuffers(1,
                               &context_ptr->fbo_depth_stencil_rbo_id);
        }

        /* Set up the framebuffer object attachments */
        rbos[0].internalformat        = internalformat_color;
        rbos[0].is_color_attachment   = true;
        rbos[0].is_depth_attachment   = false;
        rbos[0].is_stencil_attachment = false;
        rbos[0].rbo_id                = context_ptr->fbo_color_rbo_id;

        rbos[1].internalformat        = internalformat_depth_stencil;
        rbos[1].is_color_attachment   = false;
        rbos[1].is_depth_attachment   = true;
        rbos[1].is_stencil_attachment = internalformat_depth_stencil_includes_stencil;
        rbos[1].rbo_id                = context_ptr->fbo_depth_stencil_rbo_id;

        for (unsigned int n_rbo = 0;
                          n_rbo < sizeof(rbos) / sizeof(rbos[0]);
                        ++n_rbo)
        {
            const _rbo& current_rbo           = rbos[n_rbo];
            GLint       current_rbo_n_samples = 0;

            if (current_rbo.rbo_id != 0)
            {
                pGLBindRenderbuffer(GL_RENDERBUFFER,
                                    current_rbo.rbo_id);

                /* Apparently NV driver does not re-route *Multisample() calls to the
                * single-sample entry-point if n_samples == 1, so we need to fork manually.
                */
                if (n_samples == 1)
                {
                    pGLRenderbufferStorage(GL_RENDERBUFFER,
                                          current_rbo.internalformat,
                                          window_dimensions[0],
                                          window_dimensions[1]);
                }
                else
                {
                    pGLRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                                      n_samples,
                                                      current_rbo.internalformat,
                                                      window_dimensions[0],
                                                      window_dimensions[1]);
                }

                /* Make sure the effective number of samples used for the renderbuffer
                 * matches the other attachments we're using.
                 *
                 * TODO: The whole process should be more intelligent and query impl's caps
                 *       with glGetInternalformativ() or something before firing glRenderbufferStorage*()
                 *       calls. Might backfire one day but until then!
                 */
                pGLGetRenderbufferParameteriv(GL_RENDERBUFFER,
                                              GL_RENDERBUFFER_SAMPLES,
                                             &current_rbo_n_samples);

                if (n_samples_effective != 0)
                {
                    ASSERT_ALWAYS_SYNC(n_samples_effective == current_rbo_n_samples,
                                      "Default FBO attachment's n_samples mismatch detected!");
                }
                else
                {
                    n_samples_effective = current_rbo_n_samples;
                }
                /* Bind the RBO to relevant FBO attachments */
                if (current_rbo.is_color_attachment)
                {
                    /* Default FBO only uses zeroth color attachment */
                    pGLFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
                                               GL_COLOR_ATTACHMENT0,
                                               GL_RENDERBUFFER,
                                               current_rbo.rbo_id);
                }

                if (current_rbo.is_depth_attachment)
                {
                    pGLFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
                                               GL_DEPTH_ATTACHMENT,
                                               GL_RENDERBUFFER,
                                               current_rbo.rbo_id);
                }

                if (current_rbo.is_stencil_attachment)
                {
                    pGLFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,
                                               GL_STENCIL_ATTACHMENT,
                                               GL_RENDERBUFFER,
                                               current_rbo.rbo_id);
                }
            } /* if (current_rbo.rbo_id != 0) */
        } /* for (all RBOs) */

        context_ptr->fbo_n_samples_effective = n_samples_effective;

        /* Make sure the FBO is complete. Since this is a platform-specific property,
        * we need to terminate if the driver reports it cannot support the requested
        * configuration.
        */
        fbo_completeness_status = pGLCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);

        if (fbo_completeness_status != GL_FRAMEBUFFER_COMPLETE)
        {
            LOG_FATAL("Video driver does not support rendering context's target FBO. Cannot proceed.");

            ASSERT_ALWAYS_SYNC(false,
                               "No hardware support for requested renderer's configuration");
        }
    } /* if (any of the attachments need to have a physical backing) */

    /* Store the framebuffer size */
    context_ptr->fbo_size[0] = window_dimensions[0];
    context_ptr->fbo_size[1] = window_dimensions[1];

    /* Store the attachment texture formats. */
    context_ptr->fbo_color_texture_format         = raGL_utils_get_ral_texture_format_for_ogl_enum(internalformat_color);
    context_ptr->fbo_depth_stencil_texture_format = raGL_utils_get_ral_texture_format_for_ogl_enum(internalformat_depth_stencil);

    /* Log the context's FBO configuration. Could be useful for debugging in the future. */
    LOG_INFO("Using the following FBO configuration for the rendering context:\n"
             "* Color RBO:         [%s]\n"
             "* Depth RBO:         [%s]\n"
             "* Number of samples: [%u] (effective:[%u])\n",
             _ogl_context_get_internalformat_string(internalformat_color),
             _ogl_context_get_internalformat_string(internalformat_depth_stencil),
             n_samples,
             n_samples_effective);
end:
    ;
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_buffer_storage_extension(_ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_buffer_storage_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_multi_bind_extension(_ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_multi_bind_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_sparse_buffer_extension(_ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_sparse_buffer_function_pointers(context_ptr);
    _ogl_context_retrieve_GL_ARB_sparse_buffer_limits           (context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_ext_direct_state_access_extension(_ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_EXT_direct_state_access_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_ext_texture_filter_anisotropic_extension(_ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_EXT_texture_filter_anisotropic_limits(context_ptr);
}

/** TODO */
PRIVATE bool _ogl_context_is_stencil_data_included_in_internalformat(GLenum internalformat)
{
    return (internalformat == GL_DEPTH24_STENCIL8  ||
            internalformat == GL_DEPTH32F_STENCIL8);
}

/** Function called back when reference counter drops to zero. Releases WGL rendering context.
 *
 *  @param ptr Pointer to _ogl_context instance.
 **/
PRIVATE void _ogl_context_release(void* ptr)
{
    _ogl_context* context_ptr = (_ogl_context*) ptr;

    /* NOTE: This leaks the no-VAA VAO, but it's not much damage, whereas entering
     *       a rendering context from this method could be tricky under some
     *       circumstances.
     *
     * NOTE: We also leak the context's render-target and the FBO itself. Still, GL
     *       requires the driver release these assets at context termination time,
     *       so hopefully we can live with such atrocity.
     */

    /* Release arrays allocated for "limits" storage */
    if (context_ptr->limits.program_binary_formats != NULL)
    {
        delete [] context_ptr->limits.program_binary_formats;

        context_ptr->limits.program_binary_formats = NULL;
    }

    /* Release the parent context */
    if (context_ptr->parent_context != NULL)
    {
        ogl_context_release(context_ptr->parent_context);

        context_ptr->parent_context = NULL;
    }

    ogl_context_release_managers( (ogl_context) ptr);
    _ogl_context_gl_info_deinit (&context_ptr->info);

    /* Release the platform-specific bits */
    if (context_ptr->context_platform != NULL)
    {
        context_ptr->pfn_deinit(context_ptr->context_platform);

        context_ptr->context_platform = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_ES_function_pointers(_ogl_context* context_ptr)
{
    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_es.pGLActiveShaderProgram,                 "glActiveShaderProgram"},
        {&context_ptr->entry_points_es.pGLActiveTexture,                       "glActiveTexture"},
        {&context_ptr->entry_points_es.pGLAttachShader,                        "glAttachShader"},
        {&context_ptr->entry_points_es.pGLBeginQuery,                          "glBeginQuery"},
        {&context_ptr->entry_points_es.pGLBeginTransformFeedback,              "glBeginTransformFeedback"},
        {&context_ptr->entry_points_es.pGLBindAttribLocation,                  "glBindAttribLocation"},
        {&context_ptr->entry_points_es.pGLBindBuffer,                          "glBindBuffer"},
        {&context_ptr->entry_points_es.pGLBindBufferBase,                      "glBindBufferBase"},
        {&context_ptr->entry_points_es.pGLBindBufferRange,                     "glBindBufferRange"},
        {&context_ptr->entry_points_es.pGLBindFramebuffer,                     "glBindFramebuffer"},
        {&context_ptr->entry_points_es.pGLBindImageTexture,                    "glBindImageTexture"},
        {&context_ptr->entry_points_es.pGLBindProgramPipeline,                 "glBindProgramPipeline"},
        {&context_ptr->entry_points_es.pGLBindRenderbuffer,                    "glBindRenderbuffer"},
        {&context_ptr->entry_points_es.pGLBindSampler,                         "glBindSampler"},
        {&context_ptr->entry_points_es.pGLBindTexture,                         "glBindTexture"},
        {&context_ptr->entry_points_es.pGLBindTransformFeedback,               "glBindTransformFeedback"},
        {&context_ptr->entry_points_es.pGLBindVertexArray,                     "glBindVertexArray"},
        {&context_ptr->entry_points_es.pGLBindVertexBuffer,                    "glBindVertexBuffer"},
        {&context_ptr->entry_points_es.pGLBlendColor,                          "glBlendColor"},
        {&context_ptr->entry_points_es.pGLBlendEquation,                       "glBlendEquation"},
        {&context_ptr->entry_points_es.pGLBlendEquationSeparate,               "glBlendEquationSeparate"},
        {&context_ptr->entry_points_es.pGLBlendFunc,                           "glBlendFunc"},
        {&context_ptr->entry_points_es.pGLBlendFuncSeparate,                   "glBlendFuncSeparate"},
        {&context_ptr->entry_points_es.pGLBlitFramebuffer,                     "glBlitFramebuffer"},
        {&context_ptr->entry_points_es.pGLBufferData,                          "glBufferData"},
        {&context_ptr->entry_points_es.pGLBufferSubData,                       "glBufferSubData"},
        {&context_ptr->entry_points_es.pGLCheckFramebufferStatus,              "glCheckFramebufferStatus"},
        {&context_ptr->entry_points_es.pGLClear,                               "glClear"},
        {&context_ptr->entry_points_es.pGLClearBufferfi,                       "glClearBufferfi"},
        {&context_ptr->entry_points_es.pGLClearBufferfv,                       "glClearBufferfv"},
        {&context_ptr->entry_points_es.pGLClearBufferiv,                       "glClearBufferiv"},
        {&context_ptr->entry_points_es.pGLClearBufferuiv,                      "glClearBufferuiv"},
        {&context_ptr->entry_points_es.pGLClearColor,                          "glClearColor"},
        {&context_ptr->entry_points_es.pGLClearDepthf,                         "glClearDepthf"},
        {&context_ptr->entry_points_es.pGLClearStencil,                        "glClearStencil"},
        {&context_ptr->entry_points_es.pGLClientWaitSync,                      "glClientWaitSync"},
        {&context_ptr->entry_points_es.pGLColorMask,                           "glColorMask"},
        {&context_ptr->entry_points_es.pGLCompileShader,                       "glCompileShader"},
        {&context_ptr->entry_points_es.pGLCompressedTexSubImage2D,             "glCompressedTexSubImage2D"},
        {&context_ptr->entry_points_es.pGLCompressedTexSubImage3D,             "glCompressedTexSubImage3D"},
        {&context_ptr->entry_points_es.pGLCopyBufferSubData,                   "glCopyBufferSubData"},
        {&context_ptr->entry_points_es.pGLCopyTexImage2D,                      "glCopyTexImage2D"},
        {&context_ptr->entry_points_es.pGLCopyTexSubImage2D,                   "glCopyTexSubImage2D"},
        {&context_ptr->entry_points_es.pGLCopyTexSubImage3D,                   "glCopyTexSubImage3D"},
        {&context_ptr->entry_points_es.pGLCreateProgram,                       "glCreateProgram"},
        {&context_ptr->entry_points_es.pGLCreateShader,                        "glCreateShader"},
        {&context_ptr->entry_points_es.pGLCreateShaderProgramv,                "glCreateShaderProgramv"},
        {&context_ptr->entry_points_es.pGLCullFace,                            "glCullFace"},
        {&context_ptr->entry_points_es.pGLDeleteBuffers,                       "glDeleteBuffers"},
        {&context_ptr->entry_points_es.pGLDeleteFramebuffers,                  "glDeleteFramebuffers"},
        {&context_ptr->entry_points_es.pGLDeleteProgram,                       "glDeleteProgram"},
        {&context_ptr->entry_points_es.pGLDeleteProgramPipelines,              "glDeleteProgramPipelines"},
        {&context_ptr->entry_points_es.pGLDeleteQueries,                       "glDeleteQueries"},
        {&context_ptr->entry_points_es.pGLDeleteRenderbuffers,                 "glDeleteRenderbuffers"},
        {&context_ptr->entry_points_es.pGLDeleteSamplers,                      "glDeleteSamplers"},
        {&context_ptr->entry_points_es.pGLDeleteShader,                        "glDeleteShader"},
        {&context_ptr->entry_points_es.pGLDeleteSync,                          "glDeleteSync"},
        {&context_ptr->entry_points_es.pGLDeleteTextures,                      "glDeleteTextures"},
        {&context_ptr->entry_points_es.pGLDeleteTransformFeedbacks,            "glDeleteTransformFeedbacks"},
        {&context_ptr->entry_points_es.pGLDeleteVertexArrays,                  "glDeleteVertexArrays"},
        {&context_ptr->entry_points_es.pGLDepthFunc,                           "glDepthFunc"},
        {&context_ptr->entry_points_es.pGLDepthMask,                           "glDepthMask"},
        {&context_ptr->entry_points_es.pGLDepthRangef,                         "glDepthRangef"},
        {&context_ptr->entry_points_es.pGLDetachShader,                        "glDetachShader"},
        {&context_ptr->entry_points_es.pGLDisable,                             "glDisable"},
        {&context_ptr->entry_points_es.pGLDisableVertexAttribArray,            "glDisableVertexAttribArray"},
        {&context_ptr->entry_points_es.pGLDispatchCompute,                     "glDispatchCompute"},
        {&context_ptr->entry_points_es.pGLDispatchComputeIndirect,             "glDispatchComputeIndirect"},
        {&context_ptr->entry_points_es.pGLDrawArrays,                          "glDrawArrays"},
        {&context_ptr->entry_points_es.pGLDrawArraysIndirect,                  "glDrawArraysIndirect"},
        {&context_ptr->entry_points_es.pGLDrawArraysInstanced,                 "glDrawArraysInstanced"},
        {&context_ptr->entry_points_es.pGLDrawBuffers,                         "glDrawBuffers"},
        {&context_ptr->entry_points_es.pGLDrawElements,                        "glDrawElements"},
        {&context_ptr->entry_points_es.pGLDrawElementsIndirect,                "glDrawElementsIndirect"},
        {&context_ptr->entry_points_es.pGLDrawElementsInstanced,               "glDrawElementsInstanced"},
        {&context_ptr->entry_points_es.pGLDrawRangeElements,                   "glDrawRangeElements"},
        {&context_ptr->entry_points_es.pGLEnable,                              "glEnable"},
        {&context_ptr->entry_points_es.pGLEnableVertexAttribArray,             "glEnableVertexAttribArray"},
        {&context_ptr->entry_points_es.pGLEndQuery,                            "glEndQuery"},
        {&context_ptr->entry_points_es.pGLEndTransformFeedback,                "glEndTransformFeedback"},
        {&context_ptr->entry_points_es.pGLFenceSync,                           "glFenceSync"},
        {&context_ptr->entry_points_es.pGLFinish,                              "glFinish"},
        {&context_ptr->entry_points_es.pGLFlush,                               "glFlush"},
        {&context_ptr->entry_points_es.pGLFlushMappedBufferRange,              "glFlushMappedBufferRange"},
        {&context_ptr->entry_points_es.pGLFramebufferParameteri,               "glFramebufferParameteri"},
        {&context_ptr->entry_points_es.pGLFramebufferRenderbuffer,             "glFramebufferRenderbuffer"},
        {&context_ptr->entry_points_es.pGLFramebufferTexture2D,                "glFramebufferTexture2D"},
        {&context_ptr->entry_points_es.pGLFramebufferTextureLayer,             "glFramebufferTextureLayer"},
        {&context_ptr->entry_points_es.pGLFrontFace,                           "glFrontFace"},
        {&context_ptr->entry_points_es.pGLGenBuffers,                          "glGenBuffers"},
        {&context_ptr->entry_points_es.pGLGenerateMipmap,                      "glGenerateMipmap"},
        {&context_ptr->entry_points_es.pGLGenFramebuffers,                     "glGenFramebuffers"},
        {&context_ptr->entry_points_es.pGLGenProgramPipelines,                 "glGenProgramPipelines"},
        {&context_ptr->entry_points_es.pGLGenQueries,                          "glGenQueries"},
        {&context_ptr->entry_points_es.pGLGenRenderbuffers,                    "glGenRenderbuffers"},
        {&context_ptr->entry_points_es.pGLGenSamplers,                         "glGenSamplers"},
        {&context_ptr->entry_points_es.pGLGenTextures,                         "glGenTextures"},
        {&context_ptr->entry_points_es.pGLGenTransformFeedbacks,               "glGenTransformFeedbacks"},
        {&context_ptr->entry_points_es.pGLGenVertexArrays,                     "glGenVertexArrays"},
        {&context_ptr->entry_points_es.pGLGetActiveAtomicCounterBufferiv,      "glGetActiveAtomicCounterBufferiv"},
        {&context_ptr->entry_points_es.pGLGetActiveAttrib,                     "glGetActiveAttrib"},
        {&context_ptr->entry_points_es.pGLGetActiveUniform,                    "glGetActiveUniform"},
        {&context_ptr->entry_points_es.pGLGetActiveUniformBlockiv,             "glGetActiveUniformBlockiv"},
        {&context_ptr->entry_points_es.pGLGetActiveUniformBlockName,           "glGetActiveUniformBlockName"},
        {&context_ptr->entry_points_es.pGLGetActiveUniformsiv,                 "glGetActiveUniformsiv"},
        {&context_ptr->entry_points_es.pGLGetAttachedShaders,                  "glGetAttachedShaders"},
        {&context_ptr->entry_points_es.pGLGetAttribLocation,                   "glGetAttribLocation"},
        {&context_ptr->entry_points_es.pGLGetBooleani_v,                       "glGetBooleani_v"},
        {&context_ptr->entry_points_es.pGLGetBooleanv,                         "glGetBooleanv"},
        {&context_ptr->entry_points_es.pGLGetBufferParameteri64v,              "glGetBufferParameteri64v"},
        {&context_ptr->entry_points_es.pGLGetBufferParameteriv,                "glGetBufferParameteriv"},
        {&context_ptr->entry_points_es.pGLGetBufferPointerv,                   "glGetBufferPointerv"},
        {&context_ptr->entry_points_es.pGLGetError,                            "glGetError"},
        {&context_ptr->entry_points_es.pGLGetFloatv,                           "glGetFloatv"},
        {&context_ptr->entry_points_es.pGLGetFragDataLocation,                 "glGetFragDataLocation"},
        {&context_ptr->entry_points_es.pGLGetFramebufferAttachmentParameteriv, "glGetFramebufferAttachmentParameteriv"},
        {&context_ptr->entry_points_es.pGLGetFramebufferParameteriv,           "glGetFramebufferParameteriv"},
        {&context_ptr->entry_points_es.pGLGetInteger64i_v,                     "glGetInteger64i_v"},
        {&context_ptr->entry_points_es.pGLGetInteger64v,                       "glGetInteger64v"},
        {&context_ptr->entry_points_es.pGLGetIntegeri_v,                       "glGetIntegeri_v"},
        {&context_ptr->entry_points_es.pGLGetIntegerv,                         "glGetIntegerv"},
        {&context_ptr->entry_points_es.pGLGetInternalformativ,                 "glGetInternalformativ"},
        {&context_ptr->entry_points_es.pGLGetMultisamplefv,                    "glGetMultisamplefv"},
        {&context_ptr->entry_points_es.pGLGetProgramBinary,                    "glGetProgramBinary"},
        {&context_ptr->entry_points_es.pGLGetProgramInfoLog,                   "glGetProgramInfoLog"},
        {&context_ptr->entry_points_es.pGLGetProgramInterfaceiv,               "glGetProgramInterfaceiv"},
        {&context_ptr->entry_points_es.pGLGetProgramiv,                        "glGetProgramiv"},
        {&context_ptr->entry_points_es.pGLGetProgramPipelineInfoLog,           "glGetProgramPipelineInfoLog"},
        {&context_ptr->entry_points_es.pGLGetProgramPipelineiv,                "glGetProgramPipelineiv"},
        {&context_ptr->entry_points_es.pGLGetProgramResourceIndex,             "glGetProgramResourceIndex"},
        {&context_ptr->entry_points_es.pGLGetProgramResourceiv,                "glGetProgramResourceiv"},
        {&context_ptr->entry_points_es.pGLGetProgramResourceLocation,          "glGetProgramResourceLocation"},
        {&context_ptr->entry_points_es.pGLGetProgramResourceName,              "glGetProgramResourceName"},
        {&context_ptr->entry_points_es.pGLGetQueryiv,                          "glGetQueryiv"},
        {&context_ptr->entry_points_es.pGLGetQueryObjectuiv,                   "glGetQueryObjectuiv"},
        {&context_ptr->entry_points_es.pGLGetRenderbufferParameteriv,          "glGetRenderbufferParameteriv"},
        {&context_ptr->entry_points_es.pGLGetSamplerParameterfv,               "glGetSamplerParameterfv"},
        {&context_ptr->entry_points_es.pGLGetSamplerParameteriv,               "glGetSamplerParameteriv"},
        {&context_ptr->entry_points_es.pGLGetShaderInfoLog,                    "glGetShaderInfoLog"},
        {&context_ptr->entry_points_es.pGLGetShaderiv,                         "glGetShaderiv"},
        {&context_ptr->entry_points_es.pGLGetShaderPrecisionFormat,            "glGetShaderPrecisionFormat"},
        {&context_ptr->entry_points_es.pGLGetShaderSource,                     "glGetShaderSource"},
        {&context_ptr->entry_points_es.pGLGetString,                           "glGetString"},
        {&context_ptr->entry_points_es.pGLGetStringi,                          "glGetStringi"},
        {&context_ptr->entry_points_es.pGLGetSynciv,                           "glGetSynciv"},
        {&context_ptr->entry_points_es.pGLGetTexLevelParameterfv,              "glGetTexLevelParameterfv"},
        {&context_ptr->entry_points_es.pGLGetTexLevelParameteriv,              "glGetTexLevelParameteriv"},
        {&context_ptr->entry_points_es.pGLGetTexParameterfv,                   "glGetTexParameterfv"},
        {&context_ptr->entry_points_es.pGLGetTexParameteriv,                   "glGetTexParameteriv"},
        {&context_ptr->entry_points_es.pGLGetTransformFeedbackVarying,         "glGetTransformFeedbackVarying"},
        {&context_ptr->entry_points_es.pGLGetUniformBlockIndex,                "glGetUniformBlockIndex"},
        {&context_ptr->entry_points_es.pGLGetUniformfv,                        "glGetUniformfv"},
        {&context_ptr->entry_points_es.pGLGetUniformIndices,                   "glGetUniformIndices"},
        {&context_ptr->entry_points_es.pGLGetUniformiv,                        "glGetUniformiv"},
        {&context_ptr->entry_points_es.pGLGetUniformLocation,                  "glGetUniformLocation"},
        {&context_ptr->entry_points_es.pGLGetUniformuiv,                       "glGetUniformuiv"},
        {&context_ptr->entry_points_es.pGLGetVertexAttribfv,                   "glGetVertexAttribfv"},
        {&context_ptr->entry_points_es.pGLGetVertexAttribIiv,                  "glGetVertexAttribIiv"},
        {&context_ptr->entry_points_es.pGLGetVertexAttribIuiv,                 "glGetVertexAttribIuiv"},
        {&context_ptr->entry_points_es.pGLGetVertexAttribiv,                   "glGetVertexAttribiv"},
        {&context_ptr->entry_points_es.pGLGetVertexAttribPointerv,             "glGetVertexAttribPointerv"},
        {&context_ptr->entry_points_es.pGLHint,                                "glHint"},
        {&context_ptr->entry_points_es.pGLInvalidateFramebuffer,               "glInvalidateFramebuffer"},
        {&context_ptr->entry_points_es.pGLInvalidateSubFramebuffer,            "glInvalidateSubFramebuffer"},
        {&context_ptr->entry_points_es.pGLIsBuffer,                            "glIsBuffer"},
        {&context_ptr->entry_points_es.pGLIsEnabled,                           "glIsEnabled"},
        {&context_ptr->entry_points_es.pGLIsFramebuffer,                       "glIsFramebuffer"},
        {&context_ptr->entry_points_es.pGLIsProgram,                           "glIsProgram"},
        {&context_ptr->entry_points_es.pGLIsProgramPipeline,                   "glIsProgramPipeline"},
        {&context_ptr->entry_points_es.pGLIsQuery,                             "glIsQuery"},
        {&context_ptr->entry_points_es.pGLIsRenderbuffer,                      "glIsRenderbuffer"},
        {&context_ptr->entry_points_es.pGLIsSampler,                           "glIsSampler"},
        {&context_ptr->entry_points_es.pGLIsShader,                            "glIsShader"},
        {&context_ptr->entry_points_es.pGLIsSync,                              "glIsSync"},
        {&context_ptr->entry_points_es.pGLIsTexture,                           "glIsTexture"},
        {&context_ptr->entry_points_es.pGLIsTransformFeedback,                 "glIsTransformFeedback"},
        {&context_ptr->entry_points_es.pGLIsVertexArray,                       "glIsVertexArray"},
        {&context_ptr->entry_points_es.pGLLineWidth,                           "glLineWidth"},
        {&context_ptr->entry_points_es.pGLLinkProgram,                         "glLinkProgram"},
        {&context_ptr->entry_points_es.pGLMapBufferRange,                      "glMapBufferRange"},
        {&context_ptr->entry_points_es.pGLMemoryBarrier,                       "glMemoryBarrier"},
        {&context_ptr->entry_points_es.pGLMemoryBarrierByRegion,               "glMemoryBarrierByRegion"},
        {&context_ptr->entry_points_es.pGLPauseTransformFeedback,              "glPauseTransformFeedback"},
        {&context_ptr->entry_points_es.pGLPixelStorei,                         "glPixelStorei"},
        {&context_ptr->entry_points_es.pGLPolygonOffset,                       "glPolygonOffset"},
        {&context_ptr->entry_points_es.pGLProgramBinary,                       "glProgramBinary"},
        {&context_ptr->entry_points_es.pGLProgramParameteri,                   "glProgramParameteri"},
        {&context_ptr->entry_points_es.pGLProgramUniform1f,                    "glProgramUniform1f"},
        {&context_ptr->entry_points_es.pGLProgramUniform1fv,                   "glProgramUniform1fv"},
        {&context_ptr->entry_points_es.pGLProgramUniform1i,                    "glProgramUniform1i"},
        {&context_ptr->entry_points_es.pGLProgramUniform1iv,                   "glProgramUniform1iv"},
        {&context_ptr->entry_points_es.pGLProgramUniform1ui,                   "glProgramUniform1ui"},
        {&context_ptr->entry_points_es.pGLProgramUniform1uiv,                  "glProgramUniform1uiv"},
        {&context_ptr->entry_points_es.pGLProgramUniform2f,                    "glProgramUniform2f"},
        {&context_ptr->entry_points_es.pGLProgramUniform2fv,                   "glProgramUniform2fv"},
        {&context_ptr->entry_points_es.pGLProgramUniform2i,                    "glProgramUniform2i"},
        {&context_ptr->entry_points_es.pGLProgramUniform2iv,                   "glProgramUniform2iv"},
        {&context_ptr->entry_points_es.pGLProgramUniform2ui,                   "glProgramUniform2ui"},
        {&context_ptr->entry_points_es.pGLProgramUniform2uiv,                  "glProgramUniform2uiv"},
        {&context_ptr->entry_points_es.pGLProgramUniform3f,                    "glProgramUniform3f"},
        {&context_ptr->entry_points_es.pGLProgramUniform3fv,                   "glProgramUniform3fv"},
        {&context_ptr->entry_points_es.pGLProgramUniform3i,                    "glProgramUniform3i"},
        {&context_ptr->entry_points_es.pGLProgramUniform3iv,                   "glProgramUniform3iv"},
        {&context_ptr->entry_points_es.pGLProgramUniform3ui,                   "glProgramUniform3ui"},
        {&context_ptr->entry_points_es.pGLProgramUniform3uiv,                  "glProgramUniform3uiv"},
        {&context_ptr->entry_points_es.pGLProgramUniform4f,                    "glProgramUniform4f"},
        {&context_ptr->entry_points_es.pGLProgramUniform4fv,                   "glProgramUniform4fv"},
        {&context_ptr->entry_points_es.pGLProgramUniform4i,                    "glProgramUniform4i"},
        {&context_ptr->entry_points_es.pGLProgramUniform4iv,                   "glProgramUniform4iv"},
        {&context_ptr->entry_points_es.pGLProgramUniform4ui,                   "glProgramUniform4ui"},
        {&context_ptr->entry_points_es.pGLProgramUniform4uiv,                  "glProgramUniform4uiv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix2fv,             "glProgramUniformMatrix2fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix2x3fv,           "glProgramUniformMatrix2x3fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix2x4fv,           "glProgramUniformMatrix2x4fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix3fv,             "glProgramUniformMatrix3fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix3x2fv,           "glProgramUniformMatrix3x2fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix3x4fv,           "glProgramUniformMatrix3x4fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix4fv,             "glProgramUniformMatrix4fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix4x2fv,           "glProgramUniformMatrix4x2fv"},
        {&context_ptr->entry_points_es.pGLProgramUniformMatrix4x3fv,           "glProgramUniformMatrix4x3fv"},
        {&context_ptr->entry_points_es.pGLReadBuffer,                          "glReadBuffer"},
        {&context_ptr->entry_points_es.pGLReadPixels,                          "glReadPixels"},
        {&context_ptr->entry_points_es.pGLReleaseShaderCompiler,               "glReleaseShaderCompiler"},
        {&context_ptr->entry_points_es.pGLRenderbufferStorage,                 "glRenderbufferStorage"},
        {&context_ptr->entry_points_es.pGLRenderbufferStorageMultisample,      "glRenderbufferStorageMultisample"},
        {&context_ptr->entry_points_es.pGLResumeTransformFeedback,             "glResumeTransformFeedback"},
        {&context_ptr->entry_points_es.pGLSampleCoverage,                      "glSampleCoverage"},
        {&context_ptr->entry_points_es.pGLSampleMaski,                         "glSampleMaski"},
        {&context_ptr->entry_points_es.pGLSamplerParameterf,                   "glSamplerParameterf"},
        {&context_ptr->entry_points_es.pGLSamplerParameterfv,                  "glSamplerParameterfv"},
        {&context_ptr->entry_points_es.pGLSamplerParameteri,                   "glSamplerParameteri"},
        {&context_ptr->entry_points_es.pGLSamplerParameteriv,                  "glSamplerParameteriv"},
        {&context_ptr->entry_points_es.pGLScissor,                             "glScissor"},
        {&context_ptr->entry_points_es.pGLShaderBinary,                        "glShaderBinary"},
        {&context_ptr->entry_points_es.pGLShaderSource,                        "glShaderSource"},
        {&context_ptr->entry_points_es.pGLShaderStorageBlockBinding,           "glShaderStorageBlockBinding"},
        {&context_ptr->entry_points_es.pGLStencilFunc,                         "glStencilFunc"},
        {&context_ptr->entry_points_es.pGLStencilFuncSeparate,                 "glStencilFuncSeparate"},
        {&context_ptr->entry_points_es.pGLStencilMask,                         "glStencilMask"},
        {&context_ptr->entry_points_es.pGLStencilMaskSeparate,                 "glStencilMaskSeparate"},
        {&context_ptr->entry_points_es.pGLStencilOp,                           "glStencilOp"},
        {&context_ptr->entry_points_es.pGLStencilOpSeparate,                   "glStencilOpSeparate"},
        {&context_ptr->entry_points_es.pGLTexParameterf,                       "glTexParameterf"},
        {&context_ptr->entry_points_es.pGLTexParameterfv,                      "glTexParameterfv"},
        {&context_ptr->entry_points_es.pGLTexParameteri,                       "glTexParameteri"},
        {&context_ptr->entry_points_es.pGLTexParameteriv,                      "glTexParameteriv"},
        {&context_ptr->entry_points_es.pGLTexStorage2D,                        "glTexStorage2D"},
        {&context_ptr->entry_points_es.pGLTexStorage2DMultisample,             "glTexStorage2DMultisample"},
        {&context_ptr->entry_points_es.pGLTexStorage3D,                        "glTexStorage3D"},
        {&context_ptr->entry_points_es.pGLTexSubImage2D,                       "glTexSubImage2D"},
        {&context_ptr->entry_points_es.pGLTexSubImage3D,                       "glTexSubImage3D"},
        {&context_ptr->entry_points_es.pGLTransformFeedbackVaryings,           "glTransformFeedbackVaryings"},
        {&context_ptr->entry_points_es.pGLUniform1f,                           "glUniform1f"},
        {&context_ptr->entry_points_es.pGLUniform1fv,                          "glUniform1fv"},
        {&context_ptr->entry_points_es.pGLUniform1i,                           "glUniform1i"},
        {&context_ptr->entry_points_es.pGLUniform1iv,                          "glUniform1iv"},
        {&context_ptr->entry_points_es.pGLUniform1ui,                          "glUniform1ui"},
        {&context_ptr->entry_points_es.pGLUniform1uiv,                         "glUniform1uiv"},
        {&context_ptr->entry_points_es.pGLUniform2f,                           "glUniform2f"},
        {&context_ptr->entry_points_es.pGLUniform2fv,                          "glUniform2fv"},
        {&context_ptr->entry_points_es.pGLUniform2i,                           "glUniform2i"},
        {&context_ptr->entry_points_es.pGLUniform2iv,                          "glUniform2iv"},
        {&context_ptr->entry_points_es.pGLUniform2ui,                          "glUniform2ui"},
        {&context_ptr->entry_points_es.pGLUniform2uiv,                         "glUniform2uiv"},
        {&context_ptr->entry_points_es.pGLUniform3f,                           "glUniform3f"},
        {&context_ptr->entry_points_es.pGLUniform3fv,                          "glUniform3fv"},
        {&context_ptr->entry_points_es.pGLUniform3i,                           "glUniform3i"},
        {&context_ptr->entry_points_es.pGLUniform3iv,                          "glUniform3iv"},
        {&context_ptr->entry_points_es.pGLUniform3ui,                          "glUniform3ui"},
        {&context_ptr->entry_points_es.pGLUniform3uiv,                         "glUniform3uiv"},
        {&context_ptr->entry_points_es.pGLUniform4f,                           "glUniform4f"},
        {&context_ptr->entry_points_es.pGLUniform4fv,                          "glUniform4fv"},
        {&context_ptr->entry_points_es.pGLUniform4i,                           "glUniform4i"},
        {&context_ptr->entry_points_es.pGLUniform4iv,                          "glUniform4iv"},
        {&context_ptr->entry_points_es.pGLUniform4ui,                          "glUniform4ui"},
        {&context_ptr->entry_points_es.pGLUniform4uiv,                         "glUniform4uiv"},
        {&context_ptr->entry_points_es.pGLUniformBlockBinding,                 "glUniformBlockBinding"},
        {&context_ptr->entry_points_es.pGLUniformMatrix2fv,                    "glUniformMatrix2fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix2x3fv,                  "glUniformMatrix2x3fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix2x4fv,                  "glUniformMatrix2x4fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix3fv,                    "glUniformMatrix3fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix3x2fv,                  "glUniformMatrix3x2fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix3x4fv,                  "glUniformMatrix3x4fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix4fv,                    "glUniformMatrix4fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix4x2fv,                  "glUniformMatrix4x2fv"},
        {&context_ptr->entry_points_es.pGLUniformMatrix4x3fv,                  "glUniformMatrix4x3fv"},
        {&context_ptr->entry_points_es.pGLUnmapBuffer,                         "glUnmapBuffer"},
        {&context_ptr->entry_points_es.pGLUseProgram,                          "glUseProgram"},
        {&context_ptr->entry_points_es.pGLUseProgramStages,                    "glUseProgramStages"},
        {&context_ptr->entry_points_es.pGLValidateProgram,                     "glValidateProgram"},
        {&context_ptr->entry_points_es.pGLValidateProgramPipeline,             "glValidateProgramPipeline"},
        {&context_ptr->entry_points_es.pGLVertexAttrib1f,                      "glVertexAttrib1f"},
        {&context_ptr->entry_points_es.pGLVertexAttrib1fv,                     "glVertexAttrib1fv"},
        {&context_ptr->entry_points_es.pGLVertexAttrib2f,                      "glVertexAttrib2f"},
        {&context_ptr->entry_points_es.pGLVertexAttrib2fv,                     "glVertexAttrib2fv"},
        {&context_ptr->entry_points_es.pGLVertexAttrib3f,                      "glVertexAttrib3f"},
        {&context_ptr->entry_points_es.pGLVertexAttrib3fv,                     "glVertexAttrib3fv"},
        {&context_ptr->entry_points_es.pGLVertexAttrib4f,                      "glVertexAttrib4f"},
        {&context_ptr->entry_points_es.pGLVertexAttrib4fv,                     "glVertexAttrib4fv"},
        {&context_ptr->entry_points_es.pGLVertexAttribBinding,                 "glVertexAttribBinding"},
        {&context_ptr->entry_points_es.pGLVertexAttribDivisor,                 "glVertexAttribDivisor"},
        {&context_ptr->entry_points_es.pGLVertexAttribFormat,                  "glVertexAttribFormat"},
        {&context_ptr->entry_points_es.pGLVertexAttribI4i,                     "glVertexAttribI4i"},
        {&context_ptr->entry_points_es.pGLVertexAttribI4iv,                    "glVertexAttribI4iv"},
        {&context_ptr->entry_points_es.pGLVertexAttribI4ui,                    "glVertexAttribI4ui"},
        {&context_ptr->entry_points_es.pGLVertexAttribI4uiv,                   "glVertexAttribI4uiv"},
        {&context_ptr->entry_points_es.pGLVertexAttribIFormat,                 "glVertexAttribIFormat"},
        {&context_ptr->entry_points_es.pGLVertexAttribIPointer,                "glVertexAttribIPointer"},
        {&context_ptr->entry_points_es.pGLVertexAttribPointer,                 "glVertexAttribPointer"},
        {&context_ptr->entry_points_es.pGLVertexBindingDivisor,                "glVertexBindingDivisor"},
        {&context_ptr->entry_points_es.pGLViewport,                            "glViewport"},
        {&context_ptr->entry_points_es.pGLWaitSync,                            "glWaitSync"}
    };
    const uint32_t n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    _ogl_context_get_function_pointers(context_ptr,
                                       func_ptr_table,
                                       n_func_ptr_table_entries);
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_buffer_storage_function_pointers(_ogl_context* context_ptr)
{
    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_private.pGLBufferStorage, "glBufferStorage"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->entry_points_gl_arb_buffer_storage.pGLBufferStorage = ogl_context_wrappers_glBufferStorage;
        context_ptr->gl_arb_buffer_storage_support                       = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_multi_bind_function_pointers(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_private.pGLBindBuffersBase,             "glBindBuffersBase"},
        {&context_ptr->entry_points_private.pGLBindBuffersRange,            "glBindBuffersRange"},
        {&context_ptr->entry_points_gl_arb_multi_bind.pGLBindImageTextures, "glBindImageTextures"},
        {&context_ptr->entry_points_private.pGLBindSamplers,                "glBindSamplers"},
        {&context_ptr->entry_points_private.pGLBindTextures,                "glBindTextures"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->entry_points_gl_arb_multi_bind.pGLBindBuffersBase  = ogl_context_wrappers_glBindBuffersBase;
        context_ptr->entry_points_gl_arb_multi_bind.pGLBindBuffersRange = ogl_context_wrappers_glBindBuffersRange;
        context_ptr->entry_points_gl_arb_multi_bind.pGLBindSamplers     = ogl_context_wrappers_glBindSamplers;
        context_ptr->entry_points_gl_arb_multi_bind.pGLBindTextures     = ogl_context_wrappers_glBindTextures;
        context_ptr->gl_arb_multi_bind_support                          = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_sparse_buffer_function_pointers(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_gl_arb_sparse_buffer.pGLBufferPageCommitmentARB, "glBufferPageCommitmentARB"},
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->gl_arb_sparse_buffer_support = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_sparse_buffer_limits(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    if (context_ptr->gl_arb_sparse_buffer_support)
    {
        context_ptr->entry_points_private.pGLGetIntegerv(GL_SPARSE_BUFFER_PAGE_SIZE_ARB,
                                                        &context_ptr->limits_arb_sparse_buffer.sparse_buffer_page_size);

        ASSERT_DEBUG_SYNC(context_ptr->entry_points_gl.pGLGetError() == GL_NO_ERROR,
                          "Could not retrieve at least one ARB_sparse_buffer limit");
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_EXT_direct_state_access_function_pointers(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_private.pGLBindMultiTextureEXT,                                            "glBindMultiTextureEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureImage1DEXT,                                    "glCompressedTextureImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureImage2DEXT,                                    "glCompressedTextureImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureImage3DEXT,                                    "glCompressedTextureImage3DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureSubImage1DEXT,                                 "glCompressedTextureSubImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureSubImage2DEXT,                                 "glCompressedTextureSubImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureSubImage3DEXT,                                 "glCompressedTextureSubImage3DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureImage1DEXT,                                          "glCopyTextureImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureImage2DEXT,                                          "glCopyTextureImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureSubImage1DEXT,                                       "glCopyTextureSubImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureSubImage2DEXT,                                       "glCopyTextureSubImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureSubImage3DEXT,                                       "glCopyTextureSubImage3DEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLDisableVertexArrayAttribEXT,                 "glDisableVertexArrayAttribEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLEnableVertexArrayAttribEXT,                  "glEnableVertexArrayAttribEXT"},
        {&context_ptr->entry_points_private.pGLFramebufferDrawBufferEXT,                                       "glFramebufferDrawBufferEXT"},
        {&context_ptr->entry_points_private.pGLFramebufferDrawBuffersEXT,                                      "glFramebufferDrawBuffersEXT"},
        {&context_ptr->entry_points_private.pGLFramebufferReadBufferEXT,                                       "glFramebufferReadBufferEXT"},
        {&context_ptr->entry_points_private.pGLGenerateTextureMipmapEXT,                                       "glGenerateTextureMipmapEXT"},
        {&context_ptr->entry_points_private.pGLGetCompressedTextureImageEXT,                                   "glGetCompressedTextureImageEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetFramebufferParameterivEXT,                "glGetFramebufferParameterivEXT"},
        {&context_ptr->entry_points_private.pGLGetNamedBufferParameterivEXT,                                   "glGetNamedBufferParameterivEXT"},
        {&context_ptr->entry_points_private.pGLGetNamedBufferPointervEXT,                                      "glGetNamedBufferPointervEXT"},
        {&context_ptr->entry_points_private.pGLGetNamedBufferSubDataEXT,                                       "glGetNamedBufferSubDataEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedFramebufferAttachmentParameterivEXT, "glGetNamedFramebufferAttachmentParameterivEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedProgramivEXT,                        "glGetNamedProgramivEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedRenderbufferParameterivEXT,          "glGetNamedRenderbufferParameterivEXT"},
        {&context_ptr->entry_points_private.pGLGetTextureImageEXT,                                             "glGetTextureImageEXT"},
        {&context_ptr->entry_points_private.pGLGetTextureLevelParameterfvEXT,                                  "glGetTextureLevelParameterfvEXT"},
        {&context_ptr->entry_points_private.pGLGetTextureLevelParameterivEXT,                                  "glGetTextureLevelParameterivEXT"},
        {&context_ptr->entry_points_private.pGLMapNamedBufferEXT,                                              "glMapNamedBufferEXT"},
        {&context_ptr->entry_points_private.pGLNamedBufferDataEXT,                                             "glNamedBufferDataEXT"},
        {&context_ptr->entry_points_private.pGLNamedBufferSubDataEXT,                                          "glNamedBufferSubDataEXT"},
        {&context_ptr->entry_points_private.pGLNamedCopyBufferSubDataEXT,                                      "glNamedCopyBufferSubDataEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferRenderbufferEXT,             "glNamedFramebufferRenderbufferEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTexture1DEXT,                                   "glNamedFramebufferTexture1DEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTexture2DEXT,                                   "glNamedFramebufferTexture2DEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTexture3DEXT,                                   "glNamedFramebufferTexture3DEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTextureEXT,                                     "glNamedFramebufferTextureEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTextureLayerEXT,                                "glNamedFramebufferTextureLayerEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedRenderbufferStorageEXT,                 "glNamedRenderbufferStorageEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedRenderbufferStorageMultisampleEXT,      "glNamedRenderbufferStorageMultisampleEXT"},
        {&context_ptr->entry_points_private.pGLTextureBufferEXT,                                               "glTextureBufferEXT"},
        {&context_ptr->entry_points_private.pGLTextureBufferRangeEXT,                                          "glTextureBufferRangeEXT"},
        {&context_ptr->entry_points_private.pGLTextureImage1DEXT,                                              "glTextureImage1DEXT"},
        {&context_ptr->entry_points_private.pGLTextureImage2DEXT,                                              "glTextureImage2DEXT"},
        {&context_ptr->entry_points_private.pGLTextureImage3DEXT,                                              "glTextureImage3DEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameteriEXT,                                           "glTextureParameteriEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterivEXT,                                          "glTextureParameterivEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterIivEXT,                                         "glTextureParameterIivEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterIuivEXT,                                        "glTextureParameterIuivEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterfEXT,                                           "glTextureParameterfEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterfvEXT,                                          "glTextureParameterfvEXT"},
        {&context_ptr->entry_points_private.pGLTextureStorage1DEXT,                                            "glTextureStorage1DEXT"},
        {&context_ptr->entry_points_private.pGLTextureStorage2DEXT,                                            "glTextureStorage2DEXT"},
        {&context_ptr->entry_points_private.pGLTextureStorage3DEXT,                                            "glTextureStorage3DEXT"},
        {&context_ptr->entry_points_private.pGLTextureSubImage1DEXT,                                           "glTextureSubImage1DEXT"},
        {&context_ptr->entry_points_private.pGLTextureSubImage2DEXT,                                           "glTextureSubImage2DEXT"},
        {&context_ptr->entry_points_private.pGLTextureSubImage3DEXT,                                           "glTextureSubImage3DEXT"},
        {&context_ptr->entry_points_private.pGLUnmapNamedBufferEXT,                                            "glUnmapNamedBufferEXT"},
        {&context_ptr->entry_points_private.pGLVertexArrayVertexAttribOffsetEXT,                               "glVertexArrayVertexAttribOffsetEXT"},
        {&context_ptr->entry_points_private.pGLVertexArrayVertexAttribIOffsetEXT,                              "glVertexArrayVertexAttribIOffsetEXT"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        if (context_ptr->gl_arb_buffer_storage_support)
        {
            context_ptr->entry_points_private.pGLNamedBufferStorageEXT                    = (PFNGLNAMEDBUFFERSTORAGEEXTPROC) context_ptr->pfn_get_func_ptr(context_ptr->context_platform,
                                                                                                                                                           "glNamedBufferStorageEXT");
            context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedBufferStorageEXT = ogl_context_wrappers_glNamedBufferStorageEXT;

            if (context_ptr->entry_points_private.pGLNamedBufferStorageEXT == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "DSA version entry-points of GL_ARB_buffer_storage are unavailable in spite of being reported.");
            }
        }

        if (context_ptr->gl_arb_sparse_buffer_support)
        {
            context_ptr->entry_points_gl_arb_sparse_buffer.pGLNamedBufferPageCommitmentEXT = (PFNGLNAMEDBUFFERPAGECOMMITMENTARBPROC) context_ptr->pfn_get_func_ptr(context_ptr->context_platform,
                                                                                                                                                                   "glNamedBufferPageCommitmentEXT");

            if (context_ptr->entry_points_gl_arb_sparse_buffer.pGLNamedBufferPageCommitmentEXT == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "DSA version entry-points for GL_ARB_sparse_buffer are unavailable in spite of being reported.");
            }
        }

        /* ARB_texture_storage_multisample */
        context_ptr->entry_points_private.pGLTextureStorage2DMultisampleEXT = (PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC) context_ptr->pfn_get_func_ptr(context_ptr->context_platform,
                                                                                                                                                      "glTextureStorage2DMultisampleEXT");
        context_ptr->entry_points_private.pGLTextureStorage3DMultisampleEXT = (PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC) context_ptr->pfn_get_func_ptr(context_ptr->context_platform,
                                                                                                                                                      "glTextureStorage3DMultisampleEXT");

        if (context_ptr->entry_points_private.pGLTextureStorage2DMultisampleEXT == NULL ||
            context_ptr->entry_points_private.pGLTextureStorage3DMultisampleEXT == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "DSA version entry-points of GL_ARB_texture_storage_multisample are unavailable in spite of being reported.");
        }

        context_ptr->gl_ext_direct_state_access_support = true;
    }

    context_ptr->entry_points_gl_ext_direct_state_access.pGLBindMultiTextureEXT               = ogl_context_wrappers_glBindMultiTextureEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureSubImage1DEXT    = ogl_context_wrappers_glCompressedTextureSubImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureSubImage2DEXT    = ogl_context_wrappers_glCompressedTextureSubImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureSubImage3DEXT    = ogl_context_wrappers_glCompressedTextureSubImage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureImage1DEXT             = ogl_context_wrappers_glCopyTextureImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureImage2DEXT             = ogl_context_wrappers_glCopyTextureImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureSubImage1DEXT          = ogl_context_wrappers_glCopyTextureSubImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureSubImage2DEXT          = ogl_context_wrappers_glCopyTextureSubImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureSubImage3DEXT          = ogl_context_wrappers_glCopyTextureSubImage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLFramebufferDrawBufferEXT          = ogl_context_wrappers_glFramebufferDrawBufferEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLFramebufferDrawBuffersEXT         = ogl_context_wrappers_glFramebufferDrawBuffersEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLFramebufferReadBufferEXT          = ogl_context_wrappers_glFramebufferReadBufferEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGenerateTextureMipmapEXT          = ogl_context_wrappers_glGenerateTextureMipmapEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetCompressedTextureImageEXT      = ogl_context_wrappers_glGetCompressedTextureImageEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedBufferParameterivEXT      = ogl_context_wrappers_glGetNamedBufferParameterivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedBufferPointervEXT         = ogl_context_wrappers_glGetNamedBufferPointervEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedBufferSubDataEXT          = ogl_context_wrappers_glGetNamedBufferSubDataEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetTextureImageEXT                = ogl_context_wrappers_glGetTextureImageEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetTextureLevelParameterfvEXT     = ogl_context_wrappers_glGetTextureLevelParameterfvEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLGetTextureLevelParameterivEXT     = ogl_context_wrappers_glGetTextureLevelParameterivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLMapNamedBufferEXT                 = ogl_context_wrappers_glMapNamedBufferEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedBufferDataEXT                = ogl_context_wrappers_glNamedBufferDataEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedBufferSubDataEXT             = ogl_context_wrappers_glNamedBufferSubDataEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedCopyBufferSubDataEXT         = ogl_context_wrappers_glNamedCopyBufferSubDataEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTexture1DEXT      = ogl_context_wrappers_glNamedFramebufferTexture1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTexture2DEXT      = ogl_context_wrappers_glNamedFramebufferTexture2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTexture3DEXT      = ogl_context_wrappers_glNamedFramebufferTexture3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTextureEXT        = ogl_context_wrappers_glNamedFramebufferTextureEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTextureLayerEXT   = ogl_context_wrappers_glNamedFramebufferTextureLayerEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureBufferEXT                  = ogl_context_wrappers_glTextureBufferEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureBufferRangeEXT             = ogl_context_wrappers_glTextureBufferRangeEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameteriEXT              = ogl_context_wrappers_glTextureParameteriEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterivEXT             = ogl_context_wrappers_glTextureParameterivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterfEXT              = ogl_context_wrappers_glTextureParameterfEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterfvEXT             = ogl_context_wrappers_glTextureParameterfvEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterIivEXT            = ogl_context_wrappers_glTextureParameterIivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterIuivEXT           = ogl_context_wrappers_glTextureParameterIuivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureStorage1DEXT               = ogl_context_wrappers_glTextureStorage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureStorage2DEXT               = ogl_context_wrappers_glTextureStorage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureStorage2DMultisampleEXT    = ogl_context_wrappers_glTextureStorage2DMultisampleEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureStorage3DEXT               = ogl_context_wrappers_glTextureStorage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureStorage3DMultisampleEXT    = ogl_context_wrappers_glTextureStorage3DMultisampleEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureSubImage1DEXT              = ogl_context_wrappers_glTextureSubImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureSubImage2DEXT              = ogl_context_wrappers_glTextureSubImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureSubImage3DEXT              = ogl_context_wrappers_glTextureSubImage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLUnmapNamedBufferEXT               = ogl_context_wrappers_glUnmapNamedBufferEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLVertexArrayVertexAttribOffsetEXT  = ogl_context_wrappers_glVertexArrayVertexAttribOffsetEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLVertexArrayVertexAttribIOffsetEXT = ogl_context_wrappers_glVertexArrayVertexAttribIOffsetEXT;
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_EXT_texture_filter_anisotropic_limits(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    context_ptr->entry_points_private.pGLGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
                                                  &context_ptr->limits_ext_texture_filter_anisotropic.texture_max_anisotropy_ext);

    ASSERT_DEBUG_SYNC(context_ptr->entry_points_gl.pGLGetError() == GL_NO_ERROR,
                      "Could not retrieve at least one GL_EXT_texture_filter_anisotropic limit");
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_function_pointers(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_gl.pGLActiveShaderProgram,                              "glActiveShaderProgram"},
        {&context_ptr->entry_points_private.pGLActiveTexture,                               "glActiveTexture"},
        {&context_ptr->entry_points_gl.pGLAttachShader,                                     "glAttachShader"},
        {&context_ptr->entry_points_gl.pGLBeginConditionalRender,                           "glBeginConditionalRender"},
        {&context_ptr->entry_points_gl.pGLBeginQuery,                                       "glBeginQuery"},
        {&context_ptr->entry_points_private.pGLBeginTransformFeedback,                      "glBeginTransformFeedback"},
        {&context_ptr->entry_points_gl.pGLBindAttribLocation,                               "glBindAttribLocation"},
        {&context_ptr->entry_points_private.pGLBindBuffer,                                  "glBindBuffer"},
        {&context_ptr->entry_points_private.pGLBindBufferBase,                              "glBindBufferBase"},
        {&context_ptr->entry_points_private.pGLBindBufferRange,                             "glBindBufferRange"},
        {&context_ptr->entry_points_gl.pGLBindFragDataLocation,                             "glBindFragDataLocation"},
        {&context_ptr->entry_points_private.pGLBindFramebuffer,                             "glBindFramebuffer"},
        {&context_ptr->entry_points_private.pGLBindImageTexture,                            "glBindImageTexture"},
        {&context_ptr->entry_points_gl.pGLBindProgramPipeline,                              "glBindProgramPipeline"},
        {&context_ptr->entry_points_private.pGLBindRenderbuffer,                            "glBindRenderbuffer"},
        {&context_ptr->entry_points_private.pGLBindSampler,                                 "glBindSampler"},
        {&context_ptr->entry_points_private.pGLBindTexture,                                 "glBindTexture"},
        {&context_ptr->entry_points_gl.pGLBindTransformFeedback,                            "glBindTransformFeedback"},
        {&context_ptr->entry_points_private.pGLBindVertexArray,                             "glBindVertexArray"},
        {&context_ptr->entry_points_private.pGLBlendColor,                                  "glBlendColor"},
        {&context_ptr->entry_points_private.pGLBlendEquation,                               "glBlendEquation"},
        {&context_ptr->entry_points_private.pGLBlendEquationSeparate,                       "glBlendEquationSeparate"},
        {&context_ptr->entry_points_private.pGLBlendFunc,                                   "glBlendFunc"},
        {&context_ptr->entry_points_private.pGLBlendFuncSeparate,                           "glBlendFuncSeparate"},
        {&context_ptr->entry_points_private.pGLBlitFramebuffer,                             "glBlitFramebuffer"},
        {&context_ptr->entry_points_private.pGLBufferData,                                  "glBufferData"},
        {&context_ptr->entry_points_private.pGLGetBufferParameteriv,                        "glGetBufferParameteriv"},
        {&context_ptr->entry_points_private.pGLGetBufferPointerv,                           "glGetBufferPointerv"},
        {&context_ptr->entry_points_private.pGLBufferSubData,                               "glBufferSubData"},
        {&context_ptr->entry_points_gl.pGLCheckFramebufferStatus,                           "glCheckFramebufferStatus"},
        {&context_ptr->entry_points_gl.pGLClampColor,                                       "glClampColor"},
        {&context_ptr->entry_points_private.pGLClear,                                       "glClear"},
        {&context_ptr->entry_points_private.pGLClearBufferData,                             "glClearBufferData"},
        {&context_ptr->entry_points_private.pGLClearBufferSubData,                          "glClearBufferSubData"},
        {&context_ptr->entry_points_gl.pGLClearBufferfv,                                    "glClearBufferfv"},
        {&context_ptr->entry_points_gl.pGLClearBufferfi,                                    "glClearBufferfi"},
        {&context_ptr->entry_points_gl.pGLClearBufferiv,                                    "glClearBufferiv"},
        {&context_ptr->entry_points_gl.pGLClearBufferuiv,                                   "glClearBufferuiv"},
        {&context_ptr->entry_points_private.pGLClearColor,                                  "glClearColor"},
        {&context_ptr->entry_points_gl.pGLClearStencil,                                     "glClearStencil"},
        {&context_ptr->entry_points_private.pGLClearDepth,                                  "glClearDepth"},
        {&context_ptr->entry_points_private.pGLColorMask,                                   "glColorMask"},
        {&context_ptr->entry_points_gl.pGLColorMaski,                                       "glColorMaski"},
        {&context_ptr->entry_points_gl.pGLCompileShader,                                    "glCompileShader"},
        {&context_ptr->entry_points_private.pGLCompressedTexSubImage1D,                     "glCompressedTexSubImage1D"},
        {&context_ptr->entry_points_private.pGLCompressedTexSubImage2D,                     "glCompressedTexSubImage2D"},
        {&context_ptr->entry_points_private.pGLCompressedTexSubImage3D,                     "glCompressedTexSubImage3D"},
        {&context_ptr->entry_points_private.pGLCopyBufferSubData,                           "glCopyBufferSubData"},
        {&context_ptr->entry_points_gl.pGLCopyImageSubData,                                 "glCopyImageSubData"},
        {&context_ptr->entry_points_private.pGLCopyTexImage1D,                              "glCopyTexImage1D"},
        {&context_ptr->entry_points_private.pGLCopyTexImage2D,                              "glCopyTexImage2D"},
        {&context_ptr->entry_points_private.pGLCopyTexSubImage1D,                           "glCopyTexSubImage1D"},
        {&context_ptr->entry_points_private.pGLCopyTexSubImage2D,                           "glCopyTexSubImage2D"},
        {&context_ptr->entry_points_private.pGLCopyTexSubImage3D,                           "glCopyTexSubImage3D"},
        {&context_ptr->entry_points_gl.pGLCreateProgram,                                    "glCreateProgram"},
        {&context_ptr->entry_points_gl.pGLCreateShader,                                     "glCreateShader"},
        {&context_ptr->entry_points_gl.pGLCreateShaderProgramv,                             "glCreateShaderProgramv"},
        {&context_ptr->entry_points_private.pGLCullFace,                                    "glCullFace"},
        {&context_ptr->entry_points_private.pGLDeleteBuffers,                               "glDeleteBuffers"},
        {&context_ptr->entry_points_gl.pGLDebugMessageCallback,                             "glDebugMessageCallback"},
        {&context_ptr->entry_points_gl.pGLDebugMessageControl,                              "glDebugMessageControl"},
        {&context_ptr->entry_points_gl.pGLDebugMessageInsert,                               "glDebugMessageInsert"},
        {&context_ptr->entry_points_gl.pGLDeleteFramebuffers,                               "glDeleteFramebuffers"},
        {&context_ptr->entry_points_gl.pGLDeleteProgram,                                    "glDeleteProgram"},
        {&context_ptr->entry_points_gl.pGLDeleteProgramPipelines,                           "glDeleteProgramPipelines"},
        {&context_ptr->entry_points_private.pGLDeleteRenderbuffers,                         "glDeleteRenderbuffers"},
        {&context_ptr->entry_points_gl.pGLDeleteSamplers,                                   "glDeleteSamplers"},
        {&context_ptr->entry_points_gl.pGLDeleteShader,                                     "glDeleteShader"},
        {&context_ptr->entry_points_gl.pGLDeleteTextures,                                   "glDeleteTextures"},
        {&context_ptr->entry_points_gl.pGLDeleteTransformFeedbacks,                         "glDeleteTransformFeedbacks"},
        {&context_ptr->entry_points_gl.pGLDeleteQueries,                                    "glDeleteQueries"},
        {&context_ptr->entry_points_private.pGLDeleteVertexArrays,                          "glDeleteVertexArrays"},
        {&context_ptr->entry_points_private.pGLDepthFunc,                                   "glDepthFunc"},
        {&context_ptr->entry_points_private.pGLDepthMask,                                   "glDepthMask"},
        {&context_ptr->entry_points_gl.pGLDepthRange,                                       "glDepthRange"},
        {&context_ptr->entry_points_gl.pGLDetachShader,                                     "glDetachShader"},
        {&context_ptr->entry_points_private.pGLDisable,                                     "glDisable"},
        {&context_ptr->entry_points_private.pGLDisablei,                                    "glDisablei"},
        {&context_ptr->entry_points_private.pGLDisableVertexAttribArray,                    "glDisableVertexAttribArray"},
        {&context_ptr->entry_points_private.pGLDispatchCompute,                             "glDispatchCompute"},
        {&context_ptr->entry_points_private.pGLDispatchComputeIndirect,                     "glDispatchComputeIndirect"},
        {&context_ptr->entry_points_private.pGLDrawArrays,                                  "glDrawArrays"},
        {&context_ptr->entry_points_private.pGLDrawArraysIndirect,                          "glDrawArraysIndirect"},
        {&context_ptr->entry_points_private.pGLDrawArraysInstanced,                         "glDrawArraysInstanced"},
        {&context_ptr->entry_points_private.pGLDrawArraysInstancedBaseInstance,             "glDrawArraysInstancedBaseInstance"},
        {&context_ptr->entry_points_private.pGLDrawBuffer,                                  "glDrawBuffer"},
        {&context_ptr->entry_points_private.pGLDrawBuffers,                                 "glDrawBuffers"},
        {&context_ptr->entry_points_private.pGLDrawElements,                                "glDrawElements"},
        {&context_ptr->entry_points_private.pGLDrawElementsInstanced,                       "glDrawElementsInstanced"},
        {&context_ptr->entry_points_private.pGLDrawElementsInstancedBaseInstance,           "glDrawElementsInstancedBaseInstance"},
        {&context_ptr->entry_points_private.pGLDrawElementsInstancedBaseVertexBaseInstance, "glDrawElementsInstancedBaseVertexBaseInstance"},
        {&context_ptr->entry_points_private.pGLDrawRangeElements,                           "glDrawRangeElements"},
        {&context_ptr->entry_points_private.pGLDrawTransformFeedback,                       "glDrawTransformFeedback"},
        {&context_ptr->entry_points_private.pGLDrawTransformFeedbackInstanced,              "glDrawTransformFeedbackInstanced"},
        {&context_ptr->entry_points_private.pGLDrawTransformFeedbackStreamInstanced,        "glDrawTransformFeedbackStreamInstanced"},
        {&context_ptr->entry_points_private.pGLEnable,                                      "glEnable"},
        {&context_ptr->entry_points_private.pGLEnablei,                                     "glEnablei"},
        {&context_ptr->entry_points_private.pGLEnableVertexAttribArray,                     "glEnableVertexAttribArray"},
        {&context_ptr->entry_points_gl.pGLEndConditionalRender,                             "glEndConditionalRender"},
        {&context_ptr->entry_points_gl.pGLEndQuery,                                         "glEndQuery"},
        {&context_ptr->entry_points_gl.pGLEndTransformFeedback,                             "glEndTransformFeedback"},
        {&context_ptr->entry_points_gl.pGLFinish,                                           "glFinish"},
        {&context_ptr->entry_points_gl.pGLFlush,                                            "glFlush"},
        {&context_ptr->entry_points_private.pGLFramebufferParameteri,                       "glFramebufferParameteri"},
        {&context_ptr->entry_points_private.pGLFramebufferRenderbuffer,                     "glFramebufferRenderbuffer"},
        {&context_ptr->entry_points_private.pGLFramebufferTexture,                          "glFramebufferTexture"},
        {&context_ptr->entry_points_private.pGLFramebufferTexture1D,                        "glFramebufferTexture1D"},
        {&context_ptr->entry_points_private.pGLFramebufferTexture2D,                        "glFramebufferTexture2D"},
        {&context_ptr->entry_points_private.pGLFramebufferTexture3D,                        "glFramebufferTexture3D"},
        {&context_ptr->entry_points_private.pGLFramebufferTextureLayer,                     "glFramebufferTextureLayer"},
        {&context_ptr->entry_points_private.pGLFrontFace,                                   "glFrontFace"},
        {&context_ptr->entry_points_gl.pGLGenBuffers,                                       "glGenBuffers"},
        {&context_ptr->entry_points_private.pGLGenerateMipmap,                              "glGenerateMipmap"},
        {&context_ptr->entry_points_gl.pGLGenFramebuffers,                                  "glGenFramebuffers"},
        {&context_ptr->entry_points_gl.pGLGenProgramPipelines,                              "glGenProgramPipelines"},
        {&context_ptr->entry_points_gl.pGLGenRenderbuffers,                                 "glGenRenderbuffers"},
        {&context_ptr->entry_points_gl.pGLGenSamplers,                                      "glGenSamplers"},
        {&context_ptr->entry_points_gl.pGLGenTextures,                                      "glGenTextures"},
        {&context_ptr->entry_points_gl.pGLGenTransformFeedbacks,                            "glGenTransformFeedbacks"},
        {&context_ptr->entry_points_gl.pGLGenQueries,                                       "glGenQueries"},
        {&context_ptr->entry_points_private.pGLGenVertexArrays,                             "glGenVertexArrays"},
        {&context_ptr->entry_points_private.pGLGetActiveAtomicCounterBufferiv,              "glGetActiveAtomicCounterBufferiv"},
        {&context_ptr->entry_points_gl.pGLGetActiveAttrib,                                  "glGetActiveAttrib"},
        {&context_ptr->entry_points_gl.pGLGetActiveUniform,                                 "glGetActiveUniform"},
        {&context_ptr->entry_points_gl.pGLGetActiveUniformBlockName,                        "glGetActiveUniformBlockName"},
        {&context_ptr->entry_points_gl.pGLGetActiveUniformBlockiv,                          "glGetActiveUniformBlockiv"},
        {&context_ptr->entry_points_gl.pGLGetActiveUniformsiv,                              "glGetActiveUniformsiv"},
        {&context_ptr->entry_points_gl.pGLGetAttribLocation,                                "glGetAttribLocation"},
        {&context_ptr->entry_points_gl.pGLGetAttachedShaders,                               "glGetAttachedShaders"},
        {&context_ptr->entry_points_private.pGLGetBooleani_v,                               "glGetBooleani_v"},
        {&context_ptr->entry_points_private.pGLGetBooleanv,                                 "glGetBooleanv"},
        {&context_ptr->entry_points_private.pGLGetBufferParameteri64v,                      "glGetBufferParameteri64v"},
        {&context_ptr->entry_points_private.pGLGetBufferSubData,                            "glGetBufferSubData"},
        {&context_ptr->entry_points_private.pGLGetCompressedTexImage,                       "glGetCompressedTexImage"},
        {&context_ptr->entry_points_gl.pGLGetDebugMessageLog,                               "glGetDebugMessageLog"},
        {&context_ptr->entry_points_private.pGLGetDoublev,                                  "glGetDoublev"},
        {&context_ptr->entry_points_gl.pGLGetError,                                         "glGetError"},
        {&context_ptr->entry_points_private.pGLGetFloatv,                                   "glGetFloatv"},
        {&context_ptr->entry_points_gl.pGLGetFragDataLocation,                              "glGetFragDataLocation"},
        {&context_ptr->entry_points_private.pGLGetInteger64i_v,                             "glGetInteger64i_v"},
        {&context_ptr->entry_points_private.pGLGetFramebufferParameteriv,                   "glGetFramebufferParameteriv"},
        {&context_ptr->entry_points_private.pGLGetIntegeri_v,                               "glGetIntegeri_v"},
        {&context_ptr->entry_points_private.pGLGetIntegerv,                                 "glGetIntegerv"},
        {&context_ptr->entry_points_gl.pGLGetInternalformativ,                              "glGetInternalformativ"},
        {&context_ptr->entry_points_gl.pGLGetObjectLabel,                                   "glGetObjectLabel"},
        {&context_ptr->entry_points_gl.pGLGetObjectPtrLabel,                                "glGetObjectPtrLabel"},
        {&context_ptr->entry_points_gl.pGLGetProgramBinary,                                 "glGetProgramBinary"},
        {&context_ptr->entry_points_gl.pGLGetProgramiv,                                     "glGetProgramiv"},
        {&context_ptr->entry_points_gl.pGLGetProgramInfoLog,                                "glGetProgramInfoLog"},
        {&context_ptr->entry_points_gl.pGLGetProgramInterfaceiv,                            "glGetProgramInterfaceiv"},
        {&context_ptr->entry_points_gl.pGLGetProgramResourceIndex,                          "glGetProgramResourceIndex"},
        {&context_ptr->entry_points_gl.pGLGetProgramResourceName,                           "glGetProgramResourceName"},
        {&context_ptr->entry_points_gl.pGLGetProgramResourceiv,                             "glGetProgramResourceiv"},
        {&context_ptr->entry_points_gl.pGLGetProgramResourceLocation,                       "glGetProgramResourceLocation"},
        {&context_ptr->entry_points_gl.pGLGetProgramResourceLocationIndex,                  "glGetProgramResourceLocationIndex"},
        {&context_ptr->entry_points_private.pGLGetRenderbufferParameteriv,                  "glGetRenderbufferParameteriv"},
        {&context_ptr->entry_points_gl.pGLGetShaderiv,                                      "glGetShaderiv"},
        {&context_ptr->entry_points_gl.pGLGetShaderInfoLog,                                 "glGetShaderInfoLog"},
        {&context_ptr->entry_points_gl.pGLGetShaderSource,                                  "glGetShaderSource"},
        {&context_ptr->entry_points_gl.pGLGetString,                                        "glGetString"},
        {&context_ptr->entry_points_gl.pGLGetStringi,                                       "glGetStringi"},
        {&context_ptr->entry_points_private.pGLGetTexLevelParameterfv,                      "glGetTexLevelParameterfv"},
        {&context_ptr->entry_points_private.pGLGetTexLevelParameteriv,                      "glGetTexLevelParameteriv"},
        {&context_ptr->entry_points_private.pGLGetTexParameterfv,                           "glGetTexParameterfv"},
        {&context_ptr->entry_points_private.pGLGetTexParameteriv,                           "glGetTexParameteriv"},
        {&context_ptr->entry_points_private.pGLGetTexParameterIiv,                          "glGetTexParameterIiv"},
        {&context_ptr->entry_points_private.pGLGetTexParameterIuiv,                         "glGetTexParameterIuiv"},
        {&context_ptr->entry_points_private.pGLGetTexImage,                                 "glGetTexImage"},
        {&context_ptr->entry_points_gl.pGLGetTransformFeedbackVarying,                      "glGetTransformFeedbackVarying"},
        {&context_ptr->entry_points_gl.pGLGetUniformBlockIndex,                             "glGetUniformBlockIndex"},
        {&context_ptr->entry_points_gl.pGLGetUniformfv,                                     "glGetUniformfv"},
        {&context_ptr->entry_points_gl.pGLGetUniformiv,                                     "glGetUniformiv"},
        {&context_ptr->entry_points_gl.pGLGetUniformLocation,                               "glGetUniformLocation"},
        {&context_ptr->entry_points_gl.pGLGetUniformuiv,                                    "glGetUniformuiv"},
        {&context_ptr->entry_points_gl.pGLGetQueryiv,                                       "glGetQueryiv"},
        {&context_ptr->entry_points_gl.pGLGetQueryObjectiv,                                 "glGetQueryObjectiv"},
        {&context_ptr->entry_points_gl.pGLGetQueryObjectuiv,                                "glGetQueryObjectuiv"},
        {&context_ptr->entry_points_gl.pGLGetQueryObjectui64v,                              "glGetQueryObjectui64v"},
        {&context_ptr->entry_points_private.pGLGetSamplerParameterfv,                       "glGetSamplerParameterfv"},
        {&context_ptr->entry_points_private.pGLGetSamplerParameteriv,                       "glGetSamplerParameteriv"},
        {&context_ptr->entry_points_private.pGLGetSamplerParameterIiv,                      "glGetSamplerParameterIiv"},
        {&context_ptr->entry_points_private.pGLGetSamplerParameterIuiv,                     "glGetSamplerParameterIuiv"},
        {&context_ptr->entry_points_private.pGLGetVertexAttribdv,                           "glGetVertexAttribdv"},
        {&context_ptr->entry_points_private.pGLGetVertexAttribfv,                           "glGetVertexAttribfv"},
        {&context_ptr->entry_points_private.pGLGetVertexAttribIiv,                          "glGetVertexAttribIiv"},
        {&context_ptr->entry_points_private.pGLGetVertexAttribIuiv,                         "glGetVertexAttribIuiv"},
        {&context_ptr->entry_points_private.pGLGetVertexAttribiv,                           "glGetVertexAttribiv"},
        {&context_ptr->entry_points_private.pGLGetVertexAttribPointerv,                     "glGetVertexAttribPointerv"},
        {&context_ptr->entry_points_gl.pGLHint,                                             "glHint"},
        {&context_ptr->entry_points_gl.pGLInvalidateBufferData,                             "glInvalidateBufferData"},
        {&context_ptr->entry_points_gl.pGLInvalidateBufferSubData,                          "glInvalidateBufferSubData"},
        {&context_ptr->entry_points_private.pGLInvalidateFramebuffer,                       "glInvalidateFramebuffer"},
        {&context_ptr->entry_points_private.pGLInvalidateSubFramebuffer,                    "glInvalidateSubFramebuffer"},
        {&context_ptr->entry_points_gl.pGLInvalidateTexImage,                               "glInvalidateTexImage"},
        {&context_ptr->entry_points_gl.pGLInvalidateTexSubImage,                            "glInvalidateTexSubImage"},
        {&context_ptr->entry_points_gl.pGLIsBuffer,                                         "glIsBuffer"},
        {&context_ptr->entry_points_gl.pGLIsEnabled,                                        "glIsEnabled"},
        {&context_ptr->entry_points_gl.pGLIsEnabledi,                                       "glIsEnabledi"},
        {&context_ptr->entry_points_gl.pGLIsProgram,                                        "glIsProgram"},
        {&context_ptr->entry_points_gl.pGLIsShader,                                         "glIsShader"},
        {&context_ptr->entry_points_gl.pGLIsTexture,                                        "glIsTexture"},
        {&context_ptr->entry_points_gl.pGLIsTransformFeedback,                              "glIsTransformFeedback"},
        {&context_ptr->entry_points_gl.pGLIsQuery,                                          "glIsQuery"},
        {&context_ptr->entry_points_gl.pGLLineWidth,                                        "glLineWidth"},
        {&context_ptr->entry_points_gl.pGLLinkProgram,                                      "glLinkProgram"},
        {&context_ptr->entry_points_gl.pGLLogicOp,                                          "glLogicOp"},
        {&context_ptr->entry_points_private.pGLMapBuffer,                                   "glMapBuffer"},
        {&context_ptr->entry_points_private.pGLMapBufferRange,                              "glMapBufferRange"},
        {&context_ptr->entry_points_private.pGLMemoryBarrier,                               "glMemoryBarrier"},
        {&context_ptr->entry_points_private.pGLMultiDrawArrays,                             "glMultiDrawArrays"},
        {&context_ptr->entry_points_private.pGLMultiDrawArraysIndirect,                     "glMultiDrawArraysIndirect"},
        {&context_ptr->entry_points_private.pGLMultiDrawElements,                           "glMultiDrawElements"},
        {&context_ptr->entry_points_private.pGLMultiDrawElementsBaseVertex,                 "glMultiDrawElementsBaseVertex"},
        {&context_ptr->entry_points_private.pGLMultiDrawElementsIndirect,                   "glMultiDrawElementsIndirect"},
        {&context_ptr->entry_points_gl.pGLObjectLabel,                                      "glObjectLabel"},
        {&context_ptr->entry_points_gl.pGLObjectPtrLabel,                                   "glObjectPtrLabel"},
        {&context_ptr->entry_points_gl.pGLPauseTransformFeedback,                           "glPauseTransformFeedback"},
        {&context_ptr->entry_points_gl.pGLPixelStoref,                                      "glPixelStoref"},
        {&context_ptr->entry_points_gl.pGLPixelStorei,                                      "glPixelStorei"},
        {&context_ptr->entry_points_gl.pGLPointParameterf,                                  "glPointParameterf"},
        {&context_ptr->entry_points_gl.pGLPointParameterfv,                                 "glPointParameterfv"},
        {&context_ptr->entry_points_gl.pGLPointParameteri,                                  "glPointParameteri"},
        {&context_ptr->entry_points_gl.pGLPointParameteriv,                                 "glPointParameteriv"},
        {&context_ptr->entry_points_gl.pGLPolygonMode,                                      "glPolygonMode"},
        {&context_ptr->entry_points_gl.pGLPolygonOffset,                                    "glPolygonOffset"},
        {&context_ptr->entry_points_gl.pGLPopDebugGroup,                                    "glPopDebugGroup"},
        {&context_ptr->entry_points_gl.pGLPrimitiveRestartIndex,                            "glPrimitiveRestartIndex"},
        {&context_ptr->entry_points_gl.pGLProgramBinary,                                    "glProgramBinary"},
        {&context_ptr->entry_points_gl.pGLProgramParameteri,                                "glProgramParameteri"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1d,                                 "glProgramUniform1d"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1dv,                                "glProgramUniform1dv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1f,                                 "glProgramUniform1f"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1fv,                                "glProgramUniform1fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1i,                                 "glProgramUniform1i"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1iv,                                "glProgramUniform1iv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1ui,                                "glProgramUniform1ui"},
        {&context_ptr->entry_points_gl.pGLProgramUniform1uiv,                               "glProgramUniform1uiv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2d,                                 "glProgramUniform2d"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2dv,                                "glProgramUniform2dv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2f,                                 "glProgramUniform2f"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2fv,                                "glProgramUniform2fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2i,                                 "glProgramUniform2i"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2iv,                                "glProgramUniform2iv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2ui,                                "glProgramUniform2ui"},
        {&context_ptr->entry_points_gl.pGLProgramUniform2uiv,                               "glProgramUniform2uiv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3d,                                 "glProgramUniform3d"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3dv,                                "glProgramUniform3dv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3f,                                 "glProgramUniform3f"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3fv,                                "glProgramUniform3fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3i,                                 "glProgramUniform3i"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3iv,                                "glProgramUniform3iv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3ui,                                "glProgramUniform3ui"},
        {&context_ptr->entry_points_gl.pGLProgramUniform3uiv,                               "glProgramUniform3uiv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4d,                                 "glProgramUniform4d"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4dv,                                "glProgramUniform4dv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4f,                                 "glProgramUniform4f"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4fv,                                "glProgramUniform4fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4i,                                 "glProgramUniform4i"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4iv,                                "glProgramUniform4iv"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4ui,                                "glProgramUniform4ui"},
        {&context_ptr->entry_points_gl.pGLProgramUniform4uiv,                               "glProgramUniform4uiv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix2fv,                          "glProgramUniformMatrix2fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix2x3fv,                        "glProgramUniformMatrix2x3fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix2x4fv,                        "glProgramUniformMatrix2x4fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix3fv,                          "glProgramUniformMatrix3fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix3x2fv,                        "glProgramUniformMatrix3x2fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix3x4fv,                        "glProgramUniformMatrix3x4fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix4fv,                          "glProgramUniformMatrix4fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix4x2fv,                        "glProgramUniformMatrix4x2fv"},
        {&context_ptr->entry_points_gl.pGLProgramUniformMatrix4x3fv,                        "glProgramUniformMatrix4x3fv"},
        {&context_ptr->entry_points_private.pGLReadBuffer,                                  "glReadBuffer"},
        {&context_ptr->entry_points_private.pGLReadPixels,                                  "glReadPixels"},
        {&context_ptr->entry_points_private.pGLRenderbufferStorage,                         "glRenderbufferStorage"},
        {&context_ptr->entry_points_private.pGLRenderbufferStorageMultisample,              "glRenderbufferStorageMultisample"},
        {&context_ptr->entry_points_private.pGLResumeTransformFeedback,                     "glResumeTransformFeedback"},
        {&context_ptr->entry_points_gl.pGLPointSize,                                        "glPointSize"},
        {&context_ptr->entry_points_gl.pGLPushDebugGroup,                                   "glPushDebugGroup"},
        {&context_ptr->entry_points_gl.pGLSampleCoverage,                                   "glSampleCoverage"},
        {&context_ptr->entry_points_private.pGLSamplerParameterf,                           "glSamplerParameterf"},
        {&context_ptr->entry_points_private.pGLSamplerParameterfv,                          "glSamplerParameterfv"},
        {&context_ptr->entry_points_private.pGLSamplerParameterIiv,                         "glSamplerParameterIiv"},
        {&context_ptr->entry_points_private.pGLSamplerParameterIuiv,                        "glSamplerParameterIuiv"},
        {&context_ptr->entry_points_private.pGLSamplerParameteri,                           "glSamplerParameteri"},
        {&context_ptr->entry_points_private.pGLSamplerParameteriv,                          "glSamplerParameteriv"},
        {&context_ptr->entry_points_private.pGLScissor,                                     "glScissor"},
        {&context_ptr->entry_points_private.pGLShaderStorageBlockBinding,                   "glShaderStorageBlockBinding"},
        {&context_ptr->entry_points_gl.pGLShaderSource,                                     "glShaderSource"},
        {&context_ptr->entry_points_gl.pGLStencilFunc,                                      "glStencilFunc"},
        {&context_ptr->entry_points_gl.pGLStencilFuncSeparate,                              "glStencilFuncSeparate"},
        {&context_ptr->entry_points_gl.pGLStencilMask,                                      "glStencilMask"},
        {&context_ptr->entry_points_gl.pGLStencilMaskSeparate,                              "glStencilMaskSeparate"},
        {&context_ptr->entry_points_gl.pGLStencilOp,                                        "glStencilOp"},
        {&context_ptr->entry_points_gl.pGLStencilOpSeparate,                                "glStencilOpSeparate"},
        {&context_ptr->entry_points_private.pGLTexBuffer,                                   "glTexBuffer"},
        {&context_ptr->entry_points_private.pGLTexBufferRange,                              "glTexBufferRange"},
        {&context_ptr->entry_points_private.pGLTexImage1D,                                  "glTexImage1D"},
        {&context_ptr->entry_points_private.pGLTexImage2D,                                  "glTexImage2D"},
        {&context_ptr->entry_points_private.pGLTexImage3D,                                  "glTexImage3D"},
        {&context_ptr->entry_points_private.pGLTexParameterf,                               "glTexParameterf"},
        {&context_ptr->entry_points_private.pGLTexParameterfv,                              "glTexParameterfv"},
        {&context_ptr->entry_points_private.pGLTexParameteri,                               "glTexParameteri"},
        {&context_ptr->entry_points_private.pGLTexParameteriv,                              "glTexParameteriv"},
        {&context_ptr->entry_points_private.pGLTexParameterIiv,                             "glTexParameterIiv"},
        {&context_ptr->entry_points_private.pGLTexParameterIuiv,                            "glTexParameterIuiv"},
        {&context_ptr->entry_points_private.pGLTexStorage1D,                                "glTexStorage1D"},
        {&context_ptr->entry_points_private.pGLTexStorage2D,                                "glTexStorage2D"},
        {&context_ptr->entry_points_private.pGLTexStorage2DMultisample,                     "glTexStorage2DMultisample"},
        {&context_ptr->entry_points_private.pGLTexStorage3D,                                "glTexStorage3D"},
        {&context_ptr->entry_points_private.pGLTexStorage3DMultisample,                     "glTexStorage3DMultisample"},
        {&context_ptr->entry_points_private.pGLTexSubImage1D,                               "glTexSubImage1D"},
        {&context_ptr->entry_points_private.pGLTexSubImage2D,                               "glTexSubImage2D"},
        {&context_ptr->entry_points_private.pGLTexSubImage3D,                               "glTexSubImage3D"},
        {&context_ptr->entry_points_gl.pGLTextureView,                                      "glTextureView"},
        {&context_ptr->entry_points_gl.pGLTransformFeedbackVaryings,                        "glTransformFeedbackVaryings"},
        {&context_ptr->entry_points_private.pGLUniformBlockBinding,                         "glUniformBlockBinding"},
        {&context_ptr->entry_points_private.pGLUnmapBuffer,                                 "glUnmapBuffer"},
        {&context_ptr->entry_points_private.pGLUseProgram,                                  "glUseProgram"},
        {&context_ptr->entry_points_gl.pGLUseProgramStages,                                 "glUseProgramStages"},
        {&context_ptr->entry_points_gl.pGLValidateProgram,                                  "glValidateProgram"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib1d,                                   "glVertexAttrib1d"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib1dv,                                  "glVertexAttrib1dv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib1f,                                   "glVertexAttrib1f"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib1fv,                                  "glVertexAttrib1fv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib1s,                                   "glVertexAttrib1s"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib1sv,                                  "glVertexAttrib1sv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib2d,                                   "glVertexAttrib2d"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib2dv,                                  "glVertexAttrib2dv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib2f,                                   "glVertexAttrib2f"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib2fv,                                  "glVertexAttrib2fv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib2s,                                   "glVertexAttrib2s"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib2sv,                                  "glVertexAttrib2sv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib3d,                                   "glVertexAttrib3d"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib3dv,                                  "glVertexAttrib3dv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib3f,                                   "glVertexAttrib3f"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib3fv,                                  "glVertexAttrib3fv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib3s,                                   "glVertexAttrib3s"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib3sv,                                  "glVertexAttrib3sv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Nbv,                                 "glVertexAttrib4Nbv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Niv,                                 "glVertexAttrib4Niv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Nsv,                                 "glVertexAttrib4Nsv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Nub,                                 "glVertexAttrib4Nub"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Nubv,                                "glVertexAttrib4Nubv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Nuiv,                                "glVertexAttrib4Nuiv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4Nusv,                                "glVertexAttrib4Nusv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4bv,                                  "glVertexAttrib4bv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4d,                                   "glVertexAttrib4d"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4dv,                                  "glVertexAttrib4dv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4f,                                   "glVertexAttrib4f"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4fv,                                  "glVertexAttrib4fv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4iv,                                  "glVertexAttrib4iv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4s,                                   "glVertexAttrib4s"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4sv,                                  "glVertexAttrib4sv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4ubv,                                 "glVertexAttrib4ubv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4uiv,                                 "glVertexAttrib4uiv"},
        {&context_ptr->entry_points_gl.pGLVertexAttrib4usv,                                 "glVertexAttrib4usv"},
        {&context_ptr->entry_points_private.pGLVertexAttribBinding,                         "glVertexAttribBinding"},
        {&context_ptr->entry_points_private.pGLVertexAttribFormat,                          "glVertexAttribFormat"},
        {&context_ptr->entry_points_gl.pGLVertexAttribDivisor,                              "glVertexAttribDivisor"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI1i,                                  "glVertexAttribI1i"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI1iv,                                 "glVertexAttribI1iv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI1ui,                                 "glVertexAttribI1ui"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI1uiv,                                "glVertexAttribI1uiv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI2i,                                  "glVertexAttribI2i"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI2iv,                                 "glVertexAttribI2iv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI2ui,                                 "glVertexAttribI2ui"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI2uiv,                                "glVertexAttribI2uiv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI3i,                                  "glVertexAttribI3i"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI3iv,                                 "glVertexAttribI3iv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI3ui,                                 "glVertexAttribI3ui"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI3uiv,                                "glVertexAttribI3uiv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4bv,                                 "glVertexAttribI4bv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4i,                                  "glVertexAttribI4i"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4iv,                                 "glVertexAttribI4iv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4sv,                                 "glVertexAttribI4sv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4ubv,                                "glVertexAttribI4ubv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4ui,                                 "glVertexAttribI4ui"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4uiv,                                "glVertexAttribI4uiv"},
        {&context_ptr->entry_points_gl.pGLVertexAttribI4usv,                                "glVertexAttribI4usv"},
        {&context_ptr->entry_points_private.pGLVertexAttribIFormat,                         "glVertexAttribIFormat"},
        {&context_ptr->entry_points_private.pGLVertexAttribIPointer,                        "glVertexAttribIPointer"},
        {&context_ptr->entry_points_private.pGLVertexAttribLFormat,                         "glVertexAttribLFormat"},
        {&context_ptr->entry_points_private.pGLVertexAttribPointer,                         "glVertexAttribPointer"},
        {&context_ptr->entry_points_private.pGLViewport,                                    "glViewport"}
    };
    const uint32_t n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    _ogl_context_get_function_pointers(context_ptr,
                                       func_ptr_table,
                                       n_func_ptr_table_entries);

    /* Configure wrapper functions */
    context_ptr->entry_points_gl.pGLActiveTexture                               = ogl_context_wrappers_glActiveTexture;
    context_ptr->entry_points_gl.pGLBeginTransformFeedback                      = ogl_context_wrappers_glBeginTransformFeedback;
    context_ptr->entry_points_gl.pGLBindBuffer                                  = ogl_context_wrappers_glBindBuffer;
    context_ptr->entry_points_gl.pGLBindBufferBase                              = ogl_context_wrappers_glBindBufferBase;
    context_ptr->entry_points_gl.pGLBindBufferRange                             = ogl_context_wrappers_glBindBufferRange;
    context_ptr->entry_points_gl.pGLBindFramebuffer                             = ogl_context_wrappers_glBindFramebuffer;
    context_ptr->entry_points_gl.pGLBindImageTexture                            = ogl_context_wrappers_glBindImageTextureEXT;
    context_ptr->entry_points_gl.pGLBindRenderbuffer                            = ogl_context_wrappers_glBindRenderbuffer;
    context_ptr->entry_points_gl.pGLBindSampler                                 = ogl_context_wrappers_glBindSampler;
    context_ptr->entry_points_gl.pGLBindTexture                                 = ogl_context_wrappers_glBindTexture;
    context_ptr->entry_points_gl.pGLBindVertexArray                             = ogl_context_wrappers_glBindVertexArray;
    context_ptr->entry_points_gl.pGLBlendColor                                  = ogl_context_wrappers_glBlendColor;
    context_ptr->entry_points_gl.pGLBlendEquation                               = ogl_context_wrappers_glBlendEquation;
    context_ptr->entry_points_gl.pGLBlendEquationSeparate                       = ogl_context_wrappers_glBlendEquationSeparate;
    context_ptr->entry_points_gl.pGLBlendFunc                                   = ogl_context_wrappers_glBlendFunc;
    context_ptr->entry_points_gl.pGLBlendFuncSeparate                           = ogl_context_wrappers_glBlendFuncSeparate;
    context_ptr->entry_points_gl.pGLBlitFramebuffer                             = ogl_context_wrappers_glBlitFramebuffer;
    context_ptr->entry_points_gl.pGLBufferData                                  = ogl_context_wrappers_glBufferData;
    context_ptr->entry_points_gl.pGLBufferSubData                               = ogl_context_wrappers_glBufferSubData;
    context_ptr->entry_points_gl.pGLClear                                       = ogl_context_wrappers_glClear;
    context_ptr->entry_points_gl.pGLClearBufferData                             = ogl_context_wrappers_glClearBufferData;
    context_ptr->entry_points_gl.pGLClearBufferSubData                          = ogl_context_wrappers_glClearBufferSubData;
    context_ptr->entry_points_gl.pGLClearColor                                  = ogl_context_wrappers_glClearColor;
    context_ptr->entry_points_gl.pGLClearDepth                                  = ogl_context_wrappers_glClearDepth;
    context_ptr->entry_points_gl.pGLColorMask                                   = ogl_context_wrappers_glColorMask;
    context_ptr->entry_points_gl.pGLCompressedTexSubImage1D                     = ogl_context_wrappers_glCompressedTexSubImage1D;
    context_ptr->entry_points_gl.pGLCompressedTexSubImage2D                     = ogl_context_wrappers_glCompressedTexSubImage2D;
    context_ptr->entry_points_gl.pGLCompressedTexSubImage3D                     = ogl_context_wrappers_glCompressedTexSubImage3D;
    context_ptr->entry_points_gl.pGLCopyBufferSubData                           = ogl_context_wrappers_glCopyBufferSubData;
    context_ptr->entry_points_gl.pGLCopyTexImage1D                              = ogl_context_wrappers_glCopyTexImage1D;
    context_ptr->entry_points_gl.pGLCopyTexImage2D                              = ogl_context_wrappers_glCopyTexImage2D;
    context_ptr->entry_points_gl.pGLCopyTexSubImage1D                           = ogl_context_wrappers_glCopyTexSubImage1D;
    context_ptr->entry_points_gl.pGLCopyTexSubImage2D                           = ogl_context_wrappers_glCopyTexSubImage2D;
    context_ptr->entry_points_gl.pGLCopyTexSubImage3D                           = ogl_context_wrappers_glCopyTexSubImage3D;
    context_ptr->entry_points_gl.pGLCullFace                                    = ogl_context_wrappers_glCullFace;
    context_ptr->entry_points_gl.pGLDeleteBuffers                               = ogl_context_wrappers_glDeleteBuffers;
    context_ptr->entry_points_gl.pGLDeleteRenderbuffers                         = ogl_context_wrappers_glDeleteRenderbuffers;
    context_ptr->entry_points_gl.pGLDeleteVertexArrays                          = ogl_context_wrappers_glDeleteVertexArrays;
    context_ptr->entry_points_gl.pGLDepthFunc                                   = ogl_context_wrappers_glDepthFunc;
    context_ptr->entry_points_gl.pGLDepthMask                                   = ogl_context_wrappers_glDepthMask;
    context_ptr->entry_points_gl.pGLDisable                                     = ogl_context_wrappers_glDisable;
    context_ptr->entry_points_gl.pGLDisablei                                    = ogl_context_wrappers_glDisablei;
    context_ptr->entry_points_gl.pGLDisableVertexAttribArray                    = ogl_context_wrappers_glDisableVertexAttribArray;
    context_ptr->entry_points_gl.pGLDispatchCompute                             = ogl_context_wrappers_glDispatchCompute;
    context_ptr->entry_points_gl.pGLDispatchComputeIndirect                     = ogl_context_wrappers_glDispatchComputeIndirect;
    context_ptr->entry_points_gl.pGLDrawArrays                                  = ogl_context_wrappers_glDrawArrays;
    context_ptr->entry_points_gl.pGLDrawArraysIndirect                          = ogl_context_wrappers_glDrawArraysIndirect;
    context_ptr->entry_points_gl.pGLDrawArraysInstanced                         = ogl_context_wrappers_glDrawArraysInstanced;
    context_ptr->entry_points_gl.pGLDrawArraysInstancedBaseInstance             = ogl_context_wrappers_glDrawArraysInstancedBaseInstance;
    context_ptr->entry_points_gl.pGLDrawBuffer                                  = ogl_context_wrappers_glDrawBuffer;
    context_ptr->entry_points_gl.pGLDrawBuffers                                 = ogl_context_wrappers_glDrawBuffers;
    context_ptr->entry_points_gl.pGLDrawElements                                = ogl_context_wrappers_glDrawElements;
    context_ptr->entry_points_gl.pGLDrawElementsInstanced                       = ogl_context_wrappers_glDrawElementsInstanced;
    context_ptr->entry_points_gl.pGLDrawElementsInstancedBaseInstance           = ogl_context_wrappers_glDrawElementsInstancedBaseInstance;
    context_ptr->entry_points_gl.pGLDrawElementsInstancedBaseVertexBaseInstance = ogl_context_wrappers_glDrawElementsInstancedBaseVertexBaseInstance;
    context_ptr->entry_points_gl.pGLDrawRangeElements                           = ogl_context_wrappers_glDrawRangeElements;
    context_ptr->entry_points_gl.pGLDrawTransformFeedback                       = ogl_context_wrappers_glDrawTransformFeedback;
    context_ptr->entry_points_gl.pGLDrawTransformFeedbackInstanced              = ogl_context_wrappers_glDrawTransformFeedbackInstanced;
    context_ptr->entry_points_gl.pGLDrawTransformFeedbackStreamInstanced        = ogl_context_wrappers_glDrawTransformFeedbackStreamInstanced;
    context_ptr->entry_points_gl.pGLEnable                                      = ogl_context_wrappers_glEnable;
    context_ptr->entry_points_gl.pGLEnablei                                     = ogl_context_wrappers_glEnablei;
    context_ptr->entry_points_gl.pGLEnableVertexAttribArray                     = ogl_context_wrappers_glEnableVertexAttribArray;
    context_ptr->entry_points_gl.pGLFramebufferParameteri                       = ogl_context_wrappers_glFramebufferParameteri;
    context_ptr->entry_points_gl.pGLFramebufferRenderbuffer                     = ogl_context_wrappers_glFramebufferRenderbuffer;
    context_ptr->entry_points_gl.pGLFramebufferTexture                          = ogl_context_wrappers_glFramebufferTexture;
    context_ptr->entry_points_gl.pGLFramebufferTexture1D                        = ogl_context_wrappers_glFramebufferTexture1D;
    context_ptr->entry_points_gl.pGLFramebufferTexture2D                        = ogl_context_wrappers_glFramebufferTexture2D;
    context_ptr->entry_points_gl.pGLFramebufferTexture3D                        = ogl_context_wrappers_glFramebufferTexture3D;
    context_ptr->entry_points_gl.pGLFramebufferTextureLayer                     = ogl_context_wrappers_glFramebufferTextureLayer;
    context_ptr->entry_points_gl.pGLFrontFace                                   = ogl_context_wrappers_glFrontFace;
    context_ptr->entry_points_gl.pGLGenerateMipmap                              = ogl_context_wrappers_glGenerateMipmap;
    context_ptr->entry_points_gl.pGLGenVertexArrays                             = ogl_context_wrappers_glGenVertexArrays;
    context_ptr->entry_points_gl.pGLGetActiveAtomicCounterBufferiv              = ogl_context_wrappers_glGetActiveAtomicCounterBufferiv;
    context_ptr->entry_points_gl.pGLGetBooleani_v                               = ogl_context_wrappers_glGetBooleani_v;
    context_ptr->entry_points_gl.pGLGetBooleanv                                 = ogl_context_wrappers_glGetBooleanv;
    context_ptr->entry_points_gl.pGLGetBufferParameteriv                        = ogl_context_wrappers_glGetBufferParameteriv;
    context_ptr->entry_points_gl.pGLGetBufferParameteri64v                      = ogl_context_wrappers_glGetBufferParameteri64v;
    context_ptr->entry_points_gl.pGLGetBufferPointerv                           = ogl_context_wrappers_glGetBufferPointerv;
    context_ptr->entry_points_gl.pGLGetBufferSubData                            = ogl_context_wrappers_glGetBufferSubData;
    context_ptr->entry_points_gl.pGLGetCompressedTexImage                       = ogl_context_wrappers_glGetCompressedTexImage;
    context_ptr->entry_points_gl.pGLGetDoublev                                  = ogl_context_wrappers_glGetDoublev;
    context_ptr->entry_points_gl.pGLGetFloatv                                   = ogl_context_wrappers_glGetFloatv;
    context_ptr->entry_points_gl.pGLGetFramebufferParameteriv                   = ogl_context_wrappers_glGetFramebufferParameteriv;
    context_ptr->entry_points_gl.pGLGetInteger64i_v                             = ogl_context_wrappers_glGetInteger64i_v;
    context_ptr->entry_points_gl.pGLGetIntegeri_v                               = ogl_context_wrappers_glGetIntegeri_v;
    context_ptr->entry_points_gl.pGLGetIntegerv                                 = ogl_context_wrappers_glGetIntegerv;
    context_ptr->entry_points_gl.pGLGetRenderbufferParameteriv                  = ogl_context_wrappers_glGetRenderbufferParameteriv;
    context_ptr->entry_points_gl.pGLGetSamplerParameterfv                       = ogl_context_wrappers_glGetSamplerParameterfv;
    context_ptr->entry_points_gl.pGLGetSamplerParameteriv                       = ogl_context_wrappers_glGetSamplerParameteriv;
    context_ptr->entry_points_gl.pGLGetSamplerParameterIiv                      = ogl_context_wrappers_glGetSamplerParameterIiv;
    context_ptr->entry_points_gl.pGLGetSamplerParameterIuiv                     = ogl_context_wrappers_glGetSamplerParameterIuiv;
    context_ptr->entry_points_gl.pGLGetTexImage                                 = ogl_context_wrappers_glGetTexImage;
    context_ptr->entry_points_gl.pGLGetTexLevelParameterfv                      = ogl_context_wrappers_glGetTexLevelParameterfv;
    context_ptr->entry_points_gl.pGLGetTexLevelParameteriv                      = ogl_context_wrappers_glGetTexLevelParameteriv;
    context_ptr->entry_points_gl.pGLGetTexParameterfv                           = ogl_context_wrappers_glGetTexParameterfv;
    context_ptr->entry_points_gl.pGLGetTexParameterIiv                          = ogl_context_wrappers_glGetTexParameterIiv;
    context_ptr->entry_points_gl.pGLGetTexParameterIuiv                         = ogl_context_wrappers_glGetTexParameterIuiv;
    context_ptr->entry_points_gl.pGLGetTexParameteriv                           = ogl_context_wrappers_glGetTexParameteriv;
    context_ptr->entry_points_gl.pGLGetVertexAttribdv                           = ogl_context_wrappers_glGetVertexAttribdv;
    context_ptr->entry_points_gl.pGLGetVertexAttribfv                           = ogl_context_wrappers_glGetVertexAttribfv;
    context_ptr->entry_points_gl.pGLGetVertexAttribIiv                          = ogl_context_wrappers_glGetVertexAttribIiv;
    context_ptr->entry_points_gl.pGLGetVertexAttribIuiv                         = ogl_context_wrappers_glGetVertexAttribIuiv;
    context_ptr->entry_points_gl.pGLGetVertexAttribiv                           = ogl_context_wrappers_glGetVertexAttribiv;
    context_ptr->entry_points_gl.pGLGetVertexAttribPointerv                     = ogl_context_wrappers_glGetVertexAttribPointerv;
    context_ptr->entry_points_gl.pGLInvalidateFramebuffer                       = ogl_context_wrappers_glInvalidateFramebuffer;
    context_ptr->entry_points_gl.pGLInvalidateSubFramebuffer                    = ogl_context_wrappers_glInvalidateSubFramebuffer;
    context_ptr->entry_points_gl.pGLMapBuffer                                   = ogl_context_wrappers_glMapBuffer;
    context_ptr->entry_points_gl.pGLMapBufferRange                              = ogl_context_wrappers_glMapBufferRange;
    context_ptr->entry_points_gl.pGLMemoryBarrier                               = ogl_context_wrappers_glMemoryBarrier;
    context_ptr->entry_points_gl.pGLMultiDrawArrays                             = ogl_context_wrappers_glMultiDrawArrays;
    context_ptr->entry_points_gl.pGLMultiDrawArraysIndirect                     = ogl_context_wrappers_glMultiDrawArraysIndirect;
    context_ptr->entry_points_gl.pGLMultiDrawElements                           = ogl_context_wrappers_glMultiDrawElements;
    context_ptr->entry_points_gl.pGLMultiDrawElementsBaseVertex                 = ogl_context_wrappers_glMultiDrawElementsBaseVertex;
    context_ptr->entry_points_gl.pGLMultiDrawElementsIndirect                   = ogl_context_wrappers_glMultiDrawElementsIndirect;
    context_ptr->entry_points_gl.pGLReadBuffer                                  = ogl_context_wrappers_glReadBuffer;
    context_ptr->entry_points_gl.pGLReadPixels                                  = ogl_context_wrappers_glReadPixels;
    context_ptr->entry_points_gl.pGLRenderbufferStorage                         = ogl_context_wrappers_glRenderbufferStorage;
    context_ptr->entry_points_gl.pGLRenderbufferStorageMultisample              = ogl_context_wrappers_glRenderbufferStorageMultisample;
    context_ptr->entry_points_gl.pGLResumeTransformFeedback                     = ogl_context_wrappers_glResumeTransformFeedback;
    context_ptr->entry_points_gl.pGLSamplerParameterf                           = ogl_context_wrappers_glSamplerParameterf;
    context_ptr->entry_points_gl.pGLSamplerParameterfv                          = ogl_context_wrappers_glSamplerParameterfv;
    context_ptr->entry_points_gl.pGLSamplerParameterIiv                         = ogl_context_wrappers_glSamplerParameterIiv;
    context_ptr->entry_points_gl.pGLSamplerParameterIuiv                        = ogl_context_wrappers_glSamplerParameterIuiv;
    context_ptr->entry_points_gl.pGLSamplerParameteri                           = ogl_context_wrappers_glSamplerParameteri;
    context_ptr->entry_points_gl.pGLSamplerParameteriv                          = ogl_context_wrappers_glSamplerParameteriv;
    context_ptr->entry_points_gl.pGLScissor                                     = ogl_context_wrappers_glScissor;
    context_ptr->entry_points_gl.pGLShaderStorageBlockBinding                   = ogl_context_wrappers_glShaderStorageBlockBinding;
    context_ptr->entry_points_gl.pGLTexBuffer                                   = ogl_context_wrappers_glTexBuffer;
    context_ptr->entry_points_gl.pGLTexBufferRange                              = ogl_context_wrappers_glTexBufferRange;
    context_ptr->entry_points_gl.pGLTexParameterf                               = ogl_context_wrappers_glTexParameterf;
    context_ptr->entry_points_gl.pGLTexParameterfv                              = ogl_context_wrappers_glTexParameterfv;
    context_ptr->entry_points_gl.pGLTexParameterIiv                             = ogl_context_wrappers_glTexParameterIiv;
    context_ptr->entry_points_gl.pGLTexParameterIuiv                            = ogl_context_wrappers_glTexParameterIuiv;
    context_ptr->entry_points_gl.pGLTexParameteri                               = ogl_context_wrappers_glTexParameteri;
    context_ptr->entry_points_gl.pGLTexParameteriv                              = ogl_context_wrappers_glTexParameteriv;
    context_ptr->entry_points_gl.pGLTexStorage1D                                = ogl_context_wrappers_glTexStorage1D;
    context_ptr->entry_points_gl.pGLTexStorage2D                                = ogl_context_wrappers_glTexStorage2D;
    context_ptr->entry_points_gl.pGLTexStorage2DMultisample                     = ogl_context_wrappers_glTexStorage2DMultisample;
    context_ptr->entry_points_gl.pGLTexStorage3D                                = ogl_context_wrappers_glTexStorage3D;
    context_ptr->entry_points_gl.pGLTexStorage3DMultisample                     = ogl_context_wrappers_glTexStorage3DMultisample;
    context_ptr->entry_points_gl.pGLTexSubImage1D                               = ogl_context_wrappers_glTexSubImage1D;
    context_ptr->entry_points_gl.pGLTexSubImage2D                               = ogl_context_wrappers_glTexSubImage2D;
    context_ptr->entry_points_gl.pGLTexSubImage3D                               = ogl_context_wrappers_glTexSubImage3D;
    context_ptr->entry_points_gl.pGLUniformBlockBinding                         = ogl_context_wrappers_glUniformBlockBinding;
    context_ptr->entry_points_gl.pGLUnmapBuffer                                 = ogl_context_wrappers_glUnmapBuffer;
    context_ptr->entry_points_gl.pGLUseProgram                                  = ogl_context_wrappers_glUseProgram;
    context_ptr->entry_points_gl.pGLVertexAttribBinding                         = ogl_context_wrappers_glVertexAttribBinding;
    context_ptr->entry_points_gl.pGLVertexAttribFormat                          = ogl_context_wrappers_glVertexAttribFormat;
    context_ptr->entry_points_gl.pGLVertexAttribIFormat                         = ogl_context_wrappers_glVertexAttribIFormat;
    context_ptr->entry_points_gl.pGLVertexAttribIPointer                        = ogl_context_wrappers_glVertexAttribIPointer;
    context_ptr->entry_points_gl.pGLVertexAttribLFormat                         = ogl_context_wrappers_glVertexAttribLFormat;
    context_ptr->entry_points_gl.pGLVertexAttribPointer                         = ogl_context_wrappers_glVertexAttribPointer;
    context_ptr->entry_points_gl.pGLViewport                                    = ogl_context_wrappers_glViewport;
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_info(_ogl_context* context_ptr)
{
    PFNGLGETSTRINGPROC  pGLGetString  = NULL;
    PFNGLGETSTRINGIPROC pGLGetStringi = NULL;

    if (context_ptr->context_type == OGL_CONTEXT_TYPE_ES)
    {
        pGLGetString  = context_ptr->entry_points_es.pGLGetString;
        pGLGetStringi = context_ptr->entry_points_es.pGLGetStringi;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        pGLGetString  = context_ptr->entry_points_gl.pGLGetString;
        pGLGetStringi = context_ptr->entry_points_gl.pGLGetStringi;
    }

    context_ptr->info.vendor                   = pGLGetString(GL_VENDOR);
    context_ptr->info.renderer                 = pGLGetString(GL_RENDERER);
    context_ptr->info.version                  = pGLGetString(GL_VERSION);
    context_ptr->info.shading_language_version = pGLGetString(GL_SHADING_LANGUAGE_VERSION);

    /* Retrieve extensions supported by the implementation */
    if (context_ptr->context_type == OGL_CONTEXT_TYPE_ES)
    {
        context_ptr->entry_points_es.pGLGetIntegerv(GL_NUM_EXTENSIONS,
                                                   &context_ptr->limits.num_extensions);
    }
    else
    {
        context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_EXTENSIONS,
                                                         &context_ptr->limits.num_extensions);
    }

    _ogl_context_gl_info_init(&context_ptr->info,
                              &context_ptr->limits);

    if (context_ptr->info.extensions != NULL)
    {
        for (int n_extension = 0;
                 n_extension < context_ptr->limits.num_extensions;
               ++n_extension)
        {
            context_ptr->info.extensions[n_extension] = pGLGetStringi(GL_EXTENSIONS,
                                                                      n_extension);

            LOG_INFO("%s",
                     context_ptr->info.extensions[n_extension]);

            ASSERT_ALWAYS_SYNC(context_ptr->info.extensions[n_extension] != NULL,
                               "GL reports %d extensions supported but %dth extension name is NULL!",
                               context_ptr->limits.num_extensions,
                               n_extension);
        }
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_limits(_ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "_ogl_context_retrieve_GL_limits() called for a non-GL context");

    /* NOTE: This function uses private entry-points, because at the time of the call,
     *       the entry-points have not been initialized yet
     */
    context_ptr->entry_points_private.pGLGetFloatv  (GL_ALIASED_LINE_WIDTH_RANGE,
                                                     context_ptr->limits.aliased_line_width_range);
    context_ptr->entry_points_private.pGLGetFloatv  (GL_SMOOTH_LINE_WIDTH_RANGE,
                                                     context_ptr->limits.smooth_line_width_range);
    context_ptr->entry_points_private.pGLGetFloatv  (GL_LINE_WIDTH_GRANULARITY,
                                                    &context_ptr->limits.line_width_granularity);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAJOR_VERSION,
                                                    &context_ptr->limits.major_version);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,
                                                    &context_ptr->limits.max_3d_texture_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,
                                                    &context_ptr->limits.max_array_texture_layers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,
                                                    &context_ptr->limits.max_atomic_counter_buffer_bindings);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE,
                                                    &context_ptr->limits.max_atomic_counter_buffer_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,
                                                    &context_ptr->limits.max_color_attachments);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES,
                                                    &context_ptr->limits.max_color_texture_samples);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_combined_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_combined_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_combined_compute_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_combined_fragment_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS,
                                                    &context_ptr->limits.max_combined_image_units_and_fragment_outputs);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_combined_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                                                    &context_ptr->limits.max_combined_texture_image_units);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_combined_vertex_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_combined_geometry_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,
                                                    &context_ptr->limits.max_combined_uniform_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_compute_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_compute_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_IMAGE_UNIFORMS,
                                                    &context_ptr->limits.max_compute_image_uniforms);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_compute_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,
                                                    &context_ptr->limits.max_compute_shared_memory_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS,
                                                    &context_ptr->limits.max_compute_texture_image_units);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                                                    &context_ptr->limits.max_compute_work_group_invocations);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS,
                                                    &context_ptr->limits.max_compute_uniform_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_compute_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                                                    &context_ptr->limits.max_cube_map_texture_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES,
                                                    &context_ptr->limits.max_depth_texture_samples);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_DRAW_BUFFERS,
                                                    &context_ptr->limits.max_draw_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ELEMENTS_INDICES,
                                                    &context_ptr->limits.max_elements_indices);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ELEMENTS_VERTICES,
                                                    &context_ptr->limits.max_elements_vertices);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_fragment_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_fragment_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_fragment_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
                                                    &context_ptr->limits.max_fragment_uniform_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                                                    &context_ptr->limits.max_fragment_input_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_fragment_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT,
                                                    &context_ptr->limits.max_framebuffer_height);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_LAYERS,
                                                    &context_ptr->limits.max_framebuffer_layers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_SAMPLES,
                                                    &context_ptr->limits.max_framebuffer_samples);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH,
                                                    &context_ptr->limits.max_framebuffer_width);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_geometry_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_geometry_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_INPUT_COMPONENTS,
                                                    &context_ptr->limits.max_geometry_input_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES,
                                                    &context_ptr->limits.max_geometry_output_vertices);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_geometry_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS,
                                                    &context_ptr->limits.max_geometry_texture_image_units);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_BLOCKS,
                                                    &context_ptr->limits.max_geometry_uniform_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_geometry_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_IMAGE_SAMPLES,
                                                    &context_ptr->limits.max_image_samples);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_IMAGE_UNITS,
                                                    &context_ptr->limits.max_image_units);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_INTEGER_SAMPLES,
                                                    &context_ptr->limits.max_integer_samples);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET,
                                                    &context_ptr->limits.min_program_texel_offset);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET,
                                                    &context_ptr->limits.max_program_texel_offset);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE,
                                                    &context_ptr->limits.max_rectangle_texture_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,
                                                    &context_ptr->limits.max_renderbuffer_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS,
                                                    &context_ptr->limits.max_sample_mask_words);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SERVER_WAIT_TIMEOUT,
                                                    &context_ptr->limits.max_server_wait_timeout);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,
                                                    &context_ptr->limits.max_shader_storage_block_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
                                                    &context_ptr->limits.max_shader_storage_buffer_bindings);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_tess_control_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_tess_control_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_tess_control_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_tess_evaluation_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_tess_evaluation_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_tess_evaluation_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,
                                                    &context_ptr->limits.max_texture_buffer_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                                                    &context_ptr->limits.max_texture_image_units);
    context_ptr->entry_points_private.pGLGetFloatv  (GL_MAX_TEXTURE_LOD_BIAS,
                                                    &context_ptr->limits.max_texture_lod_bias);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TEXTURE_SIZE,
                                                    &context_ptr->limits.max_texture_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS,
                                                    &context_ptr->limits.max_transform_feedback_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                                                    &context_ptr->limits.max_transform_feedback_interleaved_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                                                    &context_ptr->limits.max_transform_feedback_separate_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,
                                                    &context_ptr->limits.max_uniform_block_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                                                    &context_ptr->limits.max_uniform_buffer_bindings);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,
                                                    &context_ptr->limits.max_uniform_block_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS,
                                                    &context_ptr->limits.max_vertex_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS,
                                                    &context_ptr->limits.max_vertex_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_ATTRIBS,
                                                    &context_ptr->limits.max_vertex_attribs);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                                                    &context_ptr->limits.max_vertex_output_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS,
                                                    &context_ptr->limits.max_vertex_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                                                    &context_ptr->limits.max_vertex_texture_image_units);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS,
                                                    &context_ptr->limits.max_vertex_uniform_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                                                    &context_ptr->limits.max_vertex_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MINOR_VERSION,
                                                    &context_ptr->limits.minor_version);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                                                    &context_ptr->limits.num_compressed_texture_formats);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_EXTENSIONS,
                                                    &context_ptr->limits.num_extensions);
    context_ptr->entry_points_private.pGLGetFloatv  (GL_POINT_FADE_THRESHOLD_SIZE,
                                                    &context_ptr->limits.point_fade_threshold_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_POINT_SIZE_RANGE,
                                                    context_ptr->limits.point_size_range);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,
                                                    &context_ptr->limits.shader_storage_buffer_offset_alignment);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_SUBPIXEL_BITS,
                                                    &context_ptr->limits.subpixel_bits);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT,
                                                    &context_ptr->limits.texture_buffer_offset_alignment);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                                                    &context_ptr->limits.uniform_buffer_offset_alignment);

    for (int index = 0;
             index < 3 /* x, y, z */;
           ++index)
    {
        context_ptr->entry_points_private.pGLGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,
                                                           index,
                                                           context_ptr->limits.max_compute_work_group_count + index);
        context_ptr->entry_points_private.pGLGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,
                                                           index,
                                                           context_ptr->limits.max_compute_work_group_size + index);
    }

    /* Store min max GL constant values, as per OpenGL 4.3 spec */
    context_ptr->limits.min_max_uniform_block_size = 16384;

    /* Retrieve "program binary" limits */
    context_ptr->limits.num_program_binary_formats = 0;
    context_ptr->limits.program_binary_formats     = NULL;

    context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,
                                                    &context_ptr->limits.num_program_binary_formats);

    if (context_ptr->limits.num_program_binary_formats != 0)
    {
        context_ptr->limits.program_binary_formats = new (std::nothrow) GLint[context_ptr->limits.num_program_binary_formats];

        ASSERT_ALWAYS_SYNC(context_ptr->limits.program_binary_formats != NULL,
                           "Could not allocate space for supported program binary formats array");

        if (context_ptr->limits.program_binary_formats != NULL)
        {
            context_ptr->entry_points_private.pGLGetIntegerv(GL_PROGRAM_BINARY_FORMATS,
                                                             context_ptr->limits.program_binary_formats);
        }
    } /* if (context_ptr->limits.num_program_binary_formats != 0) */
    else
    {
        context_ptr->limits.program_binary_formats = NULL;
    }
}

/** TODO */
PRIVATE bool _ogl_context_sort_descending(void* in_int_1,
                                          void* in_int_2)
{
    int int_1 = (int) in_int_1;
    int int_2 = (int) in_int_2;

    return (int_1 > int_2);
}


/** Please see header for specification */
PUBLIC void ogl_context_bind_to_current_thread(ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    ASSERT_DEBUG_SYNC(context_ptr != NULL,
                      "Input argument is NULL");

    if (context_ptr != NULL)
    {
        context_ptr->pfn_bind_to_current_thread(context_ptr->context_platform);

        _current_context = context;

        ogl_context_wrappers_set_private_functions(&context_ptr->entry_points_private);
    }
}

/** Please see header for specification */
PUBLIC void ogl_context_unbind_from_current_thread(ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    ASSERT_DEBUG_SYNC(context_ptr != NULL,
                      "Input argument is NULL");

    if (context_ptr != NULL)
    {
        context_ptr->pfn_unbind_from_current_thread(context_ptr->context_platform);

        _current_context = NULL;
        ogl_context_wrappers_set_private_functions(NULL);
    }
}

/** GL context creation handler. Queries the operating system to select the most suitable pixel format,
 *  creates the WGL context, allows OGL_info to update its data model and returns a ogl_context instance.
 *
 *  @param name          TODO
 *  @param type          TODO
 *  @param window        TODO
 *  @param in_pfd        Pixel format descriptor object instance.
 *  @param vsync_enabled TODO
 *  @param share_context TODO
 *  @param support_multisampling True if multisampling info should be extracted and made available. Slows down window creation.
 *
 *  @return A new ogl_context instance, if successful, or NULL otherwise.
*/
PUBLIC EMERALD_API ogl_context ogl_context_create_from_system_window(system_hashed_ansi_string name,
                                                                     ogl_context_type          type,
                                                                     system_window             window,
                                                                     bool                      vsync_enabled,
                                                                     ogl_context               parent_context)
{
    system_pixel_format window_pf      = NULL;
    system_pixel_format window_pf_copy = NULL;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &window_pf);

    window_pf_copy = system_pixel_format_create_copy(window_pf);

    /* The new context takes over the ownership of the duplicate pixel format object instance */
    return _ogl_context_create_from_system_window_shared(name,
                                                         type,
                                                         window,
                                                         window_pf,
                                                         vsync_enabled,
                                                         parent_context);
}

/* Please see header for spec */
PUBLIC void ogl_context_deinit_global()
{
    #ifdef _WIN32
    {
        /* Nothing to do */
    }
    #else
    {
        ogl_context_linux_deinit_global();
    }
    #endif
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_context_enumerate_msaa_samples(system_pixel_format pf,
                                                           unsigned int*       out_n_supported_samples,
                                                           unsigned int**      out_supported_samples)
{
    bool                    consider_color_internalformats         = false;
    bool                    consider_depth_stencil_internalformats = false;
    system_resizable_vector depth_stencil_samples_vector           = NULL;
    GLenum                  internalformat_color                   = GL_NONE;
    GLenum                  internalformat_depth_stencil           = GL_NONE;
    bool                    result                                 = true;
    system_resizable_vector result_vector                          = NULL;
    system_window           root_window                            = NULL;
    ogl_context             root_window_context                    = NULL;
    _ogl_context*           root_window_context_ptr                = NULL;

    result = _ogl_context_get_attachment_internalformats_for_system_pixel_format(pf,
                                                                                &internalformat_color,
                                                                                &internalformat_depth_stencil);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not determine color/depth/stencil attachment internalformats for user-specified PF");

        goto end;
    }

    /* Determine the number of samples which can be used for the specified internalformats */
    root_window = system_window_get_root_window();

    if (root_window == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not obtain root window");

        result = false;
        goto end;
    }

    system_window_get_property(root_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &root_window_context);

    if (root_window_context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Root window's rendering context is NULL");

        result = false;
        goto end;
    }

    root_window_context_ptr = (_ogl_context*) root_window_context;

    root_window_context_ptr->msaa_enumeration_color_internalformat         = internalformat_color;
    root_window_context_ptr->msaa_enumeration_depth_stencil_internalformat = internalformat_depth_stencil;

    ogl_context_request_callback_from_context_thread(root_window_context,
                                                     _ogl_context_enumerate_msaa_samples_rendering_thread_callback,
                                                     NULL); /* user_arg */

    /* Extract the information we gathered in the rendering thread. Since the number of samples
     * used for color & depth attachments must match, we can only return values that are supported
     * for both internalformats.
     */
    consider_color_internalformats         = (root_window_context_ptr->msaa_enumeration_color_internalformat         != GL_NONE);
    consider_depth_stencil_internalformats = (root_window_context_ptr->msaa_enumeration_depth_stencil_internalformat != GL_NONE);

    if (consider_color_internalformats         && root_window_context_ptr->msaa_enumeration_color_n_samples         == 0 ||
        consider_depth_stencil_internalformats && root_window_context_ptr->msaa_enumeration_depth_stencil_n_samples == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "MSAA not supported for the requested color / depth / depth-stencil internalformats");

        result = false;
        goto end;
    }

    if (consider_depth_stencil_internalformats && root_window_context_ptr->msaa_enumeration_depth_stencil_n_samples != 0)
    {
        depth_stencil_samples_vector = system_resizable_vector_create(root_window_context_ptr->msaa_enumeration_depth_stencil_n_samples);
    }

    result_vector = system_resizable_vector_create(32 /* capacity */);

    ASSERT_DEBUG_SYNC(result_vector != NULL,
                      "Out of memory");

    for (unsigned int n_depth_stencil_n_samples = 0;
                      n_depth_stencil_n_samples < (uint32_t) root_window_context_ptr->msaa_enumeration_depth_stencil_n_samples;
                    ++n_depth_stencil_n_samples)
    {
        system_resizable_vector_push(depth_stencil_samples_vector,
                                     (void*) root_window_context_ptr->msaa_enumeration_depth_stencil_samples[n_depth_stencil_n_samples]);
    }

    if (depth_stencil_samples_vector != NULL)
    {
        for (unsigned int n_result_sample = 0;
                          n_result_sample < (uint32_t) root_window_context_ptr->msaa_enumeration_color_n_samples;
                        ++n_result_sample)
        {
            /* Only store the matches */
            if (system_resizable_vector_find(depth_stencil_samples_vector,
                                             (void*) root_window_context_ptr->msaa_enumeration_color_samples[n_result_sample]) != ITEM_NOT_FOUND)
            {
                system_resizable_vector_push(result_vector,
                                             (void*) root_window_context_ptr->msaa_enumeration_color_samples[n_result_sample]);
            }
        }
    } /* if (depth_stencil_samples_vector != NULL) */
    else
    {
        /* Treat the color samples data as the needed info */
        for (unsigned int n_result_sample = 0;
                          n_result_sample < (uint32_t) root_window_context_ptr->msaa_enumeration_color_n_samples;
                        ++n_result_sample)
        {
            system_resizable_vector_push(result_vector,
                                         (void*) root_window_context_ptr->msaa_enumeration_color_samples[n_result_sample]);
        }
    }

    system_resizable_vector_sort(result_vector,
                                 _ogl_context_sort_descending);

end:
    if (!result)
    {
        *out_n_supported_samples = 0;
    }
    else
    {
        /* Copy the result data to the output variables */
        GLint* result_vector_data_ptr = NULL;

        system_resizable_vector_get_property(result_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                            &result_vector_data_ptr);
        system_resizable_vector_get_property(result_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             out_n_supported_samples);

        *out_supported_samples = new unsigned int [*out_n_supported_samples];

        ASSERT_DEBUG_SYNC(result_vector_data_ptr != NULL,
                          "system_resizable_vector returned NULL for SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY query");
        ASSERT_DEBUG_SYNC(*out_supported_samples != NULL,
                          "Out of memory");

        memcpy(*out_supported_samples,
               result_vector_data_ptr,
               sizeof(GLint) * *out_n_supported_samples);
    }

    /* Clean up */
    if (root_window_context_ptr->msaa_enumeration_color_samples != NULL)
    {
        delete [] root_window_context_ptr->msaa_enumeration_color_samples;

        root_window_context_ptr->msaa_enumeration_color_samples = NULL;
    }

    if (root_window_context_ptr->msaa_enumeration_depth_stencil_samples != NULL)
    {
        delete [] root_window_context_ptr->msaa_enumeration_depth_stencil_samples;

        root_window_context_ptr->msaa_enumeration_depth_stencil_samples = NULL;
    }

    if (depth_stencil_samples_vector != NULL)
    {
        system_resizable_vector_release(depth_stencil_samples_vector);

        depth_stencil_samples_vector = NULL;
    }

    if (result_vector != NULL)
    {
        system_resizable_vector_release(result_vector);

        result_vector = NULL;
    }
}

/* Please see header for spec */
PUBLIC EMERALD_API ogl_context ogl_context_get_current_context()
{
    return  _current_context;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_context_get_property(ogl_context          context,
                                                 ogl_context_property property,
                                                 void*                out_result)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    if (context_ptr->context_platform != NULL)
    {
        bool result = context_ptr->pfn_get_property(context_ptr->context_platform,
                                                    property,
                                                    out_result);

        if (result)
        {
            goto end;
        }
    } /* if (context_ptr->context_platform != NULL) */

    switch (property)
    {
        case OGL_CONTEXT_PROPERTY_BO_BINDINGS:
        {
            *((ogl_context_bo_bindings*) out_result) = context_ptr->bo_bindings;

            break;
        }

        case OGL_CONTEXT_PROPERTY_BUFFERS_RAGL:
        {
            *((raGL_buffers*) out_result) = context_ptr->buffers;

            break;
        }

        case OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT:
        {
            *(ral_texture_format*) out_result = context_ptr->fbo_color_texture_format;

            break;
        }

        case OGL_CONTEXT_PROPERTY_DEFAULT_FBO_DEPTH_STENCIL_TEXTURE_FORMAT:
        {
            *(ral_texture_format*) out_result = context_ptr->fbo_depth_stencil_texture_format;

            break;
        }

        case OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID:
        {
            *((GLuint*) out_result) = context_ptr->fbo_id;

            break;
        }

        case OGL_CONTEXT_PROPERTY_DEFAULT_FBO_N_SAMPLES:
        {
            *(unsigned int*) out_result = context_ptr->fbo_n_samples_effective;

            break;
        }

        case OGL_CONTEXT_PROPERTY_DEFAULT_FBO_SIZE:
        {
            ASSERT_DEBUG_SYNC(context_ptr->fbo_size[0] > 0 &&
                              context_ptr->fbo_size[1] > 0,
                              "Invalid default FB's size detected.");

            memcpy(out_result,
                   context_ptr->fbo_size,
                   sizeof(context_ptr->fbo_size) );

            break;

        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_ES,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES property requested for a non-ES context");

            *((const ogl_context_es_entrypoints**) out_result) = &context_ptr->entry_points_es;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES_EXT_TEXTURE_BUFFER:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_ES,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES_EXT_TEXTURE_BUFFER property requested for a non-ES context");

            *((const ogl_context_es_entrypoints_ext_texture_buffer**) out_result) = &context_ptr->entry_points_es_ext_texture_buffer;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints**) out_result) = &context_ptr->entry_points_gl;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_buffer_storage**) out_result) = &context_ptr->entry_points_gl_arb_buffer_storage;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SPARSE_BUFFER:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SPARSE_BUFFER property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_sparse_buffer**) out_result) = &context_ptr->entry_points_gl_arb_sparse_buffer;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_ext_direct_state_access**) out_result) = &context_ptr->entry_points_gl_ext_direct_state_access;

            break;
        }

        case OGL_CONTEXT_PROPERTY_FLYBY:
        {
            *(ogl_flyby*) out_result = context_ptr->flyby;

            break;
        }

        case OGL_CONTEXT_PROPERTY_INFO:
        {
            *((const ogl_context_gl_info**) out_result) = &context_ptr->info;

            break;
        }

        case OGL_CONTEXT_PROPERTY_IS_INTEL_DRIVER:
        {
            *(bool*) out_result = context_ptr->is_intel_driver;

            break;
        }

        case OGL_CONTEXT_PROPERTY_IS_NV_DRIVER:
        {
            *(bool*) out_result = context_ptr->is_nv_driver;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS:
        {
            *((const ogl_context_gl_limits**) out_result) = &context_ptr->limits;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS_ARB_SPARSE_BUFFER:
        {
            *((const ogl_context_gl_limits_arb_sparse_buffer**) out_result) = &context_ptr->limits_arb_sparse_buffer;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS_EXT_TEXTURE_FILTER_ANISOTROPIC:
        {
            *((const ogl_context_gl_limits_ext_texture_filter_anisotropic**) out_result) = &context_ptr->limits_ext_texture_filter_anisotropic;

            break;
        }

        case OGL_CONTEXT_PROPERTY_MAJOR_VERSION:
        {
            *(uint32_t*) out_result = context_ptr->major_version;

            break;
        }

        case OGL_CONTEXT_PROPERTY_MINOR_VERSION:
        {
            *(uint32_t*) out_result = context_ptr->minor_version;

            break;
        }

        case OGL_CONTEXT_PROPERTY_MATERIALS:
        {
            /* If there's no material manager available, create one now */
            if (context_ptr->materials == NULL)
            {
                context_ptr->materials = ogl_materials_create(context);
            }

            *((ogl_materials*) out_result) = context_ptr->materials;

            break;
        }

        case OGL_CONTEXT_PROPERTY_PIXEL_FORMAT:
        {
            *(system_pixel_format*) out_result = context_ptr->pfd;

            break;
        }

        case OGL_CONTEXT_PROPERTY_PARENT_CONTEXT:
        {
            *(ogl_context*) out_result = context_ptr->parent_context;

            break;
        }

        case OGL_CONTEXT_PROPERTY_PRIMITIVE_RENDERER:
        {
            /* If there's no primitive renderer, create one now */
            if (context_ptr->primitive_renderer == NULL)
            {
                context_ptr->primitive_renderer = ogl_primitive_renderer_create(context,
                                                                                system_hashed_ansi_string_create("Context primitive renderer") );
            }

            *((ogl_primitive_renderer*) out_result) = context_ptr->primitive_renderer;

            break;
        }

        case OGL_CONTEXT_PROPERTY_PROGRAMS:
        {
            *(ogl_programs*) out_result = context_ptr->programs;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS:
        {
            *((ogl_context_sampler_bindings*) out_result) = context_ptr->sampler_bindings;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SAMPLERS_RAGL:
        {
            *((raGL_samplers*) out_result) = context_ptr->samplers;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SHADERS:
        {
            *(ogl_shaders*) out_result = context_ptr->shaders;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SHADOW_MAPPING:
        {
            if (context_ptr->shadow_mapping == NULL)
            {
                /* Create the shadow mapping handler if this is the first incoming request */
                context_ptr->shadow_mapping = ogl_shadow_mapping_create( (ogl_context) context_ptr);
            }

            *(ogl_shadow_mapping*) out_result = context_ptr->shadow_mapping;

            break;
        }

        case OGL_CONTEXT_PROPERTY_STATE_CACHE:
        {
            *((ogl_context_state_cache*) out_result) = context_ptr->state_cache;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_ES_EXT_TEXTURE_BUFFER:
        {
            *((bool*) out_result) = context_ptr->es_ext_texture_buffer_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE:
        {
            *((bool*) out_result) = context_ptr->gl_arb_buffer_storage_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SPARSE_BUFFERS:
        {
            *((bool*) out_result) = context_ptr->gl_arb_sparse_buffer_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_DIRECT_STATE_ACCESS:
        {
            *((bool*) out_result) = context_ptr->gl_ext_direct_state_access_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_TEXTURE_FILTER_ANISOTROPIC:
        {
            *((bool*) out_result) = context_ptr->gl_ext_texture_filter_anisotropic_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TEXT_RENDERER:
        {
            /* Text renderer is not created at context creation time. If requested for the first time,
             * make sure to instantiate it, before returning the handle.
             */
            if (context_ptr->text_renderer == NULL)
            {
                /* Set up text renderer */
                const  float              text_default_size = 0.75f;
                static float              text_color[3]     = {1.0f, 1.0f, 1.0f};
                system_hashed_ansi_string window_name       = NULL;
                int                       window_size[2];

                system_window_get_property(context_ptr->window,
                                           SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                           window_size);
                system_window_get_property(context_ptr->window,
                                           SYSTEM_WINDOW_PROPERTY_NAME,
                                          &window_name);

                context_ptr->text_renderer = ogl_text_create(window_name,
                                                             context,
                                                             system_resources_get_meiryo_font_table(),
                                                             window_size[0],
                                                             window_size[1]);

                ogl_text_set_text_string_property(context_ptr->text_renderer,
                                                  TEXT_STRING_ID_DEFAULT,
                                                  OGL_TEXT_STRING_PROPERTY_SCALE,
                                                 &text_default_size);
                ogl_text_set_text_string_property(context_ptr->text_renderer,
                                                  TEXT_STRING_ID_DEFAULT,
                                                  OGL_TEXT_STRING_PROPERTY_COLOR,
                                                  text_color);
            } /* if (context_ptr->text_renderer == NULL) */

            *((ogl_text*) out_result) = context_ptr->text_renderer;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION:
        {
            *((ogl_context_texture_compression*) out_result) = context_ptr->texture_compression;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TEXTURES:
        {
            *((ogl_context_textures*) out_result) = context_ptr->textures;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TO_BINDINGS:
        {
            *((ogl_context_to_bindings*) out_result) = context_ptr->to_bindings;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TYPE:
        {
            *((ogl_context_type*) out_result) = context_ptr->context_type;

            break;
        }

        case OGL_CONTEXT_PROPERTY_WINDOW:
        {
            *((system_window*) out_result) = context_ptr->window;

            break;
        }

        case OGL_CONTEXT_PROPERTY_VAO_NO_VAAS:
        {
            *(GLuint*) out_result = context_ptr->vao_no_vaas_id;

            break;
        }

        case OGL_CONTEXT_PROPERTY_VAOS:
        {
            *(ogl_context_vaos*) out_result = context_ptr->vaos;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context property requested");
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for spec */
PUBLIC void ogl_context_init_global()
{
    #ifdef _WIN32
    {
        /* Nothing to do */
    }
    #else
    {
        ogl_context_linux_init_global();
    }
    #endif
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_context_is_extension_supported(ogl_context               context,
                                                           system_hashed_ansi_string extension_name)
{
    _ogl_context* context_ptr = (_ogl_context*) context;
    bool          result      = false;

    for (int32_t n_extension = 0;
                 n_extension < context_ptr->limits.num_extensions;
               ++n_extension)
    {
        if (system_hashed_ansi_string_is_equal_to_raw_string(extension_name,
                                                             (const char*) context_ptr->info.extensions[n_extension]) )
        {
            result = true;

            break;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool ogl_context_release_managers(ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    if (context_ptr->materials != NULL)
    {
        ogl_materials_release(context_ptr->materials);

        context_ptr->materials = NULL;
    }

    if (context_ptr->primitive_renderer != NULL)
    {
        ogl_primitive_renderer_release(context_ptr->primitive_renderer);

        context_ptr->primitive_renderer = NULL;
    }

    if (context_ptr->text_renderer != NULL)
    {
        ogl_text_release(context_ptr->text_renderer);

        context_ptr->text_renderer = NULL;
    }

    if (context_ptr->programs != NULL)
    {
        ogl_programs_release(context_ptr->programs);

        context_ptr->programs = NULL;
    }

    if (context_ptr->samplers != NULL)
    {
        raGL_samplers_release(context_ptr->samplers);

        context_ptr->samplers = NULL;
    }

    if (context_ptr->shaders != NULL)
    {
        ogl_shaders_release(context_ptr->shaders);

        context_ptr->shaders = NULL;
    }

    if (context_ptr->shadow_mapping != NULL)
    {
        ogl_shadow_mapping_release(context_ptr->shadow_mapping);

        context_ptr->shadow_mapping = NULL;
    }

    if (context_ptr->textures != NULL)
    {
        ogl_context_textures_release(context_ptr->textures);

        context_ptr->textures = NULL;
    }

    ogl_context_bind_to_current_thread(context);
    {
        if (context_ptr->buffers != NULL)
        {
            raGL_buffers_release(context_ptr->buffers);

            context_ptr->buffers = NULL;
        }
    }
    ogl_context_unbind_from_current_thread(context);


    if (context_ptr->bo_bindings != NULL)
    {
        ogl_context_bo_bindings_release(context_ptr->bo_bindings);

        context_ptr->bo_bindings = NULL;
    }

    if (context_ptr->sampler_bindings != NULL)
    {
        ogl_context_sampler_bindings_release(context_ptr->sampler_bindings);

        context_ptr->sampler_bindings = NULL;
    }

    if (context_ptr->flyby != NULL)
    {
        ogl_flyby_release(context_ptr->flyby);

        context_ptr->flyby = NULL;
    }

    if (context_ptr->state_cache != NULL)
    {
        ogl_context_state_cache_release(context_ptr->state_cache);

        context_ptr->state_cache = NULL;
    }

    if (context_ptr->texture_compression != NULL)
    {
        ogl_context_texture_compression_release(context_ptr->texture_compression);

        context_ptr->texture_compression = NULL;
    }

    if (context_ptr->to_bindings != NULL)
    {
        ogl_context_to_bindings_release(context_ptr->to_bindings);

        context_ptr->to_bindings = NULL;
    }

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_context_request_callback_from_context_thread(ogl_context                                context,
                                                                         PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback,
                                                                         void*                                      user_arg,
                                                                         bool                                       swap_buffers_afterward,
                                                                         bool                                       block_until_available)
{
    bool                  result            = false;
    _ogl_context*         context_ptr       = (_ogl_context*) context;
    ogl_rendering_handler rendering_handler = NULL;

    system_window_get_property(context_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    if (rendering_handler != NULL)
    {
        result = ogl_rendering_handler_request_callback_from_context_thread(rendering_handler,
                                                                            pfn_callback,
                                                                            user_arg,
                                                                            swap_buffers_afterward,
                                                                            block_until_available);
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Provided context must be assigned a rendering handler before it is possible to issue blocking calls from GL context thread!");
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool ogl_context_set_property(ogl_context          context,
                                     ogl_context_property property,
                                     const void*          data)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    ASSERT_DEBUG_SYNC(context_ptr != NULL,
                      "Input argument is NULL");

    if (property == OGL_CONTEXT_PROPERTY_PLATFORM_CONTEXT)
    {
        ASSERT_DEBUG_SYNC(context_ptr->context_platform == NULL,
                          "Platform-specific context instance is not NULL");

        context_ptr->context_platform = *(ogl_context_platform*) data;

        return true;
    }
    else
    {
        return context_ptr->pfn_set_property(context_ptr->context_platform,
                                             property,
                                             data);
    }
}

/** Please see header for specification */
PUBLIC void ogl_context_swap_buffers(ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    ASSERT_DEBUG_SYNC(context_ptr != NULL,
                      "Input argument is NULL");

    context_ptr->pfn_swap_buffers(context_ptr->context_platform);
}
