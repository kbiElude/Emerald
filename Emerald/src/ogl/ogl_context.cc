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
#include "ogl/ogl_context_to_bindings.h"
#include "ogl/ogl_context_wrappers.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_pixel_format_descriptor.h"
#include "ogl/ogl_primitive_renderer.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_samplers.h"
#include "ogl/ogl_shaders.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_textures.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_randomizer.h"
#include "system/system_window.h"
#include <string>

#define RANDOM_TEXTURE_WIDTH  (512)
#define RANDOM_TEXTURE_HEIGHT (512)

/** Internal variables */
typedef struct
{
    ogl_context_type            context_type;
    HDC                         device_context_handle;
    HMODULE                     opengl32_dll_handle;
    ogl_pixel_format_descriptor pfd;
    HGLRC                       wgl_rendering_context;
    system_window               window;
    system_window_handle        window_handle;

    GLuint                      vao_no_vaas_id;

    /* True if single-frame vsync has been enabled for the context, false otherwise */
    bool vsync_enabled;

    /* Current multisampling samples setting */
    uint32_t multisampling_samples;
    /* Defines how many different "samples" setting for multisampling are available for a given context */
    uint32_t n_multisampling_supported_sample;
    /* Contains n_multisampling_supported_sample entries */
    uint32_t* multisampling_supported_sample;

    ogl_context_gl_entrypoints                                  entry_points_gl;
    ogl_context_gl_entrypoints_arb_buffer_storage               entry_points_gl_arb_buffer_storage;
    ogl_context_gl_entrypoints_arb_compute_shader               entry_points_gl_arb_compute_shader;
    ogl_context_gl_entrypoints_arb_debug_output                 entry_points_gl_arb_debug_output;
    ogl_context_gl_entrypoints_arb_framebuffer_no_attachments   entry_points_gl_arb_framebuffer_no_attachments;
    ogl_context_gl_entrypoints_arb_multi_bind                   entry_points_gl_arb_multi_bind;
    ogl_context_gl_entrypoints_arb_program_interface_query      entry_points_gl_arb_program_interface_query;
    ogl_context_gl_entrypoints_arb_shader_storage_buffer_object entry_points_gl_arb_shader_storage_buffer_object;
    ogl_context_gl_entrypoints_arb_texture_storage_multisample  entry_points_gl_arb_texture_storage_multisample;
    ogl_context_gl_entrypoints_ext_direct_state_access          entry_points_gl_ext_direct_state_access;
    ogl_context_gl_entrypoints_private                          entry_points_private;
    ogl_context_gl_info                                         info;
    ogl_context_gl_limits                                       limits;
    ogl_context_gl_limits_arb_compute_shader                    limits_arb_compute_shader;
    ogl_context_gl_limits_arb_framebuffer_no_attachments        limits_arb_framebuffer_no_attachments;
    ogl_context_gl_limits_arb_shader_storage_buffer_object      limits_arb_shader_storage_buffer_object;

    /* WGL extensions */
    bool wgl_swap_control_support;
    bool wgl_swap_control_tear_support;

    PFNWGLSWAPINTERVALEXTPROC pWGLSwapIntervalEXT;

    /* OpenGL ES context support: */
    ogl_context_es_entrypoints                    entry_points_es;
    ogl_context_es_entrypoints_ext_texture_buffer entry_points_es_ext_texture_buffer;

    bool es_ext_texture_buffer_support;

    bool gl_arb_buffer_storage_support;
    bool gl_arb_compute_shader_support;
    bool gl_arb_debug_output_support;
    bool gl_arb_framebuffer_no_attachments_support;
    bool gl_arb_multi_bind_support;
    bool gl_arb_program_interface_query_support;
    bool gl_arb_shader_storage_buffer_object_support;
    bool gl_arb_texture_buffer_object_rgb32_support;
    bool gl_arb_texture_storage_multisample_support;
    bool gl_ext_direct_state_access_support;

    ogl_context_bo_bindings         bo_bindings;
    ogl_materials                   materials;
    ogl_programs                    programs;
    ogl_primitive_renderer          primitive_renderer;
    ogl_context_sampler_bindings    sampler_bindings;
    ogl_samplers                    samplers;
    ogl_shaders                     shaders;
    ogl_shadow_mapping              shadow_mapping;
    ogl_context_state_cache         state_cache;
    ogl_context_texture_compression texture_compression;
    ogl_textures                    textures;
    ogl_context_to_bindings         to_bindings;

    REFCOUNT_INSERT_VARIABLES
} _ogl_context;

struct func_ptr_table_entry
{
    void*       func_ptr;
    const char* func_name;
};


__declspec(thread) ogl_context _current_context = NULL;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_context,
                               ogl_context,
                              _ogl_context);

/* Forward declarations */
PRIVATE void _ogl_context_retrieve_GL_ARB_buffer_storage_function_pointers              (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_compute_shader_function_pointers              (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_compute_shader_limits                         (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_debug_output_function_pointers                (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_framebuffer_no_attachments_function_pointers  (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_framebuffer_no_attachments_limits             (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_multi_bind_function_pointers                  (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_program_interface_query_function_pointers     (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_shader_storage_buffer_object_function_pointers(__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_shader_storage_buffer_object_limits           (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_ARB_texture_storage_multisample_function_pointers (__inout __notnull _ogl_context* context_ptr);
PRIVATE void _ogl_context_retrieve_GL_EXT_direct_state_access_function_pointers         (__inout __notnull _ogl_context* context_ptr);

/** TODO */
PRIVATE system_hashed_ansi_string _ogl_context_get_compressed_filename(__in  __notnull void*                     user_arg,
                                                                       __in  __notnull system_hashed_ansi_string decompressed_filename,
                                                                       __out __notnull GLenum*                   out_compressed_gl_enum)
{
    _ogl_context*             context_ptr                  = (_ogl_context*) user_arg;
    unsigned int              n_compressed_internalformats = 0;
    system_hashed_ansi_string result                       = NULL;

    /* Identify location where the extension starts */
    const char* decompressed_filename_ptr          = system_hashed_ansi_string_get_buffer(decompressed_filename);
    const char* decompressed_filename_last_dot_ptr = strrchr(decompressed_filename_ptr, '.');

    ASSERT_DEBUG_SYNC(decompressed_filename_last_dot_ptr != NULL,
                      "Input file name does not use any extension");
    if (decompressed_filename_last_dot_ptr == NULL)
    {
        goto end;
    }

    /* Iterate over all internalformats */
    ogl_context_texture_compression_get_property(context_ptr->texture_compression,
                                                 OGL_CONTEXT_TEXTURE_COMPRESSION_PROPERTY_N_COMPRESSED_INTERNALFORMAT,
                                                &n_compressed_internalformats);

    for (unsigned int n_compressed_internalformat = 0;
                      n_compressed_internalformat < n_compressed_internalformats;
                    ++n_compressed_internalformat)
    {
        GLenum                    enum_value = GL_NONE;
        system_hashed_ansi_string extension  = NULL;

        ogl_context_texture_compression_get_algorithm_property(context_ptr->texture_compression,
                                                               n_compressed_internalformat,
                                                               OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_GL_VALUE,
                                                              &enum_value);
        ogl_context_texture_compression_get_algorithm_property(context_ptr->texture_compression,
                                                               n_compressed_internalformat,
                                                               OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_FILE_EXTENSION,
                                                              &extension);

        /* Form a new filename */
        std::string               result_filename;
        system_hashed_ansi_string result_filename_has;

        result_filename      = std::string(decompressed_filename_ptr, decompressed_filename_last_dot_ptr - decompressed_filename_ptr + 1);
        result_filename     += system_hashed_ansi_string_get_buffer(extension);
        result_filename_has  = system_hashed_ansi_string_create    (result_filename.c_str() );

        /* Is the file available? */
        if (system_file_enumerator_is_file_present(result_filename_has) )
        {
            result                  = result_filename_has;
            *out_compressed_gl_enum = enum_value;

            break;
        }
    } /* for (all compressed internalformats) */

end:
    return result;
}

/** Function called back when reference counter drops to zero. Releases WGL rendering context.
 *
 *  @param ptr Pointer to _ogl_context instance.
 **/
PRIVATE void _ogl_context_release(__in __notnull __deallocate(mem) void* ptr)
{
    _ogl_context* context_ptr = (_ogl_context*) ptr;

    /* NOTE: This leaks the no-VAA VAO, but it's not much damage, whereas entering
     *       a rendering context from this method could be tricky under some
     *       circumstances.
     */

    if (context_ptr->bo_bindings != NULL)
    {
        ogl_context_bo_bindings_release(context_ptr->bo_bindings);

        context_ptr->bo_bindings = NULL;
    }

    if (context_ptr->primitive_renderer != NULL)
    {
        ogl_primitive_renderer_release(context_ptr->primitive_renderer);

        context_ptr->primitive_renderer = NULL;
    }

    if (context_ptr->multisampling_supported_sample != NULL)
    {
        delete [] context_ptr->multisampling_supported_sample;
        context_ptr->multisampling_supported_sample = NULL;
    }

    if (context_ptr->sampler_bindings != NULL)
    {
        ogl_context_sampler_bindings_release(context_ptr->sampler_bindings);

        context_ptr->sampler_bindings = NULL;
    }

    if (context_ptr->shadow_mapping != NULL)
    {
        ogl_shadow_mapping_release(context_ptr->shadow_mapping);

        context_ptr->shadow_mapping = NULL;
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

    /* Release arrays allocated for "limits" storage */
    if (context_ptr->limits.program_binary_formats != NULL)
    {
        delete [] context_ptr->limits.program_binary_formats;

        context_ptr->limits.program_binary_formats = NULL;
    }


    deinit_ogl_context_gl_info         (&context_ptr->info);
    ogl_pixel_format_descriptor_release(context_ptr->pfd);

    if (::wglDeleteContext(context_ptr->wgl_rendering_context) == FALSE)
    {
        LOG_ERROR("wglDeleteContext() failed.");
    }

    if (context_ptr->opengl32_dll_handle != NULL)
    {
        ::FreeLibrary(context_ptr->opengl32_dll_handle);
    }
}

/** TODO */
PRIVATE bool _ogl_set_pixel_format_multisampling(_ogl_context* context_ptr, uint32_t n_samples)
{
    bool result = false;

    PFNWGLCHOOSEPIXELFORMATARBPROC pWGLChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC) ::wglGetProcAddress("wglChoosePixelFormatARB");

    ASSERT_ALWAYS_SYNC(pWGLChoosePixelFormatARB != NULL, "wglChoosePixelFormat() unavailable - please update your WGL implementation.");
    if (pWGLChoosePixelFormatARB != NULL)
    {
        int  alpha_bits  = 0;
        int  depth_bits  = 0;
        int  rgb_bits[3] = {0, 0, 0};

        ogl_pixel_format_descriptor_get(OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_RED_BITS,   context_ptr->pfd, rgb_bits);
        ogl_pixel_format_descriptor_get(OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_GREEN_BITS, context_ptr->pfd, rgb_bits + 1);
        ogl_pixel_format_descriptor_get(OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_BLUE_BITS,  context_ptr->pfd, rgb_bits + 2);
        ogl_pixel_format_descriptor_get(OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_ALPHA_BITS, context_ptr->pfd, &alpha_bits);
        ogl_pixel_format_descriptor_get(OGL_PIXEL_FORMAT_DESCRIPTOR_DEPTH_BITS,              context_ptr->pfd, &depth_bits);

        int   attributes[]       = {WGL_DRAW_TO_WINDOW_ARB,           GL_TRUE,
                                    WGL_SUPPORT_OPENGL_ARB,           GL_TRUE,
                                    WGL_ACCELERATION_ARB,             WGL_FULL_ACCELERATION_ARB,
                                    WGL_COLOR_BITS_ARB,               rgb_bits[0] + rgb_bits[1] + rgb_bits[2],
                                    WGL_ALPHA_BITS_ARB,               alpha_bits,
                                    WGL_DEPTH_BITS_ARB,               depth_bits,
                                    WGL_STENCIL_BITS_ARB,             0,
                                    WGL_DOUBLE_BUFFER_ARB,            GL_TRUE,
                                    WGL_SAMPLE_BUFFERS_ARB,           GL_TRUE,
                                    WGL_SAMPLES_ARB,                  n_samples,
                                    WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT, GL_TRUE,
                                    0,                      0};
        float float_attributes[] = {0.0f, 0.0f};
        UINT  n_pixel_formats    = 0;
        int   pixel_format       = 0;

        result = (pWGLChoosePixelFormatARB(context_ptr->device_context_handle,
                                           attributes,
                                           float_attributes,
                                           1,
                                           &pixel_format,
                                           &n_pixel_formats) != 0 && n_pixel_formats > 0);
    }

    return result;
}

/** TODO */
PRIVATE void APIENTRY _ogl_context_debug_message_gl_callback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,GLvoid *userParam)
{
    static char local_message[4096];

    /* Disable 131185, which basically is:
       
       [Id:131185] [Source:OpenGL] Type:[Other event] Severity:[!]: Buffer detailed info: Buffer object 14 (bound to GL_TRANSFORM_FEEDBACK_BUFFER_NV (0), and GL_TRANSFORM_FEEDBACK_BUFFER_NV, usage hint is GL_STATIC_DRAW) will use VIDEO memory as the source for buffer object operations.
       
       */
    /* 131204 which goes like:

       [File C:\!Programming\DeStereo\Emerald\src\ogl\ogl_context.cc // line 187]: [Id:131204] [Source:OpenGL] Type:[Other event] Severity:[!]: Texture state usage warning: Waste of memory: Texture 1 has mipmaps, while it's min filter is inconsistent with mipmaps.

       is invalid for texture buffers, yet is reported. 
    */
    /* 131154:

        [Id:131154] [Source:OpenGL] Type:[Performance warning] Severity:[!!]: Pixel-path performance warning: Pixel transfer is synchronized with 3D rendering.

        Happens in Cammuxer so disabling.
    */
    if (id != 131185 && id != 131204 && id != 131154)
    {
        /* This function is called back from within driver layer! Do not issue GL commands from here! */
        _ogl_context* context_ptr   = (_ogl_context*) userParam;
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

        sprintf_s(local_message,
                  "[Id:%d] [Source:%s] Type:[%s] Severity:[%s]: %s",
                  id,
                  source_name,
                  type_name,
                  severity_name,
                  message);

        LOG_INFO("%s", local_message);

        ASSERT_DEBUG_SYNC(type != GL_DEBUG_TYPE_ERROR_ARB,
                          "GL error detected");
    }
}

/** TODO */
PRIVATE bool _ogl_context_get_function_pointers(__in                   __notnull _ogl_context*         context_ptr,
                                                __in_ecount(n_entries) __notnull func_ptr_table_entry* entries,
                                                __in                             uint32_t              n_entries)
{
    bool result = true;

    for (uint32_t n_entry = 0; n_entry < n_entries; ++n_entry)
    {
        GLvoid** ptr_to_update = (GLvoid**) entries[n_entry].func_ptr;
        GLchar*  func_name     = (GLchar*)  entries[n_entry].func_name;

        *ptr_to_update = ::GetProcAddress(context_ptr->opengl32_dll_handle, func_name);

        if (*ptr_to_update == NULL)
        {
            *ptr_to_update = ::wglGetProcAddress(func_name);
        }

        if (*ptr_to_update == NULL)
        {
            LOG_ERROR("Could not retrieve function pointer to %s - update your drivers, OpenGL3.3 is required to run.",
                      func_name);

            ASSERT_ALWAYS_SYNC(false,
                               "Function import error");

            result = false;
        }
    }

    return result;
}

/** TODO */
PRIVATE void _ogl_context_initialize_es_ext_texture_buffer_extension(__inout __notnull _ogl_context* context_ptr)
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
PRIVATE void _ogl_context_initialize_gl_arb_buffer_storage_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_buffer_storage_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_compute_shader_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_compute_shader_function_pointers(context_ptr);
    _ogl_context_retrieve_GL_ARB_compute_shader_limits           (context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_debug_output_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_debug_output_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_framebuffer_no_attachments_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_framebuffer_no_attachments_function_pointers(context_ptr);
    _ogl_context_retrieve_GL_ARB_framebuffer_no_attachments_limits           (context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_multi_bind_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_multi_bind_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_program_interface_query_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_program_interface_query_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_shader_storage_buffer_object_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_shader_storage_buffer_object_function_pointers(context_ptr);
    _ogl_context_retrieve_GL_ARB_shader_storage_buffer_object_limits           (context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_arb_texture_storage_multisample_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_ARB_texture_storage_multisample_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_gl_ext_direct_state_access_extension(__inout __notnull _ogl_context* context_ptr)
{
    _ogl_context_retrieve_GL_EXT_direct_state_access_function_pointers(context_ptr);
}

/** TODO */
PRIVATE void _ogl_context_initialize_wgl_extensions(__inout __notnull _ogl_context* context_ptr)
{
    PFNWGLGETEXTENSIONSSTRINGEXTPROC pWGLGetExtensionsString = (PFNWGLGETEXTENSIONSSTRINGEXTPROC) ::wglGetProcAddress("wglGetExtensionsStringEXT");

    if (pWGLGetExtensionsString != NULL)
    {
        const char* wgl_extensions = pWGLGetExtensionsString();

        /* Is EXT_wgl_swap_control supported? */
        context_ptr->wgl_swap_control_support = (strstr(wgl_extensions, "WGL_EXT_swap_control") != NULL);

        if (context_ptr->wgl_swap_control_support)
        {
            context_ptr->pWGLSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) ::wglGetProcAddress("wglSwapIntervalEXT");
        }

        /* Is EXT_WGL_swap_control_tear supported? */
        context_ptr->wgl_swap_control_tear_support = (strstr(wgl_extensions, "WGL_EXT_swap_control_tear") != NULL);
    } /* if (pWGLGetExtensionsString != NULL) */
}

/** TODO */
PRIVATE void _ogl_context_retrieve_ES_function_pointers(__in __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->opengl32_dll_handle == NULL, "OpenGL32.dll already loaded.");
    context_ptr->opengl32_dll_handle = ::LoadLibraryA("opengl32.dll");

    if (context_ptr->opengl32_dll_handle != NULL)
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
            {&context_ptr->entry_points_es.pGLCompressedTexImage2D,                "glCompressedTexImage2D"},
            {&context_ptr->entry_points_es.pGLCompressedTexImage3D,                "glCompressedTexImage3D"},
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
            {&context_ptr->entry_points_es.pGLTexImage2D,                          "glTexImage2D"},
            {&context_ptr->entry_points_es.pGLTexImage3D,                          "glTexImage3D"},
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
    else
    {
        LOG_FATAL("Could not locate opengl32.dll");
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_multisampling_support_info(__inout __notnull _ogl_context* context_ptr)
{
    int n_stored_entries = 0;

    context_ptr->n_multisampling_supported_sample = 0;

    for (int n_iteration = 0; n_iteration < 2; ++n_iteration)
    {
        if (n_iteration == 1)
        {
            /* If this is second iteration, we know how many entries we need to allocate for the array storing
             * information on supported "amount of samples" setting for given context. */
            context_ptr->multisampling_supported_sample = new uint32_t[context_ptr->n_multisampling_supported_sample];

            ASSERT_ALWAYS_SYNC(context_ptr->multisampling_supported_sample != NULL, "Out of memory while allocating \"multisampling supported sample\" array.");
        }

        for (int n_samples = 0; n_samples < context_ptr->limits.max_color_texture_samples; ++n_samples)
        {
            bool status = _ogl_set_pixel_format_multisampling(context_ptr, n_samples);

            if (status)
            {
                /* If this is first iteration, just count this supported samples setting in. */
                if (n_iteration == 0)
                {
                    context_ptr->n_multisampling_supported_sample++;
                }
                else
                {
                    /* Second one? Store it. */
                    context_ptr->multisampling_supported_sample[n_stored_entries] = n_samples;

                    n_stored_entries++;
                }
            }
        } /* for (int n_samples = 0; n_samples < _result->limits.max_color_texture_samples; ++n_samples) */
    } /* for (int n_iteration = 0; n_iteration < 2; ++n_iteration) */
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_info(__inout __notnull _ogl_context* context_ptr)
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

    init_ogl_context_gl_info(&context_ptr->info,
                             &context_ptr->limits);

    if (context_ptr->info.extensions != NULL)
    {
        for (int n_extension = 0; n_extension < context_ptr->limits.num_extensions; ++n_extension)
        {
            context_ptr->info.extensions[n_extension] = pGLGetStringi(GL_EXTENSIONS, n_extension);

            LOG_INFO("%s", context_ptr->info.extensions[n_extension]);
            ASSERT_ALWAYS_SYNC(context_ptr->info.extensions[n_extension] != NULL,
                               "GL reports %d extensions supported but %dth extension name is NULL!",
                               context_ptr->limits.num_extensions,
                               n_extension);
        }
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_limits(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type        == OGL_CONTEXT_TYPE_GL,
                      "_ogl_context_retrieve_GL_limits() called for a non-GL context");
    ASSERT_DEBUG_SYNC(context_ptr->opengl32_dll_handle != NULL,
                      "GL limits can only be retrieved after calling _ogl_context_retrieve_GL_function_pointers()");

    if (context_ptr->opengl32_dll_handle != NULL)
    {
        /* NOTE: This function uses private entry-points, because at the time of the call,
         *       the entry-points have not been initialized yet
         */
        context_ptr->entry_points_private.pGLGetFloatv  (GL_ALIASED_LINE_WIDTH_RANGE,                       context_ptr->limits.aliased_line_width_range);
        context_ptr->entry_points_private.pGLGetFloatv  (GL_SMOOTH_LINE_WIDTH_RANGE,                        context_ptr->limits.smooth_line_width_range);
        context_ptr->entry_points_private.pGLGetFloatv  (GL_LINE_WIDTH_GRANULARITY,                        &context_ptr->limits.line_width_granularity);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAJOR_VERSION,                                 &context_ptr->limits.major_version);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,                           &context_ptr->limits.max_3d_texture_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,                      &context_ptr->limits.max_array_texture_layers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,            &context_ptr->limits.max_atomic_counter_buffer_bindings);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE,                &context_ptr->limits.max_atomic_counter_buffer_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES,                     &context_ptr->limits.max_color_texture_samples);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS,           &context_ptr->limits.max_combined_atomic_counter_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTERS,                  &context_ptr->limits.max_combined_atomic_counters);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,      &context_ptr->limits.max_combined_fragment_uniform_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS, &context_ptr->limits.max_combined_image_units_and_fragment_outputs);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,              &context_ptr->limits.max_combined_texture_image_units);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,        &context_ptr->limits.max_combined_vertex_uniform_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS,      &context_ptr->limits.max_combined_geometry_uniform_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,                   &context_ptr->limits.max_combined_uniform_blocks);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,                     &context_ptr->limits.max_cube_map_texture_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES,                     &context_ptr->limits.max_depth_texture_samples);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_DRAW_BUFFERS,                              &context_ptr->limits.max_draw_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ELEMENTS_INDICES,                          &context_ptr->limits.max_elements_indices);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_ELEMENTS_VERTICES,                         &context_ptr->limits.max_elements_vertices);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS,           &context_ptr->limits.max_fragment_atomic_counter_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTERS,                  &context_ptr->limits.max_fragment_atomic_counters);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,               &context_ptr->limits.max_fragment_uniform_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,                   &context_ptr->limits.max_fragment_uniform_blocks);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS,                 &context_ptr->limits.max_fragment_input_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS,           &context_ptr->limits.max_geometry_atomic_counter_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTERS,                  &context_ptr->limits.max_geometry_atomic_counters);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_INPUT_COMPONENTS,                 &context_ptr->limits.max_geometry_input_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES,                  &context_ptr->limits.max_geometry_output_vertices);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS,              &context_ptr->limits.max_geometry_texture_image_units);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_BLOCKS,                   &context_ptr->limits.max_geometry_uniform_blocks);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS,               &context_ptr->limits.max_geometry_uniform_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_IMAGE_SAMPLES,                             &context_ptr->limits.max_image_samples);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_IMAGE_UNITS,                               &context_ptr->limits.max_image_units);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_INTEGER_SAMPLES,                           &context_ptr->limits.max_integer_samples);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET,                      &context_ptr->limits.min_program_texel_offset);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET,                      &context_ptr->limits.max_program_texel_offset);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE,                    &context_ptr->limits.max_rectangle_texture_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,                         &context_ptr->limits.max_renderbuffer_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS,                         &context_ptr->limits.max_sample_mask_words);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SERVER_WAIT_TIMEOUT,                       &context_ptr->limits.max_server_wait_timeout);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,            &context_ptr->limits.max_shader_storage_buffer_bindings);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS,       &context_ptr->limits.max_tess_control_atomic_counter_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS,              &context_ptr->limits.max_tess_control_atomic_counters);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS,    &context_ptr->limits.max_tess_evaluation_atomic_counter_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS,           &context_ptr->limits.max_tess_evaluation_atomic_counters);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,                       &context_ptr->limits.max_texture_buffer_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,                       &context_ptr->limits.max_texture_image_units);
        context_ptr->entry_points_private.pGLGetFloatv  (GL_MAX_TEXTURE_LOD_BIAS,                          &context_ptr->limits.max_texture_lod_bias);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TEXTURE_SIZE,                              &context_ptr->limits.max_texture_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS,                &context_ptr->limits.max_transform_feedback_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, &context_ptr->limits.max_transform_feedback_interleaved_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,    &context_ptr->limits.max_transform_feedback_separate_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,                        &context_ptr->limits.max_uniform_block_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,                   &context_ptr->limits.max_uniform_buffer_bindings);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,                        &context_ptr->limits.max_uniform_block_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS,             &context_ptr->limits.max_vertex_atomic_counter_buffers);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS,                    &context_ptr->limits.max_vertex_atomic_counters);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_ATTRIBS,                            &context_ptr->limits.max_vertex_attribs);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS,                  &context_ptr->limits.max_vertex_output_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,                &context_ptr->limits.max_vertex_texture_image_units);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS,                     &context_ptr->limits.max_vertex_uniform_blocks);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,                 &context_ptr->limits.max_vertex_uniform_components);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_MINOR_VERSION,                                 &context_ptr->limits.minor_version);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,                &context_ptr->limits.num_compressed_texture_formats);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_EXTENSIONS,                                &context_ptr->limits.num_extensions);
        context_ptr->entry_points_private.pGLGetFloatv  (GL_POINT_FADE_THRESHOLD_SIZE,                     &context_ptr->limits.point_fade_threshold_size);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_POINT_SIZE_RANGE,                               context_ptr->limits.point_size_range);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_SUBPIXEL_BITS,                                 &context_ptr->limits.subpixel_bits);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT,               &context_ptr->limits.texture_buffer_offset_alignment);
        context_ptr->entry_points_private.pGLGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,               &context_ptr->limits.uniform_buffer_offset_alignment);

        /* Retrieve "program binary" limits */
        context_ptr->limits.num_program_binary_formats = 0;
        context_ptr->limits.program_binary_formats     = NULL;

        context_ptr->entry_points_private.pGLGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,
                                                        &context_ptr->limits.num_program_binary_formats);

        GLenum error_code = context_ptr->entry_points_gl.pGLGetError();

        ASSERT_DEBUG_SYNC(error_code == GL_NO_ERROR,
                          "Could not retrieve GL_NUM_PROGRAM_BINARY_FORMATS");
        if (error_code == GL_NO_ERROR)
        {
            context_ptr->limits.program_binary_formats = new (std::nothrow) GLint[context_ptr->limits.num_program_binary_formats];

            ASSERT_ALWAYS_SYNC(context_ptr->limits.program_binary_formats != NULL,
                               "Could not allocate space for supported program binary formats array");
            if (context_ptr->limits.program_binary_formats != NULL)
            {
                context_ptr->entry_points_private.pGLGetIntegerv(GL_PROGRAM_BINARY_FORMATS,
                                                                 context_ptr->limits.program_binary_formats);

                error_code = context_ptr->entry_points_gl.pGLGetError();
                ASSERT_DEBUG_SYNC(error_code == GL_NO_ERROR,
                                  "Could not retrieve GL_PROGRAM_BINARY_FORMATS");
            }
        }
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_buffer_storage_function_pointers(__inout __notnull _ogl_context* context_ptr)
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
PRIVATE void _ogl_context_retrieve_GL_ARB_compute_shader_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_private.pGLDispatchCompute,         "glDispatchCompute"},
        {&context_ptr->entry_points_private.pGLDispatchComputeIndirect, "glDispatchComputeIndirect"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->entry_points_gl_arb_compute_shader.pGLDispatchCompute         = ogl_context_wrappers_glDispatchCompute;
        context_ptr->entry_points_gl_arb_compute_shader.pGLDispatchComputeIndirect = ogl_context_wrappers_glDispatchComputeIndirect;
        context_ptr->gl_arb_compute_shader_support                                 = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_compute_shader_limits(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS, &context_ptr->limits_arb_compute_shader.max_combined_compute_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS,      &context_ptr->limits_arb_compute_shader.max_compute_atomic_counter_buffers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS,             &context_ptr->limits_arb_compute_shader.max_compute_atomic_counters);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_IMAGE_UNIFORMS,              &context_ptr->limits_arb_compute_shader.max_compute_image_uniforms);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,          &context_ptr->limits_arb_compute_shader.max_compute_shared_memory_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS,         &context_ptr->limits_arb_compute_shader.max_compute_texture_image_units);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS,              &context_ptr->limits_arb_compute_shader.max_compute_uniform_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_UNIFORM_COMPONENTS,          &context_ptr->limits_arb_compute_shader.max_compute_uniform_components);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,      &context_ptr->limits_arb_compute_shader.max_compute_work_group_invocations);

    for (int index = 0; index < 3 /* x, y, z */; ++index)
    {
        context_ptr->entry_points_private.pGLGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,
                                                           index,
                                                           context_ptr->limits_arb_compute_shader.max_compute_work_group_count + index);
        context_ptr->entry_points_private.pGLGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,
                                                           index,
                                                           context_ptr->limits_arb_compute_shader.max_compute_work_group_size + index);
    }

    ASSERT_DEBUG_SYNC(context_ptr->entry_points_gl.pGLGetError() == GL_NO_ERROR,
                      "Could not retrieve at least one ARB_compute_shader_limit limit");
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_debug_output_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_gl_arb_debug_output.pGLDebugMessageCallbackARB,"glDebugMessageCallbackARB"},
        {&context_ptr->entry_points_gl_arb_debug_output.pGLDebugMessageControlARB, "glDebugMessageControlARB"},
        {&context_ptr->entry_points_gl_arb_debug_output.pGLDebugMessageInsertARB,  "glDebugMessageInsertARB"},
        {&context_ptr->entry_points_gl_arb_debug_output.pGLGetDebugMessageLogARB,  "glGetDebugMessageLogARB"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->gl_arb_debug_output_support = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_framebuffer_no_attachments_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_gl_arb_framebuffer_no_attachments.pGLFramebufferParameteri,     "glFramebufferParameteri"},
        {&context_ptr->entry_points_gl_arb_framebuffer_no_attachments.pGLGetFramebufferParameteriv, "glGetFramebufferParameteriv"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->gl_arb_framebuffer_no_attachments_support = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_framebuffer_no_attachments_limits(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT,  &context_ptr->limits_arb_framebuffer_no_attachments.max_framebuffer_height);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_LAYERS,  &context_ptr->limits_arb_framebuffer_no_attachments.max_framebuffer_layers);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_SAMPLES, &context_ptr->limits_arb_framebuffer_no_attachments.max_framebuffer_samples);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH,   &context_ptr->limits_arb_framebuffer_no_attachments.max_framebuffer_width);

    ASSERT_DEBUG_SYNC(context_ptr->entry_points_gl.pGLGetError() == GL_NO_ERROR,
                      "Could not retrieve at least one ARB_framebuffer_no_attachments limit");
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_texture_storage_multisample_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_private.pGLTexStorage2DMultisample, "glTexStorage2DMultisample"},
        {&context_ptr->entry_points_private.pGLTexStorage3DMultisample, "glTexStorage3DMultisample"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->gl_arb_texture_storage_multisample_support                                 = true;
        context_ptr->entry_points_gl_arb_texture_storage_multisample.pGLTexStorage2DMultisample = ogl_context_wrappers_glTexStorage2DMultisample;
        context_ptr->entry_points_gl_arb_texture_storage_multisample.pGLTexStorage3DMultisample = ogl_context_wrappers_glTexStorage3DMultisample;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_EXT_direct_state_access_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_private.pGLBindMultiTextureEXT,                                              "glBindMultiTextureEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureImage1DEXT,                                      "glCompressedTextureImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureImage2DEXT,                                      "glCompressedTextureImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureImage3DEXT,                                      "glCompressedTextureImage3DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureSubImage1DEXT,                                   "glCompressedTextureSubImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureSubImage2DEXT,                                   "glCompressedTextureSubImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCompressedTextureSubImage3DEXT,                                   "glCompressedTextureSubImage3DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureImage1DEXT,                                            "glCopyTextureImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureImage2DEXT,                                            "glCopyTextureImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureSubImage1DEXT,                                         "glCopyTextureSubImage1DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureSubImage2DEXT,                                         "glCopyTextureSubImage2DEXT"},
        {&context_ptr->entry_points_private.pGLCopyTextureSubImage3DEXT,                                         "glCopyTextureSubImage3DEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLDisableVertexArrayAttribEXT,                   "glDisableVertexArrayAttribEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLEnableVertexArrayAttribEXT,                    "glEnableVertexArrayAttribEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLFramebufferDrawBufferEXT,                      "glFramebufferDrawBufferEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLFramebufferDrawBuffersEXT,                     "glFramebufferDrawBuffersEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLFramebufferReadBufferEXT,                      "glFramebufferReadBufferEXT"},
        {&context_ptr->entry_points_private.pGLGenerateTextureMipmapEXT,                                         "glGenerateTextureMipmapEXT"},
        {&context_ptr->entry_points_private.pGLGetCompressedTextureImageEXT,                                     "glGetCompressedTextureImageEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetFramebufferParameterivEXT,                  "glGetFramebufferParameterivEXT"},
        {&context_ptr->entry_points_private.pGLGetNamedBufferParameterivEXT,                                     "glGetNamedBufferParameterivEXT"},
        {&context_ptr->entry_points_private.pGLGetNamedBufferPointervEXT,                                        "glGetNamedBufferPointervEXT"},
        {&context_ptr->entry_points_private.pGLGetNamedBufferSubDataEXT,                                         "glGetNamedBufferSubDataEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedFramebufferAttachmentParameterivEXT,   "glGetNamedFramebufferAttachmentParameterivEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedProgramivEXT,                          "glGetNamedProgramivEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLGetNamedRenderbufferParameterivEXT,            "glGetNamedRenderbufferParameterivEXT"},
        {&context_ptr->entry_points_private.pGLGetTextureImageEXT,                                               "glGetTextureImageEXT"},
        {&context_ptr->entry_points_private.pGLGetTextureLevelParameterfvEXT,                                    "glGetTextureLevelParameterfvEXT"},
        {&context_ptr->entry_points_private.pGLGetTextureLevelParameterivEXT,                                    "glGetTextureLevelParameterivEXT"},
        {&context_ptr->entry_points_private.pGLMapNamedBufferEXT,                                                "glMapNamedBufferEXT"},
        {&context_ptr->entry_points_private.pGLNamedBufferDataEXT,                                               "glNamedBufferDataEXT"},
        {&context_ptr->entry_points_private.pGLNamedBufferSubDataEXT,                                            "glNamedBufferSubDataEXT"},
        {&context_ptr->entry_points_private.pGLNamedCopyBufferSubDataEXT,                                        "glNamedCopyBufferSubDataEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferRenderbufferEXT,               "glNamedFramebufferRenderbufferEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTexture1DEXT,                                     "glNamedFramebufferTexture1DEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTexture2DEXT,                                     "glNamedFramebufferTexture2DEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTexture3DEXT,                                     "glNamedFramebufferTexture3DEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTextureEXT,                                       "glNamedFramebufferTextureEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTextureFaceEXT,                                   "glNamedFramebufferTextureFaceEXT"},
        {&context_ptr->entry_points_private.pGLNamedFramebufferTextureLayerEXT,                                  "glNamedFramebufferTextureLayerEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedRenderbufferStorageEXT,                   "glNamedRenderbufferStorageEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedRenderbufferStorageMultisampleEXT,        "glNamedRenderbufferStorageMultisampleEXT"},
        {&context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedRenderbufferStorageMultisampleCoverageEXT,"glNamedRenderbufferStorageMultisampleCoverageEXT"},
        {&context_ptr->entry_points_private.pGLTextureBufferEXT,                                                 "glTextureBufferEXT"},
        {&context_ptr->entry_points_private.pGLTextureBufferRangeEXT,                                            "glTextureBufferRangeEXT"},
        {&context_ptr->entry_points_private.pGLTextureImage1DEXT,                                                "glTextureImage1DEXT"},
        {&context_ptr->entry_points_private.pGLTextureImage2DEXT,                                                "glTextureImage2DEXT"},
        {&context_ptr->entry_points_private.pGLTextureImage3DEXT,                                                "glTextureImage3DEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameteriEXT,                                             "glTextureParameteriEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterivEXT,                                            "glTextureParameterivEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterIivEXT,                                           "glTextureParameterIivEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterIuivEXT,                                          "glTextureParameterIuivEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterfEXT,                                             "glTextureParameterfEXT"},
        {&context_ptr->entry_points_private.pGLTextureParameterfvEXT,                                            "glTextureParameterfvEXT"},
        {&context_ptr->entry_points_private.pGLTextureRenderbufferEXT,                                           "glTextureRenderbufferEXT"},
        {&context_ptr->entry_points_private.pGLTextureStorage1DEXT,                                              "glTextureStorage1DEXT"},
        {&context_ptr->entry_points_private.pGLTextureStorage2DEXT,                                              "glTextureStorage2DEXT"},
        {&context_ptr->entry_points_private.pGLTextureStorage3DEXT,                                              "glTextureStorage3DEXT"},
        {&context_ptr->entry_points_private.pGLTextureSubImage1DEXT,                                             "glTextureSubImage1DEXT"},
        {&context_ptr->entry_points_private.pGLTextureSubImage2DEXT,                                             "glTextureSubImage2DEXT"},
        {&context_ptr->entry_points_private.pGLTextureSubImage3DEXT,                                             "glTextureSubImage3DEXT"},
        {&context_ptr->entry_points_private.pGLUnmapNamedBufferEXT,                                              "glUnmapNamedBufferEXT"},
        {&context_ptr->entry_points_private.pGLVertexArrayVertexAttribOffsetEXT,                                 "glVertexArrayVertexAttribOffsetEXT"},
        {&context_ptr->entry_points_private.pGLVertexArrayVertexAttribIOffsetEXT,                                "glVertexArrayVertexAttribIOffsetEXT"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        if (context_ptr->gl_arb_buffer_storage_support)
        {
            context_ptr->entry_points_private.pGLNamedBufferStorageEXT                    = (PFNGLNAMEDBUFFERSTORAGEEXTPROC) ::wglGetProcAddress("glNamedBufferStorageEXT");
            context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedBufferStorageEXT = ogl_context_wrappers_glNamedBufferStorageEXT;

            if (context_ptr->entry_points_private.pGLNamedBufferStorageEXT == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "DSA version entry-points of GL_ARB_buffer_storage are unavailable in spite of being reported.");
            }
        }

        if (context_ptr->gl_arb_texture_storage_multisample_support)
        {
            context_ptr->entry_points_private.pGLTextureStorage2DMultisampleEXT = (PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC) ::wglGetProcAddress("glTextureStorage2DMultisampleEXT");
            context_ptr->entry_points_private.pGLTextureStorage3DMultisampleEXT = (PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC) ::wglGetProcAddress("glTextureStorage3DMultisampleEXT");

            if (context_ptr->entry_points_private.pGLTextureStorage2DMultisampleEXT == NULL ||
                context_ptr->entry_points_private.pGLTextureStorage3DMultisampleEXT == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "DSA version entry-points of GL_ARB_texture_storage_multisample are unavailable in spite of being reported.");
            }
        }

        context_ptr->gl_ext_direct_state_access_support = true;
    }

    context_ptr->entry_points_gl_ext_direct_state_access.pGLBindMultiTextureEXT               = ogl_context_wrappers_glBindMultiTextureEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureImage1DEXT       = ogl_context_wrappers_glCompressedTextureImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureImage2DEXT       = ogl_context_wrappers_glCompressedTextureImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureImage3DEXT       = ogl_context_wrappers_glCompressedTextureImage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureSubImage1DEXT    = ogl_context_wrappers_glCompressedTextureSubImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureSubImage2DEXT    = ogl_context_wrappers_glCompressedTextureSubImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCompressedTextureSubImage3DEXT    = ogl_context_wrappers_glCompressedTextureSubImage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureImage1DEXT             = ogl_context_wrappers_glCopyTextureImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureImage2DEXT             = ogl_context_wrappers_glCopyTextureImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureSubImage1DEXT          = ogl_context_wrappers_glCopyTextureSubImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureSubImage2DEXT          = ogl_context_wrappers_glCopyTextureSubImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLCopyTextureSubImage3DEXT          = ogl_context_wrappers_glCopyTextureSubImage3DEXT;
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
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTextureFaceEXT    = ogl_context_wrappers_glNamedFramebufferTextureFaceEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLNamedFramebufferTextureLayerEXT   = ogl_context_wrappers_glNamedFramebufferTextureLayerEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureBufferEXT                  = ogl_context_wrappers_glTextureBufferEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureBufferRangeEXT             = ogl_context_wrappers_glTextureBufferRangeEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureImage1DEXT                 = ogl_context_wrappers_glTextureImage1DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureImage2DEXT                 = ogl_context_wrappers_glTextureImage2DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureImage3DEXT                 = ogl_context_wrappers_glTextureImage3DEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameteriEXT              = ogl_context_wrappers_glTextureParameteriEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterivEXT             = ogl_context_wrappers_glTextureParameterivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterfEXT              = ogl_context_wrappers_glTextureParameterfEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterfvEXT             = ogl_context_wrappers_glTextureParameterfvEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterIivEXT            = ogl_context_wrappers_glTextureParameterIivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureParameterIuivEXT           = ogl_context_wrappers_glTextureParameterIuivEXT;
    context_ptr->entry_points_gl_ext_direct_state_access.pGLTextureRenderbufferEXT            = ogl_context_wrappers_glTextureRenderbufferEXT;
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
PRIVATE void _ogl_context_retrieve_GL_ARB_multi_bind_function_pointers(__inout __notnull _ogl_context* context_ptr)
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
PRIVATE void _ogl_context_retrieve_GL_ARB_program_interface_query_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_gl_arb_program_interface_query.pGLGetProgramInterfaceiv,          "glGetProgramInterfaceiv"},
        {&context_ptr->entry_points_gl_arb_program_interface_query.pGLGetProgramResourceIndex,        "glGetProgramResourceIndex"},
        {&context_ptr->entry_points_gl_arb_program_interface_query.pGLGetProgramResourceName,         "glGetProgramResourceName"},
        {&context_ptr->entry_points_gl_arb_program_interface_query.pGLGetProgramResourceiv,           "glGetProgramResourceiv"},
        {&context_ptr->entry_points_gl_arb_program_interface_query.pGLGetProgramResourceLocation,     "glGetProgramResourceLocation"},
        {&context_ptr->entry_points_gl_arb_program_interface_query.pGLGetProgramResourceLocationIndex,"glGetProgramResourceLocationIndex"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->gl_arb_program_interface_query_support = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_shader_storage_buffer_object_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    func_ptr_table_entry func_ptr_table[] =
    {
        {&context_ptr->entry_points_gl_arb_shader_storage_buffer_object.pGLShaderStorageBlockBinding, "glShaderStorageBlockBinding"}
    };
    const unsigned int n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

    if (_ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries) )
    {
        context_ptr->gl_arb_shader_storage_buffer_object_support = true;
    }
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_ARB_shader_storage_buffer_object_limits(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");

    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS,        &context_ptr->limits_arb_shader_storage_buffer_object.max_combined_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,         &context_ptr->limits_arb_shader_storage_buffer_object.max_compute_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS,        &context_ptr->limits_arb_shader_storage_buffer_object.max_fragment_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS,        &context_ptr->limits_arb_shader_storage_buffer_object.max_geometry_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,             &context_ptr->limits_arb_shader_storage_buffer_object.max_shader_storage_block_size);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,        &context_ptr->limits_arb_shader_storage_buffer_object.max_shader_storage_buffer_bindings);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS,    &context_ptr->limits_arb_shader_storage_buffer_object.max_tess_control_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS, &context_ptr->limits_arb_shader_storage_buffer_object.max_tess_evaluation_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS,          &context_ptr->limits_arb_shader_storage_buffer_object.max_vertex_shader_storage_blocks);
    context_ptr->entry_points_private.pGLGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,    &context_ptr->limits_arb_shader_storage_buffer_object.shader_storage_buffer_offset_alignment);

    ASSERT_DEBUG_SYNC(context_ptr->entry_points_gl.pGLGetError() == GL_NO_ERROR,
                      "Could not retrieve at least one ARB_shader_storage_buffer_object limit");
}

/** TODO */
PRIVATE void _ogl_context_retrieve_GL_function_pointers(__inout __notnull _ogl_context* context_ptr)
{
    ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                      "GL-specific function called for a non-GL context");
    ASSERT_DEBUG_SYNC(context_ptr->opengl32_dll_handle == NULL,
                      "OpenGL32.dll already loaded.");

    context_ptr->opengl32_dll_handle = ::LoadLibraryA("opengl32.dll");

    if (context_ptr->opengl32_dll_handle != NULL)
    {
        func_ptr_table_entry func_ptr_table[] =
        {
            {&context_ptr->entry_points_gl.pGLActiveShaderProgram,                 "glActiveShaderProgram"},
            {&context_ptr->entry_points_private.pGLActiveTexture,                  "glActiveTexture"},
            {&context_ptr->entry_points_gl.pGLAttachShader,                        "glAttachShader"},
            {&context_ptr->entry_points_gl.pGLBeginConditionalRender,              "glBeginConditionalRender"},
            {&context_ptr->entry_points_gl.pGLBeginQuery,                          "glBeginQuery"},
            {&context_ptr->entry_points_private.pGLBeginTransformFeedback,         "glBeginTransformFeedback"},
            {&context_ptr->entry_points_gl.pGLBindAttribLocation,                  "glBindAttribLocation"},
            {&context_ptr->entry_points_private.pGLBindBuffer,                     "glBindBuffer"},
            {&context_ptr->entry_points_private.pGLBindBufferBase,                 "glBindBufferBase"},
            {&context_ptr->entry_points_private.pGLBindBufferRange,                "glBindBufferRange"},
            {&context_ptr->entry_points_gl.pGLBindFragDataLocation,                "glBindFragDataLocation"},
            {&context_ptr->entry_points_gl.pGLBindFramebuffer,                     "glBindFramebuffer"},
            {&context_ptr->entry_points_private.pGLBindImageTexture,               "glBindImageTexture"},
            {&context_ptr->entry_points_gl.pGLBindProgramPipeline,                 "glBindProgramPipeline"},
            {&context_ptr->entry_points_private.pGLBindSampler,                    "glBindSampler"},
            {&context_ptr->entry_points_private.pGLBindTexture,                    "glBindTexture"},
            {&context_ptr->entry_points_gl.pGLBindTransformFeedback,               "glBindTransformFeedback"},
            {&context_ptr->entry_points_private.pGLBindVertexArray,                "glBindVertexArray"},
            {&context_ptr->entry_points_private.pGLBlendColor,                     "glBlendColor"},
            {&context_ptr->entry_points_private.pGLBlendEquation,                  "glBlendEquation"},
            {&context_ptr->entry_points_private.pGLBlendEquationSeparate,          "glBlendEquationSeparate"},
            {&context_ptr->entry_points_private.pGLBlendFunc,                      "glBlendFunc"},
            {&context_ptr->entry_points_private.pGLBlendFuncSeparate,              "glBlendFuncSeparate"},
            {&context_ptr->entry_points_gl.pGLBlitFramebuffer,                     "glBlitFramebuffer"},
            {&context_ptr->entry_points_private.pGLBufferData,                     "glBufferData"},
            {&context_ptr->entry_points_private.pGLGetBufferParameteriv,           "glGetBufferParameteriv"},
            {&context_ptr->entry_points_private.pGLGetBufferPointerv,              "glGetBufferPointerv"},
            {&context_ptr->entry_points_private.pGLBufferSubData,                  "glBufferSubData"},
            {&context_ptr->entry_points_gl.pGLCheckFramebufferStatus,              "glCheckFramebufferStatus"},
            {&context_ptr->entry_points_gl.pGLClampColor,                          "glClampColor"},
            {&context_ptr->entry_points_private.pGLClear,                          "glClear"},
            {&context_ptr->entry_points_gl.pGLClearBufferfv,                       "glClearBufferfv"},
            {&context_ptr->entry_points_gl.pGLClearBufferfi,                       "glClearBufferfi"},
            {&context_ptr->entry_points_gl.pGLClearBufferiv,                       "glClearBufferiv"},
            {&context_ptr->entry_points_gl.pGLClearBufferuiv,                      "glClearBufferuiv"},
            {&context_ptr->entry_points_private.pGLClearColor,                     "glClearColor"},
            {&context_ptr->entry_points_gl.pGLClearStencil,                        "glClearStencil"},
            {&context_ptr->entry_points_gl.pGLClearDepth,                          "glClearDepth"},
            {&context_ptr->entry_points_private.pGLColorMask,                      "glColorMask"},
            {&context_ptr->entry_points_gl.pGLColorMaski,                          "glColorMaski"},
            {&context_ptr->entry_points_gl.pGLCompileShader,                       "glCompileShader"},
            {&context_ptr->entry_points_private.pGLCompressedTexImage1D,           "glCompressedTexImage1D"},
            {&context_ptr->entry_points_private.pGLCompressedTexImage2D,           "glCompressedTexImage2D"},
            {&context_ptr->entry_points_private.pGLCompressedTexImage3D,           "glCompressedTexImage3D"},
            {&context_ptr->entry_points_private.pGLCompressedTexSubImage1D,        "glCompressedTexSubImage1D"},
            {&context_ptr->entry_points_private.pGLCompressedTexSubImage2D,        "glCompressedTexSubImage2D"},
            {&context_ptr->entry_points_private.pGLCompressedTexSubImage3D,        "glCompressedTexSubImage3D"},
            {&context_ptr->entry_points_private.pGLCopyTexImage1D,                 "glCopyTexImage1D"},
            {&context_ptr->entry_points_private.pGLCopyTexImage2D,                 "glCopyTexImage2D"},
            {&context_ptr->entry_points_private.pGLCopyTexSubImage1D,              "glCopyTexSubImage1D"},
            {&context_ptr->entry_points_private.pGLCopyTexSubImage2D,              "glCopyTexSubImage2D"},
            {&context_ptr->entry_points_private.pGLCopyTexSubImage3D,              "glCopyTexSubImage3D"},
            {&context_ptr->entry_points_gl.pGLCreateProgram,                       "glCreateProgram"},
            {&context_ptr->entry_points_gl.pGLCreateShader,                        "glCreateShader"},
            {&context_ptr->entry_points_gl.pGLCreateShaderProgramv,                "glCreateShaderProgramv"},
            {&context_ptr->entry_points_private.pGLCullFace,                       "glCullFace"},
            {&context_ptr->entry_points_private.pGLDeleteBuffers,                  "glDeleteBuffers"},
            {&context_ptr->entry_points_gl.pGLDeleteFramebuffers,                  "glDeleteFramebuffers"},
            {&context_ptr->entry_points_gl.pGLDeleteProgram,                       "glDeleteProgram"},
            {&context_ptr->entry_points_gl.pGLDeleteProgramPipelines,              "glDeleteProgramPipelines"},
            {&context_ptr->entry_points_gl.pGLDeleteSamplers,                      "glDeleteSamplers"},
            {&context_ptr->entry_points_gl.pGLDeleteShader,                        "glDeleteShader"},
            {&context_ptr->entry_points_gl.pGLDeleteTextures,                      "glDeleteTextures"},
            {&context_ptr->entry_points_gl.pGLDeleteTransformFeedbacks,            "glDeleteTransformFeedbacks"},
            {&context_ptr->entry_points_gl.pGLDeleteQueries,                       "glDeleteQueries"},
            {&context_ptr->entry_points_private.pGLDeleteVertexArrays,             "glDeleteVertexArrays"},
            {&context_ptr->entry_points_private.pGLDepthFunc,                      "glDepthFunc"},
            {&context_ptr->entry_points_private.pGLDepthMask,                      "glDepthMask"},
            {&context_ptr->entry_points_gl.pGLDepthRange,                          "glDepthRange"},
            {&context_ptr->entry_points_gl.pGLDetachShader,                        "glDetachShader"},
            {&context_ptr->entry_points_private.pGLDisable,                        "glDisable"},
            {&context_ptr->entry_points_private.pGLDisablei,                       "glDisablei"},
            {&context_ptr->entry_points_private.pGLDisableVertexAttribArray,       "glDisableVertexAttribArray"},
            {&context_ptr->entry_points_private.pGLDrawArrays,                     "glDrawArrays"},
            {&context_ptr->entry_points_private.pGLDrawArraysInstanced,            "glDrawArraysInstanced"},
            {&context_ptr->entry_points_private.pGLDrawArraysInstancedBaseInstance,"glDrawArraysInstancedBaseInstance"},
            {&context_ptr->entry_points_gl.pGLDrawBuffer,                          "glDrawBuffer"},
            {&context_ptr->entry_points_gl.pGLDrawBuffers,                         "glDrawBuffers"},
            {&context_ptr->entry_points_private.pGLDrawElements,                   "glDrawElements"},
            {&context_ptr->entry_points_private.pGLDrawElementsInstanced,          "glDrawElementsInstanced"},
            {&context_ptr->entry_points_private.pGLDrawRangeElements,              "glDrawRangeElements"},
            {&context_ptr->entry_points_private.pGLDrawTransformFeedback,          "glDrawTransformFeedback"},
            {&context_ptr->entry_points_private.pGLEnable,                         "glEnable"},
            {&context_ptr->entry_points_private.pGLEnablei,                        "glEnablei"},
            {&context_ptr->entry_points_private.pGLEnableVertexAttribArray,        "glEnableVertexAttribArray"},
            {&context_ptr->entry_points_gl.pGLEndConditionalRender,                "glEndConditionalRender"},
            {&context_ptr->entry_points_gl.pGLEndQuery,                            "glEndQuery"},
            {&context_ptr->entry_points_gl.pGLEndTransformFeedback,                "glEndTransformFeedback"},
            {&context_ptr->entry_points_gl.pGLFinish,                              "glFinish"},
            {&context_ptr->entry_points_gl.pGLFlush,                               "glFlush"},
            {&context_ptr->entry_points_private.pGLFramebufferTexture,             "glFramebufferTexture"},
            {&context_ptr->entry_points_private.pGLFramebufferTexture1D,           "glFramebufferTexture1D"},
            {&context_ptr->entry_points_private.pGLFramebufferTexture2D,           "glFramebufferTexture2D"},
            {&context_ptr->entry_points_private.pGLFramebufferTexture3D,           "glFramebufferTexture3D"},
            {&context_ptr->entry_points_private.pGLFramebufferTextureLayer,        "glFramebufferTextureLayer"},
            {&context_ptr->entry_points_gl.pGLFrontFace,                           "glFrontFace"},
            {&context_ptr->entry_points_gl.pGLGenBuffers,                          "glGenBuffers"},
            {&context_ptr->entry_points_gl.pGLGenerateMipmap,                      "glGenerateMipmap"},
            {&context_ptr->entry_points_gl.pGLGenFramebuffers,                     "glGenFramebuffers"},
            {&context_ptr->entry_points_gl.pGLGenProgramPipelines,                 "glGenProgramPipelines"},
            {&context_ptr->entry_points_gl.pGLGenSamplers,                         "glGenSamplers"},
            {&context_ptr->entry_points_gl.pGLGenTextures,                         "glGenTextures"},
            {&context_ptr->entry_points_gl.pGLGenTransformFeedbacks,               "glGenTransformFeedbacks"},
            {&context_ptr->entry_points_gl.pGLGenQueries,                          "glGenQueries"},
            {&context_ptr->entry_points_gl.pGLGenVertexArrays,                     "glGenVertexArrays"},
            {&context_ptr->entry_points_private.pGLGetActiveAtomicCounterBufferiv, "glGetActiveAtomicCounterBufferiv"},
            {&context_ptr->entry_points_gl.pGLGetActiveAttrib,                     "glGetActiveAttrib"},
            {&context_ptr->entry_points_gl.pGLGetActiveUniform,                    "glGetActiveUniform"},
            {&context_ptr->entry_points_gl.pGLGetActiveUniformBlockName,           "glGetActiveUniformBlockName"},
            {&context_ptr->entry_points_gl.pGLGetActiveUniformBlockiv,             "glGetActiveUniformBlockiv"},
            {&context_ptr->entry_points_gl.pGLGetActiveUniformsiv,                 "glGetActiveUniformsiv"},
            {&context_ptr->entry_points_gl.pGLGetAttribLocation,                   "glGetAttribLocation"},
            {&context_ptr->entry_points_gl.pGLGetAttachedShaders,                  "glGetAttachedShaders"},
            {&context_ptr->entry_points_private.pGLGetBooleani_v,                  "glGetBooleani_v"},
            {&context_ptr->entry_points_private.pGLGetBooleanv,                    "glGetBooleanv"},
            {&context_ptr->entry_points_private.pGLGetBufferParameteri64v,         "glGetBufferParameteri64v"},
            {&context_ptr->entry_points_private.pGLGetBufferSubData,               "glGetBufferSubData"},
            {&context_ptr->entry_points_private.pGLGetCompressedTexImage,          "glGetCompressedTexImage"},
            {&context_ptr->entry_points_private.pGLGetDoublev,                     "glGetDoublev"},
            {&context_ptr->entry_points_gl.pGLGetError,                            "glGetError"},
            {&context_ptr->entry_points_private.pGLGetFloatv,                      "glGetFloatv"},
            {&context_ptr->entry_points_gl.pGLGetFragDataLocation,                 "glGetFragDataLocation"},
            {&context_ptr->entry_points_private.pGLGetInteger64i_v,                "glGetInteger64i_v"},
            {&context_ptr->entry_points_private.pGLGetIntegeri_v,                  "glGetIntegeri_v"},
            {&context_ptr->entry_points_private.pGLGetIntegerv,                    "glGetIntegerv"},
            {&context_ptr->entry_points_gl.pGLGetProgramBinary,                    "glGetProgramBinary"},
            {&context_ptr->entry_points_gl.pGLGetProgramiv,                        "glGetProgramiv"},
            {&context_ptr->entry_points_gl.pGLGetProgramInfoLog,                   "glGetProgramInfoLog"},
            {&context_ptr->entry_points_gl.pGLGetShaderiv,                         "glGetShaderiv"},
            {&context_ptr->entry_points_gl.pGLGetShaderInfoLog,                    "glGetShaderInfoLog"},
            {&context_ptr->entry_points_gl.pGLGetShaderSource,                     "glGetShaderSource"},
            {&context_ptr->entry_points_gl.pGLGetString,                           "glGetString"},
            {&context_ptr->entry_points_gl.pGLGetStringi,                          "glGetStringi"},
            {&context_ptr->entry_points_private.pGLGetTexLevelParameterfv,         "glGetTexLevelParameterfv"},
            {&context_ptr->entry_points_private.pGLGetTexLevelParameteriv,         "glGetTexLevelParameteriv"},
            {&context_ptr->entry_points_private.pGLGetTexParameterfv,              "glGetTexParameterfv"},
            {&context_ptr->entry_points_private.pGLGetTexParameteriv,              "glGetTexParameteriv"},
            {&context_ptr->entry_points_private.pGLGetTexParameterIiv,             "glGetTexParameterIiv"},
            {&context_ptr->entry_points_private.pGLGetTexParameterIuiv,            "glGetTexParameterIuiv"},
            {&context_ptr->entry_points_private.pGLGetTexImage,                    "glGetTexImage"},
            {&context_ptr->entry_points_gl.pGLGetTransformFeedbackVarying,         "glGetTransformFeedbackVarying"},
            {&context_ptr->entry_points_gl.pGLGetUniformBlockIndex,                "glGetUniformBlockIndex"},
            {&context_ptr->entry_points_gl.pGLGetUniformfv,                        "glGetUniformfv"},
            {&context_ptr->entry_points_gl.pGLGetUniformiv,                        "glGetUniformiv"},
            {&context_ptr->entry_points_gl.pGLGetUniformLocation,                  "glGetUniformLocation"},
            {&context_ptr->entry_points_gl.pGLGetUniformuiv,                       "glGetUniformuiv"},
            {&context_ptr->entry_points_gl.pGLGetQueryiv,                          "glGetQueryiv"},
            {&context_ptr->entry_points_gl.pGLGetQueryObjectiv,                    "glGetQueryObjectiv"},
            {&context_ptr->entry_points_gl.pGLGetQueryObjectuiv,                   "glGetQueryObjectuiv"},
            {&context_ptr->entry_points_gl.pGLGetQueryObjectui64v,                 "glGetQueryObjectui64v"},
            {&context_ptr->entry_points_private.pGLGetSamplerParameterfv,          "glGetSamplerParameterfv"},
            {&context_ptr->entry_points_private.pGLGetSamplerParameteriv,          "glGetSamplerParameteriv"},
            {&context_ptr->entry_points_private.pGLGetSamplerParameterIiv,         "glGetSamplerParameterIiv"},
            {&context_ptr->entry_points_private.pGLGetSamplerParameterIuiv,        "glGetSamplerParameterIuiv"},
            {&context_ptr->entry_points_private.pGLGetVertexAttribdv,              "glGetVertexAttribdv"},
            {&context_ptr->entry_points_private.pGLGetVertexAttribfv,              "glGetVertexAttribfv"},
            {&context_ptr->entry_points_private.pGLGetVertexAttribIiv,             "glGetVertexAttribIiv"},
            {&context_ptr->entry_points_private.pGLGetVertexAttribIuiv,            "glGetVertexAttribIuiv"},
            {&context_ptr->entry_points_private.pGLGetVertexAttribiv,              "glGetVertexAttribiv"},
            {&context_ptr->entry_points_private.pGLGetVertexAttribPointerv,        "glGetVertexAttribPointerv"},
            {&context_ptr->entry_points_gl.pGLHint,                                "glHint"},
            {&context_ptr->entry_points_gl.pGLIsBuffer,                            "glIsBuffer"},
            {&context_ptr->entry_points_gl.pGLIsEnabled,                           "glIsEnabled"},
            {&context_ptr->entry_points_gl.pGLIsEnabledi,                          "glIsEnabledi"},
            {&context_ptr->entry_points_gl.pGLIsProgram,                           "glIsProgram"},
            {&context_ptr->entry_points_gl.pGLIsShader,                            "glIsShader"},
            {&context_ptr->entry_points_gl.pGLIsTexture,                           "glIsTexture"},
            {&context_ptr->entry_points_gl.pGLIsTransformFeedback,                 "glIsTransformFeedback"},
            {&context_ptr->entry_points_gl.pGLIsQuery,                             "glIsQuery"},
            {&context_ptr->entry_points_gl.pGLLineWidth,                           "glLineWidth"},
            {&context_ptr->entry_points_gl.pGLLinkProgram,                         "glLinkProgram"},
            {&context_ptr->entry_points_gl.pGLLogicOp,                             "glLogicOp"},
            {&context_ptr->entry_points_private.pGLMapBuffer,                      "glMapBuffer"},
            {&context_ptr->entry_points_private.pGLMapBufferRange,                 "glMapBufferRange"},
            {&context_ptr->entry_points_gl.pGLMemoryBarrier,                       "glMemoryBarrier"},
            {&context_ptr->entry_points_private.pGLMultiDrawArrays,                "glMultiDrawArrays"},
            {&context_ptr->entry_points_private.pGLMultiDrawElements,              "glMultiDrawElements"},
            {&context_ptr->entry_points_private.pGLMultiDrawElementsBaseVertex,    "glMultiDrawElementsBaseVertex"},
            {&context_ptr->entry_points_gl.pGLPauseTransformFeedback,              "glPauseTransformFeedback"},
            {&context_ptr->entry_points_gl.pGLPixelStoref,                         "glPixelStoref"},
            {&context_ptr->entry_points_gl.pGLPixelStorei,                         "glPixelStorei"},
            {&context_ptr->entry_points_gl.pGLPointParameterf,                     "glPointParameterf"},
            {&context_ptr->entry_points_gl.pGLPointParameterfv,                    "glPointParameterfv"},
            {&context_ptr->entry_points_gl.pGLPointParameteri,                     "glPointParameteri"},
            {&context_ptr->entry_points_gl.pGLPointParameteriv,                    "glPointParameteriv"},
            {&context_ptr->entry_points_gl.pGLPolygonMode,                         "glPolygonMode"},
            {&context_ptr->entry_points_gl.pGLPolygonOffset,                       "glPolygonOffset"},
            {&context_ptr->entry_points_gl.pGLPrimitiveRestartIndex,               "glPrimitiveRestartIndex"},
            {&context_ptr->entry_points_gl.pGLProgramBinary,                       "glProgramBinary"},
            {&context_ptr->entry_points_gl.pGLProgramParameteri,                   "glProgramParameteri"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1d,                    "glProgramUniform1d"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1dv,                   "glProgramUniform1dv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1f,                    "glProgramUniform1f"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1fv,                   "glProgramUniform1fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1i,                    "glProgramUniform1i"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1iv,                   "glProgramUniform1iv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1ui,                   "glProgramUniform1ui"},
            {&context_ptr->entry_points_gl.pGLProgramUniform1uiv,                  "glProgramUniform1uiv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2d,                    "glProgramUniform2d"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2dv,                   "glProgramUniform2dv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2f,                    "glProgramUniform2f"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2fv,                   "glProgramUniform2fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2i,                    "glProgramUniform2i"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2iv,                   "glProgramUniform2iv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2ui,                   "glProgramUniform2ui"},
            {&context_ptr->entry_points_gl.pGLProgramUniform2uiv,                  "glProgramUniform2uiv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3d,                    "glProgramUniform3d"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3dv,                   "glProgramUniform3dv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3f,                    "glProgramUniform3f"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3fv,                   "glProgramUniform3fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3i,                    "glProgramUniform3i"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3iv,                   "glProgramUniform3iv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3ui,                   "glProgramUniform3ui"},
            {&context_ptr->entry_points_gl.pGLProgramUniform3uiv,                  "glProgramUniform3uiv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4d,                    "glProgramUniform4d"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4dv,                   "glProgramUniform4dv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4f,                    "glProgramUniform4f"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4fv,                   "glProgramUniform4fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4i,                    "glProgramUniform4i"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4iv,                   "glProgramUniform4iv"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4ui,                   "glProgramUniform4ui"},
            {&context_ptr->entry_points_gl.pGLProgramUniform4uiv,                  "glProgramUniform4uiv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix2fv,             "glProgramUniformMatrix2fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix2x3fv,           "glProgramUniformMatrix2x3fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix2x4fv,           "glProgramUniformMatrix2x4fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix3fv,             "glProgramUniformMatrix3fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix3x2fv,           "glProgramUniformMatrix3x2fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix3x4fv,           "glProgramUniformMatrix3x4fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix4fv,             "glProgramUniformMatrix4fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix4x2fv,           "glProgramUniformMatrix4x2fv"},
            {&context_ptr->entry_points_gl.pGLProgramUniformMatrix4x3fv,           "glProgramUniformMatrix4x3fv"},
            {&context_ptr->entry_points_gl.pGLReadBuffer,                          "glReadBuffer"},
            {&context_ptr->entry_points_private.pGLReadPixels,                     "glReadPixels"},
            {&context_ptr->entry_points_private.pGLResumeTransformFeedback,        "glResumeTransformFeedback"},
            {&context_ptr->entry_points_gl.pGLPointSize,                           "glPointSize"},
            {&context_ptr->entry_points_gl.pGLSampleCoverage,                      "glSampleCoverage"},
            {&context_ptr->entry_points_private.pGLSamplerParameterf,              "glSamplerParameterf"},
            {&context_ptr->entry_points_private.pGLSamplerParameterfv,             "glSamplerParameterfv"},
            {&context_ptr->entry_points_private.pGLSamplerParameterIiv,            "glSamplerParameterIiv"},
            {&context_ptr->entry_points_private.pGLSamplerParameterIuiv,           "glSamplerParameterIuiv"},
            {&context_ptr->entry_points_private.pGLSamplerParameteri,              "glSamplerParameteri"},
            {&context_ptr->entry_points_private.pGLSamplerParameteriv,             "glSamplerParameteriv"},
            {&context_ptr->entry_points_private.pGLScissor,                        "glScissor"},
            {&context_ptr->entry_points_gl.pGLShaderSource,                        "glShaderSource"},
            {&context_ptr->entry_points_gl.pGLStencilFunc,                         "glStencilFunc"},
            {&context_ptr->entry_points_gl.pGLStencilFuncSeparate,                 "glStencilFuncSeparate"},
            {&context_ptr->entry_points_gl.pGLStencilMask,                         "glStencilMask"},
            {&context_ptr->entry_points_gl.pGLStencilMaskSeparate,                 "glStencilMaskSeparate"},
            {&context_ptr->entry_points_gl.pGLStencilOp,                           "glStencilOp"},
            {&context_ptr->entry_points_gl.pGLStencilOpSeparate,                   "glStencilOpSeparate"},
            {&context_ptr->entry_points_private.pGLTexBuffer,                      "glTexBuffer"},
            {&context_ptr->entry_points_private.pGLTexBufferRange,                 "glTexBufferRange"},
            {&context_ptr->entry_points_private.pGLTexImage1D,                     "glTexImage1D"},
            {&context_ptr->entry_points_private.pGLTexImage2D,                     "glTexImage2D"},
            {&context_ptr->entry_points_private.pGLTexImage3D,                     "glTexImage3D"},
            {&context_ptr->entry_points_private.pGLTexParameterf,                  "glTexParameterf"},
            {&context_ptr->entry_points_private.pGLTexParameterfv,                 "glTexParameterfv"},
            {&context_ptr->entry_points_private.pGLTexParameteri,                  "glTexParameteri"},
            {&context_ptr->entry_points_private.pGLTexParameteriv,                 "glTexParameteriv"},
            {&context_ptr->entry_points_private.pGLTexParameterIiv,                "glTexParameterIiv"},
            {&context_ptr->entry_points_private.pGLTexParameterIuiv,               "glTexParameterIuiv"},
            {&context_ptr->entry_points_private.pGLTexStorage1D,                   "glTexStorage1D"},
            {&context_ptr->entry_points_private.pGLTexStorage2D,                   "glTexStorage2D"},
            {&context_ptr->entry_points_private.pGLTexStorage3D,                   "glTexStorage3D"},
            {&context_ptr->entry_points_private.pGLTexSubImage1D,                  "glTexSubImage1D"},
            {&context_ptr->entry_points_private.pGLTexSubImage2D,                  "glTexSubImage2D"},
            {&context_ptr->entry_points_private.pGLTexSubImage3D,                  "glTexSubImage3D"},
            {&context_ptr->entry_points_gl.pGLTransformFeedbackVaryings,           "glTransformFeedbackVaryings"},
            {&context_ptr->entry_points_gl.pGLUniformBlockBinding,                 "glUniformBlockBinding"},
            {&context_ptr->entry_points_private.pGLUnmapBuffer,                    "glUnmapBuffer"},
            {&context_ptr->entry_points_private.pGLUseProgram,                     "glUseProgram"},
            {&context_ptr->entry_points_gl.pGLUseProgramStages,                    "glUseProgramStages"},
            {&context_ptr->entry_points_gl.pGLValidateProgram,                     "glValidateProgram"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib1d,                      "glVertexAttrib1d"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib1dv,                     "glVertexAttrib1dv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib1f,                      "glVertexAttrib1f"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib1fv,                     "glVertexAttrib1fv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib1s,                      "glVertexAttrib1s"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib1sv,                     "glVertexAttrib1sv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib2d,                      "glVertexAttrib2d"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib2dv,                     "glVertexAttrib2dv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib2f,                      "glVertexAttrib2f"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib2fv,                     "glVertexAttrib2fv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib2s,                      "glVertexAttrib2s"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib2sv,                     "glVertexAttrib2sv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib3d,                      "glVertexAttrib3d"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib3dv,                     "glVertexAttrib3dv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib3f,                      "glVertexAttrib3f"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib3fv,                     "glVertexAttrib3fv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib3s,                      "glVertexAttrib3s"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib3sv,                     "glVertexAttrib3sv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Nbv,                    "glVertexAttrib4Nbv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Niv,                    "glVertexAttrib4Niv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Nsv,                    "glVertexAttrib4Nsv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Nub,                    "glVertexAttrib4Nub"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Nubv,                   "glVertexAttrib4Nubv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Nuiv,                   "glVertexAttrib4Nuiv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4Nusv,                   "glVertexAttrib4Nusv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4bv,                     "glVertexAttrib4bv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4d,                      "glVertexAttrib4d"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4dv,                     "glVertexAttrib4dv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4f,                      "glVertexAttrib4f"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4fv,                     "glVertexAttrib4fv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4iv,                     "glVertexAttrib4iv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4s,                      "glVertexAttrib4s"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4sv,                     "glVertexAttrib4sv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4ubv,                    "glVertexAttrib4ubv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4uiv,                    "glVertexAttrib4uiv"},
            {&context_ptr->entry_points_gl.pGLVertexAttrib4usv,                    "glVertexAttrib4usv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribDivisor,                 "glVertexAttribDivisor"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI1i,                     "glVertexAttribI1i"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI1iv,                    "glVertexAttribI1iv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI1ui,                    "glVertexAttribI1ui"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI1uiv,                   "glVertexAttribI1uiv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI2i,                     "glVertexAttribI2i"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI2iv,                    "glVertexAttribI2iv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI2ui,                    "glVertexAttribI2ui"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI2uiv,                   "glVertexAttribI2uiv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI3i,                     "glVertexAttribI3i"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI3iv,                    "glVertexAttribI3iv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI3ui,                    "glVertexAttribI3ui"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI3uiv,                   "glVertexAttribI3uiv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4bv,                    "glVertexAttribI4bv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4i,                     "glVertexAttribI4i"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4iv,                    "glVertexAttribI4iv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4sv,                    "glVertexAttribI4sv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4ubv,                   "glVertexAttribI4ubv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4ui,                    "glVertexAttribI4ui"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4uiv,                   "glVertexAttribI4uiv"},
            {&context_ptr->entry_points_gl.pGLVertexAttribI4usv,                   "glVertexAttribI4usv"},
            {&context_ptr->entry_points_private.pGLVertexAttribIPointer,           "glVertexAttribIPointer"},
            {&context_ptr->entry_points_private.pGLVertexAttribPointer,            "glVertexAttribPointer"},
            {&context_ptr->entry_points_private.pGLViewport,                       "glViewport"}
        };
        const uint32_t n_func_ptr_table_entries = sizeof(func_ptr_table) / sizeof(func_ptr_table[0]);

        _ogl_context_get_function_pointers(context_ptr,
                                           func_ptr_table,
                                           n_func_ptr_table_entries);

        /* Configure wrapper functions */
        context_ptr->entry_points_gl.pGLActiveTexture                   = ogl_context_wrappers_glActiveTexture;
        context_ptr->entry_points_gl.pGLBeginTransformFeedback          = ogl_context_wrappers_glBeginTransformFeedback;
        context_ptr->entry_points_gl.pGLBindBuffer                      = ogl_context_wrappers_glBindBuffer;
        context_ptr->entry_points_gl.pGLBindBufferBase                  = ogl_context_wrappers_glBindBufferBase;
        context_ptr->entry_points_gl.pGLBindBufferRange                 = ogl_context_wrappers_glBindBufferRange;
        context_ptr->entry_points_gl.pGLBindImageTexture                = ogl_context_wrappers_glBindImageTextureEXT;
        context_ptr->entry_points_gl.pGLBindSampler                     = ogl_context_wrappers_glBindSampler;
        context_ptr->entry_points_gl.pGLBindTexture                     = ogl_context_wrappers_glBindTexture;
        context_ptr->entry_points_gl.pGLBindVertexArray                 = ogl_context_wrappers_glBindVertexArray;
        context_ptr->entry_points_gl.pGLBlendColor                      = ogl_context_wrappers_glBlendColor;
        context_ptr->entry_points_gl.pGLBlendEquation                   = ogl_context_wrappers_glBlendEquation;
        context_ptr->entry_points_gl.pGLBlendEquationSeparate           = ogl_context_wrappers_glBlendEquationSeparate;
        context_ptr->entry_points_gl.pGLBlendFunc                       = ogl_context_wrappers_glBlendFunc;
        context_ptr->entry_points_gl.pGLBlendFuncSeparate               = ogl_context_wrappers_glBlendFuncSeparate;
        context_ptr->entry_points_gl.pGLBufferData                      = ogl_context_wrappers_glBufferData;
        context_ptr->entry_points_gl.pGLBufferSubData                   = ogl_context_wrappers_glBufferSubData;
        context_ptr->entry_points_gl.pGLClear                           = ogl_context_wrappers_glClear;
        context_ptr->entry_points_gl.pGLClearColor                      = ogl_context_wrappers_glClearColor;
        context_ptr->entry_points_gl.pGLColorMask                       = ogl_context_wrappers_glColorMask;
        context_ptr->entry_points_gl.pGLCompressedTexImage1D            = ogl_context_wrappers_glCompressedTexImage1D;
        context_ptr->entry_points_gl.pGLCompressedTexImage2D            = ogl_context_wrappers_glCompressedTexImage2D;
        context_ptr->entry_points_gl.pGLCompressedTexImage3D            = ogl_context_wrappers_glCompressedTexImage3D;
        context_ptr->entry_points_gl.pGLCompressedTexSubImage1D         = ogl_context_wrappers_glCompressedTexSubImage1D;
        context_ptr->entry_points_gl.pGLCompressedTexSubImage2D         = ogl_context_wrappers_glCompressedTexSubImage2D;
        context_ptr->entry_points_gl.pGLCompressedTexSubImage3D         = ogl_context_wrappers_glCompressedTexSubImage3D;
        context_ptr->entry_points_gl.pGLCopyTexImage1D                  = ogl_context_wrappers_glCopyTexImage1D;
        context_ptr->entry_points_gl.pGLCopyTexImage2D                  = ogl_context_wrappers_glCopyTexImage2D;
        context_ptr->entry_points_gl.pGLCopyTexSubImage1D               = ogl_context_wrappers_glCopyTexSubImage1D;
        context_ptr->entry_points_gl.pGLCopyTexSubImage2D               = ogl_context_wrappers_glCopyTexSubImage2D;
        context_ptr->entry_points_gl.pGLCopyTexSubImage3D               = ogl_context_wrappers_glCopyTexSubImage3D;
        context_ptr->entry_points_gl.pGLCullFace                        = ogl_context_wrappers_glCullFace;
        context_ptr->entry_points_gl.pGLDeleteBuffers                   = ogl_context_wrappers_glDeleteBuffers;
        context_ptr->entry_points_gl.pGLDeleteVertexArrays              = ogl_context_wrappers_glDeleteVertexArrays;
        context_ptr->entry_points_gl.pGLDepthFunc                       = ogl_context_wrappers_glDepthFunc;
        context_ptr->entry_points_gl.pGLDepthMask                       = ogl_context_wrappers_glDepthMask;
        context_ptr->entry_points_gl.pGLDisable                         = ogl_context_wrappers_glDisable;
        context_ptr->entry_points_gl.pGLDisablei                        = ogl_context_wrappers_glDisablei;
        context_ptr->entry_points_gl.pGLDisableVertexAttribArray        = ogl_context_wrappers_glDisableVertexAttribArray;
        context_ptr->entry_points_gl.pGLDrawArrays                      = ogl_context_wrappers_glDrawArrays;
        context_ptr->entry_points_gl.pGLDrawArraysInstanced             = ogl_context_wrappers_glDrawArraysInstanced;
        context_ptr->entry_points_gl.pGLDrawArraysInstancedBaseInstance = ogl_context_wrappers_glDrawArraysInstancedBaseInstance;
        context_ptr->entry_points_gl.pGLDrawElements                    = ogl_context_wrappers_glDrawElements;
        context_ptr->entry_points_gl.pGLDrawElementsInstanced           = ogl_context_wrappers_glDrawElementsInstanced;
        context_ptr->entry_points_gl.pGLDrawRangeElements               = ogl_context_wrappers_glDrawRangeElements;
        context_ptr->entry_points_gl.pGLDrawTransformFeedback           = ogl_context_wrappers_glDrawTransformFeedback;
        context_ptr->entry_points_gl.pGLEnable                          = ogl_context_wrappers_glEnable;
        context_ptr->entry_points_gl.pGLEnablei                         = ogl_context_wrappers_glEnablei;
        context_ptr->entry_points_gl.pGLEnableVertexAttribArray         = ogl_context_wrappers_glEnableVertexAttribArray;
        context_ptr->entry_points_gl.pGLFramebufferTexture              = ogl_context_wrappers_glFramebufferTexture;
        context_ptr->entry_points_gl.pGLFramebufferTexture1D            = ogl_context_wrappers_glFramebufferTexture1D;
        context_ptr->entry_points_gl.pGLFramebufferTexture2D            = ogl_context_wrappers_glFramebufferTexture2D;
        context_ptr->entry_points_gl.pGLFramebufferTexture3D            = ogl_context_wrappers_glFramebufferTexture3D;
        context_ptr->entry_points_gl.pGLFramebufferTextureLayer         = ogl_context_wrappers_glFramebufferTextureLayer;
        context_ptr->entry_points_gl.pGLGetActiveAtomicCounterBufferiv  = ogl_context_wrappers_glGetActiveAtomicCounterBufferiv;
        context_ptr->entry_points_gl.pGLGetBooleani_v                   = ogl_context_wrappers_glGetBooleani_v;
        context_ptr->entry_points_gl.pGLGetBooleanv                     = ogl_context_wrappers_glGetBooleanv;
        context_ptr->entry_points_gl.pGLGetBufferParameteriv            = ogl_context_wrappers_glGetBufferParameteriv;
        context_ptr->entry_points_gl.pGLGetBufferParameteri64v          = ogl_context_wrappers_glGetBufferParameteri64v;
        context_ptr->entry_points_gl.pGLGetBufferPointerv               = ogl_context_wrappers_glGetBufferPointerv;
        context_ptr->entry_points_gl.pGLGetBufferSubData                = ogl_context_wrappers_glGetBufferSubData;
        context_ptr->entry_points_gl.pGLGetCompressedTexImage           = ogl_context_wrappers_glGetCompressedTexImage;
        context_ptr->entry_points_gl.pGLGetDoublev                      = ogl_context_wrappers_glGetDoublev;
        context_ptr->entry_points_gl.pGLGetFloatv                       = ogl_context_wrappers_glGetFloatv;
        context_ptr->entry_points_gl.pGLGetInteger64i_v                 = ogl_context_wrappers_glGetInteger64i_v;
        context_ptr->entry_points_gl.pGLGetIntegeri_v                   = ogl_context_wrappers_glGetIntegeri_v;
        context_ptr->entry_points_gl.pGLGetIntegerv                     = ogl_context_wrappers_glGetIntegerv;
        context_ptr->entry_points_gl.pGLGetSamplerParameterfv           = ogl_context_wrappers_glGetSamplerParameterfv;
        context_ptr->entry_points_gl.pGLGetSamplerParameteriv           = ogl_context_wrappers_glGetSamplerParameteriv;
        context_ptr->entry_points_gl.pGLGetSamplerParameterIiv          = ogl_context_wrappers_glGetSamplerParameterIiv;
        context_ptr->entry_points_gl.pGLGetSamplerParameterIuiv         = ogl_context_wrappers_glGetSamplerParameterIuiv;
        context_ptr->entry_points_gl.pGLGetTexImage                     = ogl_context_wrappers_glGetTexImage;
        context_ptr->entry_points_gl.pGLGetTexLevelParameterfv          = ogl_context_wrappers_glGetTexLevelParameterfv;
        context_ptr->entry_points_gl.pGLGetTexLevelParameteriv          = ogl_context_wrappers_glGetTexLevelParameteriv;
        context_ptr->entry_points_gl.pGLGetTexParameterfv               = ogl_context_wrappers_glGetTexParameterfv;
        context_ptr->entry_points_gl.pGLGetTexParameterIiv              = ogl_context_wrappers_glGetTexParameterIiv;
        context_ptr->entry_points_gl.pGLGetTexParameterIuiv             = ogl_context_wrappers_glGetTexParameterIuiv;
        context_ptr->entry_points_gl.pGLGetTexParameteriv               = ogl_context_wrappers_glGetTexParameteriv;
        context_ptr->entry_points_gl.pGLGetVertexAttribdv               = ogl_context_wrappers_glGetVertexAttribdv;
        context_ptr->entry_points_gl.pGLGetVertexAttribfv               = ogl_context_wrappers_glGetVertexAttribfv;
        context_ptr->entry_points_gl.pGLGetVertexAttribIiv              = ogl_context_wrappers_glGetVertexAttribIiv;
        context_ptr->entry_points_gl.pGLGetVertexAttribIuiv             = ogl_context_wrappers_glGetVertexAttribIuiv;
        context_ptr->entry_points_gl.pGLGetVertexAttribiv               = ogl_context_wrappers_glGetVertexAttribiv;
        context_ptr->entry_points_gl.pGLGetVertexAttribPointerv         = ogl_context_wrappers_glGetVertexAttribPointerv;
        context_ptr->entry_points_gl.pGLMapBuffer                       = ogl_context_wrappers_glMapBuffer;
        context_ptr->entry_points_gl.pGLMapBufferRange                  = ogl_context_wrappers_glMapBufferRange;
        context_ptr->entry_points_gl.pGLMultiDrawArrays                 = ogl_context_wrappers_glMultiDrawArrays;
        context_ptr->entry_points_gl.pGLMultiDrawElements               = ogl_context_wrappers_glMultiDrawElements;
        context_ptr->entry_points_gl.pGLMultiDrawElementsBaseVertex     = ogl_context_wrappers_glMultiDrawElementsBaseVertex;
        context_ptr->entry_points_gl.pGLReadPixels                      = ogl_context_wrappers_glReadPixels;
        context_ptr->entry_points_gl.pGLResumeTransformFeedback         = ogl_context_wrappers_glResumeTransformFeedback;
        context_ptr->entry_points_gl.pGLSamplerParameterf               = ogl_context_wrappers_glSamplerParameterf;
        context_ptr->entry_points_gl.pGLSamplerParameterfv              = ogl_context_wrappers_glSamplerParameterfv;
        context_ptr->entry_points_gl.pGLSamplerParameterIiv             = ogl_context_wrappers_glSamplerParameterIiv;
        context_ptr->entry_points_gl.pGLSamplerParameterIuiv            = ogl_context_wrappers_glSamplerParameterIuiv;
        context_ptr->entry_points_gl.pGLSamplerParameteri               = ogl_context_wrappers_glSamplerParameteri;
        context_ptr->entry_points_gl.pGLSamplerParameteriv              = ogl_context_wrappers_glSamplerParameteriv;
        context_ptr->entry_points_gl.pGLScissor                         = ogl_context_wrappers_glScissor;
        context_ptr->entry_points_gl.pGLTexBuffer                       = ogl_context_wrappers_glTexBuffer;
        context_ptr->entry_points_gl.pGLTexImage1D                      = ogl_context_wrappers_glTexImage1D;
        context_ptr->entry_points_gl.pGLTexImage2D                      = ogl_context_wrappers_glTexImage2D;
        context_ptr->entry_points_gl.pGLTexImage3D                      = ogl_context_wrappers_glTexImage3D;
        context_ptr->entry_points_gl.pGLTexParameterf                   = ogl_context_wrappers_glTexParameterf;
        context_ptr->entry_points_gl.pGLTexParameterfv                  = ogl_context_wrappers_glTexParameterfv;
        context_ptr->entry_points_gl.pGLTexParameterIiv                 = ogl_context_wrappers_glTexParameterIiv;
        context_ptr->entry_points_gl.pGLTexParameterIuiv                = ogl_context_wrappers_glTexParameterIuiv;
        context_ptr->entry_points_gl.pGLTexParameteri                   = ogl_context_wrappers_glTexParameteri;
        context_ptr->entry_points_gl.pGLTexParameteriv                  = ogl_context_wrappers_glTexParameteriv;
        context_ptr->entry_points_gl.pGLTexStorage1D                    = ogl_context_wrappers_glTexStorage1D;
        context_ptr->entry_points_gl.pGLTexStorage2D                    = ogl_context_wrappers_glTexStorage2D;
        context_ptr->entry_points_gl.pGLTexStorage3D                    = ogl_context_wrappers_glTexStorage3D;
        context_ptr->entry_points_gl.pGLTexSubImage1D                   = ogl_context_wrappers_glTexSubImage1D;
        context_ptr->entry_points_gl.pGLTexSubImage2D                   = ogl_context_wrappers_glTexSubImage2D;
        context_ptr->entry_points_gl.pGLTexSubImage3D                   = ogl_context_wrappers_glTexSubImage3D;
        context_ptr->entry_points_gl.pGLUnmapBuffer                     = ogl_context_wrappers_glUnmapBuffer;
        context_ptr->entry_points_gl.pGLUseProgram                      = ogl_context_wrappers_glUseProgram;
        context_ptr->entry_points_gl.pGLVertexAttribIPointer            = ogl_context_wrappers_glVertexAttribIPointer;
        context_ptr->entry_points_gl.pGLVertexAttribPointer             = ogl_context_wrappers_glVertexAttribPointer;
        context_ptr->entry_points_gl.pGLViewport                        = ogl_context_wrappers_glViewport;
    }
    else
    {
        LOG_FATAL("Could not locate opengl32.dll");
    }
}

/** Please see header for specification */
PUBLIC void ogl_context_bind_to_current_thread(__in __notnull ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    ::wglMakeCurrent(context_ptr->device_context_handle, context_ptr->wgl_rendering_context);

    _current_context = context;
    ogl_context_wrappers_set_private_functions( (context_ptr != NULL) ? &context_ptr->entry_points_private : NULL);
}

/** Please see header for specification */
PUBLIC void ogl_context_unbind_from_current_thread()
{
    ::wglMakeCurrent(NULL, NULL);
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
PUBLIC EMERALD_API ogl_context ogl_context_create_from_system_window(__in __notnull system_hashed_ansi_string   name,
                                                                     __in __notnull ogl_context_type            type,
                                                                     __in __notnull system_window               window,
                                                                     __in __notnull ogl_pixel_format_descriptor in_pfd,
                                                                     __in           bool                        vsync_enabled,
                                                                     __in           ogl_context                 share_context,
                                                                     __in           bool                        allow_multisampling)
{
    system_window_handle window_handle = NULL;
    ogl_context          result        = NULL;

    if (system_window_get_handle(window, &window_handle) )
    {
        system_window_dc window_dc = NULL;

        if (system_window_get_dc(window, &window_dc) )
        {
            const PIXELFORMATDESCRIPTOR* system_pfd = _ogl_pixel_format_descriptor_get_descriptor(in_pfd);

            if (system_pfd != NULL)
            {
                /* Configure the device context handle to use the desired pixel format. */
                int pixel_format_index = ::ChoosePixelFormat(window_dc,
                                                             system_pfd);

                if (pixel_format_index == 0)
                {
                    LOG_FATAL("Requested pixel format is unavailable on the running platform!");
                }
                else
                {
                    /* Set the pixel format. */
                    if (!::SetPixelFormat(window_dc, pixel_format_index, system_pfd) )
                    {
                        LOG_FATAL("Could not set pixel format.");
                    }
                    else
                    {
                        /* Create a temporary WGL context that we will use to initialize WGL context for GL3.3 */
                        HGLRC temp_wgl_context = ::wglCreateContext(window_dc);

                        ::wglMakeCurrent(window_dc,
                                         temp_wgl_context);

                        /* Create WGL rendering context */
                        PFNWGLCREATECONTEXTATTRIBSARBPROC pWGLCreateContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC) ::wglGetProcAddress("wglCreateContextAttribsARB");

                        /* Okay, let's check the func ptr */
                        if (pWGLCreateContextAttribs == NULL)
                        {
                            LOG_FATAL("OpenGL 4.2 is unavailable - could not obtain func ptr to wglCreateContextAttribsARB! Update your drivers.");
                        }
                        else
                        {
                            /* If context we are to share objects with is not NULL, lock corresponding renderer thread before continuing.
                             * Otherwise GL impl could potentially forbid the context creation (happens on NViDiA drivers) */
                            system_window         share_context_window            = NULL;
                            ogl_rendering_handler share_context_rendering_handler = NULL;

                            if (share_context != NULL)
                            {
                                share_context_window = ((_ogl_context*)share_context)->window;

                                if (!system_window_get_rendering_handler(share_context_window, &share_context_rendering_handler) )
                                {
                                    ASSERT_DEBUG_SYNC(false, "Could not retrieve rendering handler for shared context's window.");
                                }

                                ogl_rendering_handler_lock_bound_context(share_context_rendering_handler);
                            }

                            /* Okay, try creating the context */
                            int context_major_version      = 4;
                            int context_minor_version      = 2;
                            int context_profile_mask_key   = 0;
                            int context_profile_mask_value = 0;

                            if (type == OGL_CONTEXT_TYPE_ES)
                            {
                                context_major_version      = 3;
                                context_minor_version      = 1;
                                context_profile_mask_key   = WGL_CONTEXT_PROFILE_MASK_ARB;
                                context_profile_mask_value = WGL_CONTEXT_ES2_PROFILE_BIT_EXT;
                            }
                            else
                            {
                                ASSERT_DEBUG_SYNC(type == OGL_CONTEXT_TYPE_GL,
                                                  "Unrecognized context type requested");
                            }

                            const int wgl_attrib_list[]     = {WGL_CONTEXT_MAJOR_VERSION_ARB, context_major_version,
                                                               WGL_CONTEXT_MINOR_VERSION_ARB, context_minor_version,
                                                               WGL_CONTEXT_FLAGS_ARB,
                            #ifdef _DEBUG
                                                                                              WGL_CONTEXT_DEBUG_BIT_ARB,
                            #else
                                                                                              0,
                            #endif
                                                              WGL_CONTEXT_FLAGS_ARB,          WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                                              context_profile_mask_key,       context_profile_mask_value,
                                                              0
                                                              };
                            HGLRC     wgl_rendering_context = pWGLCreateContextAttribs(window_dc,
                                                                                       (share_context != NULL ? ((_ogl_context*)share_context)->wgl_rendering_context : NULL),
                                                                                       wgl_attrib_list);

                            /* Sharing? Unlock corresponding renderer thread */
                            if (share_context != NULL)
                            {
                                ogl_rendering_handler_unlock_bound_context(share_context_rendering_handler);
                            }

                            if (wgl_rendering_context == NULL)
                            {
                                LOG_FATAL("Could not create WGL rendering context. [GetLastError():%d]", ::GetLastError() );
                            }
                            else
                            {
                                /* Cool. Create the ogl_context instance. */
                                _ogl_context* _result = new (std::nothrow) _ogl_context;

                                ASSERT_ALWAYS_SYNC(_result != NULL, "Out of memory when creating ogl_context instance.");
                                if (_result != NULL)
                                {
                                    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(_result,
                                                                                   _ogl_context_release,
                                                                                   OBJECT_TYPE_OGL_CONTEXT,
                                                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Contexts\\",
                                                                                                                                           system_hashed_ansi_string_get_buffer(name) )
                                                                                   );

                                    memset(&_result->limits,
                                           0,
                                           sizeof(_result->limits) );

                                    _result->bo_bindings                                = NULL;
                                    _result->context_type                               = type;
                                    _result->device_context_handle                      = window_dc;
                                    _result->es_ext_texture_buffer_support              = false;
                                    _result->gl_arb_buffer_storage_support              = false;
                                    _result->gl_arb_compute_shader_support              = false;
                                    _result->gl_arb_debug_output_support                = false;
                                    _result->gl_arb_framebuffer_no_attachments_support  = false;
                                    _result->gl_arb_multi_bind_support                  = false;
                                    _result->gl_arb_program_interface_query_support     = false;
                                    _result->gl_arb_texture_buffer_object_rgb32_support = false;
                                    _result->gl_ext_direct_state_access_support         = false;

                                    _result->wgl_swap_control_support                   = false;
                                    _result->wgl_swap_control_tear_support              = false;
                                    _result->pWGLSwapIntervalEXT                        = NULL;

                                    _result->primitive_renderer                         = NULL;
                                    _result->materials                                  = ogl_materials_create( (ogl_context) _result);
                                    _result->opengl32_dll_handle                        = NULL;
                                    _result->multisampling_samples                      = 0;
                                    _result->multisampling_supported_sample             = NULL;
                                    _result->n_multisampling_supported_sample           = 0;
                                    _result->pfd                                        = in_pfd;
                                    _result->programs                                   = ogl_programs_create();
                                    _result->sampler_bindings                           = NULL;
                                    _result->state_cache                                = NULL;
                                    _result->samplers                                   = ogl_samplers_create( (ogl_context) _result);
                                    _result->shaders                                    = ogl_shaders_create();
                                    _result->shadow_mapping                             = NULL;
                                    _result->texture_compression                        = NULL;
                                    _result->textures                                   = ogl_textures_create( (ogl_context) _result);
                                    _result->to_bindings                                = NULL;
                                    _result->vao_no_vaas_id                             = 0;
                                    _result->vsync_enabled                              = vsync_enabled;
                                    _result->wgl_rendering_context                      = wgl_rendering_context;
                                    _result->window                                     = window;
                                    _result->window_handle                              = window_handle;

                                    ogl_pixel_format_descriptor_retain(in_pfd);

                                    result = (ogl_context) _result;

                                    /* Initialize WGL extensions */
                                    _ogl_context_initialize_wgl_extensions(_result);

                                    /* Associate DC with the WGL context and with current thread.*/
                                    ::wglMakeCurrent(NULL, NULL);
                                    ::wglMakeCurrent(window_dc, wgl_rendering_context);

                                    /* OpenGL ES support is supposed to be low-level. For OpenGL, we use additional tools like
                                     * state caching to improve rendering efficiency.
                                     */
                                    if (type == OGL_CONTEXT_TYPE_ES)
                                    {
                                        _ogl_context_retrieve_ES_function_pointers(_result);

                                        /* Retrieve GL context info */
                                        _ogl_context_retrieve_GL_info(_result);

                                        /* If GL_EXT_texture_buffer is supported, initialize func pointers */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_EXT_texture_buffer") ))
                                        {
                                            _ogl_context_initialize_es_ext_texture_buffer_extension(_result);
                                        }
                                    }
                                    else
                                    {
                                        ASSERT_DEBUG_SYNC(type == OGL_CONTEXT_TYPE_GL,
                                                          "Unrecognized context type");

                                        /* Retrieve function pointers for the context */
                                        _ogl_context_retrieve_GL_function_pointers(_result);

                                        /* Initialize state caching mechanisms */
                                        _result->bo_bindings         = ogl_context_bo_bindings_create        ( (ogl_context) _result);
                                        _result->sampler_bindings    = ogl_context_sampler_bindings_create   ( (ogl_context) _result);
                                        _result->state_cache         = ogl_context_state_cache_create        ( (ogl_context) _result);
                                        _result->texture_compression = ogl_context_texture_compression_create( (ogl_context) _result);
                                        _result->to_bindings         = ogl_context_to_bindings_create        ( (ogl_context) _result);

                                        /* Retrieve GL context limitations */
                                        _ogl_context_retrieve_GL_limits(_result);

                                        /* Retrieve GL context info */
                                        _ogl_context_retrieve_GL_info(_result);

                                        /* If GL_ARB_bufffer_storage is supported, initialize func pointers */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_buffer_storage") ))
                                        {
                                            _ogl_context_initialize_gl_arb_buffer_storage_extension(_result);
                                        }

                                        /* If GL_ARB_compute_shader is supported, initialize func pointers and info variables */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_compute_shader") ))
                                        {
                                            _ogl_context_initialize_gl_arb_compute_shader_extension(_result);
                                        }

                                        /* If GL_ARB_framebuffer_no_attachments is supported, initialize func pointers and info variables */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_framebuffer_no_attachments") ))
                                        {
                                            _ogl_context_initialize_gl_arb_framebuffer_no_attachments_extension(_result);
                                        }

                                        /* If GL_ARB_multi_bind is supported, initialize func pointers */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_multi_bind") ))
                                        {
                                            _ogl_context_initialize_gl_arb_multi_bind_extension(_result);
                                        }

                                        /* If GL_ARB_program_interface_query is supported, initialize func pointers */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_program_interface_query") ))
                                        {
                                            _ogl_context_initialize_gl_arb_program_interface_query_extension(_result);
                                        }

                                        /* If GL_ARB_shader_storage_buffer_object is supported, initialize func pointers and info variables */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_shader_storage_buffer_object") ))
                                        {
                                            _ogl_context_initialize_gl_arb_shader_storage_buffer_object_extension(_result);
                                        }

                                        /* If GL_ARB_texture_storage_multisample is supported, initialize func pointers */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_texture_storage_multisample") ))
                                        {
                                            _ogl_context_initialize_gl_arb_texture_storage_multisample_extension(_result);
                                        }

                                        /* Try to initialize texture buffer object RGB32 extension */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_texture_buffer_object_rgb32") ))
                                        {
                                            _result->gl_arb_texture_buffer_object_rgb32_support = true;
                                        }
                                        else
                                        {
                                            LOG_ERROR("GL_ARB_texture_buffer_object_rgb32 extension is not supported - demo will crash if 3-banded SH is used.");
                                        }

                                        /* Try to initialize DSA func ptrs */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_EXT_direct_state_access") ))
                                        {
                                            _ogl_context_initialize_gl_ext_direct_state_access_extension(_result);
                                        }
                                        else
                                        {
                                            ASSERT_ALWAYS_SYNC(false, "Direct State Access OpenGL extension is unavailable - the demo is very likely to crash");
                                        }

                                        /* Try to initialize debug output func ptrs */
                                        if (ogl_context_is_extension_supported(result,
                                                                               system_hashed_ansi_string_create("GL_ARB_debug_output") ))
                                        {
                                            _ogl_context_initialize_gl_arb_debug_output_extension(_result);

                                            #ifdef _DEBUG
                                            {
                                                /* Debug build! Go ahead and regitser for callbacks */
                                                _result->entry_points_gl_arb_debug_output.pGLDebugMessageCallbackARB(_ogl_context_debug_message_gl_callback,
                                                                                                                     _result);
                                                _result->entry_points_gl_arb_debug_output.pGLDebugMessageControlARB (GL_DONT_CARE,
                                                                                                                     GL_DONT_CARE,
                                                                                                                     GL_DONT_CARE,
                                                                                                                     0,
                                                                                                                     NULL,
                                                                                                                     true);

                                                _result->entry_points_private.pGLEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

                                                ASSERT_DEBUG_SYNC(_result->entry_points_gl.pGLGetError() == GL_NO_ERROR,
                                                                  "Could not set up OpenGL debug output callbacks!");
                                            }
                                            #endif
                                        }

                                        /* Retrieve context-specific multi-sampling support info */
                                        if (allow_multisampling)
                                        {
                                            _ogl_context_retrieve_multisampling_support_info(_result);
                                        }

                                        /* Set up cache storage */
                                        ogl_context_bo_bindings_init        (_result->bo_bindings,
                                                                            &_result->entry_points_private);
                                        ogl_context_sampler_bindings_init   (_result->sampler_bindings,
                                                                            &_result->entry_points_private);
                                        ogl_context_state_cache_init        (_result->state_cache,
                                                                            &_result->entry_points_private);
                                        ogl_context_texture_compression_init(_result->texture_compression,
                                                                            &_result->entry_points_private);
                                        ogl_context_to_bindings_init        (_result->to_bindings,
                                                                            &_result->entry_points_private);

                                        /* Initialize shadow mapping handler */
                                        _result->shadow_mapping = ogl_shadow_mapping_create( (ogl_context) _result);

                                        /* Set up the zero-VAA VAO */
                                        _result->entry_points_gl.pGLGenVertexArrays(1,
                                                                                   &_result->vao_no_vaas_id);

                                    } /* if (type == OGL_CONTEXT_TYPE_GL) */

                                    /* Set context-specific vsync setting */
                                    ogl_context_set_vsync( (ogl_context) _result, vsync_enabled);

                                    /* Update gfx_image alternative file getter provider so that it can
                                     * search for compressed textures supported by this rendering context.
                                     */
                                    gfx_image_set_global_property(GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_FUNC_PTR,
                                                                  _ogl_context_get_compressed_filename);
                                    gfx_image_set_global_property(GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_USER_ARG,
                                                                  _result);

                                    /* Log GPU info */
                                    LOG_INFO("Renderer:                 [%s]\n"
                                             "Shading language version: [%s]\n"
                                             "Vendor:                   [%s]\n"
                                             "Version:                  [%s]\n",
                                             _result->info.renderer,
                                             _result->info.shading_language_version,
                                             _result->info.vendor,
                                             _result->info.version);
                                }
                            }
                        }

                        /* Unbind the thread from the context. It is to be picked up by rendering thread */
                        ::wglMakeCurrent(NULL, NULL);

                        /* Release the temporary context now. */
                        ::wglDeleteContext(temp_wgl_context);
                    }
                }
            }
            else
            {
                LOG_FATAL("Could not retrieve filled pixel format descriptor.");
            }
        }
    }
    else
    {
        LOG_FATAL("Could not retrieve system window handle.");
    }

    return result;
}

/* Please see header for spec */
PUBLIC ogl_context ogl_context_get_current_context()
{
    return  _current_context;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_context_get_property(__in  __notnull ogl_context          context,
                                                 __in            ogl_context_property property,
                                                 __out __notnull void*                out_result)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    switch (property)
    {
        case OGL_CONTEXT_PROPERTY_BO_BINDINGS:
        {
            *((ogl_context_bo_bindings*) out_result) = context_ptr->bo_bindings;

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

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_COMPUTE_SHADER:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_COMPUTE_SHADER property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_compute_shader**) out_result) = &context_ptr->entry_points_gl_arb_compute_shader;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_FRAMEBUFFER_NO_ATTACHMENTS:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_FRAMEBUFFER_NO_ATTACHMENTS property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_framebuffer_no_attachments**) out_result) = &context_ptr->entry_points_gl_arb_framebuffer_no_attachments;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_PROGRAM_INTERFACE_QUERY:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_PROGRAM_INTERFACE_QUERY property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_program_interface_query**) out_result) = &context_ptr->entry_points_gl_arb_program_interface_query;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SHADER_STORAGE_BUFFER_OBJECT:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SHADER_STORAGE_BUFFER_OBJECT property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_shader_storage_buffer_object**) out_result) = &context_ptr->entry_points_gl_arb_shader_storage_buffer_object;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_TEXTURE_STORAGE_MULTISAMPLE:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_TEXTURE_STORAGE_MULTISAMPLE property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_arb_texture_storage_multisample**) out_result) = &context_ptr->entry_points_gl_arb_texture_storage_multisample;

            break;
        }

        case OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS:
        {
            ASSERT_DEBUG_SYNC(context_ptr->context_type == OGL_CONTEXT_TYPE_GL,
                              "OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS property requested for a non-GL context");

            *((const ogl_context_gl_entrypoints_ext_direct_state_access**) out_result) = &context_ptr->entry_points_gl_ext_direct_state_access;

            break;
        }

        case OGL_CONTEXT_PROPERTY_INFO:
        {
            *((const ogl_context_gl_info**) out_result) = &context_ptr->info;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS:
        {
            *((const ogl_context_gl_limits**) out_result) = &context_ptr->limits;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS_ARB_COMPUTE_SHADER:
        {
            *((const ogl_context_gl_limits_arb_compute_shader**) out_result) = &context_ptr->limits_arb_compute_shader;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS_ARB_FRAMEBUFFER_NO_ATTACHMENTS:
        {
            *((const ogl_context_gl_limits_arb_framebuffer_no_attachments**) out_result) = &context_ptr->limits_arb_framebuffer_no_attachments;

            break;
        }

        case OGL_CONTEXT_PROPERTY_LIMITS_ARB_SHADER_STORAGE_BUFFER_OBJECT:
        {
            *((const ogl_context_gl_limits_arb_shader_storage_buffer_object**) out_result) = &context_ptr->limits_arb_shader_storage_buffer_object;

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

        case OGL_CONTEXT_PROPERTY_MATERIALS:
        {
            *((ogl_materials*) out_result) = context_ptr->materials;

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

        case OGL_CONTEXT_PROPERTY_SAMPLERS:
        {
            *((ogl_samplers*) out_result) = context_ptr->samplers;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SHADERS:
        {
            *(ogl_shaders*) out_result = context_ptr->shaders;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SHADOW_MAPPING:
        {
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

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_FRAMEBUFFER_NO_ATTACHMENTS:
        {
            *((bool*) out_result) = context_ptr->gl_arb_framebuffer_no_attachments_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_PROGRAM_INTERFACE_QUERY:
        {
            *((bool*) out_result) = context_ptr->gl_arb_program_interface_query_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SHADER_STORAGE_BUFFER_OBJECT:
        {
            *((bool*) out_result) = context_ptr->gl_arb_shader_storage_buffer_object_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_TEXTURE_STORAGE_MULTISAMPLE:
        {
            *((bool*) out_result) = context_ptr->gl_arb_texture_storage_multisample_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_DIRECT_STATE_ACCESS:
        {
            *((bool*) out_result) = context_ptr->gl_ext_direct_state_access_support;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION:
        {
            *((ogl_context_texture_compression*) out_result) = context_ptr->texture_compression;

            break;
        }

        case OGL_CONTEXT_PROPERTY_TEXTURES:
        {
            *((ogl_textures*) out_result) = context_ptr->textures;

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

#ifdef _WIN32
        case OGL_CONTEXT_PROPERTY_DC:
        {
            *((HDC*) out_result) = ((_ogl_context*)context)->device_context_handle;

            break;
        }

        case OGL_CONTEXT_PROPERTY_GL_CONTEXT:
        {
            *((HGLRC*) out_result) = context_ptr->wgl_rendering_context;

            break;
        }
#endif /* _WIN32 */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context property requested");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_context_is_extension_supported(__in __notnull ogl_context               context,
                                                           __in __notnull system_hashed_ansi_string extension_name)
{
    _ogl_context* context_ptr = (_ogl_context*) context;
    bool          result      = false;

    for (int32_t n_extension = 0; n_extension < context_ptr->limits.num_extensions; ++n_extension)
    {
        if (system_hashed_ansi_string_is_equal_to_raw_string(extension_name, (const char*) context_ptr->info.extensions[n_extension]) )
        {
            result = true;

            break;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool ogl_context_release_managers(__in __notnull ogl_context context)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    if (context_ptr->materials != NULL)
    {
        ogl_materials_release(context_ptr->materials);

        context_ptr->materials = NULL;
    }

    if (context_ptr->programs != NULL)
    {
        ogl_programs_release(context_ptr->programs);

        context_ptr->programs = NULL;
    }

    if (context_ptr->samplers != NULL)
    {
        ogl_samplers_release(context_ptr->samplers);

        context_ptr->samplers = NULL;
    }

    if (context_ptr->shaders != NULL)
    {
        ogl_shaders_release(context_ptr->shaders);

        context_ptr->shaders = NULL;
    }

    if (context_ptr->textures != NULL)
    {
        ogl_textures_release(context_ptr->textures);

        context_ptr->textures = NULL;
    }

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_context_request_callback_from_context_thread(__in __notnull ogl_context                                context,
                                                                         __in __notnull PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback,
                                                                         __in           void*                                      user_arg,
                                                                         __in           bool                                       block_until_available)
{
    bool                  result            = false;
    _ogl_context*         context_ptr       = (_ogl_context*) context;
    ogl_rendering_handler rendering_handler = NULL;

    result = system_window_get_rendering_handler(context_ptr->window,
                                                &rendering_handler);

    ASSERT_DEBUG_SYNC(result && rendering_handler != NULL,
                      "Provided context must be assigned a rendering handler before it is possible to issue blocking calls from GL context thread!");

    if (result &&
        rendering_handler != NULL)
    {
        result = ogl_rendering_handler_request_callback_from_context_thread(rendering_handler,
                                                                            pfn_callback,
                                                                            user_arg,
                                                                            block_until_available);
    }
    else
    {
        result = false;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_context_retrieve_multisampling_info(__in  __notnull ogl_context      context,
                                                                __out __notnull uint32_t*        out_n_multisampling_samples,
                                                                __out __notnull const uint32_t** out_multisampling_samples)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    *out_n_multisampling_samples = context_ptr->n_multisampling_supported_sample;
    *out_multisampling_samples   = context_ptr->multisampling_supported_sample;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_context_set_multisampling(__in __notnull ogl_context context,
                                                      __in           uint32_t    n_samples)
{
    _ogl_context* context_ptr = (_ogl_context*) context;

    return _ogl_set_pixel_format_multisampling(context_ptr, n_samples);
}

/** Please see header for specification */
PUBLIC bool ogl_context_set_vsync(__in __notnull ogl_context context,
                                  __in           bool        vsync_enabled)
{
    _ogl_context* context_ptr = (_ogl_context*) context;
    BOOL          result      = false;

    ASSERT_ALWAYS_SYNC(context_ptr->wgl_swap_control_support,
                       "WGL_EXT_swap_control extension not supported. Update your drivers.");

    if (context_ptr->pWGLSwapIntervalEXT != NULL)
    {
        int swap_interval = (context_ptr->wgl_swap_control_tear_support) ? -1 : /* use adaptive vsync  */
                                                                            1;  /* force regular vsync */

        result = context_ptr->pWGLSwapIntervalEXT(vsync_enabled ? swap_interval
                                                                : 0);

        ASSERT_DEBUG_SYNC(result == TRUE,
                          "Failed to set VSync");
    }

    return result == TRUE ? true : false;
}

